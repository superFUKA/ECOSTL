#pragma once
#include <cassert>
#include <cstdint>
#include <new>
#include "../../Core/Utility.h"
#include "../../Memory/Allocate/MemoryAllocatorAPI.h"

namespace utl
{
	/// <summary>
	/// 要素数を変更できる配列
	/// 要素数ちょうどのメモリを確保し、Resizeのたびに確保し直す
	/// </summary>
	/// <typeparam name="Type"></typeparam>
	template<typename Type>
	class ResizableArray
	{
	public:
		// 作成
		//----------------------

		/// <summary>
		/// 空の配列を作成する
		/// </summary>
		/// <param name="allocator">nullptrならC++標準の手段で確保する</param>
		explicit ResizableArray(MemoryAllocatorDelegate* allocator = nullptr)
			: alloc_(allocator)
			, begin_(nullptr)
			, size_(0)
		{
		}
		explicit ResizableArray(const MemoryAllocatorRef& allocator)
			: alloc_(allocator)
			, begin_(nullptr)
			, size_(0)
		{
		}

		/// <summary>
		/// 要素数を指定して作成する。すべての要素を同じコンストラクタ引数で作成する
		/// </summary>
		template<typename...ArgTypes>
		explicit ResizableArray(const uint32_t size, MemoryAllocatorDelegate* allocator, ArgTypes&&...args)
			: alloc_(allocator)
			, begin_(nullptr)
			, size_(0)
		{
			Create(size, args...);
		}
		template<typename...ArgTypes>
		explicit ResizableArray(const uint32_t size, const MemoryAllocatorRef& allocator, ArgTypes&&...args)
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
			return Resize(size, args...);
		}

		// 代入
		//----------------------

		/// <summary>
		/// コピーして作成。メモリアロケータもコピー元と同じものを使う
		/// </summary>
		ResizableArray(const ResizableArray& src)
			: alloc_(src.alloc_)
			, begin_(nullptr)
			, size_(0)
		{
			CopyFrom(src);
		}

		/// <summary>
		/// コピーして代入。メモリアロケータは今のものを使い続ける
		/// </summary>
		ResizableArray& operator=(const ResizableArray& src)
		{
			if (this != &src)
			{
				Reset();
				CopyFrom(src);
			}
			return *this;
		}

		ResizableArray(ResizableArray&& src)noexcept
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
		ResizableArray& operator=(ResizableArray&& src)noexcept
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

			Destroy(begin_, size_);
			alloc_.Free(begin_);
			begin_ = nullptr;
			size_ = 0;
		}

		~ResizableArray()
		{
			Reset();
		}

		// 操作
		//----------------------

		/// <summary>
		/// 要素数を変更する
		/// 減らすときは末尾から破棄し、増やすときは引数で作成した要素を末尾に追加する
		/// 残る要素は新しいメモリへ移動するので、アドレスが変わる
		/// </summary>
		/// <param name="afterSize">0ならメモリを開放する</param>
		/// <param name="...args">増える要素のコンストラクタ引数</param>
		/// <returns>要素数が変わったらtrue。同じ要素数・メモリ確保失敗ならfalse</returns>
		template<typename...ArgTypes>
		bool Resize(const uint32_t afterSize, ArgTypes&&...args)
		{
			const uint32_t beforeSize = size_;
			if (beforeSize == afterSize)
			{
				return false;
			}

			if (afterSize == 0)
			{
				Reset();
				return true;
			}

			Type* newMem = alloc_.template Allocate<Type>(afterSize);
			if (newMem == nullptr)
			{
				return false;
			}

			// 残す要素を新しいメモリへ移動
			const uint32_t moveNum = afterSize < beforeSize ? afterSize : beforeSize;
			if constexpr (IsTriviallyCopyableV<Type>)
			{
				if (moveNum != 0)
				{
					MemCopy(static_cast<void*>(newMem), begin_, sizeof(Type) * moveNum);
				}
			}
			else
			{
				for (uint32_t i = 0; i < moveNum; ++i)
				{
					new (newMem + i) Type(Move(*(begin_ + i)));
				}
			}

			// 増える要素を作成
			for (uint32_t i = moveNum; i < afterSize; ++i)
			{
				new (newMem + i) Type(args...);
			}

			// 古いメモリを破棄
			if (begin_ != nullptr)
			{
				Destroy(begin_, beforeSize);
				alloc_.Free(begin_);
			}
			begin_ = newMem;
			size_ = afterSize;
			return true;
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
		static void Destroy(Type* begin, const uint32_t num)
		{
			if constexpr (!IsTriviallyDestructibleV<Type>)
			{
				for (uint32_t i = 0; i < num; ++i)
				{
					(begin + i)->~Type();
				}
			}
			else
			{
				(void)begin;
				(void)num;
			}
		}

		void CopyFrom(const ResizableArray& src)
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
