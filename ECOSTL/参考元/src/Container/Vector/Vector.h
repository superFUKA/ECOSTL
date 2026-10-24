#pragma once
#include "src/Memory/Memory.hpp"
#include <cassert>

namespace utl
{
	template<typename Type>
	class Vector
	{
	public:
		//------------------------------------------
		// 作成
		//------------------------------------------

		Vector(MemoryAllocatorAPI* alloc)
			: alloc_(alloc)
			, begin_(nullptr)
			, size_(0)
			, capacity_(0)
		{
		}

		Vector()
			: alloc_(nullptr)
			, begin_(nullptr)
			, size_(0)
			, capacity_(0)
		{
		}

		Vector(const uint32_t capacity, MemoryAllocatorAPI* alloc = nullptr)
			: alloc_(alloc)
			, begin_(nullptr)
			, size_(0)
			, capacity_(0)
		{
			Reserve(capacity);
		}

		//------------------------------------------
		// 代入
		//------------------------------------------

		/// <summary>
		/// コピーして作成
		/// </summary>
		/// <param name="src"></param>
		Vector(const Vector& src)
			: alloc_(src.alloc_.Get())
			, begin_(nullptr)
			, size_(0)
			, capacity_(0)
		{
			*this = src;
		}

		/// <summary>
		/// コピーして代入
		/// </summary>
		/// <param name="src"></param>
		/// <returns></returns>
		inline Vector& operator=(const Vector& src)
		{
			if (this == &src)
			{
				return *this;
			}

			Reset();

			size_ = src.size_;
			capacity_ = src.size_;

			// 新しいメモリを確保
			begin_ = alloc_.Allocate<Type>(capacity_);
			assert(begin_ != nullptr && "メモリを確保できませんでした");

			if constexpr (std::is_trivially_copyable_v<Type>)
			{
				if (size_ > 0)
				{
					::memcpy(begin_, src.begin_, sizeof(Type) * size_);
				}
			}
			else
			{
				for (uint32_t i = 0; i < size_; i++)
				{
					new (begin_ + i) Type(*(src.begin_ + i));
				}
			}
			return *this;
		}


		/// <summary>
		/// 移動して作成する
		/// </summary>
		/// <param name="src"></param>
		Vector(Vector&& src)noexcept
			: alloc_(src.alloc_.Get())
			, begin_(src.begin_)
			, size_(src.size_)
			, capacity_(src.capacity_)
		{
			src.begin_ = nullptr;
			src.size_ = 0;
			src.capacity_ = 0;
		}
		/// <summary>
		/// 移動して代入する
		/// </summary>
		/// <param name="src"></param>
		/// <returns></returns>
		inline Vector& operator =(Vector&& src)noexcept
		{
			if (this == &src)
			{
				return *this;
			}

			Reset();

			alloc_ = src.alloc_.Get();
			begin_ = src.begin_;
			size_ = src.size_;
			capacity_ = src.capacity_;

			src.begin_ = nullptr;
			src.size_ = 0;
			src.capacity_ = 0;
			return *this;
		}

		//------------------------------------------
		// 開放
		//------------------------------------------

		~Vector()
		{
			Reset();
		}

		/// <summary>
		/// 要素をすべて削除する
		/// </summary>
		inline void Clear()
		{
			if constexpr (!std::is_trivially_destructible_v<Type>)
			{
				for (uint32_t i = 0; i < size_; ++i)
				{
					Type* ptr = begin_ + i;
					ptr->~Type();
				}
			}
			size_ = 0;
		}

		/// <summary>
		/// 要素全て削除してメモリを開放する
		/// </summary>
		inline void Reset()
		{
			if (begin_ == nullptr)
			{
				return;
			}

			Clear();
			alloc_.Free(begin_);
			begin_ = nullptr;
			capacity_ = 0;
		}

		/// <summary>
		/// 末尾の要素を削除する
		/// </summary>
		inline void PopBack()
		{
			if (size_ == 0)
			{
				return;
			}
			if constexpr (!std::is_trivially_destructible_v<Type>)
			{
				(begin_ + size_ - 1)->~Type();
			}
			--size_;
		}

		//------------------------------------------
		// 削除
		//------------------------------------------

