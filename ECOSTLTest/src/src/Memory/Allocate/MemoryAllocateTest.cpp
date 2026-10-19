// ヘッダー単体でコンパイルできることを確かめるため、最初に読み込む
#include "ECOSTL/src/Memory/Allocate/MemoryAllocate.h"

#include <gtest/gtest.h>

#include <initializer_list>

TEST(AlignedAllocate, DefaultAlignment) {
	// アライメントを指定しなくてもStandardMinAlignの境界に合わせる
	for (uint32_t size : { 1u, 7u, 16u, 100u })
	{
		void* mem = utl::AlignedAllocate(size);
		ASSERT_NE(mem, nullptr);
		EXPECT_EQ(reinterpret_cast<uintptr_t>(mem) % utl::StandardMinAlign, 0u);
		utl::AlignedFree(mem);
	}
}

TEST(AlignedAllocate, VariousAlignments) {
	for (uint32_t align = 1; align <= 4096; align *= 2)
	{
		for (uint32_t size : { 0u, 1u, align - 1, align, align + 1, 1000u })
		{
			void* mem = utl::AlignedAllocate(size, align);
			ASSERT_NE(mem, nullptr);
			EXPECT_EQ(reinterpret_cast<uintptr_t>(mem) % align, 0u) << "align=" << align << " size=" << size;

			// 確保したサイズ全体に書き込めることを確認
			char* bytes = static_cast<char*>(mem);
			for (uint32_t i = 0; i < size; ++i)
			{
				bytes[i] = static_cast<char>(i);
			}

			utl::AlignedFree(mem);
		}
	}
}

TEST(AlignedAllocate, AllocationsDoNotOverlap) {
	char* a = static_cast<char*>(utl::AlignedAllocate(64, 64));
	char* b = static_cast<char*>(utl::AlignedAllocate(64, 64));
	ASSERT_NE(a, nullptr);
	ASSERT_NE(b, nullptr);

	for (int i = 0; i < 64; ++i)
	{
		a[i] = 1;
		b[i] = 2;
	}
	for (int i = 0; i < 64; ++i)
	{
		EXPECT_EQ(a[i], 1);
		EXPECT_EQ(b[i], 2);
	}

	utl::AlignedFree(a);
	utl::AlignedFree(b);
}

TEST(AlignedFree, NullIsSafe) {
	utl::AlignedFree(nullptr);

	SUCCEED();
}
