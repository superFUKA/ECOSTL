#pragma once

//=========================================================
// コンパイラやCPUの違いを吸収するマクロ
//=========================================================

// UTL_FORCEINLINE : 必ず展開させる
// UTL_NOINLINE    : 展開させない 大きな関数に展開されて、コンパイラの展開の上限に達するのを防ぐ
// UTL_PREFETCH    : アドレスのメモリをキャッシュに先読みする 対応していない環境では何もしない
#if defined(_MSC_VER)
	#include <intrin.h>
	#define UTL_FORCEINLINE __forceinline
	#define UTL_NOINLINE __declspec(noinline)
	#if defined(_M_X64) || defined(_M_IX86)
		#include <xmmintrin.h>
		#define UTL_PREFETCH(address) _mm_prefetch(reinterpret_cast<const char*>(address), _MM_HINT_T0)
	#elif defined(_M_ARM64) || defined(_M_ARM)
		#define UTL_PREFETCH(address) __prefetch(reinterpret_cast<const void*>(address))
	#else
		#define UTL_PREFETCH(address) ((void)(address))
	#endif
#elif defined(__GNUC__) || defined(__clang__)
	#define UTL_FORCEINLINE inline __attribute__((always_inline))
	#define UTL_NOINLINE __attribute__((noinline))
	#define UTL_PREFETCH(address) __builtin_prefetch(reinterpret_cast<const void*>(address), 0, 3)
#else
	#define UTL_FORCEINLINE inline
	#define UTL_NOINLINE
	#define UTL_PREFETCH(address) ((void)(address))
#endif
