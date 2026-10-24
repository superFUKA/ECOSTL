#pragma once
#include <cassert>
#include "src/Memory/Memory.hpp"

namespace utl
{

	/// <summary>
	/// 要素数を変更することができる配列
	/// </summary>
	/// <typeparam name="Type"></typeparam>
	template<typename Type>
	class ResizableArray
	{
	public:

		//--------------------------------------
		// 作成
		//--------------------------------------

		/// <summary>
		/// 空のDynamicArrayを作成する
		/// </summary>
		ResizableArray()
			: alloc_(nullptr)
			, begin_(nullptr)
			, size_(0)
		{

		}

		template<typename...ArgTypes>
		void Create(const uint32_t size, MemoryAllocatorAPI* alloc = nullptr, ArgTypes&&...args)
		{
			Reset();

			alloc_.Reset(alloc);
			begin_ = alloc_.Allocate<Type>(size);
			size_ = size;

			for (uint32_t i = 0; i < size; ++i)
			{
				Type* ptr = begin_ + i;
				new (ptr) Type(Move(args...));
			}
		}

		template<typename...ArgTypes>
		ResizableArray(const uint32_t size, MemoryAllocatorAPI* alloc = nullptr, ArgTypes&&...args)
			: alloc_(nullptr)
			, begin_(nullptr)
			, size_(0)
		{
			Create(size, alloc, args...);
		}

		//--------------------------------------
		// 代入
		//--------------------------------------

		ResizableArray(const ResizableArray& src)
			: alloc_(src.alloc_.Get())
			, begin_(src.alloc_.Allocate<Type>(src.Size()))
			, size_(src.size_)
		{
			MoveMem(begin_, src.Data(), src.Size());
		}

		ResizableArray& operator=(const ResizableArray& src)
		{
			Reset();

			alloc_.Reset(src.alloc_.Get());
			begin_ = alloc_.Allocate<Type>(src.Size());
			size_ = src.Size();

			MoveMem(begin_, src.Data(), src.Size());

			return *this;
		}

		ResizableArray(ResizableArray&& src)noexcept
			: alloc_(src.alloc_.Get())
			, begin_(src.begin_)
			, size_(src.size_)
		{
			src.alloc_.Reset();
			src.begin_ = nullptr;
			src.size_ = 0;
		}

		ResizableArray& operator=(ResizableArray&& src)noexcept
		{
			Reset();

			alloc_.Reset(src.alloc_.Get());
			begin_ = src.begin_;
			size_ = src.Size();

			src.alloc_.Reset();
			src.begin_ = nullptr;
			src.size_ = 0;
			return *this;
		}

		//--------------------------------------
		// 開放
		//--------------------------------------

		/// <summary>
		/// 
		/// </summary>
		void Reset()
		{
			if (begin_ == nullptr)
			{
				return;
			}

			for (uint32_t i = 0; i < size_; ++i)
			{
				Get(i)->~Type();
			}

			alloc_.Free(begin_);
			begin_ = nullptr;
			size_ = 0;
		}


		~ResizableArray()
		{
			Reset();
		}

		//--------------------------------------
		// 操作
		//--------------------------------------

		template<typename...ArgTypes>
		bool Resize(const uint32_t afterSize, ArgTypes&&...args)
		{
			const uint32_t beforeSize = size_;
			// 今とサイズが変わらなければ何もしない
			if (beforeSize == afterSize)
			{
				return false;
			}

			// 変更後要素がなくなるならリセット
			if (afterSize == 0)
			{
				Reset();
				return true;
			}



			// 新しいメモリを確保
			Type* newMem = alloc_.Allocate<Type>(afterSize);

			// 移動するメモリがあるなら移動
			uint32_t moveNum = afterSize;
			if (moveNum > beforeSize)
			{
				moveNum = beforeSize;
			}
			if (moveNum > 0)
			{
				MoveMem(newMem, begin_, moveNum);
			}

			// 今使ってるメモリに配置されたインスタンスを開放し、新しいメモリをセット
			for (uint32_t i = 0; i < beforeSize; ++i)
			{
				Get(i)->~Type();
			}
			alloc_.Free(begin_);
			begin_ = newMem;

			// 要素が作成されるならコンストラクタを呼んでインスタンスを配置
			if (beforeSize < afterSize)
			{
				for (uint32_t i = beforeSize; i < afterSize; ++i)
				{
					new (Get(i)) Type(args...);
				}
			}

			size_ = afterSize;

			return true;
		}


		//--------------------------------------
		// 取得
		//--------------------------------------

		inline Type& operator[](const uint32_t index)const
		{
			return *Get(index);
		}

		inline Type* Data()const
		{
			return begin_;
		}

		inline const uint32_t Size()const
		{
			return size_;
		}

		inline Type* Get(const uint32_t index)const
		{
			assert(index < size_ && "範囲外アクセスです");
			return begin_ + index;
		}

	private:
		void MoveMem(Type* destBegin, Type* srcBegin, uint32_t size)
		{
			for (uint32_t i = 0; i < size; ++i)
			{
				Type* ptr = destBegin + i;
				new (ptr) Type(Move(*(srcBegin + i)));
			}
		}
	private:

		MemoryAllocatorAPIHolder alloc_;

		Type* begin_;

		uint32_t size_;

	};
}

