#pragma once
#include <cassert>
#include <cstdint>
#include <new>
#include "../../Core/Utility.h"
#include "../../Memory/Allocate/MemoryAllocatorAPI.h"

namespace utl
{
	/// <summary>
	/// 要素数とメモリ確保のタイミング・方法を実行時に指定して作成する固定長の配列
	/// 作成後に要素数は変えられない（変えたいときはResizableArray）
	/// </summary>
	/// <typeparam name="Type"></typeparam>
	template<typename Type>
	class DynamicArray
	{
	public:
		// 作成
		//----------------------

		/// <summary>
		/// 空の配列を作成する
		/// </summary>
		/// <param name="allocator">nullptrならC++標準の手段で確保する</param>
		explicit DynamicArray(MemoryAllocatorDelegate* allocator = nullptr)
			: alloc_(allocator)
			, begin_(nullptr)
			, size_(0)
		{
		}
		explicit DynamicArray(const MemoryAllocatorRef& allocator)
			: alloc_(allocator)
			, begin_(nullptr)
			, size_(0)
		{
		}

		/// <summary>
		/// 要素数を指定して作成する。すべての要素を同じコンストラクタ引数で作成する
		/// </summary>
		/// <param name="size">0ならメモリを確保しない</param>
		/// <param name="allocator">nullptrならC++標準の手段で確保する</param>
		template<typename...ArgTypes>
		explicit DynamicArray(const uint32_t size, MemoryAllocatorDelegate* allocator, ArgTypes&&...args)
			: alloc_(allocator)
			, begin_(nullptr)
			, size_(0)
		{
			Create(size, args...);
		}
		template<typename...ArgTypes>
		explicit DynamicArray(const uint32_t size, const MemoryAllocatorRef& allocator, ArgTypes&&...args)
			: alloc_(allocator)
			, begin_(nullptr)
			, size_(0)
		{
			Create(size, args...);
		}

		/// <summary>
		/// 今の要素を破棄し、要素数を指定して作り直す
		/// メモリアロケータは今のものを使い続ける
		/// </summary>
		/// <returns>メモリを確保できたらtrue。size==0ならtrue</returns>
		template<typename...ArgTypes>
		bool Create(const uint32_t size, ArgTypes&&...args)
		{
			Reset();
			if (size == 0)
			{
				return true;
			}

			Type* mem = alloc_.template Allocate<Type>(size);
			if (mem == nullptr)
			{
				return false;
			}
			begin_ = mem;
			size_ = size;
			for (uint32_t i = 0; i < size; ++i)
			{
				new (begin_ + i) Type(args...);
			}
			return true;
		}

		// 代入
		//----------------------

		/// <summary>
		/// コピーして作成。メモリアロケータもコピー元と同じものを使う
		/// </summary>
		DynamicArray(const DynamicArray& src)
			: alloc_(src.alloc_)
			, begin_(nullptr)
			, size_(0)
		{
			CopyFrom(src);
		}

		/// <summary>
		/// コピーして代入。メモリアロケータは今のものを使い続ける
		/// </summary>
		DynamicArray& operator=(const DynamicArray& src)
		{
			if (this != &src)
			{
				Reset();
				CopyFrom(src);
			}
			return *this;
		}

		DynamicArray(DynamicArray&& src)noexcept
			: alloc_(src.alloc_)
			, begin_(src.begin_)
			, size_(src.size_)
		{
			src.begin_ = nullptr;
			src.size_ = 0;
		}

		/// <summary>
		/// 移動して代入。メモリアロケータも移動元のものを引き継ぐ
		/// </summary>
		DynamicArray& operator=(DynamicArray&& src)noexcept
		{
			if (this != &src)
			{
				Reset();
				alloc_ = src.alloc_;
				begin_ = src.begin_;
				size_ = src.size_;
				src.begin_ = nullptr;
				src.size_ = 0;
			}
			return *this;
		}

		// 開放
		//----------------------

		/// <summary>
		/// 要素を破棄してメモリを開放する
		/// </summary>
		void Reset()
		{
			if (begin_ == nullptr)
			{
				return;
			}

			if constexpr (!IsTriviallyDestructibleV<Type>)
			{
				for (uint32_t i = 0; i < size_; ++i)
				{
					(begin_ + i)->~Type();
				}
			}
			alloc_.Free(begin_);
			begin_ = nullptr;
			size_ = 0;
		}

		~DynamicArray()
		{
			Reset();
		}

		// 取得
		//----------------------

		inline Type& operator[](const uint32_t index)
		{
			assert(index < size_ && "範囲外アクセスです");
			return *(begin_ + index);
		}
		inline const Type& operator[](const uint32_t index)const
		{
			assert(index < size_ && "範囲外アクセスです");
			return *(begin_ + index);
		}

		/// <summary>
		/// 添え字の要素を取得する
		/// </summary>
		/// <returns>範囲外ならnullptr</returns>
		inline Type* Get(const uint32_t index)
		{
			return index < size_ ? begin_ + index : nullptr;
		}
		inline const Type* Get(const uint32_t index)const
		{
			return index < size_ ? begin_ + index : nullptr;
		}

		inline Type* Data()
		{
			return begin_;
		}
		inline const Type* Data()const
		{
			return begin_;
		}

		inline Type* Begin()
		{
			return begin_;
		}
		inline const Type* Begin()const
		{
			return begin_;
		}
		inline Type* End()
		{
			return begin_ + size_;
		}
		inline const Type* End()const
		{
			return begin_ + size_;
		}

		// 範囲forループ用
		inline Type* begin() { return Begin(); }
		inline const Type* begin()const { return Begin(); }
		inline Type* end() { return End(); }
		inline const Type* end()const { return End(); }

		inline uint32_t Size()const
		{
			return size_;
		}

		inline bool IsEmpty()const
		{
			return size_ == 0;
		}

		inline bool IsValidIndex(const uint32_t index)const
		{
			return index < size_;
		}

		inline const MemoryAllocatorRef& MemAllocator()const
		{
			return alloc_;
		}

	private:
		void CopyFrom(const DynamicArray& src)
		{
			if (src.size_ == 0)
			{
				return;
			}
			Type* mem = alloc_.template Allocate<Type>(src.size_);
			if (mem == nullptr)
			{
				return;
			}
			begin_ = mem;
			size_ = src.size_;
			if constexpr (IsTriviallyCopyableV<Type>)
			{
				MemCopy(static_cast<void*>(begin_), src.begin_, sizeof(Type) * size_);
			}
			else
			{
				for (uint32_t i = 0; i < size_; ++i)
				{
					new (begin_ + i) Type(src.begin_[i]);
				}
			}
		}

	private:
		MemoryAllocatorRef alloc_;
		Type* begin_;
		uint32_t size_;
	};
}