		/// <summary>
		/// 指定した添え字の要素を削除し、後続要素を左に詰める（順序維持）
		/// </summary>
		/// <param name="index">削除する要素の添え字</param>
		/// <returns>削除できたら true、範囲外なら false</returns>
		inline bool Erase(const uint32_t index)
		{
			if (index >= size_)
			{
				return false;
			}

			// 最後尾なら PopBack と同等
			if (index == size_ - 1)
			{
				if constexpr (!std::is_trivially_destructible_v<Type>)
				{
					(begin_ + index)->~Type();
				}
				--size_;
				return true;
			}

			if constexpr (std::is_trivially_copyable_v<Type>)
			{
				::memmove(begin_ + index, begin_ + index + 1, sizeof(Type) * (size_ - index - 1));
			}
			else
			{
				// 削除対象を破棄
				(begin_ + index)->~Type();

				// index+1 以降を左に詰める（Move 構築 -> 旧を破棄）
				for (uint32_t i = index; i < size_ - 1; ++i)
				{
					new (begin_ + i) Type(Move(*(begin_ + (i + 1))));
					(begin_ + (i + 1))->~Type();
				}
			}

			--size_;
			return true;
		}

		//------------------------------------------
		// 追加
		//------------------------------------------

		inline Type* PushBack(const Type& src)
		{
			EnsureCapacityForOne();

			Type* ptr = begin_ + size_;

			new (ptr) Type(src);

			++size_;

			return ptr;
		}

		inline Type* PushBack()
		{
			EnsureCapacityForOne();

			Type* ptr = begin_ + size_;

			new (ptr) Type();

			++size_;

			return ptr;
		}

		template<typename...ArgTypes>
		inline Type* EmplaceBack(ArgTypes&&...args)
		{
			EnsureCapacityForOne();

			Type* ptr = begin_ + size_;

			new (ptr) Type(Forward<ArgTypes>(args)...);

			++size_;
			return ptr;
		}

		inline Type* AppendRange(const Type* src, const uint32_t count)
		{
			if (src == nullptr || count == 0)
			{
				return begin_ + size_;
			}

			const uint32_t insertIndex = size_;
			const bool isSelfRange = begin_ != nullptr && src >= begin_ && src < begin_ + size_;
			const uint32_t srcOffset = isSelfRange ? static_cast<uint32_t>(src - begin_) : 0;

			ReserveSlack(size_ + count);

			if (isSelfRange)
			{
				src = begin_ + srcOffset;
			}

			Type* dest = begin_ + insertIndex;

			if constexpr (std::is_trivially_copyable_v<Type>)
			{
				::memmove(dest, src, sizeof(Type) * count);
			}
			else
			{
				for (uint32_t i = 0; i < count; ++i)
				{
					new (dest + i) Type(src[i]);
				}
			}

			size_ += count;
			return dest;
		}

		inline Type* AppendRange(const Vector& src)
		{
			return AppendRange(src.Begin(), src.Size());
		}

		inline Type* Merge(const Vector& src)
		{
			return AppendRange(src);
		}
		//------------------------------------------
		// 挿入
		//------------------------------------------

		inline Type* Insert(const uint32_t index, const Type& value)
		{
			assert(index <= size_ && "範囲外Insertです");

			// 末尾は PushBack に委譲
			if (index == size_)
			{
				return PushBack(value);
			}

			// 容量確保
			if (capacity_ <= size_)
			{
				// size_+1 を満たすように拡張（既存ポリシーに合わせる）
				ReserveSlack(size_ + 1);
			}

			if constexpr (std::is_trivially_copyable_v<Type>)
			{
				::memmove(begin_ + index + 1, begin_ + index, sizeof(Type) * (size_ - index));
				new (begin_ + index) Type(value);
			}
			else
			{
				// 後ろへ 1 つずつずらす（後ろから Move 構築して元を破棄）
				for (uint32_t i = size_; i > index; --i)
				{
					new (begin_ + i) Type(Move(*(begin_ + (i - 1))));
					(begin_ + (i - 1))->~Type();
				}

				// 挿入
				new (begin_ + index) Type(value);
			}
			++size_;
			return begin_ + index;
		}

		inline Type* Insert(const uint32_t index, Type&& value)
		{
			assert(index <= size_ && "範囲外Insertです");

			if (index == size_)
			{
				// PushBack(Type&&) が無いので EmplaceBack で代替
				return EmplaceBack(Move(value));
			}

			if (capacity_ <= size_)
			{
				ReserveSlack(size_ + 1);
			}

			if constexpr (std::is_trivially_copyable_v<Type>)
			{
				::memmove(begin_ + index + 1, begin_ + index, sizeof(Type) * (size_ - index));
				new (begin_ + index) Type(Move(value));
			}
			else
			{
				for (uint32_t i = size_; i > index; --i)
				{
					new (begin_ + i) Type(Move(*(begin_ + (i - 1))));
					(begin_ + (i - 1))->~Type();
				}

				new (begin_ + index) Type(Move(value));
			}
			++size_;

			return begin_ + index;
		}

