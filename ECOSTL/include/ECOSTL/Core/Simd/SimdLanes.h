#pragma once
// 薄い SIMD ラッパー: F4(SSE 4レーン) と F8(AVX2 8レーン)。MSVC / GCC / Clang 対応。x86-64 だけで使える(UTL_SIMD_X86 が 1 のとき)。
//  - F4 は x86-64 なら常に使える(SSE2)。F8 は AVX2+FMA のある CPU だけ(実行時に判定: utl::simd::UseAvx2())。
//  - AVX2 の命令は、プロジェクトに /arch:AVX2 や -mavx2 を付けなくても使えるよう、GCC/Clang は target 属性(pragma)、MSVC は何も付けずに <immintrin.h> を使う。
//    そのため F8 を使うコード(カーネル)は UTL_SIMD_AVX2_BEGIN / UTL_SIMD_AVX2_END の間に書く。F8 の値は、その間の関数どうしでだけ受け渡す
//    (外の関数との受け渡しはポインタで。GCC/Clang では外の関数から F8 の関数を展開できない)。
//    同じカーネルを F4 版と F8 版で2回定義したいときは、「F を定義してから同じ本体を2回 include」する。
//  - 演算は同じ名前(+ - * /, Fma, Sqrt, Min, Max, Select, 比較)でそろえてあり、カーネルはレーンの型 F だけに依存して書ける。
//    Set1・Zero・Load・Load3・LoadRows4 は最初の引数(値は使わない)でレーンの型を選ぶ: Set1(F{}, 1.0f)。
//  - 比較の結果はマスク(全ビット1/0 の F)。Select(mask, a, b) は mask が立っているレーンで a。
//  - 構造体の配列(AoS)の列からの読み書きには Load3(12B 構造体 x,y,z)・LoadRows4/StoreRows4(16B 構造体)を使う。
// testGame の gameLib/Math/SimdLanes.h から移植した。
#include <cstdint>

#if defined(__SSE2__) || defined(_M_X64) || defined(_M_AMD64)
#define UTL_SIMD_X86 1
#include <immintrin.h>
#ifdef _MSC_VER
#include <intrin.h>
#endif
#else
#define UTL_SIMD_X86 0
#endif

#if UTL_SIMD_X86
#if defined(_MSC_VER) && !defined(__clang__)
#define UTL_SIMD_AVX2_BEGIN
#define UTL_SIMD_AVX2_END
#elif defined(__clang__)
#define UTL_SIMD_AVX2_BEGIN _Pragma("clang attribute push(__attribute__((target(\"avx2,fma\"))), apply_to=function)")
#define UTL_SIMD_AVX2_END _Pragma("clang attribute pop")
#else
#define UTL_SIMD_AVX2_BEGIN _Pragma("GCC push_options") _Pragma("GCC target(\"avx2,fma\")")
#define UTL_SIMD_AVX2_END _Pragma("GCC pop_options")
#endif
#endif

// SIMD のレーン抽象(F4=SSE 4レーン / F8=AVX2 8レーン)の名前空間
namespace utl::simd
{
	/// <summary>
	/// この CPU で AVX2+FMA を使えるか(OS が YMM レジスタを保存することも確かめる)。呼ぶたびに CPU を調べる
	/// </summary>
	inline bool CpuHasAvx2()
	{
#if UTL_SIMD_X86
#if defined(_MSC_VER) && !defined(__clang__)
		int info[4];
		__cpuid(info, 0);
		if (info[0] < 7) { return false; }
		__cpuid(info, 1);
		const bool osxsave = (info[2] & (1 << 27)) != 0, avx = (info[2] & (1 << 28)) != 0, fma = (info[2] & (1 << 12)) != 0;
		if (!osxsave || !avx || !fma) { return false; }
		if ((_xgetbv(0) & 6) != 6) { return false; }
		__cpuidex(info, 7, 0);
		return (info[1] & (1 << 5)) != 0;
#else
		__builtin_cpu_init();
		return __builtin_cpu_supports("avx2") && __builtin_cpu_supports("fma");
#endif
#else
		return false;
#endif
	}

