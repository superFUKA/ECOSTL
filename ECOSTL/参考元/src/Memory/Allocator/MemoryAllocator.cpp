#include "MemoryAllocator.h"
#include <cstdlib>

namespace utl
{
	void* MemoryAllocatorAPIHolder::Allocate(const uint32_t size, const uint32_t align)
	{
		if (alloc_ == nullptr)
		{
			return malloc(size);
		}
		else
		{
			return alloc_->Allocate(size, align);
		}
	}
	void MemoryAllocatorAPIHolder::Free(void* allocated)
	{
		if (alloc_ == nullptr)
		{
			free(allocated);
		}
		else
		{
			alloc_->Free(allocated);
		}
	}
}
