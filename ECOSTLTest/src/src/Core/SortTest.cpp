// ヘッダー単体でコンパイルできることを確かめるため、最初に読み込む
#include "ECOSTL/src/Core/Sort.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace
{
	struct Record
	{
		int key;
		int order;
	};

	// 重複を多く含む疑似乱数列を作る
	std::vector<int> MakeRandom(const uint32_t count, const int range)
	{
		std::vector<int> values(count);
		uint32_t state = 12345u;
		for (uint32_t i = 0; i < count; i++)
		{
			state = state * 1664525u + 1013904223u;
			values[i] = static_cast<int>((state >> 8) % static_cast<uint32_t>(range)) - range / 2;
		}
		return values;
	}

	std::vector<Record> MakeRecords(const uint32_t count, const int range)
	{
		const std::vector<int> keys = MakeRandom(count, range);
		std::vector<Record> records(count);
		for (uint32_t i = 0; i < count; i++)
		{
			records[i] = { keys[i], static_cast<int>(i) };
		}
		return records;
	}

	bool IsAscending(const std::vector<int>& v)
	{
		for (size_t i = 1; i < v.size(); i++)
		{
			if (v[i - 1] > v[i]) return false;
		}
		return true;
	}

	bool IsDescending(const std::vector<int>& v)
	{
		for (size_t i = 1; i < v.size(); i++)
		{
			if (v[i - 1] < v[i]) return false;
		}
		return true;
	}

	// 同じキーの要素が元の順序のままか
	bool IsStable(const std::vector<Record>& v)
	{
		for (size_t i = 1; i < v.size(); i++)
		{
			if (v[i - 1].key == v[i].key && v[i - 1].order > v[i].order) return false;
		}
		return true;
	}

	struct AbsLess
	{
		bool operator()(const int a, const int b) const
		{
			return (a < 0 ? -a : a) < (b < 0 ? -b : b);
		}
	};
}

TEST(SortTest, AscendingAndDescending)
{
	// 17 以上は IntroSort、16 以下は挿入ソートを通る
	for (const uint32_t count : { 0u, 1u, 2u, 3u, 16u, 17u, 100u, 1000u })
	{
		std::vector<int> v = MakeRandom(count, 50);
		utl::Sort(v.data(), count);
		EXPECT_TRUE(IsAscending(v)) << count;

		v = MakeRandom(count, 50);
		utl::Sort(v.data(), count, true);
		EXPECT_TRUE(IsDescending(v)) << count;
	}
}

TEST(SortTest, KeepsElements)
{
	std::vector<int> v = MakeRandom(500, 40);
	std::vector<int> counts(40, 0);
	for (const int x : v) counts[x + 20]++;
	utl::Sort(v.data(), 500);
	for (const int x : v) counts[x + 20]--;
	for (const int c : counts) EXPECT_EQ(c, 0);
}

TEST(SortTest, SpecialPatterns)
{
	const uint32_t n = 2000;
	std::vector<int> sorted(n), reversed(n), same(n, 7), organ(n);
	for (uint32_t i = 0; i < n; i++)
	{
		sorted[i] = static_cast<int>(i);
		reversed[i] = static_cast<int>(n - i);
		organ[i] = static_cast<int>(i < n / 2 ? i : n - i);
	}
	utl::Sort(sorted.data(), n);
	utl::Sort(reversed.data(), n);
	utl::Sort(same.data(), n);
	utl::Sort(organ.data(), n);
	EXPECT_TRUE(IsAscending(sorted));
	EXPECT_TRUE(IsAscending(reversed));
	EXPECT_TRUE(IsAscending(same));
	EXPECT_TRUE(IsAscending(organ));
}

TEST(SortTest, NullAndTiny)
{
	int* null = nullptr;
	utl::Sort(null, 10);
	int one = 5;
	utl::Sort(&one, 1);
	EXPECT_EQ(one, 5);
}

TEST(SortTest, CompareFunction)
{
	std::vector<int> v = MakeRandom(300, 60);
	utl::Sort(v.data(), 300, AbsLess{});
	for (size_t i = 1; i < v.size(); i++)
	{
		EXPECT_FALSE(AbsLess{}(v[i], v[i - 1]));
	}

	v = MakeRandom(300, 60);
	utl::Sort(v.data(), 300, AbsLess{}, true);
	for (size_t i = 1; i < v.size(); i++)
	{
		EXPECT_FALSE(AbsLess{}(v[i - 1], v[i]));
	}

	v = MakeRandom(300, 60);
	utl::Sort(v.data(), 300, [](const int a, const int b) { return a > b; });
	EXPECT_TRUE(IsDescending(v));
}

