#pragma once
// malloc/free（AlignedAllocate/AlignedFree）で確保するアロケータ。使う側が作って、MemoryAllocatorDelegate として渡す（DI）。
// グローバルではない。既定のアロケータ（Ref に何も渡さないときの動き）を無効にしたビルド（ECOSTL_DISABLE_DEFAULT_ALLOCATOR）でも使える。
//
//   utl::MallocAllocator mallocAllocator;
//   utl::Vector<int> v(mallocAllocator.GetAPI());
#include <cstdint>
#include "ECOSTL/Memory/Allocator/MemoryAllocator.h"
#include "ECOSTL/Memory/Allocator/MemoryAllocate.h"

// 汎用ユーティリティ名前空間
namespace utl
{
	class MallocAllocator
	{
	public:
		MallocAllocator()
		{
			api_.Register<MallocAllocator, &MallocAllocator::Allocate, &MallocAllocator::Free>(this);
		}

		// 自分自身を指す API を持つので、コピー・ムーブはしない
		MallocAllocator(const MallocAllocator&) = delete;
		MallocAllocator& operator=(const MallocAllocator&) = delete;

		/// <summary>
		/// コンテナなどに渡す API。このアロケータが生きている間だけ使える
		/// </summary>
		MemoryAllocatorDelegate* GetAPI()
		{
			return &api_;
		}

		// MemoryAllocatorDelegate から呼ばれる確保（align は2の累乗。0 なら既定の境界）
		void* Allocate(const uint32_t size, const uint32_t align)
		{
			return AlignedAllocate(size, align);
		}

		// MemoryAllocatorDelegate から呼ばれる開放
		bool Free(void* memory)
		{
			AlignedFree(memory);
			return true;
		}

	private:
		MemoryAllocatorDelegate api_;  // 自分を登録した API
	};
}
