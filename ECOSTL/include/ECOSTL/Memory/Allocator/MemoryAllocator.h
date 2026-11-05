#pragma once
// メモリアロケータを呼ぶ仕組み。ECOSTL dev の実装（MemoryAllocatorDelegate / MemoryAllocatorRef）を元に、testGame 版の良い所を合わせた。
//  - MemoryAllocatorDelegate : 任意のアロケータ型の確保・開放のメンバ関数を、仮想関数なしで呼ぶ型消去の口（所有しない）
//  - MemoryAllocatorRef      : Delegate を借りて持ち、確保・開放する。コンテナなどはこれをメンバーに持つ
//  - DeleteFunc / DeleteAs   : 作ったときの型で破棄して開放する関数（スマートポインタが作成時に保持する）
//
// 確保・開放はすべて MemoryAllocatorRef を通るので、ここでログを出す（型 memory。確保・開放は Trace、失敗は Error）。
//   確保ごとのログは ECO_LOG_COMPILE_LEVEL を 1 以上にすればコンパイルで消える。
// アロケータは、使う側が作って渡す（DI）。グローバルなアロケータは持たない。確保の計数・フックは CountingAllocator.h。
// 既定のアロケータ: Ref に Delegate を渡さなければ、AlignedAllocate/AlignedFree（malloc/free）で確保する。
//   ECOSTL_DISABLE_DEFAULT_ALLOCATOR を定義すると、既定のアロケータを使わない。Delegate を渡さずに確保しようとすると
//   assert で止まり、確保は nullptr・開放は false を返す（渡し忘れを見つけるため）。
#include <cassert>
#include <cstdint>
#include <new>
#include "ECOSTL/Core/Template/Template.h"
#include "ECOSTL/Memory/Allocator/MemoryAllocate.h"
#include "ECOSTL/Logger/Log.h"

// 汎用ユーティリティ名前空間
namespace utl
{
	/// <summary>
	/// メモリアロケータのメンバ関数を呼び出すためのデリゲート
	/// NOTE:登録したアロケータのインスタンスは、このデリゲートで確保したメモリがすべて開放されるまで同じアドレスで生存している必要がある
	/// </summary>
	class MemoryAllocatorDelegate
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
		MemoryAllocatorDelegate()
			: ptr_(nullptr)
			, callAlloc_(nullptr)
			, callFree_(nullptr)
		{
		}

		/// <summary>
		/// 使用するメモリアロケータを登録する（確保なし）
		/// </summary>
		/// <typeparam name="AllocType">メモリアロケータの型</typeparam>
		/// <typeparam name="AllocateFunc">メモリを確保するメンバ関数のポインタ</typeparam>
		/// <typeparam name="FreeFunc">メモリを開放するメンバ関数のポインタ</typeparam>
		/// <param name="alloc">使用するアロケータのアドレス（所有しない）</param>
		template<typename AllocType, MemberAllocateFuncPtr<AllocType> AllocateFunc, MemberFreeFuncPtr<AllocType> FreeFunc>
		inline void Register(AllocType* alloc)
		{
			ptr_ = static_cast<void*>(alloc);
			callAlloc_ = &AllocateThunk<AllocType, AllocateFunc>;
			callFree_ = &FreeThunk<AllocType, FreeFunc>;
		}

		/// <summary>
		/// 登録先で確保する。未登録なら nullptr
		/// </summary>
		inline void* Allocate(uint32_t size, uint32_t align)
		{
			if (ptr_ == nullptr || callAlloc_ == nullptr)
			{
				return nullptr;
			}
			return callAlloc_(ptr_, size, align);
		}

		/// <summary>
		/// 登録先で開放する。未登録なら false
		/// </summary>
		inline bool Free(void* memory)
		{
			if (ptr_ == nullptr || callFree_ == nullptr)
			{
				return false;
			}
			return callFree_(ptr_, memory);
		}

