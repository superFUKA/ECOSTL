#pragma once
#include <cinttypes>
#include <cstdlib>

namespace utl
{
	/// <summary>
	/// メモリアロケータのメンバ関数を呼び出すためのデリゲート
	/// </summary>
	class MemoryAllocatorDelegate
	{
	private:
		// 確保
		//----------

		template<typename AllocType>
		using MemberAllocateFuncPtr = void* (AllocType::*)(uint32_t size, uint32_t align);

		using CallMemberAllocateFuncPtr = void* (*)(void* user, uint32_t size, uint32_t align);

		template<typename AllocType, MemberAllocateFuncPtr<AllocType> Func>
		static void* AllocateThunk(void* user, uint32_t size, uint32_t align)
		{
			return (static_cast<AllocType*>(user)->*Func)(size, align);
		}

		// 開放
		//----------

		template<typename AllocType>
		using MemberFreeFuncPtr = bool (AllocType::*)(void*);

		using CallMemberFreeFuncPtr = bool (*)(void* user, void* memory);

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

		template<typename AllocType, MemberAllocateFuncPtr<AllocType> AllocateFunc, MemberFreeFuncPtr<AllocType> FreeFunc>
		inline void Register(AllocType* alloc)
		{
			ptr_ = static_cast<void*>(alloc);
			callAlloc_ = &AllocateThunk<AllocType, AllocateFunc>;
			callFree_ = &FreeThunk<AllocType, FreeFunc>;
		}


		inline void* Allocate(uint32_t size, uint32_t align)
		{
			if (ptr_ == nullptr || callAlloc_ == nullptr)
			{
				return nullptr;
			}
			return callAlloc_(ptr_, size, align);
		}

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
		MemoryAllocatorRef(MemoryAllocatorDelegate* alloc = nullptr)
			:alloc_(alloc)
		{}

		MemoryAllocatorRef(const MemoryAllocatorRef& src)
			:alloc_(src.alloc_)
		{}

		MemoryAllocatorDelegate* Get()const
		{
			return alloc_;
		}

		[[nodiscard]] void* Allocate(const uint32_t size, const uint32_t align = 0)
		{
			if (alloc_ == nullptr)
			{
				return malloc(size);
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
			void* mem = Allocate(sizeof(Type) * num);
			return static_cast<Type*>(mem);
		}
		void Free(void* allocated)
		{
			if (alloc_ == nullptr)
			{
				free(allocated);
			}
			else
			{
				alloc_->Free(allocated);
			}
		}

		void Reset(MemoryAllocatorDelegate* alloc = nullptr)
		{
			alloc_ = alloc;
		}

	private:

		MemoryAllocatorDelegate* alloc_;

	};
}
