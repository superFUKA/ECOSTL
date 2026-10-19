// ヘッダー単体でコンパイルできることを確かめるため、最初に読み込む
#include "ECOSTL/src/Memory/SmartPointer/SharedPtr.h"

#include <gtest/gtest.h>

#include <set>
#include <utility>


namespace
{
	/// <summary>
	/// 生成と破棄の回数を数えるテスト用の型
	/// </summary>
	struct Tracked
	{
		static inline int alive = 0;
		static inline int destroyed = 0;

		static void ResetCount()
		{
			alive = 0;
			destroyed = 0;
		}

		Tracked(int v = 0, int w = 0)
			: value(v)
			, value2(w)
		{
			++alive;
		}
		Tracked(const Tracked& src)
			: value(src.value)
			, value2(src.value2)
		{
			++alive;
		}
		virtual ~Tracked()
		{
			--alive;
			++destroyed;
		}

		int value;
		int value2;
	};

	struct DerivedTracked : Tracked
	{
		static inline int derivedDestroyed = 0;

		DerivedTracked(int v)
			: Tracked(v)
		{}
		~DerivedTracked() override
		{
			++derivedDestroyed;
		}
	};

	/// <summary>
	/// 確保と開放の回数を数えるテスト用のアロケータ
	/// </summary>
	class CountingAllocator
	{
	public:
		void* Allocate(uint32_t size, uint32_t align)
		{
			++allocCount;
			lastSize = size;
			lastAlign = align;
			lastAllocated = malloc(size);
			live.insert(lastAllocated);
			return lastAllocated;
		}
		bool Free(void* memory)
		{
			++freeCount;
			lastFreed = memory;
			// 確保していないアドレスは開放しない（不正な開放として数える）
			if (live.erase(memory) == 0)
			{
				++badFreeCount;
				return false;
			}
			free(memory);
			return true;
		}

		int allocCount = 0;
		int freeCount = 0;
		int badFreeCount = 0;
		uint32_t lastSize = 0;
		uint32_t lastAlign = 0;
		std::set<void*> live;
		void* lastAllocated = nullptr;
		void* lastFreed = nullptr;
	};

	class SharedPtrTest : public ::testing::Test
	{
	protected:
		void SetUp() override
		{
			Tracked::ResetCount();
			DerivedTracked::derivedDestroyed = 0;
			delegate.Register<CountingAllocator, &CountingAllocator::Allocate, &CountingAllocator::Free>(&alloc);
		}

		CountingAllocator alloc;
		utl::MemoryAllocatorDelegate delegate;
	};
}

//----------------------------------------
// 作成
//----------------------------------------

TEST_F(SharedPtrTest, DefaultIsEmpty) {
	utl::SharedPtr<Tracked> ptr;

	EXPECT_FALSE(ptr);
	EXPECT_EQ(ptr.Get(), nullptr);
}

TEST_F(SharedPtrTest, NullptrConstructIsEmpty) {
	utl::SharedPtr<Tracked> ptr(nullptr);

	EXPECT_FALSE(ptr);
	EXPECT_EQ(ptr.Get(), nullptr);
}

TEST_F(SharedPtrTest, MakeSharedPassesArguments) {
	utl::SharedPtr<Tracked> ptr = utl::MakeShared<Tracked>(3, 4);

	ASSERT_TRUE(ptr);
	EXPECT_EQ(ptr->value, 3);
	EXPECT_EQ(ptr->value2, 4);
	EXPECT_EQ((*ptr).value, 3);
	EXPECT_EQ(Tracked::alive, 1);
}

TEST_F(SharedPtrTest, MakeSharedWithoutArguments) {
	utl::SharedPtr<Tracked> ptr = utl::MakeShared<Tracked>();

	ASSERT_TRUE(ptr);
	EXPECT_EQ(ptr->value, 0);
}

TEST_F(SharedPtrTest, MakeSharedWithAllocAllocatesOnce) {
	{
		utl::SharedPtr<Tracked> ptr = utl::MakeSharedWithAlloc<Tracked>(&delegate, 7);

		ASSERT_TRUE(ptr);
		EXPECT_EQ(ptr->value, 7);
		// インスタンスとカウンタをまとめて1回で確保している
		EXPECT_EQ(alloc.allocCount, 1);
		EXPECT_EQ(alloc.lastSize, sizeof(utl::RefCounterToIns<Tracked>));
		EXPECT_EQ(alloc.freeCount, 0);
	}

	EXPECT_EQ(alloc.freeCount, 1);
	EXPECT_EQ(alloc.lastFreed, alloc.lastAllocated);
	EXPECT_EQ(Tracked::alive, 0);
}

