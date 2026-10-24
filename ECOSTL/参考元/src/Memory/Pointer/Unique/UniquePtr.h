#pragma once
#include <cassert>
#include <type_traits>
#include "src/Logger/Logger.hpp"
#include "../../Allocator/MemoryAllocator.h"

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
		friend UniquePtr<T> MakeUniqueWithAlloc(MemoryAllocatorAPI* allocator, ArgTypes&&...args);

		//----------------------------------------
		// 作成
		//----------------------------------------

		/// <summary>
		/// 作成したインスタンスとインスタンスを作成したメモリアロケータを指定して作成するコンストラクタ
		/// MakeUniqueからのみ使用することを想定している
		/// </summary>
		/// <param name="ptr"></param>
		/// <param name="allocator"></param>
		UniquePtr(Type* ptr, MemoryAllocatorAPI* allocator)
			: alloc_(allocator)
			, ptr_(ptr)
		{

		}

	public:

		/// <summary>
		/// 空のUniquePtrを作成するコンストラクタ
		/// </summary>
		UniquePtr()
			: alloc_(nullptr)
			, ptr_(nullptr)
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
			:alloc_(src.alloc_), ptr_(src.ptr_)
		{
			src.alloc_.Reset();
			src.ptr_ = nullptr;
		}
		/// <summary>
		/// ムーブを行う
		/// </summary>
		/// <param name="src"></param>
		/// <returns></returns>
		UniquePtr& operator=(UniquePtr&& src)noexcept
		{
			// すでに確保してるリソースがあったら開放
			Reset();

			alloc_ = src.alloc_;
			ptr_ = src.ptr_;

			src.alloc_.Reset();
			src.ptr_ = nullptr;

			return *this;
		}

		/// <summary>
		/// 派生クラスからキャストしてムーブ
		/// </summary>
		/// <typeparam name="BaseType"></typeparam>
		/// <param name=""></param>
		template<typename SubType>
		UniquePtr(UniquePtr<SubType>&& src) noexcept
			: alloc_(src.alloc_.Get())
			, ptr_(src.Get())
		{
			src.MemAllocator().Reset();
			src.ptr_ = nullptr;
		}
		//template<typename BaseType>
		//UniquePtr(UniquePtr<BaseType>&& src) noexcept
		//	:alloc_(src.MemAllocator()), ptr_(src.Get())
		//{
		//	src.MemAllocator().Reset();
		//	src.ptr_ = nullptr;
		//}
		template<typename BaseType>
		UniquePtr& operator=(UniquePtr<BaseType>&& src)noexcept
		{
			// すでに確保してるリソースがあったら開放
			Reset();

			alloc_ = src.alloc_;
			ptr_ = src.ptr_;

			src.alloc_.Reset();
			src.ptr_ = nullptr;

			return *this;
		}

		UniquePtr(nullptr_t)noexcept
			:alloc_(nullptr), ptr_(nullptr)
		{

		}
		UniquePtr& operator=(nullptr_t)noexcept
		{
			Reset();
			return *this;
		}


		//----------------------------------------
		// 開放
		//----------------------------------------

		/// <summary>
		/// インスタンスを開放する
		/// </summary>
		void Reset()noexcept
		{
			if (!(*this))
			{
				return;
			}

			ptr_->~Type();

			void* mem = static_cast<void*>(ptr_);
			alloc_.Free(mem);

			alloc_.Reset();
			ptr_ = nullptr;
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
		Type* operator->()
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

		operator bool()const
		{
			return ptr_ != nullptr;
		}

		MemoryAllocatorAPIHolder& MemAllocator()
		{
			return alloc_;
		}

		//----------------------------------------
		// 比較
		//----------------------------------------

		const bool operator==(const Type* ptr)const
		{
			return ptr_ == ptr;
		}
		const bool operator==(const UniquePtr<Type>& ptr)const
		{
			return ptr_ == ptr.Get();
		}

		const bool operator!=(const Type* ptr)const
		{
			return !((*this) == ptr);
		}
		const bool operator!=(const UniquePtr<Type>& ptr)const
		{
			return !((*this) == ptr);
		}




	private:

		MemoryAllocatorAPIHolder alloc_;

		Type* ptr_;

	};

	/// <summary>
	/// アロケータを指定してUniquePtrを作成する関数
	/// </summary>
	/// <typeparam name="Type"></typeparam>
	/// <typeparam name="...ArgTypes"></typeparam>
	/// <param name="...args"></param>
	/// <param name="allocator"></param>
	/// <returns></returns>
	template<typename Type, typename...ArgTypes>
	inline UniquePtr<Type> MakeUniqueWithAlloc(MemoryAllocatorAPI* allocator, ArgTypes&&...args)
	{
		uint32_t typeSize = sizeof(Type);
		MemoryAllocatorAPIHolder allocHolder(allocator);

		// メモリを確保してインスタンスを配置する
		void* mem = nullptr;
		mem = allocHolder.Allocate(typeSize);
		new (mem) Type(args...);

		// uniquePtrを作成する
		return UniquePtr<Type>(static_cast<Type*>(mem), allocator);
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
		return MakeUniqueWithAlloc<Type>(nullptr, args...);
	}
}
