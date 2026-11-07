#pragma once
// 連続したメモリに要素を並べる可変長配列 Vector<T>。
// ECOSTL dev の実装を土台にした（自分の要素を渡したときの Insert/Resize/PushBack、1.5 倍の伸び方、コピー代入での領域の使い回しなどが dev のテストで確かめられている）。
// testGame 版から ReserveSlack・FitSlack・Assign・EraseRange・初期化子リストのコンストラクタ・比較を引き継いだ。
#include <cassert>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <new>
#include "ECOSTL/Core/Template/Template.h"
#include "ECOSTL/Memory/Allocator/MemoryAllocator.h"
#include "ECOSTL/Core/Memory/MemCopy.h"

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
		explicit Vector(MemoryAllocatorDelegate* allocator)
			: alloc_(allocator)
			, begin_(nullptr)
			, size_(0)
			, capacity_(0)
		{
		}
		explicit Vector(const MemoryAllocatorRef& allocator)
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
		Vector(uint32_t capacity, MemoryAllocatorDelegate* allocator)
			: alloc_(allocator)
			, begin_(nullptr)
			, size_(0)
			, capacity_(0)
		{
			Reserve(capacity);
		}
		Vector(uint32_t capacity, const MemoryAllocatorRef& allocator)
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

			if constexpr (IsTriviallyCopyableV<Type>)
			{
				if (src.size_ > 0)
				{
					MemCopy(begin_, src.begin_, sizeof(Type) * src.size_);
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

			if constexpr (IsTriviallyCopyableV<Type>)
			{
				MemMove(begin_ + index, begin_ + index + 1, sizeof(Type) * (size_ - index - 1));
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

			if constexpr (IsTriviallyCopyableV<Type>)
			{
				// 自分の範囲からでも重ならないのでmemcpyでいい
				MemCopy(dest, src, sizeof(Type) * count);
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
		// testGame 版から引き継いだ機能（ECOSTL の流儀の名前で）
		//------------------------------------------

		/// <summary>初期化子リストの値で作る（例: Vector&lt;int&gt; v({ 1, 2, 3 }, alloc)）</summary>
		Vector(std::initializer_list<Type> list, MemoryAllocatorDelegate* allocator = nullptr)
			: alloc_(allocator)
			, begin_(nullptr)
			, size_(0)
			, capacity_(0)
		{
			Assign(list.begin(), static_cast<uint32_t>(list.size()));
		}

		/// <summary>
		/// 余裕を持って容量を capacity 以上にする（1.5 倍ずつ増やした大きさにする。縮小はしない）
		/// 少しずつ増える使い方で、確保の回数を減らすために使う
		/// </summary>
		/// <returns>確保できなければ false（容量は変わらない）</returns>
		inline bool ReserveSlack(const uint32_t capacity)
		{
			if (capacity_ >= capacity)
			{
				return true;
			}
			return Reserve(GetSlack(capacity_, capacity));
		}

		/// <summary>容量を、要素数に余裕を持たせた大きさに合わせる（Fit より少し大きめ。縮めもする）</summary>
		/// <returns>確保できなければ false（容量は変わらない）</returns>
		inline bool FitSlack()
		{
			const uint32_t target = GetSlack(1, size_);
			if (target < capacity_)
			{
				return ShrinkTo(target);
			}
			return Reserve(target);
		}

		/// <summary>全要素を捨てて、count 個の value のコピーで置き換える</summary>
		/// <returns>確保できなければ false（要素は空になる）</returns>
		inline bool Assign(const uint32_t count, const Type& value)
		{
			Clear();
			if (!Reserve(count))
			{
				return false;
			}
			for (uint32_t i = 0; i < count; ++i) { EmplaceBack(value); }
			return true;
		}

		/// <summary>全要素を捨てて、src から count 個のコピーで置き換える（src が自分の要素を指していてはいけない）</summary>
		/// <returns>確保できなければ false（要素は空になる）</returns>
		inline bool Assign(const Type* src, const uint32_t count)
		{
			assert((count == 0 || !IsOwnElement(src)) && "自分の要素から Assign はできません");
			Clear();
			if (!Reserve(count))
			{
				return false;
			}
			for (uint32_t i = 0; i < count; ++i) { EmplaceBack(src[i]); }
			return true;
		}

		/// <summary>
		/// 添え字 index から count 個を消して、後ろを一度で詰める（順序維持）。範囲外なら何もせず false
		/// </summary>
		inline bool EraseRange(const uint32_t index, const uint32_t count)
		{
			if (count == 0)
			{
				return index <= size_;
			}
			if (index >= size_ || count > size_ - index)
			{
				return false;
			}
			for (uint32_t i = index + count; i < size_; ++i)
			{
				begin_[i - count] = Move(begin_[i]);
			}
			if constexpr (!IsTriviallyDestructibleV<Type>)
			{
				for (uint32_t i = size_ - count; i < size_; ++i)
				{
					(begin_ + i)->~Type();
				}
			}
			size_ -= count;
			return true;
		}

		bool operator==(const Vector& other)const
		{
			if (size_ != other.size_)
			{
				return false;
			}
			for (uint32_t i = 0; i < size_; ++i)
			{
				if (!(begin_[i] == other.begin_[i]))
				{
					return false;
				}
			}
			return true;
		}
		bool operator!=(const Vector& other)const
		{
			return !(*this == other);
		}

	private:
		// start から 1.5 倍ずつ増やして need を超える大きさを返す（最小 2。あふれるなら UINT32_MAX）
		static uint32_t GetSlack(const uint32_t start, const uint32_t need)
		{
			uint64_t capacity = start < 2 ? 2 : start;
			while (capacity <= need)
			{
				capacity += capacity >> 1;
			}
			return capacity > UINT32_MAX ? UINT32_MAX : static_cast<uint32_t>(capacity);
		}

	public:

		//------------------------------------------
		// メモリ操作
		//------------------------------------------

		/// <summary>
		/// 要素数を変える
		/// 増えた要素はargsをコンストラクタ引数にして作る
		/// 減らしてもメモリは開放しない
		/// </summary>
		/// <param name="...args">自分の要素を渡してもよい</param>
		/// <returns>確保できなければ false（要素数も中身も変わらない）</returns>
		template<typename...ArgTypes>
		inline bool Resize(const uint32_t size, const ArgTypes&...args)
		{
			// もしメモリが足りなければ、新しいメモリに先に作ってから移す
			if (size > capacity_)
			{
				return ReallocateAndConstruct(GetGrowCapacity(size), size,
					[&](Type* ptr) { new (ptr) Type(args...); });
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
			return true;
		}

		/// <summary>
		/// キャパシティを拡大
		/// 縮小はしない
		/// </summary>
		/// <returns>確保できなければ false（容量は変わらない）</returns>
		inline bool Reserve(const uint32_t capacity)
		{
			// 指定した容量が今と同じかそれ以下だったら何もしない
			if (capacity_ >= capacity)
			{
				return true;
			}

			return SetCapacity(capacity);
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
		/// <returns>確保できなければ false（容量は変わらない）</returns>
		inline bool Fit()
		{
			return SetCapacity(size_);
		}

		/// <summary>
		/// キャパシティを縮小
		/// 拡大はしない
		/// 要素数より小さくはならない
		/// </summary>
		/// <returns>確保できなければ false（容量は変わらない）</returns>
		inline bool ShrinkTo(const uint32_t capacity)
		{
			// 指定した容量が今と同じかそれ以上だったら何もしない
			if (capacity_ <= capacity)
			{
				return true;
			}
			return SetCapacity(capacity < size_ ? size_ : capacity);
		}

		/// <summary>
		/// キャパシティを変更
		/// 要素数より小さい値なら何もしない
		/// 0ならメモリを開放する
		/// </summary>
		/// <returns>確保できなければ false（容量は変わらない）。要素数より小さい値で何もしなかったときは true</returns>
		inline bool SetCapacity(const uint32_t capacity)
		{
			if (capacity == capacity_ || capacity < size_)
			{
				return true;
			}

			if (capacity == 0)
			{
				Reset();
				return true;
			}

			return ReallocateAndConstruct(capacity, size_, [](Type*) {});
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

			if (capacity_ <= size_ && !Grow(size_ + 1))
			{
				return nullptr;
			}

			if constexpr (IsTriviallyCopyableV<Type>)
			{
				MemMove(begin_ + index + 1, begin_ + index, sizeof(Type) * (size_ - index));
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
		inline bool Grow(const uint32_t need)
		{
			// 指定した容量が今と同じかそれ以下だったら何もしない
			if (capacity_ >= need)
			{
				return true;
			}

			return SetCapacity(GetGrowCapacity(need));
		}

		/// <summary>
		/// count個の要素を破棄する
		/// </summary>
		static inline void Destroy(Type* first, const uint32_t count)
		{
			if constexpr (!IsTriviallyDestructibleV<Type>)
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
			if (newBegin == nullptr)
			{
				UTL_LOG(Warn | ::utl::logtype::Container, "vector grow failed size={size} newCapacity={newCapacity} elementSize={elementSize}", size_, newCapacity, static_cast<uint32_t>(sizeof(Type)));
			}
			// 失敗は戻り値で呼ぶ側へ伝える（止めない）
			if (newBegin == nullptr)
			{
				return false;
			}

			for (uint32_t i = size_; i < newSize; ++i)
			{
				construct(newBegin + i);
			}

			// 新しいメモリに要素を移して、前のメモリの要素を破棄
			if constexpr (IsTriviallyCopyableV<Type>)
			{
				if (size_ > 0)
				{
					MemCopy(newBegin, begin_, sizeof(Type) * size_);
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
