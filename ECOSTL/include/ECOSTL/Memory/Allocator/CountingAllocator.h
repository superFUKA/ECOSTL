#pragma once
// 別のアロケータを包んで、確保の回数・バイト数を数え、確保と開放のたびにフックを呼ぶアロケータ。
// グローバルな計数・フックの代わり。数えたい範囲(ゲームの1フレーム、ECSのWorldなど)に、使う側がこれを渡す(DI)。
//
//   utl::CountingAllocator counting;              // 中身は既定のアロケータ(inner を渡せば、そのアロケータを包む)
//   utl::Vector<int> v(counting.GetAPI());        // v の確保が counting に数えられる
//   counting.GetStats().count;                    // 確保回数
#include <cstdint>
#include "MemoryAllocator.h"
#include "ECOSTL/Core/Atomic/Atomic.h"

// 汎用ユーティリティ名前空間
namespace utl
{
	/// <summary>
	/// 確保の累計。値はその時点のもので、2つの値が同じ瞬間のものとは限らない
	/// </summary>
	struct AllocStats
	{
		uint64_t count = 0;      // 確保回数
		uint64_t bytes = 0;      // 確保バイト数の合計
		uint64_t freeCount = 0;  // 開放回数
	};

	/// <summary>
	/// 確保・開放のたびに呼ばれるフック。user は登録時に渡したもの。memory は確保・開放したアドレス(確保失敗なら nullptr)
	/// 複数のスレッドから同時に呼ばれうる
	/// </summary>
	struct AllocHook
	{
		using AllocateFn = void (*)(void* user, void* memory, uint32_t size, uint32_t align);
		using FreeFn = void (*)(void* user, void* memory);

		AllocateFn onAllocate = nullptr;
		FreeFn onFree = nullptr;
		void* user = nullptr;
	};

	/// <summary>
	/// 別のアロケータを包んで数えるアロケータ。計数は relaxed の原子加算なので、複数スレッドから使ってよい
	/// フックの付け替えは、このアロケータで確保・開放していないときに行うこと
	/// </summary>
	class CountingAllocator
	{
	public:
		/// <param name="inner">実際に確保するアロケータ(所有しない)。nullptr なら既定のアロケータ（MemoryAllocatorRef と同じ）</param>
		explicit CountingAllocator(MemoryAllocatorDelegate* inner = nullptr)
			: inner_(inner)
		{
			api_.Register<CountingAllocator, &CountingAllocator::Allocate, &CountingAllocator::Free>(this);
		}

		// 自分自身を指す API を持つので、コピー・ムーブはしない
		CountingAllocator(const CountingAllocator&) = delete;
		CountingAllocator& operator=(const CountingAllocator&) = delete;

		/// <summary>
		/// コンテナなどに渡す API。このアロケータが生きている間だけ使える
		/// </summary>
		MemoryAllocatorDelegate* GetAPI()
		{
			return &api_;
		}

		void SetHook(const AllocHook& hook)
		{
			hook_ = hook;
		}

		AllocStats GetStats()const
		{
			AllocStats stats;
			stats.count = count_.Load();
			stats.bytes = bytes_.Load();
			stats.freeCount = freeCount_.Load();
			return stats;
		}

		// MemoryAllocatorDelegate から呼ばれる確保
		void* Allocate(const uint32_t size, const uint32_t align)
		{
			count_.FetchAdd(1);
			bytes_.FetchAdd(size);
			void* memory = inner_.Allocate(size, align);
			if (hook_.onAllocate != nullptr)
			{
				hook_.onAllocate(hook_.user, memory, size, align);
			}
			return memory;
		}

		// MemoryAllocatorDelegate から呼ばれる開放
		bool Free(void* memory)
		{
			if (memory == nullptr)
			{
				return true;
			}
			freeCount_.FetchAdd(1);
			if (hook_.onFree != nullptr)
			{
				hook_.onFree(hook_.user, memory);
			}
			inner_.Free(memory);
			return true;
		}

	private:
		MemoryAllocatorRef inner_;  // 実際に確保するアロケータ
		MemoryAllocatorDelegate api_;          // 自分を登録した API
		AllocHook hook_;
		AtomicCounter64 count_;
		AtomicCounter64 bytes_;
		AtomicCounter64 freeCount_;
	};
}