	// 内部実装(外から直接使わない)
	namespace detail
	{
		// CPU の判定結果。最初に呼んだときに一度だけ調べる(関数内の static の初期化はスレッドセーフ)
		inline bool CpuHasAvx2Cached()
		{
			static const bool has = CpuHasAvx2();
			return has;
		}
		// SSE 版に固定するか(比較・テスト用。起動時か、カーネルを走らせていないときに切り替える)
		inline bool& ForceSseRef()
		{
			static bool force = false;
			return force;
		}
	}

	/// <summary>
	/// AVX2 版のカーネルを使うか。CPU が対応していて、ForceSse(true) にしていなければ true
	/// </summary>
	inline bool UseAvx2()
	{
		return !detail::ForceSseRef() && detail::CpuHasAvx2Cached();
	}

	/// <summary>
	/// true で SSE 版に固定する(比較・テスト用)。false で CPU の判定に戻す。カーネルを走らせていないときに呼ぶ
	/// </summary>
	inline void ForceSse(const bool force)
	{
		detail::ForceSseRef() = force;
	}

#if UTL_SIMD_X86
	// ---------------- F4: SSE 4レーン ----------------
	struct F4
	{
		// レーン数
		static constexpr uint32_t N = 4;
		// 4レーン分の値(SSE レジスタ)
		__m128 v;
		F4() = default;
		F4(const __m128 x) : v(x) {}
	};
	inline F4 Set1(F4, const float x) { return _mm_set1_ps(x); }
	inline F4 Zero(F4) { return _mm_setzero_ps(); }
	inline F4 Load(F4, const float* p) { return _mm_loadu_ps(p); }
	inline void Store(const F4 a, float* p) { _mm_storeu_ps(p, a.v); }
	inline F4 operator+(F4 a, F4 b) { return _mm_add_ps(a.v, b.v); }
	inline F4 operator-(F4 a, F4 b) { return _mm_sub_ps(a.v, b.v); }
	inline F4 operator*(F4 a, F4 b) { return _mm_mul_ps(a.v, b.v); }
	inline F4 operator/(F4 a, F4 b) { return _mm_div_ps(a.v, b.v); }
	// a*b+c。SSE には FMA が無いので、掛けてから足す(丸めは2回。F8 の Fma と最後のビットが違うことがある)
	inline F4 Fma(F4 a, F4 b, F4 c) { return _mm_add_ps(_mm_mul_ps(a.v, b.v), c.v); }
	inline F4 Sqrt(F4 a) { return _mm_sqrt_ps(a.v); }
	inline F4 Min(F4 a, F4 b) { return _mm_min_ps(a.v, b.v); }
	inline F4 Max(F4 a, F4 b) { return _mm_max_ps(a.v, b.v); }
	inline F4 Abs(F4 a) { return _mm_andnot_ps(_mm_set1_ps(-0.0f), a.v); }
	inline F4 CmpLt(F4 a, F4 b) { return _mm_cmplt_ps(a.v, b.v); }
	inline F4 CmpGt(F4 a, F4 b) { return _mm_cmpgt_ps(a.v, b.v); }
	inline F4 CmpLe(F4 a, F4 b) { return _mm_cmple_ps(a.v, b.v); }
	inline F4 And(F4 a, F4 b) { return _mm_and_ps(a.v, b.v); }
	inline F4 Or(F4 a, F4 b) { return _mm_or_ps(a.v, b.v); }
	// ~a & b
	inline F4 AndNot(F4 a, F4 b) { return _mm_andnot_ps(a.v, b.v); }
	inline F4 Select(F4 mask, F4 a, F4 b) { return _mm_or_ps(_mm_and_ps(mask.v, a.v), _mm_andnot_ps(mask.v, b.v)); }
	// 各レーンの最上位ビットを集めたビット列(レーン k がビット k)
	inline uint32_t MoveMask(F4 a) { return static_cast<uint32_t>(_mm_movemask_ps(a.v)); }
	// 12B 構造体 {x,y,z} の列から N 個読んで x/y/z に分ける(p は先頭の x)
	inline void Load3(F4, const float* p, F4& x, F4& y, F4& z)
	{
		x = _mm_setr_ps(p[0], p[3], p[6], p[9]);
		y = _mm_setr_ps(p[1], p[4], p[7], p[10]);
		z = _mm_setr_ps(p[2], p[5], p[8], p[11]);
	}
	// 16B 構造体 {a,b,c,d} の列(間隔 4 個の float)から N 個読んで a/b/c/d に分ける
	inline void LoadRows4(F4, const float* p, F4& a, F4& b, F4& c, F4& d)
	{
		__m128 r0 = _mm_loadu_ps(p), r1 = _mm_loadu_ps(p + 4), r2 = _mm_loadu_ps(p + 8), r3 = _mm_loadu_ps(p + 12);
		_MM_TRANSPOSE4_PS(r0, r1, r2, r3);
		a = r0; b = r1; c = r2; d = r3;
	}
	// LoadRows4 の逆。各レーン k の {a[k],b[k],c[k],d[k]} を dst + k*strideFloats に書く
	inline void StoreRows4(float* dst, const uint32_t strideFloats, F4 a, F4 b, F4 c, F4 d)
	{
		__m128 r0 = a.v, r1 = b.v, r2 = c.v, r3 = d.v;
		_MM_TRANSPOSE4_PS(r0, r1, r2, r3);
		_mm_storeu_ps(dst, r0); _mm_storeu_ps(dst + strideFloats, r1);
		_mm_storeu_ps(dst + 2 * strideFloats, r2); _mm_storeu_ps(dst + 3 * strideFloats, r3);
	}
	inline void StoreLanes(const F4 a, float* out) { _mm_storeu_ps(out, a.v); }

