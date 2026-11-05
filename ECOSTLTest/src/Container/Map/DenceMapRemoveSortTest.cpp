// DenceMap / ChunkDenceMap の Remove と Sort / SortByKey の単体テスト（旧 testGame/extras/DenceMapSelfTest.cpp）。
// 確保回数は、マップに渡した CountingAllocator で数える（グローバルの operator new は差し替えない）。
#include <gtest/gtest.h>

#include "ECOSTL.hpp"

#include "ECOSTL/Memory/Allocator/CountingAllocator.h"
#include "ECOSTL/Memory/Allocator/MallocAllocator.h"

#include <cstdio>
#include <vector>

namespace
{
	// ok が偽なら msg を添えて gtest の失敗にする。
	void Expect(bool ok, const char* msg)
	{
		EXPECT_TRUE(ok) << msg;
	}

	// 値 = key * 10 + 7
	int ValueOf(utl::SparseKey k) { return static_cast<int>(k) * 10 + 7; }

	// 残っているキー全部で値が正しいか
	template<typename Map>
	bool CheckAll(Map& m, const std::vector<utl::SparseKey>& alive, const std::vector<utl::SparseKey>& dead)
	{
		for (auto k : alive)
		{
			int* v = m.Find(k);
			if (!v || *v != ValueOf(k)) { return false; }
		}
		for (auto k : dead)
		{
			if (m.Find(k) != nullptr) { return false; }
		}
		return m.Size() == alive.size();
	}

	// keys の全てのキーを、ValueOf(key) を値にして m へ挿入する。
	template<typename Map>
	void FillMap(Map& m, const std::vector<utl::SparseKey>& keys)
	{
		for (auto k : keys) { m.Insert(k, ValueOf(k)); }
	}

	// Remove を繰り返しても残りの要素・個数・確保回数が正しいか確かめる。
	template<typename Map>
	void TestRemove(const char* name)
	{
		// 途中・先頭・末尾・唯一の要素
		const std::vector<utl::SparseKey> keys = { 3, 100, 7, 42, 9 };
		for (size_t victim = 0; victim < keys.size(); ++victim)
		{
			Map m;
			FillMap(m, keys);
			m.Remove(keys[victim]);
			std::vector<utl::SparseKey> alive, dead = { keys[victim] };
			for (size_t i = 0; i < keys.size(); ++i) { if (i != victim) { alive.push_back(keys[i]); } }
			char buf[128];
			std::snprintf(buf, sizeof(buf), "%s Remove 位置%zu のあと残りの値が正しい", name, victim);
			Expect(CheckAll(m, alive, dead), buf);
		}
		{
			Map m;
			m.Insert(5u, ValueOf(5));
			m.Remove(5u);
			Expect(m.Size() == 0 && m.Find(5u) == nullptr, "唯一の要素を消す");
			m.Insert(5u, ValueOf(5));
			Expect(m.Find(5u) && *m.Find(5u) == ValueOf(5), "消したあと同じキーを入れ直せる");
		}
		{
			// 連続して消す
			Map m;
			std::vector<utl::SparseKey> alive, dead;
			for (utl::SparseKey k = 0; k < 200; k += 3) { alive.push_back(k); }
			FillMap(m, alive);
			unsigned s = 12345;
			while (alive.size() > 1)
			{
				s = s * 1664525u + 1013904223u;
				size_t i = (s >> 8) % alive.size();
				m.Remove(alive[i]);
				dead.push_back(alive[i]);
				alive.erase(alive.begin() + i);
				if (!CheckAll(m, alive, dead)) { Expect(false, "ランダムに消し続けて整合"); break; }
			}
		}
	}

	// 並べ替えの試験用の要素（優先度と値）。
	struct Item
	{
		int prio;   // 並べ替えに使う優先度
		int value; // = ValueOf(key)
	};

