#pragma once
// 自動でメモリを開放するポインタ UniquePtr<T> と配列版 UniquePtr<T[]>。
// ECOSTL dev（タスク #32）の実装を土台に、testGame 版の配列版と std と同じ名前（get/reset）を加えた。
#include <cassert>
#include <cstdint>
#include <new>
#include "ECOSTL/Core/Template/Template.h"
#include "ECOSTL/Memory/Allocator/MemoryAllocator.h"

namespace utl
{
	/// <summary>
	/// 自動でメモリを開放するポインタ
	/// </summary>
	/// <typeparam name="Type"></typeparam>
	template<typename Type>
	class UniquePtr
	{
	private:
		template<typename SrcType>
		friend class UniquePtr;
	private:

		template<typename T, typename...ArgTypes>
		friend UniquePtr<T> MakeUniqueWithAlloc(MemoryAllocatorDelegate* allocator, ArgTypes&&...args);

		//----------------------------------------
		// 作成
		//----------------------------------------

		/// <summary>
		/// 作成したインスタンスとインスタンスを作成したメモリアロケータを指定して作成するコンストラクタ
		/// MakeUniqueからのみ使用することを想定している
		/// </summary>
		/// <param name="ptr"></param>
		/// <param name="allocator"></param>
		UniquePtr(Type* ptr, MemoryAllocatorDelegate* allocator)
			: alloc_(allocator)
			, ptr_(ptr)
			, block_(const_cast<RemoveCVType<Type>*>(ptr))
			, delete_(&DeleteAs<RemoveCVType<Type>>)
		{

		}

	public:

		/// <summary>
		/// 空のUniquePtrを作成するコンストラクタ
		/// </summary>
		UniquePtr()noexcept
			: alloc_(nullptr)
			, ptr_(nullptr)
			, block_(nullptr)
			, delete_(nullptr)
		{
		}

		UniquePtr(NullptrT)noexcept
			: UniquePtr()
		{

		}

		//----------------------------------------
		// 代入
		//----------------------------------------

		/// <summary>
		/// コピーは禁止
		/// </summary>
		/// <param name=""></param>
		UniquePtr(const UniquePtr&) = delete;
		/// <summary>
		/// コピーは禁止
		/// </summary>
		/// <param name=""></param>
		/// <returns></returns>
		UniquePtr& operator=(const UniquePtr&) = delete;

		/// <summary>
		/// ムーブを行う
		/// </summary>
		/// <param name="src"></param>
		UniquePtr(UniquePtr&& src)noexcept
			: alloc_(src.alloc_)
			, ptr_(src.ptr_)
			, block_(src.block_)
			, delete_(src.delete_)
		{
			src.Clear();
		}
		/// <summary>
		/// ムーブを行う
		/// すでに管理してるインスタンスは、引き継いだ後に開放する
		/// </summary>
		/// <param name="src"></param>
		/// <returns></returns>
		UniquePtr& operator=(UniquePtr&& src)noexcept
		{
			// 自分自身をムーブしても、いったん一時オブジェクトに移して戻すだけになる
			UniquePtr(Move(src)).Swap(*this);
			return *this;
		}

		/// <summary>
		/// 派生クラスからキャストしてムーブ
		/// </summary>
		/// <typeparam name="SubType"></typeparam>
		/// <param name="src"></param>
		template<typename SubType, EnableIfType<IsConvertibleV<SubType*, Type*>, int> = 0>
		UniquePtr(UniquePtr<SubType>&& src)noexcept
			: alloc_(src.alloc_)
			, ptr_(src.ptr_)
			, block_(src.block_)
			, delete_(src.delete_)
		{
			src.Clear();
		}
		/// <summary>
		/// 派生クラスからキャストしてムーブ
		/// </summary>
		/// <typeparam name="SubType"></typeparam>
		/// <param name="src"></param>
		/// <returns></returns>
		template<typename SubType, EnableIfType<IsConvertibleV<SubType*, Type*>, int> = 0>
		UniquePtr& operator=(UniquePtr<SubType>&& src)noexcept
		{
			UniquePtr(Move(src)).Swap(*this);
			return *this;
		}

		UniquePtr& operator=(NullptrT)noexcept
		{
			Reset();
			return *this;
		}

		/// <summary>
		/// 管理しているインスタンスを入れ替える
		/// </summary>
		/// <param name="other"></param>
		void Swap(UniquePtr& other)noexcept
		{
			utl::Swap(alloc_, other.alloc_);
			utl::Swap(ptr_, other.ptr_);
			utl::Swap(block_, other.block_);
			utl::Swap(delete_, other.delete_);
		}


