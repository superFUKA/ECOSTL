#include "ResizableMemoryBlock.h"
#include <memory>

namespace utl
{
	ResizableMemoryBlock::ResizableMemoryBlock(MemoryAllocatorAPI* alloc)
		: alloc_(alloc), mem_(nullptr), capacity_(0)
	{

	}

	ResizableMemoryBlock::ResizableMemoryBlock(const uint32_t size, MemoryAllocatorAPI* alloc)
		: alloc_(alloc), mem_(nullptr), capacity_(0)
	{
		Resize(size);
	}

	ResizableMemoryBlock::ResizableMemoryBlock(const ResizableMemoryBlock& src)
		: alloc_(src.alloc_), mem_(nullptr), capacity_(0)
	{
		*this = src;
	}

	ResizableMemoryBlock& ResizableMemoryBlock::operator=(const ResizableMemoryBlock& src)
	{
		if (this == &src)
		{
			return *this;
		}

		Copy(src.Data(), src.Capacity());

		return *this;
	}

	void ResizableMemoryBlock::Reset()
	{
		if (mem_ == nullptr)
		{
			return;
		}
		alloc_.Free(mem_);
		mem_ = nullptr;
		capacity_ = 0;
	}

	ResizableMemoryBlock::~ResizableMemoryBlock()
	{
		Reset();
	}

	void ResizableMemoryBlock::ChangeMemoryAllocator(MemoryAllocatorAPI* alloc)
	{
		// 今使ってるアロケータと同じものを指定してたら何もしない
		if (alloc == alloc_.Get())
		{
			return;
		}

		// 新しいメモリを確保する
		MemoryAllocatorAPIHolder holder(alloc);
		void* newMem = holder.Allocate(capacity_);
		// 新しいメモリに今のメモリの内容を移す
		memcpy(newMem, mem_, capacity_);

		// 今のメモリを開放して、新しいメモリとアロケータをセットする
		alloc_.Free(mem_);
		mem_ = newMem;
		alloc_.Reset(alloc);
	}

	bool ResizableMemoryBlock::Resize(const uint32_t capacity)
	{
		// サイズの大きさに変わりがなければ何もしない
		if (capacity_ == capacity)
		{
			return false;
		}
		void* newMem = nullptr;

		// 変更前と変更後で小さいほうのサイズが移動するメモリのサイズ
		uint32_t moveMemSize = capacity;
		if (capacity > capacity_)
		{
			moveMemSize = capacity_;
		}

		if (capacity > 0)
		{
			// 新しく使用するメモリを確保
			newMem = alloc_.Allocate(capacity);
			if (newMem == nullptr)
			{
				return false;
			}
		}

		bool isMove = moveMemSize != 0;
		if (isMove)
		{
			if (newMem == nullptr)
			{
				return false;
			}
			memcpy(newMem, mem_, moveMemSize);
		}

		alloc_.Free(mem_);
		mem_ = newMem;
		capacity_ = capacity;

		return true;
	}

	void ResizableMemoryBlock::Copy(const void* begin, const uint32_t memSize)
	{
		// 今管理してるメモリを開放する
		Reset();

		// 管理してるメモリと同じメモリかコピー元のメモリが空なら何もしない
		bool isEqual = begin == mem_ && capacity_ == memSize;
		bool isNonMem = memSize <= 0;
		if (isEqual || isNonMem)
		{
			return;
		}

		Resize(memSize);

		memcpy(mem_, begin, memSize);


	}
}
