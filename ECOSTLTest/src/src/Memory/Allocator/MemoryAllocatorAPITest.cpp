// ヘッダー単体でコンパイルできることを確かめるため、最初に読み込む
#include "ECOSTL/src/Memory/Allocate/MemoryAllocatorAPI.h"

#include <gtest/gtest.h>

#include <vector>


namespace
{
	/// <summary>
	/// 呼び出し内容を記録するテスト用のアロケータ
	/// </summary>
	class RecordingAllocator
	{
	public:
		void* Allocate(uint32_t size, uint32_t align)
		{
			lastSize = size;
			lastAlign = align;
			++allocCount;
			return malloc(size);
		}

		bool Free(void* memory)
		{
			freed.push_back(memory);
			free(memory);
			return freeResult;
		}

		uint32_t lastSize = 0;
		uint32_t lastAlign = 0;
		int allocCount = 0;
		std::vector<void*> freed;
		bool freeResult = true;
	};

	utl::MemoryAllocatorDelegate MakeDelegate(RecordingAllocator* alloc)
	{
		utl::MemoryAllocatorDelegate delegate;
		delegate.Register<RecordingAllocator, &RecordingAllocator::Allocate, &RecordingAllocator::Free>(alloc);
		return delegate;
	}
}

//----------------------------------------
// MemoryAllocatorDelegate
//----------------------------------------

TEST(MemoryAllocatorDelegate, UnregisteredAllocateReturnsNull) {
	utl::MemoryAllocatorDelegate delegate;

	EXPECT_EQ(delegate.Allocate(16, 8), nullptr);
}

TEST(MemoryAllocatorDelegate, UnregisteredFreeReturnsFalse) {
	utl::MemoryAllocatorDelegate delegate;
	int dummy = 0;

	EXPECT_FALSE(delegate.Free(&dummy));
}

TEST(MemoryAllocatorDelegate, AllocateForwardsSizeAndAlign) {
	RecordingAllocator alloc;
	utl::MemoryAllocatorDelegate delegate = MakeDelegate(&alloc);

	void* mem = delegate.Allocate(32, 16);

	EXPECT_NE(mem, nullptr);
	EXPECT_EQ(alloc.allocCount, 1);
	EXPECT_EQ(alloc.lastSize, 32u);
	EXPECT_EQ(alloc.lastAlign, 16u);

	delegate.Free(mem);
}

TEST(MemoryAllocatorDelegate, FreeForwardsPointerAndResult) {
	RecordingAllocator alloc;
	utl::MemoryAllocatorDelegate delegate = MakeDelegate(&alloc);

	void* mem = delegate.Allocate(8, 0);
	EXPECT_TRUE(delegate.Free(mem));
	ASSERT_EQ(alloc.freed.size(), 1u);
	EXPECT_EQ(alloc.freed[0], mem);

	alloc.freeResult = false;
	void* mem2 = delegate.Allocate(8, 0);
	EXPECT_FALSE(delegate.Free(mem2));
}

TEST(MemoryAllocatorDelegate, ReRegisterSwitchesAllocator) {
	RecordingAllocator first;
	RecordingAllocator second;
	utl::MemoryAllocatorDelegate delegate = MakeDelegate(&first);

	delegate.Register<RecordingAllocator, &RecordingAllocator::Allocate, &RecordingAllocator::Free>(&second);
	void* mem = delegate.Allocate(4, 0);
	delegate.Free(mem);

	EXPECT_EQ(first.allocCount, 0);
	EXPECT_EQ(second.allocCount, 1);
	EXPECT_EQ(second.freed.size(), 1u);
}

//----------------------------------------
// MemoryAllocatorRef
//----------------------------------------

TEST(MemoryAllocatorRef, DefaultHasNoDelegate) {
	utl::MemoryAllocatorRef ref;

	EXPECT_EQ(ref.Get(), nullptr);
}

TEST(MemoryAllocatorRef, DefaultUsesStandardAllocation) {
	utl::MemoryAllocatorRef ref;

	void* mem = ref.Allocate(64);
	ASSERT_NE(mem, nullptr);
	// 書き込めることを確認
	static_cast<char*>(mem)[63] = 1;

	ref.Free(mem);
}