		//----------------------------------------
		// 開放
		//----------------------------------------

		/// <summary>
		/// インスタンスを開放する
		/// 作成したときの型で破棄し、作成したときのアドレスを開放する
		/// </summary>
		void Reset()noexcept
		{
			if (block_ == nullptr)
			{
				return;
			}

			// 先に空にしてから破棄する（破棄中にこのポインタが使われても開放済みのものを指さない）
			MemoryAllocatorRef alloc = alloc_;
			void* block = block_;
			DeleteFunc deleteFunc = delete_;
			Clear();

			const bool freed = deleteFunc(alloc, block);
			assert(freed && "メモリの開放に失敗しました");
			(void)freed;
		}

		~UniquePtr()
		{
			Reset();
		}



		//----------------------------------------
		// 取得
		//----------------------------------------

		/// <summary>
		/// 管理してるインスタンスのポインタを取得
		/// </summary>
		/// <returns></returns>
		inline Type* Get()const
		{
			return ptr_;
		}

		Type* operator->()const
		{
			assert(ptr_ != nullptr && "nullptrの実体にアクセスしようとしています！");
			return Get();
		}

		/// <summary>
		/// 管理してるインスタンスの実際を参照
		/// </summary>
		/// <returns></returns>
		Type& operator*()const
		{
			assert(ptr_ != nullptr && "nullptrの実体にアクセスしようとしています！");
			return *ptr_;
		}

		explicit operator bool()const
		{
			return ptr_ != nullptr;
		}

		/// <summary>
		/// インスタンスを作成したメモリアロケータ
		/// </summary>
		/// <returns></returns>
		const MemoryAllocatorRef& MemAllocator()const
		{
			return alloc_;
		}

		//----------------------------------------
		// 比較
		//----------------------------------------

		bool operator==(const Type* ptr)const
		{
			return ptr_ == ptr;
		}
		bool operator==(const UniquePtr<Type>& ptr)const
		{
			return ptr_ == ptr.Get();
		}

		bool operator!=(const Type* ptr)const
		{
			return !((*this) == ptr);
		}
		bool operator!=(const UniquePtr<Type>& ptr)const
		{
			return !((*this) == ptr);
		}




	private:

		/// <summary>
		/// 開放せずに空にする（ムーブ元に使う）
		/// </summary>
		void Clear()noexcept
		{
			alloc_.Reset();
			ptr_ = nullptr;
			block_ = nullptr;
			delete_ = nullptr;
		}

		MemoryAllocatorRef alloc_;

		Type* ptr_;

		// 確保したメモリブロックの先頭（派生クラスから変換するとptr_とずれることがある）
		void* block_;

		// 作成したときの型で破棄して開放する関数
		DeleteFunc delete_;

	};

	/// <summary>
	/// アロケータを指定してUniquePtrを作成する関数
	/// 確保できなければ空のUniquePtrを返す
	/// </summary>
	/// <typeparam name="Type"></typeparam>
	/// <typeparam name="...ArgTypes"></typeparam>
	/// <param name="...args"></param>
	/// <param name="allocator"></param>
	/// <returns></returns>
	template<typename Type, typename...ArgTypes>
	inline UniquePtr<Type> MakeUniqueWithAlloc(MemoryAllocatorDelegate* allocator, ArgTypes&&...args)
	{
		MemoryAllocatorRef allocHolder(allocator);

		// メモリを確保してインスタンスを配置する
		Type* ins = allocHolder.New<Type>(Forward<ArgTypes>(args)...);
		if (ins == nullptr)
		{
			return UniquePtr<Type>();
		}

		// uniquePtrを作成する
		return UniquePtr<Type>(ins, allocator);
	}

	/// <summary>
	/// UniquePtrを作成する関数
	/// </summary>
	/// <typeparam name="Type"></typeparam>
	/// <typeparam name="...ArgTypes"></typeparam>
	/// <param name="...args"></param>
	/// <returns></returns>
	template<typename Type, typename...ArgTypes>
	inline UniquePtr<Type> MakeUnique(ArgTypes&&...args)
	{
		return MakeUniqueWithAlloc<Type>(nullptr, Forward<ArgTypes>(args)...);
	}

	/// <summary>
	/// 配列を持つ UniquePtr（std::unique_ptr&lt;T[]&gt; 相当）。MakeUniqueArray / MakeUniqueArrayWithAlloc で作る
	/// testGame 版から持ち込んだ。要素数のあふれの確認と、要素の作成中に例外が出たときの後始末を加えた
	/// </summary>
	template<typename Type>
	class UniquePtr<Type[]>
	{
		template<typename T>
		friend UniquePtr<T[]> MakeUniqueArrayWithAlloc(MemoryAllocatorDelegate* allocator, uint32_t count);

