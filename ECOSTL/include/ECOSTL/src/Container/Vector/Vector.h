#pragma once
#include <cassert>
#include <cstdint>
#include <cstring>
#include <new>
#include <type_traits>
#include "../../Core/Utility.h"
#include "../../Memory/Allocate/MemoryAllocatorAPI.h"

namespace utl
{
	/// <summary>
	/// 要素を連続したメモリに並べる可変長配列
	/// </summary>
	/// <typeparam name="Type"></typeparam>
	template<typename Type>
	class Vector
	{
	public:
		// 作成
		//----------------------

		Vector()
			: alloc_(nullptr)
			, begin_(nullptr)
			, size_(0)
			, capacity_(0)
		{
		}

		/// <summary>
		/// 使用するメモリアロケータを指定して作成する
		/// </summary>
		/// <param name="allocator">nullptrならC++標準の手段で確保する</param>
		explicit Vector(MemoryAllocatorRef* allocator)
			: alloc_(allocator)
			, begin_(nullptr)
			, size_(0)
			, capacity_(0)
		{
		}

		/// <summary>
		/// キャパシティとメモリアロケータを指定して作成する
		/// </summary>
		/// <param name="capacity">0ならメモリを確保しない</param>
		/// <param name="allocator">nullptrならC++標準の手段で確保する</param>
		Vector(uint32_t capacity, MemoryAllocatorRef* allocator)
			: alloc_(allocator)
			, begin_(nullptr)
			, size_(0)
			, capacity_(0)
		{
			Reserve(capacity);
		}

		// 代入
		//----------------------

		/// <summary>
		/// コピーして作成
		/// メモリアロケータもコピー元と同じものを使う
		/// </summary>
		/// <param name="src"></param>
		Vector(const Vector& src)
			: alloc_(src.alloc_)
			, begin_(nullptr)
			, size_(0)
			, capacity_(0)
		{
			*this = src;
		}

		/// <summary>
		/// コピーして代入
		/// メモリアロケータは今のものを使い続ける
		/// 今のメモリに入りきるならそのまま使う
		/// </summary>
		/// <param name="src"></param>
		/// <returns></returns>
		inline Vector& operator=(const Vector& src)
		{
			if (this == &src)
			{
				return *this;
			}

			Clear();

			// 入りきらないときだけ確保し直す
			if (capacity_ < src.size_)
			{
				if (!ReallocateAndConstruct(src.size_, 0, [](Type*) {}))
				{
					return *this;
				}
			}

			if constexpr (std::is_trivially_copyable_v<Type>)
			{
				if (src.size_ > 0)
				{
					::memcpy(begin_, src.begin_, sizeof(Type) * src.size_);
				}
			}
			else
			{
				for (uint32_t i = 0; i < src.size_; i++)
				{
					new (begin_ + i) Type(*(src.begin_ + i));
				}
			}
			size_ = src.size_;
			return *this;
		}

		/// <summary>
		/// 移動して作成する
		/// </summary>
		/// <param name="src"></param>
		Vector(Vector&& src)noexcept
			: alloc_(src.alloc_)
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
		/// メモリアロケータも移動元のものを引き継ぐ
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

			alloc_ = src.alloc_;
			begin_ = src.begin_;
			size_ = src.size_;
			capacity_ = src.capacity_;

			src.begin_ = nullptr;
			src.size_ = 0;
			src.capacity_ = 0;
			return *this;
		}

		~Vector()
		{
			Reset();
		}

		// 削除
		// メモリは開放しない
		//----------------------

		/// <summary>
		/// 要素をすべて削除する
		/// メモリは開放しない
		/// </summary>
		inline void Clear()
		{
			Destroy(begin_, size_);
			size_ = 0;
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
			Destroy(begin_ + size_ - 1, 1);
			--size_;
		}

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
				PopBack();
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

		// 追加
		//----------------------

		/// <summary>
		/// 末尾に追加
		/// </summary>
		/// <param name="src">自分の要素を渡してもよい</param>
		/// <returns></returns>
		inline Type* PushBack(const Type& src)
		{
			return EmplaceBack(src);
		}
		/// <summary>
		/// 末尾にムーブして追加
		/// </summary>
		/// <param name="src">自分の要素を渡してもよい</param>
		/// <returns></returns>
		inline Type* PushBack(Type&& src)
		{
			return EmplaceBack(Move(src));
		}
		/// <summary>
		/// 末尾に追加
		/// </summary>
		/// <returns></returns>
		inline Type* PushBack()
		{
			return EmplaceBack();
		}