TEST_F(SharedPtrTest, WritesThroughPointer) {
	utl::SharedPtr<Tracked> ptr = utl::MakeShared<Tracked>(1);

	ptr->value = 10;
	(*ptr).value2 = 20;

	EXPECT_EQ(ptr.Get()->value, 10);
	EXPECT_EQ(ptr.Get()->value2, 20);
}

//----------------------------------------
// コピー
//----------------------------------------

TEST_F(SharedPtrTest, CopyConstructSharesInstance) {
	utl::SharedPtr<Tracked> a = utl::MakeShared<Tracked>(1);
	utl::SharedPtr<Tracked> b(a);

	EXPECT_TRUE(a);
	EXPECT_TRUE(b);
	EXPECT_EQ(a.Get(), b.Get());
	EXPECT_EQ(Tracked::alive, 1);
}

TEST_F(SharedPtrTest, CopyConstructFromEmpty) {
	utl::SharedPtr<Tracked> a;
	utl::SharedPtr<Tracked> b(a);

	EXPECT_FALSE(b);
}

TEST_F(SharedPtrTest, ChangesAreVisibleFromCopies) {
	utl::SharedPtr<Tracked> a = utl::MakeShared<Tracked>(1);
	utl::SharedPtr<Tracked> b = a;

	b->value = 42;

	EXPECT_EQ(a->value, 42);
}

TEST_F(SharedPtrTest, ReleasedWhenLastReferenceGone) {
	utl::SharedPtr<Tracked> a = utl::MakeSharedWithAlloc<Tracked>(&delegate, 1);
	{
		utl::SharedPtr<Tracked> b = a;
		utl::SharedPtr<Tracked> c = b;
		EXPECT_EQ(Tracked::alive, 1);
	}

	// まだaが参照している
	EXPECT_EQ(Tracked::alive, 1);
	EXPECT_EQ(alloc.freeCount, 0);

	a.Reset();
	EXPECT_EQ(Tracked::alive, 0);
	EXPECT_EQ(Tracked::destroyed, 1);
	EXPECT_EQ(alloc.freeCount, 1);
}

TEST_F(SharedPtrTest, CopyAssignSharesInstance) {
	utl::SharedPtr<Tracked> a = utl::MakeShared<Tracked>(1);
	utl::SharedPtr<Tracked> b;

	b = a;

	EXPECT_EQ(a.Get(), b.Get());
	EXPECT_EQ(Tracked::alive, 1);

	a.Reset();
	EXPECT_EQ(Tracked::alive, 1);
	EXPECT_EQ(b->value, 1);
}

TEST_F(SharedPtrTest, CopyAssignReleasesPreviousInstance) {
	utl::SharedPtr<Tracked> a = utl::MakeShared<Tracked>(1);
	utl::SharedPtr<Tracked> b = utl::MakeSharedWithAlloc<Tracked>(&delegate, 2);

	b = a;

	EXPECT_EQ(b->value, 1);
	EXPECT_EQ(Tracked::alive, 1);
	EXPECT_EQ(Tracked::destroyed, 1);
	EXPECT_EQ(alloc.freeCount, 1);
}

TEST_F(SharedPtrTest, CopyAssignEmpty) {
	utl::SharedPtr<Tracked> a;
	utl::SharedPtr<Tracked> b = utl::MakeShared<Tracked>(1);

	b = a;

	EXPECT_FALSE(b);
	EXPECT_EQ(Tracked::alive, 0);
}

TEST_F(SharedPtrTest, SelfAssignKeepsInstance) {
	utl::SharedPtr<Tracked> a = utl::MakeShared<Tracked>(1);
	utl::SharedPtr<Tracked>& ref = a;

	a = ref;

	ASSERT_TRUE(a);
	EXPECT_EQ(a->value, 1);
	EXPECT_EQ(Tracked::alive, 1);
}

TEST_F(SharedPtrTest, AssignSameInstanceKeepsCount) {
	utl::SharedPtr<Tracked> a = utl::MakeShared<Tracked>(1);
	utl::SharedPtr<Tracked> b = a;

	b = a;
	a.Reset();

	// bの参照が残っているので開放されない
	EXPECT_EQ(Tracked::alive, 1);
	b.Reset();
	EXPECT_EQ(Tracked::alive, 0);
}

//----------------------------------------
// ムーブ
//----------------------------------------