	// ---------------- F8: AVX2 8レーン ----------------
	UTL_SIMD_AVX2_BEGIN
	struct F8
	{
		// レーン数
		static constexpr uint32_t N = 8;
		// 8レーン分の値(AVX レジスタ)
		__m256 v;
		F8() = default;
		F8(const __m256 x) : v(x) {}
	};
	inline F8 Set1(F8, const float x) { return _mm256_set1_ps(x); }
	inline F8 Zero(F8) { return _mm256_setzero_ps(); }
	inline F8 Load(F8, const float* p) { return _mm256_loadu_ps(p); }
	inline void Store(const F8 a, float* p) { _mm256_storeu_ps(p, a.v); }
	inline F8 operator+(F8 a, F8 b) { return _mm256_add_ps(a.v, b.v); }
	inline F8 operator-(F8 a, F8 b) { return _mm256_sub_ps(a.v, b.v); }
	inline F8 operator*(F8 a, F8 b) { return _mm256_mul_ps(a.v, b.v); }
	inline F8 operator/(F8 a, F8 b) { return _mm256_div_ps(a.v, b.v); }
	// a*b+c(FMA。丸めは1回)
	inline F8 Fma(F8 a, F8 b, F8 c) { return _mm256_fmadd_ps(a.v, b.v, c.v); }
	inline F8 Sqrt(F8 a) { return _mm256_sqrt_ps(a.v); }
	inline F8 Min(F8 a, F8 b) { return _mm256_min_ps(a.v, b.v); }
	inline F8 Max(F8 a, F8 b) { return _mm256_max_ps(a.v, b.v); }
	inline F8 Abs(F8 a) { return _mm256_andnot_ps(_mm256_set1_ps(-0.0f), a.v); }
	inline F8 CmpLt(F8 a, F8 b) { return _mm256_cmp_ps(a.v, b.v, _CMP_LT_OQ); }
	inline F8 CmpGt(F8 a, F8 b) { return _mm256_cmp_ps(a.v, b.v, _CMP_GT_OQ); }
	inline F8 CmpLe(F8 a, F8 b) { return _mm256_cmp_ps(a.v, b.v, _CMP_LE_OQ); }
	inline F8 And(F8 a, F8 b) { return _mm256_and_ps(a.v, b.v); }
	inline F8 Or(F8 a, F8 b) { return _mm256_or_ps(a.v, b.v); }
	// ~a & b
	inline F8 AndNot(F8 a, F8 b) { return _mm256_andnot_ps(a.v, b.v); }
	inline F8 Select(F8 mask, F8 a, F8 b) { return _mm256_blendv_ps(b.v, a.v, mask.v); }
	// 各レーンの最上位ビットを集めたビット列(レーン k がビット k)
	inline uint32_t MoveMask(F8 a) { return static_cast<uint32_t>(_mm256_movemask_ps(a.v)); }
	// 12B 構造体 {x,y,z} の列から、x/y/z を別々のレジスタへ gather で読む(8 個分。p は先頭の x)
	inline void Load3(F8, const float* p, F8& x, F8& y, F8& z)
	{
		const __m256i idx = _mm256_setr_epi32(0, 3, 6, 9, 12, 15, 18, 21);
		x = _mm256_i32gather_ps(p, idx, 4);
		y = _mm256_i32gather_ps(p + 1, idx, 4);
		z = _mm256_i32gather_ps(p + 2, idx, 4);
	}
	// 内部実装(外から直接使わない)
	namespace detail
	{
		// 各 128bit レーン内で 4x4 を転置する(AVX2 の shuffle は 128bit 内に閉じるため)
		inline void Transpose4InLane(__m256& r0, __m256& r1, __m256& r2, __m256& r3)
		{
			const __m256 t0 = _mm256_unpacklo_ps(r0, r1), t1 = _mm256_unpacklo_ps(r2, r3);
			const __m256 t2 = _mm256_unpackhi_ps(r0, r1), t3 = _mm256_unpackhi_ps(r2, r3);
			r0 = _mm256_shuffle_ps(t0, t1, 0x44); r1 = _mm256_shuffle_ps(t0, t1, 0xEE);
			r2 = _mm256_shuffle_ps(t2, t3, 0x44); r3 = _mm256_shuffle_ps(t2, t3, 0xEE);
		}
	}
	// 16B 構造体 {a,b,c,d} の列(間隔 4 個の float)から 8 個読み、レーン k が k 個目の要素になるよう転置して a..d に入れる
	inline void LoadRows4(F8, const float* p, F8& a, F8& b, F8& c, F8& d)
	{
		// r_i = [要素 i | 要素 i+4] にして 4x4 転置すると、レーン k が要素 k になる
		__m256 r0 = _mm256_insertf128_ps(_mm256_castps128_ps256(_mm_loadu_ps(p)), _mm_loadu_ps(p + 16), 1);
		__m256 r1 = _mm256_insertf128_ps(_mm256_castps128_ps256(_mm_loadu_ps(p + 4)), _mm_loadu_ps(p + 20), 1);
		__m256 r2 = _mm256_insertf128_ps(_mm256_castps128_ps256(_mm_loadu_ps(p + 8)), _mm_loadu_ps(p + 24), 1);
		__m256 r3 = _mm256_insertf128_ps(_mm256_castps128_ps256(_mm_loadu_ps(p + 12)), _mm_loadu_ps(p + 28), 1);
		detail::Transpose4InLane(r0, r1, r2, r3);
		a = r0; b = r1; c = r2; d = r3;
	}
	// LoadRows4 の逆。a..d を 8 個分の4要素に戻して dst へ書く(要素と要素の間隔は s 個の float)
	inline void StoreRows4(float* dst, const uint32_t s, F8 a, F8 b, F8 c, F8 d)
	{
		__m256 r0 = a.v, r1 = b.v, r2 = c.v, r3 = d.v;
		detail::Transpose4InLane(r0, r1, r2, r3);
		_mm_storeu_ps(dst, _mm256_castps256_ps128(r0)); _mm_storeu_ps(dst + 4 * s, _mm256_extractf128_ps(r0, 1));
		_mm_storeu_ps(dst + s, _mm256_castps256_ps128(r1)); _mm_storeu_ps(dst + 5 * s, _mm256_extractf128_ps(r1, 1));
		_mm_storeu_ps(dst + 2 * s, _mm256_castps256_ps128(r2)); _mm_storeu_ps(dst + 6 * s, _mm256_extractf128_ps(r2, 1));
		_mm_storeu_ps(dst + 3 * s, _mm256_castps256_ps128(r3)); _mm_storeu_ps(dst + 7 * s, _mm256_extractf128_ps(r3, 1));
	}
	inline void StoreLanes(const F8 a, float* out) { _mm256_storeu_ps(out, a.v); }
	UTL_SIMD_AVX2_END
#endif
}