		/// <summary>
		/// コンストラクタ引数を渡して末尾に追加
		/// </summary>
		/// <typeparam name="...ArgTypes"></typeparam>
		/// <param name="...args">自分の要素を渡してもよい</param>
		/// <returns>確保できなければnullptr</returns>
		template<typename...ArgTypes>
		inline Type* EmplaceBack(ArgTypes&&...args)
		{
			if (size_ < capacity_)
			{
				Type* ptr = begin_ + size_;
				new (ptr) Type(Forward<ArgTypes>(args)...);
				++size_;
				return ptr;
			}

			// 引数が今の要素を指してるかもしれないので、新しいメモリに先に作ってから移す
			const bool isSuccess = ReallocateAndConstruct(GetGrowCapacity(size_ + 1), size_ + 1,
				[&](Type* ptr) { new (ptr) Type(Forward<ArgTypes>(args)...); });
			if (!isSuccess)
			{
				return nullptr;
			}
			return begin_ + (size_ - 1);
		}

		/// <summary>
		/// まとめて追加
		/// </summary>
		/// <param name="src">自分の要素でもよい</param>
		/// <param name="count"></param>
		/// <returns></returns>
		inline Type* AppendRange(const Type* src, const uint32_t count)
		{
			if (src == nullptr || count == 0)
			{
				return begin_ + size_;
			}

			const uint32_t insertIndex = size_;
			const bool isSelfRange = IsOwnElement(src);
			const uint32_t srcOffset = isSelfRange ? static_cast<uint32_t>(src - begin_) : 0;

			Grow(size_ + count);

			// メモリが移動していたら指し直す
			if (isSelfRange)
			{
				src = begin_ + srcOffset;
			}

			Type* dest = begin_ + insertIndex;

			if constexpr (std::is_trivially_copyable_v<Type>)
			{
				// 自分の範囲からでも重ならないのでmemcpyでいい
				::memcpy(dest, src, sizeof(Type) * count);
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

		//------------------------------------------
		// 挿入
		//------------------------------------------

		/// <summary>
		/// 指定した添え字に挿入する
		/// </summary>
		/// <param name="index">Size()以下</param>
		/// <param name="value">自分の要素を渡してもよい</param>
		/// <returns>範囲外ならnullptr</returns>
		inline Type* Insert(const uint32_t index, const Type& value)
		{
			// 自分の要素ならずらすと中身が変わるので、先にコピーしておく
			if (IsOwnElement(&value))
			{
				return Emplace(index, value);
			}
			return EmplaceAt(index, value);
		}

		inline Type* Insert(const uint32_t index, Type&& value)
		{
			if (IsOwnElement(&value))
			{
				return Emplace(index, Move(value));
			}
			return EmplaceAt(index, Move(value));
		}

		/// <summary>
		/// コンストラクタ引数を渡して指定した添え字に挿入する
		/// 途中に挿入するときは一度ローカルに作ってからムーブする
		/// 要素を挿入するだけならInsertの方が速い
		/// </summary>
		/// <param name="index">Size()以下</param>
		/// <param name="...args">自分の要素を渡してもよい</param>
		/// <returns>範囲外ならnullptr</returns>
		template<typename... ArgTypes>
		inline Type* Emplace(const uint32_t index, ArgTypes&&... args)
		{
			assert(index <= size_ && "範囲外Insertです");
			if (index > size_)
			{
				return nullptr;
			}

			if (index == size_)
			{
				// 完全転送して末尾構築
				return EmplaceBack(Forward<ArgTypes>(args)...);
			}

			// 引数が要素を指してるかもしれないので先に作っておく
			Type value(Forward<ArgTypes>(args)...);
			return EmplaceAt(index, Move(value));
		}

		//------------------------------------------
		// 取得
		//------------------------------------------

		/// <summary>
		/// 範囲外はnullptrを返す
		/// </summary>
		inline Type* Get(const uint32_t index)
		{
			if (index >= size_)
			{
				return nullptr;
			}
			return begin_ + index;
		}
		inline const Type* Get(const uint32_t index)const
		{
			if (index >= size_)
			{
				return nullptr;
			}
			return begin_ + index;
		}

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

		inline Type* Begin()
		{
			return begin_;
		}
		inline const Type* Begin()const
		{
			return begin_;
		}

		/// <summary>
		/// 最後の要素の次を返す
		/// </summary>
		inline Type* End()
		{
			return begin_ + size_;
		}
		inline const Type* End()const
		{
			return begin_ + size_;
		}

		// 範囲for文用
		inline Type* begin() { return Begin(); }
		inline const Type* begin()const { return Begin(); }
		inline Type* end() { return End(); }
		inline const Type* end()const { return End(); }

		/// <summary>
		/// 最後の要素を返す
		/// 空ならnullptr
		/// </summary>
		inline Type* Back()
		{
			if (size_ == 0)
			{
				return nullptr;
			}
			return begin_ + (size_ - 1);
		}
		inline const Type* Back()const
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

		inline bool IsEmpty()const
		{
			return size_ == 0;
		}

		/// <summary>
		/// 引数の添え字が範囲内か
		/// </summary>
		/// <param name="index"></param>
		/// <returns></returns>
		inline bool IsValidIndex(const uint32_t index)const
		{
			return index < size_;
		}

		inline const MemoryAllocatorRef& MemAllocator()const
		{
			return alloc_;
		}

		//------------------------------------------
		// メモリ操作
		//------------------------------------------

		/// <summary>
		/// 要素数を変える
		/// 増えた要素はargsをコンストラクタ引数にして作る
		/// 減らしてもメモリは開放しない
		/// </summary>
		/// <param name="...args">自分の要素を渡してもよい</param>
		template<typename...ArgTypes>
		inline void Resize(const uint32_t size, const ArgTypes&...args)
		{
			// もしメモリが足りなければ、新しいメモリに先に作ってから移す
			if (size > capacity_)
			{
				ReallocateAndConstruct(GetGrowCapacity(size), size,
					[&](Type* ptr) { new (ptr) Type(args...); });
				return;
			}

			// 要素数が増えたならその分を初期化
			for (uint32_t i = size_; i < size; ++i)
			{
				new (begin_ + i) Type(args...);
			}

			// 要素数が減ったならその分開放
			if (size < size_)
			{
				Destroy(begin_ + size, size_ - size);
			}

			size_ = size;
		}

		/// <summary>
		/// キャパシティを拡大
		/// 縮小はしない
		/// </summary>
		/// <param name="capacity"></param>
		inline void Reserve(const uint32_t capacity)
		{
			// 指定した容量が今と同じかそれ以下だったら何もしない
			if (capacity_ >= capacity)
			{
				return;
			}

			SetCapacity(capacity);
		}

		// メモリの開放
		//----------------------

		/// <summary>
		/// 要素全て削除してメモリを開放する
		/// 作った直後と同じ状態になる
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
		/// キャパシティを要素数に合わせる
		/// 要素数が0ならメモリを開放する
		/// </summary>
		inline void Fit()
		{
			SetCapacity(size_);
		}

		/// <summary>
		/// キャパシティを縮小
		/// 拡大はしない
		/// 要素数より小さくはならない
		/// </summary>
		/// <param name="capacity"></param>
		inline void ShrinkTo(const uint32_t capacity)
		{
			// 指定した容量が今と同じかそれ以上だったら何もしない
			if (capacity_ <= capacity)
			{
				return;
			}
			SetCapacity(capacity < size_ ? size_ : capacity);
		}

		/// <summary>
		/// キャパシティを変更
		/// 要素数より小さい値なら何もしない
		/// 0ならメモリを開放する
		/// </summary>
		/// <param name="capacity"></param>
		inline void SetCapacity(const uint32_t capacity)
		{
			if (capacity == capacity_ || capacity < size_)
			{
				return;
			}

			if (capacity == 0)
			{
				Reset();
				return;
			}

			ReallocateAndConstruct(capacity, size_, [](Type*) {});
		}

	private:

		/// <summary>
		/// 指定した添え字に直接作る
		/// argsに自分の要素を渡してはいけない
		/// </summary>
		template<typename... ArgTypes>
		inline Type* EmplaceAt(const uint32_t index, ArgTypes&&... args)
		{
			assert(index <= size_ && "範囲外Insertです");
			if (index > size_)
			{
				return nullptr;
			}

			if (index == size_)
			{
				return EmplaceBack(Forward<ArgTypes>(args)...);
			}

			if (capacity_ <= size_)
			{
				Grow(size_ + 1);
				if (capacity_ <= size_)
				{
					return nullptr;
				}
			}

			if constexpr (std::is_trivially_copyable_v<Type>)
			{
				::memmove(begin_ + index + 1, begin_ + index, sizeof(Type) * (size_ - index));
			}
			else
			{
				// 後ろへ 1 つずつずらす（後ろから Move 構築して元を破棄）
				for (uint32_t i = size_; i > index; --i)
				{
					new (begin_ + i) Type(Move(*(begin_ + (i - 1))));
					(begin_ + (i - 1))->~Type();
				}
			}

			new (begin_ + index) Type(Forward<ArgTypes>(args)...);
			++size_;

			return begin_ + index;
		}

		/// <summary>
		/// ptrが自分の要素を指しているか
		/// </summary>
		inline bool IsOwnElement(const Type* ptr)const
		{
			return begin_ != nullptr && ptr >= begin_ && ptr < begin_ + size_;
		}

		/// <summary>
		/// 広げるときの新しい容量を返す
		/// 今の容量を1.5倍した値とneedの大きい方
		/// </summary>
		inline uint32_t GetGrowCapacity(const uint32_t need)const
		{
			uint32_t grown = 2;
			if (capacity_ >= 2)
			{
				const uint32_t add = capacity_ >> 1;
				// 上限を超えるならUINT32_MAXにする
				grown = capacity_ > UINT32_MAX - add ? UINT32_MAX : capacity_ + add;
			}
			return need > grown ? need : grown;
		}

		/// <summary>
		/// 余裕をもってキャパシティを拡大する
		/// </summary>
		inline void Grow(const uint32_t need)
		{
			// 指定した容量が今と同じかそれ以下だったら何もしない
			if (capacity_ >= need)
			{
				return;
			}

			SetCapacity(GetGrowCapacity(need));
		}

		/// <summary>
		/// count個の要素を破棄する
		/// </summary>
		static inline void Destroy(Type* first, const uint32_t count)
		{
			if constexpr (!std::is_trivially_destructible_v<Type>)
			{
				for (uint32_t i = 0; i < count; ++i)
				{
					(first + i)->~Type();
				}
			}
		}

		/// <summary>
		/// 新しいメモリを確保して、増えた分の要素をconstructで作ってから今の要素を移す
		/// 先に作るので、constructが今の要素を使っていてもいい
		/// </summary>
		/// <param name="newCapacity">newSize以上</param>
		/// <param name="newSize">今の要素数以上</param>
		/// <param name="construct">受け取ったアドレスに要素を1つ作る関数</param>
		/// <returns>確保できなければfalse</returns>
		template<typename ConstructFunc>
		inline bool ReallocateAndConstruct(const uint32_t newCapacity, const uint32_t newSize, ConstructFunc&& construct)
		{
			assert(newCapacity >= newSize && newSize >= size_);

			Type* newBegin = alloc_.Allocate<Type>(newCapacity);
			assert(newBegin != nullptr && "メモリを確保できませんでした");
			if (newBegin == nullptr)
			{
				return false;
			}

			for (uint32_t i = size_; i < newSize; ++i)
			{
				construct(newBegin + i);
			}

			// 新しいメモリに要素を移して、前のメモリの要素を破棄
			if constexpr (std::is_trivially_copyable_v<Type>)
			{
				if (size_ > 0)
				{
					::memcpy(newBegin, begin_, sizeof(Type) * size_);
				}
			}
			else
			{
				for (uint32_t i = 0; i < size_; ++i)
				{
					new (newBegin + i) Type(Move(*(begin_ + i)));
				}
				Destroy(begin_, size_);
			}

			// 前のメモリを開放し、新しいメモリをセット
			alloc_.Free(begin_);
			begin_ = newBegin;
			size_ = newSize;
			capacity_ = newCapacity;
			return true;
		}

	private:

		MemoryAllocatorRef alloc_;

		Type* begin_;

		uint32_t size_;

		uint32_t capacity_;

	};
}
