#include "ECOSTL/Core/Atomic/Atomic.h"
#include "ECOSTL/Core/Platform/Platform.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <thread>
#include <vector>

namespace
{
	enum class Color : uint8_t
	{
		Red,
		Green,
		Blue,
	};
}

TEST(Atomic, DefaultIsZero)
{
	utl::Atomic<uint32_t> value;
	EXPECT_EQ(value.Load(), 0u);

	utl::Atomic<int*> pointer;
	EXPECT_EQ(pointer.Load(), nullptr);
}

TEST(Atomic, StoreAndLoadInEveryOrder)
{
	utl::Atomic<uint64_t> value(1);
	value.Store(2, utl::MemoryOrder::Relaxed);
	EXPECT_EQ(value.Load(utl::MemoryOrder::Relaxed), 2u);
	value.Store(3, utl::MemoryOrder::Release);
	EXPECT_EQ(value.Load(utl::MemoryOrder::Acquire), 3u);
	value.Store(4, utl::MemoryOrder::SequentiallyConsistent);
	EXPECT_EQ(value.Load(utl::MemoryOrder::SequentiallyConsistent), 4u);
}

TEST(Atomic, EverySizeKeepsAllBits)
{
	utl::Atomic<uint8_t> a(0xA5);
	utl::Atomic<uint16_t> b(0xA5C3);
	utl::Atomic<uint32_t> c(0xA5C3F00Fu);
	utl::Atomic<uint64_t> d(0xA5C3F00F12345678ull);
	EXPECT_EQ(a.Load(), 0xA5);
	EXPECT_EQ(b.Load(), 0xA5C3);
	EXPECT_EQ(c.Load(), 0xA5C3F00Fu);
	EXPECT_EQ(d.Load(), 0xA5C3F00F12345678ull);

	EXPECT_EQ(a.Exchange(0x5A), 0xA5);
	EXPECT_EQ(b.Exchange(0x3C5A), 0xA5C3);
	EXPECT_EQ(c.Exchange(0x0FF03C5Au), 0xA5C3F00Fu);
	EXPECT_EQ(d.Exchange(0x876543210FF03C5Aull), 0xA5C3F00F12345678ull);
	EXPECT_EQ(a.Load(), 0x5A);
	EXPECT_EQ(b.Load(), 0x3C5A);
	EXPECT_EQ(c.Load(), 0x0FF03C5Au);
	EXPECT_EQ(d.Load(), 0x876543210FF03C5Aull);
}

TEST(Atomic, CompareExchangeSucceedsOnlyWhenEqual)
{
	utl::Atomic<int32_t> value(10);

	int32_t expected = 10;
	EXPECT_TRUE(value.CompareExchange(expected, 20));
	EXPECT_EQ(value.Load(), 20);
	EXPECT_EQ(expected, 10);

	// 違うときは書かず、今の値を expected に入れる
	expected = 10;
	EXPECT_FALSE(value.CompareExchange(expected, 30));
	EXPECT_EQ(value.Load(), 20);
	EXPECT_EQ(expected, 20);
}

TEST(Atomic, CompareExchangeWeakEventuallySucceeds)
{
	utl::Atomic<uint16_t> value(7);
	uint16_t expected = 7;
	while (!value.CompareExchangeWeak(expected, 8))
	{
		ASSERT_EQ(expected, 7);
	}
	EXPECT_EQ(value.Load(), 8);
}

TEST(Atomic, FetchAddAndSubReturnPreviousValue)
{
	utl::Atomic<int64_t> value(5);
	EXPECT_EQ(value.FetchAdd(3), 5);
	EXPECT_EQ(value.Load(), 8);
	EXPECT_EQ(value.FetchSub(10), 8);
	EXPECT_EQ(value.Load(), -2);
}

