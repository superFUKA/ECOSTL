#pragma once
#include <cstdint>
#include <new>
#include "MemoryAllocate.h"
#include "../../Core/Utility.h"

namespace utl
{
	/// <summary>
	/// メモリアロケータのメンバ関数を呼び出すためのデリゲート
	/// NOTE:登録したアロケータのインスタンスは、このデリゲートで確保したメモリがすべて開放されるまで同じアドレスで生存している必要がある
	/// </summary>
	class MemoryAllocatorDelegate
	{
	private:
		// 確保
		//----------

		// メモリを確保するメンバ関数のポインタ
		template<typename AllocType>
		using MemberAllocateFuncPtr = void* (AllocType::*)(uint32_t size, uint32_t align);
		// アロケータのメモリを確保するメンバ関数を呼び出す関数ポインタ
		using CallMemberAllocateFuncPtr = void* (*)(void* user, uint32_t size, uint32_t align);
		// アロケータのメモリを確保するメンバ関数を呼び出す
		template<typename AllocType, MemberAllocateFuncPtr<AllocType> Func>
		static void* AllocateThunk(void* user, uint32_t size, uint32_t align)
		{
			return (static_cast<AllocType*>(user)->*Func)(size, align);
		}

		// 開放
		//----------

		// メモリを開放するメンバ関数のポインタ
		template<typename AllocType>
		using MemberFreeFuncPtr = bool (AllocType::*)(void*);
		// アロケータのメモリを開放するメンバ関数を呼び出す関数ポインタ
		using CallMemberFreeFuncPtr = bool (*)(void* user, void* memory);
		// アロケータのメモリを開放するメンバ関数を呼び出す
		template<typename AllocType, MemberFreeFuncPtr<AllocType> Func>
		static bool FreeThunk(void* user, void* memory)
		{
			return (static_cast<AllocType*>(user)->*Func)(memory);
		}


	public:
		MemoryAllocatorDelegate()
			: ptr_(nullptr)
			, callAlloc_(nullptr)
			, callFree_(nullptr)
		{}

		/// <summary>
		/// 使用するメモリアロケータを登録する
		/// </summary>
		/// <typeparam name="AllocType">メモリアロケータの型</typeparam>
		/// <typeparam name="AllocateFunc">メモリを確保するメンバ関数のポインタ</typeparam>
		/// <typeparam name="FreeFunc">メモリを開放するメンバ関数のポインタ</typeparam>
		/// <param name="alloc">使用するアロケータのアドレス</param>
		template<typename AllocType, MemberAllocateFuncPtr<AllocType> AllocateFunc, MemberFreeFuncPtr<AllocType> FreeFunc>
		inline void Register(AllocType* alloc)
		{
			ptr_ = static_cast<void*>(alloc);
			callAlloc_ = &AllocateThunk<AllocType, AllocateFunc>;
			callFree_ = &FreeThunk<AllocType, FreeFunc>;
		}

		/// <summary>
		/// メモリ確保
		/// </summary>
		/// <param name="size"></param>
		/// <param name="align"></param>
		/// <returns></returns>
		inline void* Allocate(uint32_t size, uint32_t align)
		{
			if (ptr_ == nullptr || callAlloc_ == nullptr)
			{
				return nullptr;
			}
			return callAlloc_(ptr_, size, align);
		}

		/// <summary>
		/// メモリ開放
		/// </summary>
		/// <param name="memory"></param>
		/// <returns></returns>
		inline bool Free(void* memory)
		{
			if (ptr_ == nullptr || callFree_ == nullptr)
			{
				return false;
			}
			return callFree_(ptr_, memory);
		}

	private:

		// メモリアロケータのインスタンスへのアドレス
		void* ptr_;
		// メモリを確保する処理の関数ポインタ
		CallMemberAllocateFuncPtr callAlloc_;
		// メモリを開放する処理の関数ポインタ
		CallMemberFreeFuncPtr callFree_;
	};