TEST_F(SharedPtrTest, MoveConstructTransfersReference) {
	utl::SharedPtr<Tracked> src = utl::MakeShared<Tracked>(5);
	Tracked* raw = src.Get();

	utl::SharedPtr<Tracked> dst(std::move(src));

	EXPECT_FALSE(src);
	EXPECT_EQ(dst.Get(), raw);

	dst.Reset();
	EXPECT_EQ(Tracked::alive, 0);
}

TEST_F(SharedPtrTest, MoveAssignReleasesPreviousInstance) {
	utl::SharedPtr<Tracked> src = utl::MakeShared<Tracked>(1);
	utl::SharedPtr<Tracked> dst = utl::MakeSharedWithAlloc<Tracked>(&delegate, 2);

	dst = std::move(src);

	EXPECT_FALSE(src);
	EXPECT_EQ(dst->value, 1);
	EXPECT_EQ(Tracked::alive, 1);
	EXPECT_EQ(alloc.freeCount, 1);
}

TEST_F(SharedPtrTest, MoveConstructFromDerived) {
	utl::SharedPtr<DerivedTracked> src = utl::MakeSharedWithAlloc<DerivedTracked>(&delegate, 9);
	Tracked* raw = src.Get();

	{
		utl::SharedPtr<Tracked> dst(std::move(src));

		EXPECT_FALSE(src);
		EXPECT_EQ(dst.Get(), raw);
		EXPECT_EQ(dst->value, 9);
	}

	// 基底クラスのポインタから派生クラスのデストラクタが呼ばれる
	EXPECT_EQ(DerivedTracked::derivedDestroyed, 1);
	EXPECT_EQ(Tracked::alive, 0);
	EXPECT_EQ(alloc.freeCount, 1);
}

TEST_F(SharedPtrTest, MoveAssignFromDerived) {
	utl::SharedPtr<DerivedTracked> src = utl::MakeSharedWithAlloc<DerivedTracked>(&delegate, 9);
	utl::SharedPtr<Tracked> dst = utl::MakeShared<Tracked>(1);

	dst = std::move(src);

	EXPECT_FALSE(src);
	EXPECT_EQ(dst->value, 9);
	EXPECT_EQ(Tracked::alive, 1);

	dst.Reset();
	EXPECT_EQ(DerivedTracked::derivedDestroyed, 1);
	EXPECT_EQ(alloc.freeCount, 1);
}

//----------------------------------------
// 開放
//----------------------------------------

TEST_F(SharedPtrTest, ResetEmptyIsSafe) {
	utl::SharedPtr<Tracked> ptr;

	ptr.Reset();

	EXPECT_FALSE(ptr);
}

TEST_F(SharedPtrTest, ResetTwiceIsSafe) {
	utl::SharedPtr<Tracked> ptr = utl::MakeSharedWithAlloc<Tracked>(&delegate, 1);

	ptr.Reset();
	ptr.Reset();

	EXPECT_EQ(Tracked::destroyed, 1);
	EXPECT_EQ(alloc.freeCount, 1);
}

TEST_F(SharedPtrTest, AssignNullptrReleasesInstance) {
	utl::SharedPtr<Tracked> ptr = utl::MakeSharedWithAlloc<Tracked>(&delegate, 1);

	ptr = nullptr;

	EXPECT_FALSE(ptr);
	EXPECT_EQ(Tracked::destroyed, 1);
	EXPECT_EQ(alloc.freeCount, 1);
}

//----------------------------------------
// 比較
//----------------------------------------

TEST_F(SharedPtrTest, CompareWithRawPointer) {
	utl::SharedPtr<Tracked> ptr = utl::MakeShared<Tracked>(1);
	Tracked other;

	EXPECT_TRUE(ptr == ptr.Get());
	EXPECT_FALSE(ptr != ptr.Get());
	EXPECT_TRUE(ptr != &other);
	EXPECT_FALSE(ptr == &other);
}

TEST_F(SharedPtrTest, EmptyNeverEqualsRawPointer) {
	utl::SharedPtr<Tracked> empty;
	Tracked other;

	EXPECT_FALSE(empty == &other);
	EXPECT_TRUE(empty != &other);
}