		template<typename... ArgTypes>
		inline Type* Emplace(const uint32_t index, ArgTypes&&... args)
		{
			assert(index <= size_ && "範囲外Insertです");

			if (index == size_)
			{
				// 完全転送して末尾構築
				return EmplaceBack(Forward<ArgTypes>(args)...);
			}

			if (capacity_ <= size_)
			{
				ReserveSlack(size_ + 1);
			}

			if constexpr (std::is_trivially_copyable_v<Type>)
			{
				::memmove(begin_ + index + 1, begin_ + index, sizeof(Type) * (size_ - index));
				new (begin_ + index) Type(Forward<ArgTypes>(args)...);
			}
			else
			{
				for (uint32_t i = size_; i > index; --i)
				{
					new (begin_ + i) Type(Move(*(begin_ + (i - 1))));
					(begin_ + (i - 1))->~Type();
				}

				new (begin_ + index) Type(Forward<ArgTypes>(args)...);
			}
			++size_;

			return begin_ + index;
		}

		//------------------------------------------
		// 取得
		//------------------------------------------

		inline Type* Get(const uint32_t index)const
		{
			// 範囲外はNullを返す
			if (index >= size_)
			{
				return nullptr;
			}
			return begin_ + index;
		}

		inline Type& operator[](const uint32_t index)const
		{
			assert(index < size_ && "範囲外アクセスです");
			return *(begin_ + index);
		}
		inline Type& operator[](const uint32_t index)
		{
			assert(index < size_ && "範囲外アクセスです");
			return *(begin_ + index);
		}

		inline Type* Begin()const
		{
			return begin_;
		}

		inline Type* Back()const
		{
			if (size_ == 0)
			{
				return nullptr;
			}
			return begin_ + (size_ - 1);
		}

		inline uint32_t Size()const
		{
			return size_;
		}

		inline uint32_t Capacity()const
		{
			return capacity_;
		}

		inline bool Empty()const
		{
			return size_ == 0;
		}

		/// <summary>
		/// 引数の添え字が範囲内か返す関数
		/// </summary>
		/// <param name="index"></param>
		/// <returns></returns>
		inline bool IsContain(const uint32_t index)const
		{
			if (index >= size_)
			{
				return false;
			}
			return true;
		}


		inline MemoryAllocatorAPI* GetAlloc()const
		{
			return alloc_.Get();
		}

		//------------------------------------------
		// メモリ操作
		//------------------------------------------

		static inline uint32_t GetSlack(const uint32_t start, const uint32_t need)
		{
			uint32_t newCapacity = start;
			if (start < 2)
			{
				newCapacity = 2;
			}
			// 必要な容量より大きくなるまで繰り返す
			while (newCapacity <= need)
			{
				// 今の値と今の値を半減した値を足した数をセット
				newCapacity = newCapacity + (newCapacity >> 1);
			}
			return newCapacity;
		}

		template<typename...ArgTypes>
		inline void Resize(const uint32_t size, ArgTypes&&...args)
		{
			// もしメモリが足りなければ確保
			if (size > capacity_)
			{
				ReserveSlack(size);
			}

			// 要素数が増えたならその分を初期化
			for (uint32_t i = size_; i < size; ++i)
			{
				Type* ptr = begin_ + i;
				new (ptr) Type(Forward<ArgTypes>(args)...);
			}

			// 要素数が減ったならその分開放
			if constexpr (!std::is_trivially_destructible_v<Type>)
			{
				for (uint32_t i = size; i < size_; ++i)
				{
					Type* ptr = begin_ + i;
					ptr->~Type();
				}
			}

			size_ = size;
		}

		inline void Resize(const uint32_t size, const Type& src)
		{
			// もしメモリが足りなければ確保
			if (size > capacity_)
			{
				ReserveSlack(size);
			}

			// 要素数が増えたならその分を初期化
			for (uint32_t i = size_; i < size; ++i)
			{
				Type* ptr = begin_ + i;
				new (ptr) Type(src);
			}

			// 要素数が減ったならその分開放
			if constexpr (!std::is_trivially_destructible_v<Type>)
			{
				for (uint32_t i = size; i < size_; ++i)
				{
					Type* ptr = begin_ + i;
					ptr->~Type();
				}
			}

			size_ = size;
		}