	// sorter で並べ替えた後、優先度順になり、キーと値の対応が保たれるか確かめる。
	template<typename Sorter>
	void TestSortImpl(const char* name, Sorter sorter, bool descending)
	{
		for (uint32_t n : { 0u, 1u, 2u, 3u, 17u, 1000u })
		{
			// Sort の確保ゼロの確認用に、マップの確保を数える
			utl::MallocAllocator baseAllocator;
			utl::CountingAllocator counting{ baseAllocator.GetAPI() };
			utl::DenceMap<Item> m(counting.GetAPI());
			std::vector<utl::SparseKey> keys;
			unsigned s = 777 + n;
			for (uint32_t i = 0; i < n; ++i)
			{
				s = s * 1664525u + 1013904223u;
				utl::SparseKey k = i * 5 + 2;
				keys.push_back(k);
				m.Insert(k, Item{ static_cast<int>((s >> 12) % 50), ValueOf(k) });
			}
			// 途中を何件か消した状態でも確認する
			for (uint32_t i = 3; i < n; i += 7) { m.Remove(keys[i]); }
			std::vector<utl::SparseKey> alive;
			std::vector<int> prio(keys.size());
			for (uint32_t i = 0; i < n; ++i)
			{
				if (m.Find(keys[i])) { alive.push_back(keys[i]); prio[i] = m.Find(keys[i])->prio; }
			}
			std::vector<utl::DenceMap<Item>::SortEntry<int>> work(m.Size());

			const uint64_t before = counting.GetStats().count;
			sorter(m, work.data());
			const uint64_t after = counting.GetStats().count;

			char buf[128];
			std::snprintf(buf, sizeof(buf), "%s n=%u 確保ゼロ", name, n);
			Expect(before == after, buf);

			bool ok = m.Size() == alive.size();
			for (auto k : alive)
			{
				Item* it = m.Find(k);
				if (!it || it->value != ValueOf(k)) { ok = false; }
			}
			std::snprintf(buf, sizeof(buf), "%s n=%u 全キーで値が同じ", name, n);
			Expect(ok, buf);

			bool sorted = true;
			const Item* b = m.Begin();
			for (size_t i = 1; i < m.Size(); ++i)
			{
				if (descending ? b[i - 1].prio < b[i].prio : b[i - 1].prio > b[i].prio) { sorted = false; }
			}
			std::snprintf(buf, sizeof(buf), "%s n=%u 並びが整列", name, n);
			Expect(sorted, buf);

			// GetKeys の並びとも整合（添え字 i のキーの値が Begin()[i]）
			bool keyOk = true;
			for (size_t i = 0; i < m.Size(); ++i)
			{
				if (b[i].value != ValueOf(m.GetKeys()[static_cast<uint32_t>(i)])) { keyOk = false; }
			}
			std::snprintf(buf, sizeof(buf), "%s n=%u GetKeys と並びが整合", name, n);
			Expect(keyOk, buf);

			// 並べ替えのあとに消す・足すも壊れない
			if (!alive.empty())
			{
				m.Remove(alive[0]);
				m.Insert(99999u, Item{ 1, ValueOf(99999) });
				bool ok2 = m.Find(alive[0]) == nullptr && m.Find(99999u) && m.Find(99999u)->value == ValueOf(99999);
				for (size_t i = 1; i < alive.size(); ++i) { if (!m.Find(alive[i]) || m.Find(alive[i])->value != ValueOf(alive[i])) { ok2 = false; } }
				std::snprintf(buf, sizeof(buf), "%s n=%u 並べ替え後の削除・追加", name, n);
				Expect(ok2, buf);
			}
		}
	}

	// DenceMap と ChunkDenceMap の Sort / SortByKey を、昇順・降順で確かめる。
	void TestSort()
	{
		TestSortImpl("SortByKey", [](utl::DenceMap<Item>& m, auto* w) { m.SortByKey([](const Item& i) { return i.prio; }, w); }, false);
		TestSortImpl("Sort", [](utl::DenceMap<Item>& m, auto* w) { m.Sort([](const Item& a, const Item& b) { return a.prio > b.prio; }, w); }, true);
	}
}

// DenceMap の Remove が正しく動くか確かめる。
TEST(DenceMap, Remove) { TestRemove<utl::DenceMap<int>>("DenceMap"); }
// ChunkDenceMap の Remove が正しく動くか確かめる。
TEST(DenceMap, ChunkRemove) { TestRemove<utl::ChunkDenceMap<int>>("ChunkDenceMap"); }
// DenceMap / ChunkDenceMap の Sort と SortByKey が正しく並べ替えるか確かめる。
TEST(DenceMap, Sort) { TestSort(); }