		UniquePtr(Type* ptr, const uint32_t count, MemoryAllocatorDelegate* allocator)
			: alloc_(allocator)
			, ptr_(ptr)
			, count_(count)
		{
		}

	public:
		UniquePtr()noexcept
			: alloc_(nullptr)
			, ptr_(nullptr)
			, count_(0)
		{
		}
		UniquePtr(NullptrT)noexcept
			: UniquePtr()
		{
		}

		UniquePtr(const UniquePtr&) = delete;
		UniquePtr& operator=(const UniquePtr&) = delete;

		UniquePtr(UniquePtr&& src)noexcept
			: alloc_(src.alloc_)
			, ptr_(src.ptr_)
			, count_(src.count_)
		{
			src.Clear();
		}
		UniquePtr& operator=(UniquePtr&& src)noexcept
		{
			UniquePtr(Move(src)).Swap(*this);
			return *this;
		}
		UniquePtr& operator=(NullptrT)noexcept
		{
			Reset();
			return *this;
		}

		~UniquePtr()
		{
			Reset();
		}

		void Swap(UniquePtr& other)noexcept
		{
			utl::Swap(alloc_, other.alloc_);
			utl::Swap(ptr_, other.ptr_);
			utl::Swap(count_, other.count_);
		}

		/// <summary>
		/// 要素を逆順に破棄して領域を開放し、空にする
		/// </summary>
		void Reset()noexcept
		{
			if (ptr_ == nullptr)
			{
				return;
			}
			MemoryAllocatorRef alloc = alloc_;
			Type* ptr = ptr_;
			const uint32_t count = count_;
			Clear();
			if constexpr (!IsTriviallyDestructibleV<Type>)
			{
				for (uint32_t i = count; i > 0; --i)
				{
					ptr[i - 1].~Type();
				}
			}
			const bool freed = alloc.Free(static_cast<void*>(const_cast<RemoveCVType<Type>*>(ptr)));
			assert(freed && "メモリの開放に失敗しました");
			(void)freed;
		}

		inline Type* Get()const
		{
			return ptr_;
		}
		inline uint32_t Count()const
		{
			return count_;
		}
		inline Type& operator[](const uint32_t index)const
		{
			assert(ptr_ != nullptr && index < count_ && "範囲外アクセスです");
			return ptr_[index];
		}
		explicit operator bool()const
		{
			return ptr_ != nullptr;
		}

		bool operator==(const Type* ptr)const
		{
			return ptr_ == ptr;
		}
		bool operator!=(const Type* ptr)const
		{
			return ptr_ != ptr;
		}

	private:
		// 開放せずに空にする（ムーブ元に使う）
		void Clear()noexcept
		{
			alloc_.Reset();
			ptr_ = nullptr;
			count_ = 0;
		}

		MemoryAllocatorRef alloc_;  // 確保元のアロケータ
		Type* ptr_;  // 配列の先頭（空なら nullptr。このクラスが所有）
		uint32_t count_;  // 要素数
	};

	/// <summary>
	/// アロケータを指定して配列を作る。要素は値初期化される（std::make_unique&lt;T[]&gt;(n) と同じ。数値型は 0）
	/// 確保できない、または大きさがあふれるなら空を返す。要素の作成中に例外が出たら、作った要素を破棄して開放してから投げ直す
	/// </summary>
	template<typename Type>
	inline UniquePtr<Type[]> MakeUniqueArrayWithAlloc(MemoryAllocatorDelegate* allocator, const uint32_t count)
	{
		MemoryAllocatorRef allocHolder(allocator);
		// 要素数0でも有効なポインタにするため、1個分は確保する
		Type* ptr = allocHolder.Allocate<Type>(count == 0 ? 1 : count);
		if (ptr == nullptr)
		{
			return UniquePtr<Type[]>();
		}
		uint32_t constructed = 0;
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
		try
		{
#endif
			for (; constructed < count; ++constructed)
			{
				new (ptr + constructed) Type();
			}
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
		}
		catch (...)
		{
			for (uint32_t i = constructed; i > 0; --i)
			{
				ptr[i - 1].~Type();
			}
			allocHolder.Free(ptr);
			throw;
		}
#endif
		return UniquePtr<Type[]>(ptr, count, allocator);
	}

	/// <summary>
	/// 既定のアロケータで配列を作る
	/// </summary>
	template<typename Type>
	inline UniquePtr<Type[]> MakeUniqueArray(const uint32_t count)
	{
		return MakeUniqueArrayWithAlloc<Type>(nullptr, count);
	}
}
