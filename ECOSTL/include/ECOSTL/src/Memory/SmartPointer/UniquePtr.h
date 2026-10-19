#pragma once
#include <cassert>
#include <cstddef>
#include <type_traits>
#include <utility>
#include "../Allocator/MemoryAllocatorAPI.h"

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
			, block_(const_cast<std::remove_cv_t<Type>*>(ptr))
			, delete_(&DeleteAs<std::remove_cv_t<Type>>)
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

		UniquePtr(std::nullptr_t)noexcept
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
			UniquePtr(std::move(src)).Swap(*this);
			return *this;
		}

		/// <summary>
		/// 派生クラスからキャストしてムーブ
		/// </summary>
		/// <typeparam name="SubType"></typeparam>
		/// <param name="src"></param>
		template<typename SubType, std::enable_if_t<std::is_convertible_v<SubType*, Type*>, int> = 0>
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
		template<typename SubType, std::enable_if_t<std::is_convertible_v<SubType*, Type*>, int> = 0>
		UniquePtr& operator=(UniquePtr<SubType>&& src)noexcept
		{
			UniquePtr(std::move(src)).Swap(*this);
			return *this;
		}

		UniquePtr& operator=(std::nullptr_t)noexcept
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
			std::swap(alloc_, other.alloc_);
			std::swap(ptr_, other.ptr_);
			std::swap(block_, other.block_);
			std::swap(delete_, other.delete_);
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
		Type* ins = allocHolder.New<Type>(std::forward<ArgTypes>(args)...);
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
		return MakeUniqueWithAlloc<Type>(nullptr, std::forward<ArgTypes>(args)...);
	}
}