TEST(SortTest, Strings)
{
	std::vector<std::string> v = { "pear", "apple", "fig", "banana", "cherry", "apple" };
	utl::Sort(v.data(), static_cast<uint32_t>(v.size()));
	const std::vector<std::string> expected = { "apple", "apple", "banana", "cherry", "fig", "pear" };
	EXPECT_EQ(v, expected);
}

TEST(SortTest, SortArrayWithRelatedArrays)
{
	const uint32_t n = 400;
	std::vector<int> keys = MakeRandom(n, 1000);
	std::vector<int> doubled(n);
	std::vector<std::string> names(n);
	for (uint32_t i = 0; i < n; i++)
	{
		doubled[i] = keys[i] * 2;
		names[i] = std::to_string(keys[i]);
	}
	utl::SortArray(keys.data(), n, false, doubled.data(), names.data());
	EXPECT_TRUE(IsAscending(keys));
	for (uint32_t i = 0; i < n; i++)
	{
		EXPECT_EQ(doubled[i], keys[i] * 2);
		EXPECT_EQ(names[i], std::to_string(keys[i]));
	}

	utl::SortArray(keys.data(), n, true, doubled.data());
	EXPECT_TRUE(IsDescending(keys));
	for (uint32_t i = 0; i < n; i++)
	{
		EXPECT_EQ(doubled[i], keys[i] * 2);
	}
}

TEST(SortTest, SortArrayCmpWithRelatedArray)
{
	const uint32_t n = 300;
	std::vector<int> keys = MakeRandom(n, 200);
	std::vector<int> copy = keys;
	utl::SortArrayCmp(keys.data(), n, AbsLess{}, false, copy.data());
	EXPECT_EQ(keys, copy);
	for (size_t i = 1; i < n; i++)
	{
		EXPECT_FALSE(AbsLess{}(keys[i], keys[i - 1]));
	}
}

TEST(SortTest, SortMember)
{
	std::vector<Record> v = MakeRecords(500, 30);
	utl::SortMember(v.data(), 500, &Record::key);
	for (size_t i = 1; i < v.size(); i++) EXPECT_LE(v[i - 1].key, v[i].key);

	v = MakeRecords(500, 30);
	utl::SortMember(v.data(), 500, &Record::key, true);
	for (size_t i = 1; i < v.size(); i++) EXPECT_GE(v[i - 1].key, v[i].key);

	v = MakeRecords(500, 30);
	utl::SortMemberCmp(v.data(), 500, &Record::key, [](const int a, const int b) { return a > b; });
	for (size_t i = 1; i < v.size(); i++) EXPECT_GE(v[i - 1].key, v[i].key);

	// 並べ替えても要素（キーと元の位置の組）は壊れない
	std::vector<Record> original = MakeRecords(500, 30);
	for (const Record& r : v) EXPECT_EQ(r.key, original[r.order].key);
}

TEST(SortTest, StableSort)
{
	std::vector<Record> v = MakeRecords(1000, 10);
	utl::StableSortMember(v.data(), 1000, utl::MemoryAllocatorRef(), &Record::key);
	EXPECT_TRUE(IsStable(v));
	for (size_t i = 1; i < v.size(); i++) EXPECT_LE(v[i - 1].key, v[i].key);

	// 降順でも同じキーの元の順序は保たれる
	v = MakeRecords(1000, 10);
	utl::StableSortMember(v.data(), 1000, utl::MemoryAllocatorRef(), &Record::key, true);
	EXPECT_TRUE(IsStable(v));
	for (size_t i = 1; i < v.size(); i++) EXPECT_GE(v[i - 1].key, v[i].key);

	v = MakeRecords(1000, 10);
	utl::StableSortMemberCmp(v.data(), 1000, utl::MemoryAllocatorRef(), &Record::key, [](const int a, const int b) { return a > b; });
	EXPECT_TRUE(IsStable(v));
	for (size_t i = 1; i < v.size(); i++) EXPECT_GE(v[i - 1].key, v[i].key);
}