TEST(MemoryAllocatorRef, AllocateUsesDelegate) {
	RecordingAllocator alloc;
	utl::MemoryAllocatorDelegate delegate = MakeDelegate(&alloc);
	utl::MemoryAllocatorRef ref(&delegate);

	void* mem = ref.Allocate(24, 8);

	EXPECT_EQ(alloc.allocCount, 1);
	EXPECT_EQ(alloc.lastSize, 24u);
	EXPECT_EQ(alloc.lastAlign, 8u);

	ref.Free(mem);
	ASSERT_EQ(alloc.freed.size(), 1u);
	EXPECT_EQ(alloc.freed[0], mem);
}

TEST(MemoryAllocatorRef, AllocateDefaultAlignIsZero) {
	RecordingAllocator alloc;
	utl::MemoryAllocatorDelegate delegate = MakeDelegate(&alloc);
	utl::MemoryAllocatorRef ref(&delegate);

	void* mem = ref.Allocate(4);

	EXPECT_EQ(alloc.lastAlign, 0u);

	ref.Free(mem);
}

TEST(MemoryAllocatorRef, TypedAllocateRequestsSizeForCount) {
	RecordingAllocator alloc;
	utl::MemoryAllocatorDelegate delegate = MakeDelegate(&alloc);
	utl::MemoryAllocatorRef ref(&delegate);

	double* mem = ref.Allocate<double>(5);

	EXPECT_NE(mem, nullptr);
	EXPECT_EQ(alloc.lastSize, sizeof(double) * 5);

	ref.Free(mem);
}

TEST(MemoryAllocatorRef, CopySharesDelegate) {
	RecordingAllocator alloc;
	utl::MemoryAllocatorDelegate delegate = MakeDelegate(&alloc);
	utl::MemoryAllocatorRef ref(&delegate);

	utl::MemoryAllocatorRef copy(ref);

	EXPECT_EQ(copy.Get(), &delegate);
}

TEST(MemoryAllocatorRef, ResetChangesDelegate) {
	RecordingAllocator alloc;
	utl::MemoryAllocatorDelegate delegate = MakeDelegate(&alloc);
	utl::MemoryAllocatorRef ref;

	ref.Reset(&delegate);
	EXPECT_EQ(ref.Get(), &delegate);

	ref.Reset();
	EXPECT_EQ(ref.Get(), nullptr);

	// 標準の手段に戻っていること
	void* mem = ref.Allocate(4);
	ref.Free(mem);
	EXPECT_EQ(alloc.allocCount, 0);
}

//----------------------------------------
// 不具合の再現
//----------------------------------------

namespace
{
	struct alignas(64) OverAligned
	{
		char data[64];
	};
}

TEST(MemoryAllocatorRef, TypedAllocatePassesAlignment) {
	RecordingAllocator alloc;
	utl::MemoryAllocatorDelegate delegate = MakeDelegate(&alloc);
	utl::MemoryAllocatorRef ref(&delegate);

	OverAligned* mem = ref.Allocate<OverAligned>(2);

	EXPECT_EQ(alloc.lastAlign, alignof(OverAligned));

	ref.Free(mem);
}

TEST(MemoryAllocatorRef, StandardAllocationHonorsAlignment) {
	utl::MemoryAllocatorRef ref;

	void* mem = ref.Allocate(100, 256);
	ASSERT_NE(mem, nullptr);
	EXPECT_EQ(reinterpret_cast<uintptr_t>(mem) % 256, 0u);

	ref.Free(mem);
}

TEST(MemoryAllocatorRef, TypedAllocateOverflowReturnsNull) {
	RecordingAllocator alloc;
	utl::MemoryAllocatorDelegate delegate = MakeDelegate(&alloc);
	utl::MemoryAllocatorRef ref(&delegate);

	// sizeof(double) * num が uint32_t に収まらない
	double* mem = ref.Allocate<double>(0x40000000u);

	EXPECT_EQ(mem, nullptr);
	EXPECT_EQ(alloc.allocCount, 0);
}

TEST(MemoryAllocatorRef, FreeNullIsSafe) {
	utl::MemoryAllocatorRef ref;

	EXPECT_TRUE(ref.Free(nullptr));
}