TEST(Atomic, FetchAddWrapsAround)
{
	utl::Atomic<uint8_t> small(250);
	EXPECT_EQ(small.FetchAdd(10), 250);
	EXPECT_EQ(small.Load(), 4);

	utl::Atomic<uint32_t> zero(0);
	EXPECT_EQ(zero.FetchSub(1), 0u);
	EXPECT_EQ(zero.Load(), UINT32_MAX);
}

TEST(Atomic, WorksWithPointerEnumAndBool)
{
	int a = 1;
	int b = 2;
	utl::Atomic<int*> pointer(&a);
	EXPECT_EQ(pointer.Exchange(&b), &a);
	int* expected = &b;
	EXPECT_TRUE(pointer.CompareExchange(expected, nullptr));
	EXPECT_EQ(pointer.Load(), nullptr);

	utl::Atomic<Color> color(Color::Green);
	EXPECT_EQ(color.Exchange(Color::Blue), Color::Green);
	EXPECT_EQ(color.Load(), Color::Blue);

	utl::Atomic<bool> flag(false);
	EXPECT_FALSE(flag.Exchange(true));
	EXPECT_TRUE(flag.Load());
}

TEST(Atomic, ConcurrentFetchAddLosesNothing)
{
	constexpr uint32_t THREAD_NUM = 8;
	constexpr uint32_t ADD_NUM = 100000;

	utl::Atomic<uint64_t> total;
	std::vector<std::thread> threads;
	for (uint32_t t = 0; t < THREAD_NUM; ++t)
	{
		threads.emplace_back([&total]
			{
				for (uint32_t i = 0; i < ADD_NUM; ++i)
				{
					total.FetchAdd(1, utl::MemoryOrder::Relaxed);
				}
			});
	}
	for (std::thread& thread : threads)
	{
		thread.join();
	}
	EXPECT_EQ(total.Load(), static_cast<uint64_t>(THREAD_NUM) * ADD_NUM);
}

TEST(Atomic, ConcurrentCompareExchangeLosesNothing)
{
	constexpr uint32_t THREAD_NUM = 8;
	constexpr uint32_t ADD_NUM = 50000;

	// 比較交換で「読む → 1 足して書く」を作る。他のスレッドに先を越されたらやり直す
	utl::Atomic<uint32_t> total;
	std::vector<std::thread> threads;
	for (uint32_t t = 0; t < THREAD_NUM; ++t)
	{
		threads.emplace_back([&total]
			{
				for (uint32_t i = 0; i < ADD_NUM; ++i)
				{
					uint32_t current = total.Load(utl::MemoryOrder::Relaxed);
					while (!total.CompareExchangeWeak(current, current + 1))
					{
						UTL_CPU_PAUSE();
					}
				}
			});
	}
	for (std::thread& thread : threads)
	{
		thread.join();
	}
	EXPECT_EQ(total.Load(), THREAD_NUM * ADD_NUM);
}

TEST(Atomic, ExchangeWorksAsLock)
{
	constexpr uint32_t THREAD_NUM = 8;
	constexpr uint32_t ADD_NUM = 20000;

	// Exchange で作った鍵で、原子的でない加算を守る。鍵が効いていなければ合計が合わなくなる
	utl::Atomic<bool> locked(false);
	uint64_t unprotected = 0;
	std::vector<std::thread> threads;
	for (uint32_t t = 0; t < THREAD_NUM; ++t)
	{
		threads.emplace_back([&locked, &unprotected]
			{
				for (uint32_t i = 0; i < ADD_NUM; ++i)
				{
					while (locked.Exchange(true, utl::MemoryOrder::Acquire))
					{
						UTL_CPU_PAUSE();
					}
					unprotected += 1;
					locked.Store(false, utl::MemoryOrder::Release);
				}
			});
	}
	for (std::thread& thread : threads)
	{
		thread.join();
	}
	EXPECT_EQ(unprotected, static_cast<uint64_t>(THREAD_NUM) * ADD_NUM);
}