	private:
		// メモリアロケータのインスタンスへのアドレス（所有しない）
		void* ptr_;
		// メモリを確保する処理の関数ポインタ
		CallMemberAllocateFuncPtr callAlloc_;
		// メモリを開放する処理の関数ポインタ
		CallMemberFreeFuncPtr callFree_;
	};

	/// <summary>
	/// メモリアロケータの参照を持つクラス（Delegate を所有しない）
	/// Delegate が無ければ既定のアロケータ（AlignedAllocate/AlignedFree）を使う。ECOSTL_DISABLE_DEFAULT_ALLOCATOR で無効にできる
	/// 確保は常に境界を揃えて行うので、確保と開放の組は Allocate と Free の1通りだけ
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
		{
		}
		MemoryAllocatorRef& operator=(const MemoryAllocatorRef& alloc)noexcept
		{
			alloc_ = alloc.alloc_;
			return *this;
		}

		MemoryAllocatorDelegate* Get()const
		{
			return alloc_;
		}

		/// <summary>
		/// メモリを確保する。align は2の累乗（0 なら既定の境界）。確保できなければ nullptr
		/// </summary>
		[[nodiscard]] void* Allocate(const uint32_t size, const uint32_t align = 0)
		{
			void* memory = nullptr;
			if (alloc_ != nullptr)
			{
				memory = alloc_->Allocate(size, align);
			}
			else
			{
#if defined(ECOSTL_DISABLE_DEFAULT_ALLOCATOR)
				UTL_LOG(Error | ::utl::logtype::Memory, "allocate without allocator size={size} align={align}", size, align);
				assert(false && "アロケータが渡されていません（ECOSTL_DISABLE_DEFAULT_ALLOCATOR が有効）");
				return nullptr;
#else
				UTL_LOG_ONCE(Info | ::utl::logtype::Memory, "default allocator in use (no allocator was passed) size={size}", size);
				memory = AlignedAllocate(size, align);
#endif
			}
			if (memory == nullptr)
			{
				UTL_LOG(Error | ::utl::logtype::Memory, "allocate failed size={size} align={align} allocator={allocator}", size, align, static_cast<const void*>(alloc_));
				return nullptr;
			}
			UTL_LOG(Trace | ::utl::logtype::Memory, "allocate {ptr} size={size} align={align} allocator={allocator}", memory, size, align, static_cast<const void*>(alloc_));
			return memory;
		}

		/// <summary>
		/// 特定の型のインスタンスを任意の個数格納できるサイズの空メモリを確保する。サイズがあふれるなら nullptr
		/// </summary>
		template<typename Type>
		[[nodiscard]] Type* Allocate(const uint32_t num)
		{
			if (num > UINT32_MAX / sizeof(Type))
			{
				UTL_LOG(Error | ::utl::logtype::Memory, "allocate size overflow num={num} elementSize={elementSize}", num, static_cast<uint32_t>(sizeof(Type)));
				return nullptr;
			}
			void* mem = Allocate(static_cast<uint32_t>(sizeof(Type) * num), static_cast<uint32_t>(alignof(Type)));
			return static_cast<Type*>(mem);
		}

		/// <summary>
		/// メモリを開放する。nullptr なら何もしない
		/// </summary>
		bool Free(void* allocated)
		{
			if (allocated == nullptr)
			{
				return true;
			}
			UTL_LOG(Trace | ::utl::logtype::Memory, "free {ptr} allocator={allocator}", allocated, static_cast<const void*>(alloc_));
			if (alloc_ != nullptr)
			{
				const bool freed = alloc_->Free(allocated);
				if (!freed)
				{
					UTL_LOG(Error | ::utl::logtype::Memory, "free failed {ptr} allocator={allocator}", allocated, static_cast<const void*>(alloc_));
				}
				return freed;
			}
#if defined(ECOSTL_DISABLE_DEFAULT_ALLOCATOR)
			UTL_LOG(Error | ::utl::logtype::Memory, "free without allocator {ptr}", allocated);
			assert(false && "アロケータが渡されていません（ECOSTL_DISABLE_DEFAULT_ALLOCATOR が有効）");
			return false;
#else
			AlignedFree(allocated);
			return true;
#endif
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

		MemoryAllocatorDelegate* alloc_;  // 借りている Delegate（所有しない。nullptr なら既定のアロケータ）
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
