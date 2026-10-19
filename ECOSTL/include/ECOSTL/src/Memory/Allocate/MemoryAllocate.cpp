#include "MemoryAllocate.h"
#include <cassert>
#include <cstdlib>

namespace utl
{
	// mallocで多めに確保して境界に合わせ、その直前にmallocが返したアドレスを書いておく
	void* AlignedAllocate(uint32_t size, uint32_t align)
	{
		assert((align & (align - 1)) == 0 && "アライメントが2の累乗ではありません");
		if (align < StandardMinAlign)
		{
			align = StandardMinAlign;
		}

		// 境界に合わせるためにずらす分と、元のアドレスを書いておく分を足す
		const size_t extra = static_cast<size_t>(align) - 1 + sizeof(void*);
		if (static_cast<size_t>(size) > SIZE_MAX - extra)
		{
			return nullptr;
		}
		void* raw = malloc(static_cast<size_t>(size) + extra);
		if (raw == nullptr)
		{
			return nullptr;
		}

		const uintptr_t start = reinterpret_cast<uintptr_t>(raw) + sizeof(void*);
		const uintptr_t aligned = (start + align - 1) & ~static_cast<uintptr_t>(align - 1);
		static_cast<void**>(reinterpret_cast<void*>(aligned))[-1] = raw;
		return reinterpret_cast<void*>(aligned);
	}

	void AlignedFree(void* allocated)
	{
		if (allocated == nullptr)
		{
			return;
		}
		free(static_cast<void**>(allocated)[-1]);
	}
}
