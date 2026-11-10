#pragma once
// 原子的な操作（std::atomic の代わり）。コンパイラの組み込み関数で書く。
//  - AtomicCounter64 : 64bit の計数。relaxed の加算と読み取りだけを持つ。順序の保証が要る同期には使わないこと。
//  - Atomic<T>       : 整数・ポインタ・列挙の値を、順序の保証（MemoryOrder）を指定して読み書き・交換・比較交換・加減算する。
//
// Atomic<T> の実装
//  - GCC / clang（clang-cl を含む）: __atomic_* の組み込み関数。順序の指定はそのまま渡す。
//  - MSVC の x64 / x86: 読みは普通の読み（揃った 8 バイト以下の読みは原子的で、CPU が順序を保つ）。
//      書きは、SequentiallyConsistent だけ交換命令（xchg。後続の読みとの順序を保つ）、それ以外は普通の書き。
//      交換・比較交換・加減算は _Interlocked* （常に SequentiallyConsistent の強さ。指定した順序より弱くはならない）。
//  - MSVC の ARM64: 読み書きの前後にメモリバリア（__dmb）を置く。この環境ではビルドも実行も確かめていない。
#include <cstdint>
#include <cstring>
#include "ECOSTL/Core/Template/TypeTraits.h"
#if defined(_MSC_VER) && !defined(__clang__)
#include <intrin.h>
#endif

// 汎用ユーティリティ名前空間
namespace utl
{
	/// <summary>
	/// 複数のスレッドから足してよい 64bit の計数。順序の保証はない（relaxed）
	/// </summary>
	class AtomicCounter64
	{
	public:
		constexpr AtomicCounter64() = default;
		AtomicCounter64(const AtomicCounter64&) = delete;
		AtomicCounter64& operator=(const AtomicCounter64&) = delete;

		/// <summary>value を足し、足す前の値を返す</summary>
		inline uint64_t FetchAdd(const uint64_t value)noexcept
		{
#if defined(_MSC_VER) && !defined(__clang__)
			return static_cast<uint64_t>(_InterlockedExchangeAdd64(reinterpret_cast<volatile long long*>(&value_), static_cast<long long>(value)));
#else
			return __atomic_fetch_add(&value_, value, __ATOMIC_RELAXED);
#endif
		}

		/// <summary>今の値</summary>
		inline uint64_t Load()const noexcept
		{
#if defined(_MSC_VER) && !defined(__clang__)
			// 揃った 64bit の読み取りは x64/ARM64 では原子的。volatile で最適化による読み飛ばしを防ぐ
			return *reinterpret_cast<const volatile uint64_t*>(&value_);
#else
			return __atomic_load_n(&value_, __ATOMIC_RELAXED);
#endif
		}

	private:
		alignas(8) uint64_t value_ = 0;
	};

	/// <summary>
	/// 原子的な操作の順序の保証（std::memory_order と同じ意味）
	/// </summary>
	enum class MemoryOrder : uint8_t
	{
		Relaxed,                 // 順序を保証しない（値が壊れないことだけ）
		Acquire,                 // 読み: 対になる Release の書きより前の書き込みが見える
		Release,                 // 書き: これより前の書き込みを、対になる Acquire の読みに見せる
		AcquireRelease,          // 読んで書く操作: Acquire と Release の両方
		SequentiallyConsistent,  // 全スレッドで1つの順序に並ぶ（いちばん強い。既定）
	};

	/// <summary>
	/// 複数のスレッドから同時に読み書きしてよい値
	/// T は整数・ポインタ・列挙のいずれかで、大きさは 1・2・4・8 バイト
	/// </summary>
	template<typename T>
	class Atomic
	{
		static_assert(IsIntegralV<T> || IsPointerV<T> || IsEnumV<T>, "Atomic の型は整数・ポインタ・列挙のいずれか");
		static_assert(sizeof(T) == 1 || sizeof(T) == 2 || sizeof(T) == 4 || sizeof(T) == 8, "Atomic の型の大きさは 1・2・4・8 バイト");

	public:
		constexpr Atomic()noexcept
			:value_()
		{
		}

		constexpr explicit Atomic(const T value)noexcept
			:value_(value)
		{
		}

		Atomic(const Atomic&) = delete;
		Atomic& operator=(const Atomic&) = delete;

		/// <summary>今の値を読む</summary>
		inline T Load(const MemoryOrder order = MemoryOrder::SequentiallyConsistent)const noexcept
		{
#if defined(_MSC_VER) && !defined(__clang__)
			T result;
			if constexpr (sizeof(T) == 1)
			{
				result = FromBits(__iso_volatile_load8(reinterpret_cast<const volatile __int8*>(&value_)));
			}
			else if constexpr (sizeof(T) == 2)
			{
				result = FromBits(__iso_volatile_load16(reinterpret_cast<const volatile __int16*>(&value_)));
			}
			else if constexpr (sizeof(T) == 4)
			{
				result = FromBits(__iso_volatile_load32(reinterpret_cast<const volatile __int32*>(&value_)));
			}
			else
			{
				result = FromBits(__iso_volatile_load64(reinterpret_cast<const volatile __int64*>(&value_)));
			}
			if (order != MemoryOrder::Relaxed)
			{
				BarrierAfterLoad();
			}
			return result;
#else
			T result;
			__atomic_load(const_cast<T*>(&value_), &result, ToBuiltinOrder(order));
			return result;
#endif
		}

