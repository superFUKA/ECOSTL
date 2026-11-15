#include "ECOSTL/Concurrency/WorkStageGroup.h"

#include <gtest/gtest.h>

#include <atomic>
#include <cstdint>
#include <thread>
#include <vector>

namespace
{
	// 段ごとの記録：仕事 i を実行した回数と、受け取った番号
	struct Recorder
	{
		uint32_t runs[8] = {};
		uint32_t total = 0;
	};

	void Record(void* ctx, const uint32_t index)
	{
		Recorder& recorder = *static_cast<Recorder*>(ctx);
		++recorder.runs[index];
		++recorder.total;
	}
}

TEST(WorkStageGroup, NumbersFollowTheOrderOfAdd)
{
	Recorder a;
	Recorder b;
	Recorder c;
	utl::WorkStageGroup group;
	EXPECT_TRUE(group.Add(utl::WorkStage{ 3, &Record, &a }));
	EXPECT_TRUE(group.Add(utl::WorkStage{ 0, nullptr, nullptr }));  // 仕事が 0 個の段は関数が無くてもよい
	EXPECT_TRUE(group.Add(utl::WorkStage{ 2, &Record, &b }));
	EXPECT_TRUE(group.Add(utl::WorkStage{ 4, &Record, &c }));
	EXPECT_EQ(group.GetStageCount(), 4u);
	EXPECT_EQ(group.GetTotalCount(), 9u);

	const utl::WorkStage work = group.GetWork();
	ASSERT_EQ(work.count, 9u);
	// 番号は段 a の 0..2、段 b の 0..1、段 c の 0..3 に続けて振られる。1 つずつ確かめる
	work.body(work.ctx, 3);
	EXPECT_EQ(b.runs[0], 1u);
	work.body(work.ctx, 5);
	EXPECT_EQ(c.runs[0], 1u);
	work.body(work.ctx, 8);
	EXPECT_EQ(c.runs[3], 1u);

	// 逆の順でも、すべての仕事がちょうど 1 回ずつ実行される(上の 3 つと合わせて 2 回になる所を除く)
	for (uint32_t i = work.count; i-- > 0;)
	{
		work.body(work.ctx, i);
	}
	EXPECT_EQ(a.total, 3u);
	EXPECT_EQ(b.total, 3u);
	EXPECT_EQ(c.total, 6u);
	for (uint32_t i = 0; i < 3; ++i)
	{
		EXPECT_EQ(a.runs[i], 1u);
	}
	EXPECT_EQ(b.runs[0], 2u);
	EXPECT_EQ(b.runs[1], 1u);
	EXPECT_EQ(c.runs[0], 2u);
	EXPECT_EQ(c.runs[1], 1u);
	EXPECT_EQ(c.runs[2], 1u);
	EXPECT_EQ(c.runs[3], 2u);
}

TEST(WorkStageGroup, RunSerialRunsEverything)
{
	Recorder a;
	Recorder b;
	utl::WorkStageGroup group;
	group.Add(utl::WorkStage{ 2, &Record, &a });
	group.Add(utl::WorkStage{ 5, &Record, &b });
	group.GetWork().RunSerial();
	EXPECT_EQ(a.total, 2u);
	EXPECT_EQ(b.total, 5u);
}

TEST(WorkStageGroup, RejectsWhatItCannotHold)
{
	Recorder recorder;
	utl::WorkStageGroup group;
	// 仕事があるのに関数が無い段は足さない
	EXPECT_FALSE(group.Add(utl::WorkStage{ 1, nullptr, &recorder }));
	EXPECT_EQ(group.GetStageCount(), 0u);

	// 上限まで足せて、その次は足さない
	for (uint32_t i = 0; i < utl::WorkStageGroup::MAX_STAGES; ++i)
	{
		EXPECT_TRUE(group.Add(utl::WorkStage{ 1, &Record, &recorder }));
	}
	EXPECT_FALSE(group.Add(utl::WorkStage{ 1, &Record, &recorder }));
	EXPECT_EQ(group.GetStageCount(), utl::WorkStageGroup::MAX_STAGES);
	EXPECT_EQ(group.GetTotalCount(), utl::WorkStageGroup::MAX_STAGES);

	// 仕事の数の和が uint32 を超える段は足さない(それまでの段はそのまま)
	group.Clear();
	EXPECT_TRUE(group.Add(utl::WorkStage{ UINT32_MAX - 1, &Record, &recorder }));
	EXPECT_TRUE(group.Add(utl::WorkStage{ 1, &Record, &recorder }));
	EXPECT_FALSE(group.Add(utl::WorkStage{ 1, &Record, &recorder }));
	EXPECT_EQ(group.GetStageCount(), 2u);
	EXPECT_EQ(group.GetTotalCount(), UINT32_MAX);
}

TEST(WorkStageGroup, ClearEmptiesForReuse)
{
	Recorder a;
	utl::WorkStageGroup group;
	group.Add(utl::WorkStage{ 3, &Record, &a });
	group.Clear();
	EXPECT_EQ(group.GetStageCount(), 0u);
	EXPECT_EQ(group.GetTotalCount(), 0u);
	EXPECT_EQ(group.GetWork().count, 0u);
	group.GetWork().RunSerial();
	EXPECT_EQ(a.total, 0u);

	// 空にした後も、前と同じように使える
	Recorder b;
	EXPECT_TRUE(group.Add(utl::WorkStage{ 2, &Record, &b }));
	group.GetWork().RunSerial();
	EXPECT_EQ(b.total, 2u);
}

namespace
{
	// 仕事ごとに実行した回数を数える段(スレッドから同時に呼ばれる)
	struct CountingStage
	{
		std::atomic<uint32_t> calls[16] = {};
		uint32_t count = 0;

		utl::WorkStage Get()
		{
			return utl::WorkStage{ count, [](void* ctx, const uint32_t index) { static_cast<CountingStage*>(ctx)->calls[index].fetch_add(1); }, this };
		}
	};
}

TEST(WorkStageGroup, RunsEveryWorkOnceFromManyThreads)
{
	for (const uint32_t threads : { 1u, 3u, 8u })
	{
		CountingStage a;
		CountingStage b;
		CountingStage c;
		a.count = 5;
		b.count = 0;
		c.count = 7;
		utl::WorkStageGroup group;
		ASSERT_TRUE(group.Add(a.Get()));
		ASSERT_TRUE(group.Add(b.Get()));
		ASSERT_TRUE(group.Add(c.Get()));
		const utl::WorkStage work = group.GetWork();
		ASSERT_EQ(work.count, 12u);

		// 実行器の代わり：threads 本で番号を取り合う
		std::atomic<uint32_t> next{ 0 };
		std::vector<std::thread> workers;
		for (uint32_t t = 0; t < threads; ++t)
		{
			workers.emplace_back([&]()
				{
					for (uint32_t index = next.fetch_add(1); index < work.count; index = next.fetch_add(1))
					{
						work.body(work.ctx, index);
					}
				});
		}
		for (std::thread& worker : workers)
		{
			worker.join();
		}
		for (uint32_t i = 0; i < a.count; ++i)
		{
			EXPECT_EQ(a.calls[i].load(), 1u) << "threads " << threads;
		}
		for (uint32_t i = 0; i < c.count; ++i)
		{
			EXPECT_EQ(c.calls[i].load(), 1u) << "threads " << threads;
		}
	}
}