TEST(SortTest, StableSortArrayWithRelatedArrays)
{
	const uint32_t n = 1000;
	const std::vector<int> input = MakeRandom(n, 20);
	std::vector<int> keys = input;
	std::vector<int> order(n);
	std::vector<std::string> names(n);
	for (uint32_t i = 0; i < n; i++)
	{
		order[i] = static_cast<int>(i);
		names[i] = std::to_string(i);
	}
	utl::StableSortArray(keys.data(), n, utl::MemoryAllocatorRef(), false, order.data(), names.data());
	EXPECT_TRUE(IsAscending(keys));
	for (uint32_t i = 0; i < n; i++)
	{
		EXPECT_EQ(keys[i], input[order[i]]);
		EXPECT_EQ(names[i], std::to_string(order[i]));
		if (i > 0 && keys[i - 1] == keys[i]) EXPECT_LT(order[i - 1], order[i]);
	}
}

TEST(SortTest, StableSortWithCompare)
{
	// 絶対値が同じ値（-3 と 3 など）は元の順序のまま
	const uint32_t n = 500;
	const std::vector<int> input = MakeRandom(n, 20);
	std::vector<int> v = input;
	std::vector<int> order(n);
	for (uint32_t i = 0; i < n; i++) order[i] = static_cast<int>(i);
	utl::StableSortArrayCmp(v.data(), n, utl::MemoryAllocatorRef(), AbsLess{}, false, order.data());
	for (uint32_t i = 1; i < n; i++)
	{
		EXPECT_FALSE(AbsLess{}(v[i], v[i - 1]));
		if (!AbsLess{}(v[i - 1], v[i])) EXPECT_LT(order[i - 1], order[i]);
	}

	v = input;
	utl::StableSort(v.data(), n, utl::MemoryAllocatorRef());
	EXPECT_TRUE(IsAscending(v));
	v = input;
	utl::StableSort(v.data(), n, utl::MemoryAllocatorRef(), true);
	EXPECT_TRUE(IsDescending(v));
	v = input;
	utl::StableSort(v.data(), n, utl::MemoryAllocatorRef(), [](const int a, const int b) { return a > b; });
	EXPECT_TRUE(IsDescending(v));
}

TEST(SortTest, StableSortNullAndTiny)
{
	int* null = nullptr;
	utl::StableSort(null, 10, utl::MemoryAllocatorRef());
	int one = 5;
	utl::StableSort(&one, 1, utl::MemoryAllocatorRef());
	EXPECT_EQ(one, 5);
}

namespace
{
	// 確保と解放の回数を数え、failするとnullptrを返すアロケータ
	struct CountingSortAllocator
	{
		void* Allocate(uint32_t size, uint32_t align)
		{
			if (fail)
			{
				return nullptr;
			}
			++allocCount;
			return utl::AlignedAllocate(size, align);
		}
		bool Free(void* memory)
		{
			++freeCount;
			utl::AlignedFree(memory);
			return true;
		}
		int allocCount = 0;
		int freeCount = 0;
		bool fail = false;
	};
}

TEST(SortTest, StableSortUsesGivenAllocator)
{
	CountingSortAllocator counter;
	utl::MemoryAllocatorDelegate delegate;
	delegate.Register<CountingSortAllocator, &CountingSortAllocator::Allocate, &CountingSortAllocator::Free>(&counter);

	std::vector<int> v = { 5, 3, 9, 1, 7, 2 };
	EXPECT_TRUE(utl::StableSort(v.data(), static_cast<uint32_t>(v.size()), utl::MemoryAllocatorRef(&delegate)));
	EXPECT_EQ(counter.allocCount, 1);
	EXPECT_EQ(counter.freeCount, 1);
	EXPECT_TRUE(std::is_sorted(v.begin(), v.end()));
}

TEST(SortTest, StableSortAllocationFailureLeavesArrayUntouched)
{
	CountingSortAllocator counter;
	counter.fail = true;
	utl::MemoryAllocatorDelegate delegate;
	delegate.Register<CountingSortAllocator, &CountingSortAllocator::Allocate, &CountingSortAllocator::Free>(&counter);

	std::vector<int> v = { 5, 3, 9, 1 };
	EXPECT_FALSE(utl::StableSort(v.data(), static_cast<uint32_t>(v.size()), utl::MemoryAllocatorRef(&delegate)));
	EXPECT_EQ(v, (std::vector<int>{ 5, 3, 9, 1 }));
}