		/// <summary>値を書く</summary>
		inline void Store(const T value, const MemoryOrder order = MemoryOrder::SequentiallyConsistent)noexcept
		{
#if defined(_MSC_VER) && !defined(__clang__)
	#if defined(_M_X64) || defined(_M_IX86)
			if (order == MemoryOrder::SequentiallyConsistent)
			{
				// 交換命令で書く（後続の読みが、この書きより前に並ばないようにする）
				(void)Exchange(value, order);
				return;
			}
			StoreRaw(value);
	#else
			if (order != MemoryOrder::Relaxed)
			{
				BarrierFull();
			}
			StoreRaw(value);
			if (order == MemoryOrder::SequentiallyConsistent)
			{
				BarrierFull();
			}
	#endif
#else
			T desired = value;
			__atomic_store(&value_, &desired, ToBuiltinOrder(order));
#endif
		}

		/// <summary>値を書き、書く前の値を返す</summary>
		inline T Exchange(const T value, const MemoryOrder order = MemoryOrder::SequentiallyConsistent)noexcept
		{
#if defined(_MSC_VER) && !defined(__clang__)
			(void)order;
			if constexpr (sizeof(T) == 1)
			{
				return FromBits(_InterlockedExchange8(reinterpret_cast<volatile char*>(&value_), ToBits<char>(value)));
			}
			else if constexpr (sizeof(T) == 2)
			{
				return FromBits(_InterlockedExchange16(reinterpret_cast<volatile short*>(&value_), ToBits<short>(value)));
			}
			else if constexpr (sizeof(T) == 4)
			{
				return FromBits(_InterlockedExchange(reinterpret_cast<volatile long*>(&value_), ToBits<long>(value)));
			}
			else
			{
				return FromBits(_InterlockedExchange64(reinterpret_cast<volatile __int64*>(&value_), ToBits<__int64>(value)));
			}
#else
			T desired = value;
			T result;
			__atomic_exchange(&value_, &desired, &result, ToBuiltinOrder(order));
			return result;
#endif
		}

		/// <summary>
		/// 今の値が expected と同じなら desired を書いて true を返す。違えば、今の値を expected に入れて false を返す
		/// 失敗して読むときの順序は、order から書きの保証を除いたもの
		/// </summary>
		inline bool CompareExchange(T& expected, const T desired, const MemoryOrder order = MemoryOrder::SequentiallyConsistent)noexcept
		{
#if defined(_MSC_VER) && !defined(__clang__)
			(void)order;
			if constexpr (sizeof(T) == 1)
			{
				const char comparand = ToBits<char>(expected);
				const char old = _InterlockedCompareExchange8(reinterpret_cast<volatile char*>(&value_), ToBits<char>(desired), comparand);
				if (old == comparand) { return true; }
				expected = FromBits(old);
				return false;
			}
			else if constexpr (sizeof(T) == 2)
			{
				const short comparand = ToBits<short>(expected);
				const short old = _InterlockedCompareExchange16(reinterpret_cast<volatile short*>(&value_), ToBits<short>(desired), comparand);
				if (old == comparand) { return true; }
				expected = FromBits(old);
				return false;
			}
			else if constexpr (sizeof(T) == 4)
			{
				const long comparand = ToBits<long>(expected);
				const long old = _InterlockedCompareExchange(reinterpret_cast<volatile long*>(&value_), ToBits<long>(desired), comparand);
				if (old == comparand) { return true; }
				expected = FromBits(old);
				return false;
			}
			else
			{
				const __int64 comparand = ToBits<__int64>(expected);
				const __int64 old = _InterlockedCompareExchange64(reinterpret_cast<volatile __int64*>(&value_), ToBits<__int64>(desired), comparand);
				if (old == comparand) { return true; }
				expected = FromBits(old);
				return false;
			}
#else
			T wanted = desired;
			return __atomic_compare_exchange(&value_, &expected, &wanted, false, ToBuiltinOrder(order), ToBuiltinFailureOrder(order));
#endif
		}

		/// <summary>
		/// CompareExchange と同じだが、値が同じでも失敗することがある（その分、環境によっては速い）
		/// 成功するまで繰り返すループの中で使う
		/// </summary>
		inline bool CompareExchangeWeak(T& expected, const T desired, const MemoryOrder order = MemoryOrder::SequentiallyConsistent)noexcept
		{
#if defined(_MSC_VER) && !defined(__clang__)
			return CompareExchange(expected, desired, order);
#else
			T wanted = desired;
			return __atomic_compare_exchange(&value_, &expected, &wanted, true, ToBuiltinOrder(order), ToBuiltinFailureOrder(order));
#endif
		}