TEST_F(SharedPtrTest, CompareWithSharedPtr) {
	utl::SharedPtr<Tracked> a = utl::MakeShared<Tracked>(1);
	utl::SharedPtr<Tracked> copy = a;
	utl::SharedPtr<Tracked> b = utl::MakeShared<Tracked>(1);
	utl::SharedPtr<Tracked> emptyA;
	utl::SharedPtr<Tracked> emptyB;

	EXPECT_TRUE(a == copy);
	EXPECT_FALSE(a != copy);
	EXPECT_TRUE(a != b);
	EXPECT_TRUE(emptyA == emptyB);
	EXPECT_TRUE(a != emptyA);
	EXPECT_TRUE(emptyA != a);
}

//----------------------------------------
// 不具合の再現
//----------------------------------------

namespace
{
	/// <summary>
	/// 2番目の基底クラス（派生クラスの先頭とアドレスがずれる）
	/// </summary>
	struct SecondBase
	{
		virtual ~SecondBase() = default;

		int second = 0;
	};

	struct MultiDerived : Tracked, SecondBase
	{
		static inline int multiDestroyed = 0;

		MultiDerived(int v)
			: Tracked(v)
		{
			second = v * 10;
		}
		~MultiDerived() override
		{
			++multiDestroyed;
		}
	};

	/// <summary>
	/// 仮想デストラクタを持たない基底クラス
	/// </summary>
	struct PlainBase
	{
		static inline int baseDestroyed = 0;

		~PlainBase()
		{
			++baseDestroyed;
		}
	};

	struct PlainDerived : PlainBase
	{
		static inline int derivedDestroyed = 0;

		~PlainDerived()
		{
			++derivedDestroyed;
		}
	};

	struct alignas(64) OverAligned
	{
		char data[64];
	};

	/// <summary>
	/// コンストラクタで例外を投げる型
	/// </summary>
	struct ThrowOnConstruct
	{
		ThrowOnConstruct()
		{
			throw 1;
		}
	};

	/// <summary>
	/// ムーブしかできない型
	/// </summary>
	struct MoveOnly
	{
		explicit MoveOnly(int v)
			: value(v)
		{}
		MoveOnly(MoveOnly&&) = default;
		MoveOnly(const MoveOnly&) = delete;

		int value;
	};

	struct HoldsMoveOnly
	{
		HoldsMoveOnly(MoveOnly&& src)
			: held(std::move(src))
		{}

		MoveOnly held;
	};

	/// <summary>
	/// 常に確保に失敗するアロケータ
	/// </summary>
	class FailingAllocator
	{
	public:
		void* Allocate(uint32_t, uint32_t)
		{
			return nullptr;
		}
		bool Free(void*)
		{
			return true;
		}
	};
}

// boolへ暗黙に変換できない（別の型どうしの比較がboolの比較になるのを防ぐ）
static_assert(!std::is_convertible_v<utl::SharedPtr<int>, bool>);
static_assert(std::is_constructible_v<bool, utl::SharedPtr<int>>);
// 変換できない型からは作れない
static_assert(!std::is_constructible_v<utl::SharedPtr<int>, const utl::SharedPtr<double>&>);
static_assert(std::is_nothrow_move_constructible_v<utl::SharedPtr<int>>);

TEST_F(SharedPtrTest, MoveToSecondBasePointsToSubobject) {
	MultiDerived::multiDestroyed = 0;
	utl::SharedPtr<MultiDerived> src = utl::MakeSharedWithAlloc<MultiDerived>(&delegate, 3);
	SecondBase* expected = src.Get();

	{
		utl::SharedPtr<SecondBase> dst(std::move(src));

		// 基底クラスの部分オブジェクトを指している
		EXPECT_EQ(dst.Get(), expected);
		EXPECT_EQ(dst->second, 30);
	}

	EXPECT_EQ(MultiDerived::multiDestroyed, 1);
	EXPECT_EQ(Tracked::alive, 0);
	EXPECT_EQ(alloc.freeCount, 1);
	EXPECT_EQ(alloc.badFreeCount, 0);
}

TEST_F(SharedPtrTest, MoveToNonVirtualBaseCallsDerivedDestructor) {
	PlainBase::baseDestroyed = 0;
	PlainDerived::derivedDestroyed = 0;
	{
		utl::SharedPtr<PlainBase> dst = utl::MakeSharedWithAlloc<PlainDerived>(&delegate);
	}

	// 作成したときの型で破棄される
	EXPECT_EQ(PlainDerived::derivedDestroyed, 1);
	EXPECT_EQ(PlainBase::baseDestroyed, 1);
}

