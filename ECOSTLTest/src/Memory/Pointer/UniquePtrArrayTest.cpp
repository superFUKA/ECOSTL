// utl::UniquePtr / MakeUnique / MakeUniqueArray の試験。
#include <gtest/gtest.h>

#include "ECOSTL.hpp"

namespace
{
	int g_alive = 0;   // 生きている Counted の数（リーク・二重解放の検出用）

	// 生成・破棄で g_alive を増減させる、仮想デストラクタ付きの試験用の型。
	struct Counted
	{
		int value;   // 生成時に渡された値
		explicit Counted(const int v = 0) : value(v) { ++g_alive; }
		virtual ~Counted() { --g_alive; }
	};
	// 基底クラス経由の破棄を試すための派生型。
	struct Derived : Counted
	{
		explicit Derived(const int v) : Counted(v) {}
	};
	struct alignas(64) Wide
	{
		float v[16];   // 64 バイト境界を確かめるためのダミーの領域
	};
}

// MakeUnique で作った値が Reset で破棄され、g_alive が元に戻ることを確かめる。
TEST(UniquePtr, MakeAndReset)
{
	g_alive = 0;
	{
		utl::UniquePtr<Counted> p = utl::MakeUnique<Counted>(5);
		ASSERT_TRUE(p);
		EXPECT_EQ(p->value, 5);
		EXPECT_EQ(g_alive, 1);
		utl::UniquePtr<Counted> q = static_cast<utl::UniquePtr<Counted>&&>(p);
		EXPECT_FALSE(p);
		EXPECT_EQ(q.Get()->value, 5);
		q.Reset();
		EXPECT_EQ(g_alive, 0);
	}
	EXPECT_EQ(g_alive, 0);
}

// 基底の UniquePtr に入れた派生型が、破棄時に派生側のデストラクタまで呼ばれることを確かめる。
TEST(UniquePtr, DerivedToBaseDeletesDerived)
{
	g_alive = 0;
	{
		utl::UniquePtr<Counted> p = utl::MakeUnique<Derived>(3);
		EXPECT_EQ(p->value, 3);
		EXPECT_EQ(g_alive, 1);
	}
	EXPECT_EQ(g_alive, 0);
}

// MakeUniqueArray の要素が 0 初期化され、破棄時に各要素のデストラクタが呼ばれることを確かめる。
TEST(UniquePtr, ArrayIsZeroInitializedAndDestroysEach)
{
	utl::UniquePtr<float[]> a = utl::MakeUniqueArray<float>(100);
	for (size_t i = 0; i < 100; ++i) { EXPECT_EQ(a[i], 0.0f); }
	a[7] = 2.0f;
	EXPECT_EQ(a.Get()[7], 2.0f);

	g_alive = 0;
	{
		utl::UniquePtr<Counted[]> c = utl::MakeUniqueArray<Counted>(4);
		EXPECT_EQ(g_alive, 4);
	}
	EXPECT_EQ(g_alive, 0);
}

// 64 バイト境界を要求する型でも、確保されたアドレスが境界に揃うことを確かめる。
TEST(UniquePtr, OverAlignedTypeIsAligned)
{
	utl::UniquePtr<Wide[]> w = utl::MakeUniqueArray<Wide>(3);
	EXPECT_EQ(reinterpret_cast<uintptr_t>(w.Get()) % 64, 0u);
	utl::UniquePtr<Wide> one = utl::MakeUnique<Wide>();
	EXPECT_EQ(reinterpret_cast<uintptr_t>(one.Get()) % 64, 0u);
}

// UniquePtr を Vector に入れて移動しても、破棄が一度ずつ行われることを確かめる。
TEST(UniquePtr, InVector)
{
	g_alive = 0;
	{
		utl::Vector<utl::UniquePtr<Counted>> v;
		for (int i = 0; i < 10; ++i) { v.PushBack(utl::MakeUnique<Counted>(i)); }
		EXPECT_EQ(g_alive, 10);
		EXPECT_EQ(v[9]->value, 9);
	}
	EXPECT_EQ(g_alive, 0);
}