		/// <summary>value を足し、足す前の値を返す（整数のみ。あふれたら折り返す）</summary>
		inline T FetchAdd(const T value, const MemoryOrder order = MemoryOrder::SequentiallyConsistent)noexcept
			requires IsIntegralV<T>
		{
#if defined(_MSC_VER) && !defined(__clang__)
			(void)order;
			if constexpr (sizeof(T) == 1)
			{
				return FromBits(_InterlockedExchangeAdd8(reinterpret_cast<volatile char*>(&value_), ToBits<char>(value)));
			}
			else if constexpr (sizeof(T) == 2)
			{
				return FromBits(_InterlockedExchangeAdd16(reinterpret_cast<volatile short*>(&value_), ToBits<short>(value)));
			}
			else if constexpr (sizeof(T) == 4)
			{
				return FromBits(_InterlockedExchangeAdd(reinterpret_cast<volatile long*>(&value_), ToBits<long>(value)));
			}
			else
			{
				return FromBits(_InterlockedExchangeAdd64(reinterpret_cast<volatile __int64*>(&value_), ToBits<__int64>(value)));
			}
#else
			return __atomic_fetch_add(&value_, value, ToBuiltinOrder(order));
#endif
		}

		/// <summary>value を引き、引く前の値を返す（整数のみ。あふれたら折り返す）</summary>
		inline T FetchSub(const T value, const MemoryOrder order = MemoryOrder::SequentiallyConsistent)noexcept
			requires IsIntegralV<T>
		{
#if defined(_MSC_VER) && !defined(__clang__)
			// 符号なしの引き算として折り返した値を足す（符号つきでも、ビットの並びは引き算と同じ結果になる）
			const uint64_t negated = 0ull - static_cast<uint64_t>(value);
			return FetchAdd(static_cast<T>(negated), order);
#else
			return __atomic_fetch_sub(&value_, value, ToBuiltinOrder(order));
#endif
		}

	private:

#if defined(_MSC_VER) && !defined(__clang__)
		// T と、同じ大きさの整数（組み込み関数の引数の型）を、ビットの並びのまま移し替える
		template<typename Bits>
		static inline Bits ToBits(const T value)noexcept
		{
			Bits bits;
			memcpy(&bits, &value, sizeof(T));
			return bits;
		}

		template<typename Bits>
		static inline T FromBits(const Bits bits)noexcept
		{
			T value;
			memcpy(&value, &bits, sizeof(T));
			return value;
		}

		// 順序の保証なしで書く
		inline void StoreRaw(const T value)noexcept
		{
			if constexpr (sizeof(T) == 1)
			{
				__iso_volatile_store8(reinterpret_cast<volatile __int8*>(&value_), ToBits<__int8>(value));
			}
			else if constexpr (sizeof(T) == 2)
			{
				__iso_volatile_store16(reinterpret_cast<volatile __int16*>(&value_), ToBits<__int16>(value));
			}
			else if constexpr (sizeof(T) == 4)
			{
				__iso_volatile_store32(reinterpret_cast<volatile __int32*>(&value_), ToBits<__int32>(value));
			}
			else
			{
				__iso_volatile_store64(reinterpret_cast<volatile __int64*>(&value_), ToBits<__int64>(value));
			}
		}

		// 読みの後に置くバリア（x64 / x86 は CPU が順序を保つので要らない）
		static inline void BarrierAfterLoad()noexcept
		{
	#if defined(_M_ARM64) || defined(_M_ARM)
			__dmb(0x9);  // ISHLD
	#endif
		}

		// 書きの前後に置くバリア（x64 / x86 では呼ばない）
		static inline void BarrierFull()noexcept
		{
	#if defined(_M_ARM64) || defined(_M_ARM)
			__dmb(0xB);  // ISH
	#endif
		}
#else
		static constexpr int ToBuiltinOrder(const MemoryOrder order)noexcept
		{
			switch (order)
			{
			case MemoryOrder::Relaxed: return __ATOMIC_RELAXED;
			case MemoryOrder::Acquire: return __ATOMIC_ACQUIRE;
			case MemoryOrder::Release: return __ATOMIC_RELEASE;
			case MemoryOrder::AcquireRelease: return __ATOMIC_ACQ_REL;
			default: return __ATOMIC_SEQ_CST;
			}
		}

		// 比較交換が失敗したとき（読むだけ）の順序。書きの保証は付けられない
		static constexpr int ToBuiltinFailureOrder(const MemoryOrder order)noexcept
		{
			switch (order)
			{
			case MemoryOrder::Relaxed: return __ATOMIC_RELAXED;
			case MemoryOrder::Acquire: return __ATOMIC_ACQUIRE;
			case MemoryOrder::Release: return __ATOMIC_RELAXED;
			case MemoryOrder::AcquireRelease: return __ATOMIC_ACQUIRE;
			default: return __ATOMIC_SEQ_CST;
			}
		}
#endif

		// 値。大きさと同じ境界に置く（境界をまたぐと原子的に読み書きできない）
		alignas(sizeof(T)) T value_;
	};
}