		/// <summary>
		/// キャパシティを要素数にいい感じに合わせる
		/// </summary>
		inline void FitSlack()
		{
			uint32_t newCapacity = GetSlack(1, size_);
			ChangeCapacity(newCapacity);
		}

		/// <summary>
		/// キャパシティを要素数に合わせる
		/// </summary>
		inline void Fit()
		{
			ChangeCapacity(size_);
		}

		/// <summary>
		/// いい感じに余裕をもってキャパシティを拡大する
		/// </summary>
		/// <param name="capacity"></param>
		inline void ReserveSlack(const uint32_t capacity)
		{
			// 指定した容量が今と同じかそれ以下だったら何もしない
			if (capacity_ >= capacity)
			{
				return;
			}

			uint32_t newCapacity = GetSlack(capacity_, capacity);
			ChangeCapacity(newCapacity);
		}

		/// <summary>
		/// キャパシティを拡大
		/// 縮小はしない
		/// </summary>
		/// <param name="capacity"></param>
		inline void Reserve(uint32_t capacity)
		{
			if (capacity == 0)
			{
				capacity = 1;
			}
			// 指定した容量が今と同じかそれ以下だったら何もしない
			if (capacity_ >= capacity)
			{
				return;
			}

			ChangeCapacity(capacity);
		}

		/// <summary>
		/// キャパシティを縮小
		/// 拡大はしない
		/// サイズより小さくはならない
		/// </summary>
		/// <param name="capacity"></param>
		inline void Reduce(const uint32_t capacity)
		{
			// 指定した容量が今と同じかそれ以下だったら何もしない
			if (capacity_ <= capacity)
			{
				return;
			}
			ChangeCapacity(capacity);
		}

		/// <summary>
		/// キャパシティを変更
		/// サイズより小さくはならない
		/// </summary>
		/// <param name="capacity"></param>
		inline void ChangeCapacity(const uint32_t capacity)
		{
			const bool equalCapacity = capacity == capacity_;
			const bool smallerSize = size_ > capacity;

			if (equalCapacity || smallerSize)
			{
				return;
			}

			ChangeMemorySize(capacity);

		}

		/// <summary>
		/// メモリのサイズを変更し、再配置を行う
		/// 容量が要素数より少ない場合、要素は開放される
		/// </summary>
		/// <param name="capacity"></param>
		inline void ChangeMemorySize(const uint32_t capacity)
		{
			if (capacity == capacity_)
			{
				return;
			}

			// 新しいメモリを確保
			Type* newBegin = alloc_.Allocate<Type>(capacity);
			assert(newBegin != nullptr && "メモリを確保できませんでした");
			if (newBegin == nullptr)
			{
				return;
			}

			uint32_t afterSize = size_;
			if (size_ > capacity)
			{
				afterSize = capacity;
			}

			if constexpr (std::is_trivially_copyable_v<Type>)
			{
				if (afterSize > 0 && begin_ != nullptr)
				{
					::memmove(newBegin, begin_, sizeof(Type) * afterSize);
				}
			}
			else
			{
				// 新しいメモリに要素をコピー
				for (uint32_t i = 0; i < afterSize; ++i)
				{
					new (newBegin + i) Type(Move(*(begin_ + i)));
				}
			}

			// 前のメモリのインスタンスを開放
			if constexpr (!std::is_trivially_destructible_v<Type>)
			{
				for (uint32_t i = 0; i < size_; ++i)
				{
					Type* ptr = begin_ + i;
					ptr->~Type();
				}
			}

			// 前のメモリを開放し、新しいメモリをセット
			if (begin_ != nullptr)
			{
				alloc_.Free(begin_);
				begin_ = nullptr;
			}
			begin_ = newBegin;
			size_ = afterSize;
			capacity_ = capacity;
		}

	private:

		inline void EnsureCapacityForOne()
		{
			if (capacity_ <= size_)
			{
				ReserveSlack(size_ + 1);
			}
		}

	private:

		MemoryAllocatorAPIHolder alloc_;

		Type* begin_;

		uint32_t size_;

		uint32_t capacity_;
	};
}