	/// <summary>
	/// メモリアロケータの参照を持つクラス
	/// 指定されてなければC++標準の手段を使う
	/// </summary>
	class MemoryAllocatorRef
	{
	public:

		explicit MemoryAllocatorRef(MemoryAllocatorDelegate* alloc = nullptr)
			:alloc_(alloc)
		{
		}

		MemoryAllocatorRef(const MemoryAllocatorRef& alloc)noexcept
			:alloc_(alloc.alloc_)
		{}
		MemoryAllocatorRef& operator=(const MemoryAllocatorRef& alloc)noexcept
		{
			alloc_ = alloc.alloc_;
			return *this;
		}

		MemoryAllocatorRef(MemoryAllocatorRef&& alloc)noexcept
			:alloc_(alloc.alloc_)
		{}
		MemoryAllocatorRef& operator=(MemoryAllocatorRef&& alloc)noexcept
		{
			alloc_ = Move(alloc.alloc_);
			return *this;
		}

		MemoryAllocatorDelegate* Get()const
		{
			return alloc_;
		}

		/// <summary>
		/// メモリを確保する
		/// </summary>
		[[nodiscard]] void* Allocate(const uint32_t size, const uint32_t align = 0)
		{
			if (alloc_ == nullptr)
			{
				return AlignedAllocate(size, align);
			}
			else
			{
				return alloc_->Allocate(size, align);
			}
		}


		/// <summary>
		/// 特定の型のインスタンスを任意の個数格納できるサイズの空メモリを確保する
		/// </summary>
		/// <typeparam name="Type"></typeparam>
		/// <param name="num"></param>
		/// <returns></returns>
		template<typename Type>
		[[nodiscard]] Type* Allocate(const uint32_t num)
		{
			if (num > UINT32_MAX / sizeof(Type))
			{
				return nullptr;
			}
			void* mem = Allocate(static_cast<uint32_t>(sizeof(Type) * num), static_cast<uint32_t>(alignof(Type)));
			return static_cast<Type*>(mem);
		}

		/// <summary>
		/// メモリを開放する
		/// </summary>
		bool Free(void* allocated)
		{
			if (allocated == nullptr)
			{
				return true;
			}
			if (alloc_ == nullptr)
			{
				AlignedFree(allocated);
				return true;
			}
			else
			{
				return alloc_->Free(allocated);
			}
		}

		/// <summary>
		/// メモリを確保してインスタンスを作成する
		/// 確保できなければnullptrを返す。コンストラクタが例外を投げたら確保したメモリを開放する
		/// </summary>
		template<typename Type, typename...ArgTypes>
		[[nodiscard]] Type* New(ArgTypes&&...args)
		{
			void* mem = Allocate(static_cast<uint32_t>(sizeof(Type)), static_cast<uint32_t>(alignof(Type)));
			if (mem == nullptr)
			{
				return nullptr;
			}

			// 例外で抜けたときにメモリを開放する
			struct FreeGuard
			{
				~FreeGuard()
				{
					if (mem != nullptr)
					{
						alloc->Free(mem);
					}
				}
				MemoryAllocatorRef* alloc;
				void* mem;
			};
			FreeGuard guard{ this, mem };

			Type* ins = new (mem) Type(Forward<ArgTypes>(args)...);
			guard.mem = nullptr;
			return ins;
		}

		void Reset(MemoryAllocatorDelegate* alloc = nullptr)
		{
			alloc_ = alloc;
		}

	private:

		MemoryAllocatorDelegate* alloc_;

	};

	/// <summary>
	/// インスタンスを破棄してメモリを開放する関数
	/// 作成したときの型で破棄するために、スマートポインタが作成時に保持する
	/// </summary>
	using DeleteFunc = bool (*)(MemoryAllocatorRef& alloc, void* block);

	/// <summary>
	/// blockをType型のインスタンスとして破棄し、メモリを開放する
	/// </summary>
	template<typename Type>
	bool DeleteAs(MemoryAllocatorRef& alloc, void* block)
	{
		static_cast<Type*>(block)->~Type();
		return alloc.Free(block);
	}
}
