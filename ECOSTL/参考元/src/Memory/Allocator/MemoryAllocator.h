#pragma once
#include <cinttypes>

namespace utl
{
	class MemoryAllocatorAPI
	{
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
		MemoryAllocatorAPI()
			: ptr_(nullptr)
			, callAlloc_(nullptr)
			, callFree_(nullptr)
		{
		}

		template<typename AllocType, MemberAllocateFuncPtr<AllocType> AllocateFunc, MemberFreeFuncPtr<AllocType> FreeFunc>
		void Register(AllocType* alloc)
		{
			ptr_ = static_cast<void*>(alloc);
			callAlloc_ = &AllocateThunk<AllocType, AllocateFunc>;
			callFree_ = &FreeThunk<AllocType, FreeFunc>;
		}


		void* Allocate(uint32_t size, uint32_t align)
		{
			if (ptr_ == nullptr || callAlloc_ == nullptr)
			{
				return nullptr;
			}
			return callAlloc_(ptr_, size, align);
		}

		bool Free(void* memory)
		{
			if (ptr_ == nullptr || callFree_ == nullptr)
			{
				return false;
			}
			return callFree_(ptr_, memory);
		}

	private:

		void* ptr_;

		CallMemberAllocateFuncPtr callAlloc_;

		CallMemberFreeFuncPtr callFree_;
	};

	/// <summary>
	/// メモリアロケータの参照を受け取るときに便利なクラス
	/// メモリアロケータが指定されてればそれを使い、
	/// 指定されてなければC++標準の手段を使う
	/// </summary>
	class MemoryAllocatorAPIHolder
	{
	public:
		MemoryAllocatorAPIHolder(MemoryAllocatorAPI* alloc = nullptr)
			:alloc_(alloc)
		{
		}

		MemoryAllocatorAPIHolder(const MemoryAllocatorAPIHolder& src)
			:alloc_(src.alloc_)
		{
		}

		MemoryAllocatorAPI* Get()const
		{
			return alloc_;
		}

		[[nodiscard]] void* Allocate(const uint32_t size, const uint32_t align = 0);

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
		void Free(void* allocated);

		void Reset(MemoryAllocatorAPI* alloc = nullptr)
		{
			alloc_ = alloc;
		}

	private:

		MemoryAllocatorAPI* alloc_;

	};
}