TEST_F(SharedPtrTest, SelfMoveAssignKeepsInstance) {
	utl::SharedPtr<Tracked> ptr = utl::MakeSharedWithAlloc<Tracked>(&delegate, 1);
	utl::SharedPtr<Tracked>& ref = ptr;

	ptr = std::move(ref);

	ASSERT_TRUE(ptr);
	EXPECT_EQ(ptr->value, 1);
	EXPECT_EQ(Tracked::alive, 1);
	EXPECT_EQ(alloc.freeCount, 0);
}

TEST_F(SharedPtrTest, MakeSharedPassesAlignment) {
	utl::SharedPtr<OverAligned> ptr = utl::MakeSharedWithAlloc<OverAligned>(&delegate);

	EXPECT_EQ(alloc.lastAlign, alignof(utl::RefCounterToIns<OverAligned>));
}

TEST_F(SharedPtrTest, MakeSharedAlignsWithStandardAllocation) {
	utl::SharedPtr<OverAligned> ptr = utl::MakeShared<OverAligned>();

	ASSERT_TRUE(ptr);
	EXPECT_EQ(reinterpret_cast<uintptr_t>(ptr.Get()) % alignof(OverAligned), 0u);
}

TEST_F(SharedPtrTest, ThrowingConstructorFreesMemory) {
	EXPECT_THROW(utl::MakeSharedWithAlloc<ThrowOnConstruct>(&delegate), int);

	EXPECT_EQ(alloc.allocCount, 1);
	EXPECT_EQ(alloc.freeCount, 1);
}

TEST_F(SharedPtrTest, AllocationFailureReturnsEmpty) {
	FailingAllocator failing;
	utl::MemoryAllocatorDelegate failingDelegate;
	failingDelegate.Register<FailingAllocator, &FailingAllocator::Allocate, &FailingAllocator::Free>(&failing);

	utl::SharedPtr<Tracked> ptr = utl::MakeSharedWithAlloc<Tracked>(&failingDelegate, 1);

	EXPECT_FALSE(ptr);
	EXPECT_EQ(ptr.UseCount(), 0);
	EXPECT_EQ(Tracked::alive, 0);
}

TEST_F(SharedPtrTest, UnregisteredDelegateReturnsEmpty) {
	utl::MemoryAllocatorDelegate unregistered;

	utl::SharedPtr<Tracked> ptr = utl::MakeSharedWithAlloc<Tracked>(&unregistered, 1);

	EXPECT_FALSE(ptr);
	EXPECT_EQ(Tracked::alive, 0);
}

TEST_F(SharedPtrTest, MakeSharedForwardsMoveOnlyArgument) {
	utl::SharedPtr<HoldsMoveOnly> ptr = utl::MakeShared<HoldsMoveOnly>(MoveOnly(3));

	ASSERT_TRUE(ptr);
	EXPECT_EQ(ptr->held.value, 3);
}

TEST_F(SharedPtrTest, CopyConstructFromDerivedSharesInstance) {
	utl::SharedPtr<MultiDerived> src = utl::MakeSharedWithAlloc<MultiDerived>(&delegate, 4);

	{
		utl::SharedPtr<SecondBase> dst(src);
		utl::SharedPtr<Tracked> other;
		other = src;

		EXPECT_EQ(src.UseCount(), 3);
		EXPECT_EQ(dst.Get(), static_cast<SecondBase*>(src.Get()));
		EXPECT_EQ(dst->second, 40);
		EXPECT_EQ(other->value, 4);
	}

	// 変換したコピーが消えても元は残っている
	EXPECT_EQ(src.UseCount(), 1);
	EXPECT_EQ(Tracked::alive, 1);

	src.Reset();
	EXPECT_EQ(Tracked::alive, 0);
	EXPECT_EQ(alloc.freeCount, 1);
	EXPECT_EQ(alloc.badFreeCount, 0);
}

TEST_F(SharedPtrTest, UseCountFollowsCopies) {
	utl::SharedPtr<Tracked> a = utl::MakeShared<Tracked>(1);
	EXPECT_EQ(a.UseCount(), 1);

	utl::SharedPtr<Tracked> b = a;
	EXPECT_EQ(a.UseCount(), 2);

	utl::SharedPtr<Tracked> c = std::move(b);
	EXPECT_EQ(a.UseCount(), 2);
	EXPECT_EQ(b.UseCount(), 0);

	c.Reset();
	EXPECT_EQ(a.UseCount(), 1);
}

TEST_F(SharedPtrTest, EmptyEqualsNullptr) {
	utl::SharedPtr<Tracked> empty;

	EXPECT_TRUE(empty == nullptr);
	EXPECT_FALSE(empty != nullptr);
}
