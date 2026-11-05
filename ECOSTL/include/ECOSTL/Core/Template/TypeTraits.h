#pragma once
// 型特性（<type_traits> の代わり）。std を使わず、テンプレートの特殊化とコンパイラの組み込み関数で書く。
// 継承は使わない（継承ゼロの方針）。判定は「名前V」の constexpr bool 変数テンプレート、変換は「名前Type」の別名で使う。
// ECOSTL dev の TypeTraits.h（IntegralConstant を継承する形）を元に、継承をやめた。

// 汎用ユーティリティ名前空間
namespace utl
{
	//----------------------------------------
	// 基本
	//----------------------------------------

	// 型をそのまま Type に持つ（推論を止めるときなどに使う）
	template<typename T>
	struct TypeIdentity
	{
		using Type = T;
	};
	template<typename T>
	using TypeIdentityType = typename TypeIdentity<T>::Type;

	// テンプレートに依存する false（static_assert を、実体化されたときだけ失敗させる）
	template<typename...>
	inline constexpr bool DependentFalseV = false;

	// nullptr の型（std::nullptr_t の代わり）
	using NullptrT = decltype(nullptr);

	//----------------------------------------
	// 判定（特殊化で書けるもの）
	//----------------------------------------

	template<typename T, typename U>
	inline constexpr bool IsSameV = false;
	template<typename T>
	inline constexpr bool IsSameV<T, T> = true;

	template<typename T>
	inline constexpr bool IsConstV = false;
	template<typename T>
	inline constexpr bool IsConstV<const T> = true;

	template<typename T>
	inline constexpr bool IsLvalueReferenceV = false;
	template<typename T>
	inline constexpr bool IsLvalueReferenceV<T&> = true;

	template<typename T>
	inline constexpr bool IsPointerV = false;
	template<typename T>
	inline constexpr bool IsPointerV<T*> = true;
	template<typename T>
	inline constexpr bool IsPointerV<T* const> = true;
	template<typename T>
	inline constexpr bool IsPointerV<T* volatile> = true;
	template<typename T>
	inline constexpr bool IsPointerV<T* const volatile> = true;

	//----------------------------------------
	// 変換
	//----------------------------------------

	template<typename T> struct RemoveReference { using Type = T; };
	template<typename T> struct RemoveReference<T&> { using Type = T; };
	template<typename T> struct RemoveReference<T&&> { using Type = T; };
	template<typename T>
	using RemoveReferenceType = typename RemoveReference<T>::Type;

	template<typename T> struct AddConst { using Type = const T; };
	template<typename T>
	using AddConstType = typename AddConst<T>::Type;

	template<typename T> struct RemoveConst { using Type = T; };
	template<typename T> struct RemoveConst<const T> { using Type = T; };
	template<typename T>
	using RemoveConstType = typename RemoveConst<T>::Type;

	template<typename T> struct RemoveVolatile { using Type = T; };
	template<typename T> struct RemoveVolatile<volatile T> { using Type = T; };
	template<typename T>
	using RemoveVolatileType = typename RemoveVolatile<T>::Type;

	template<typename T>
	using RemoveCVType = RemoveConstType<RemoveVolatileType<T>>;

	template<typename T> struct RemovePointer { using Type = T; };
	template<typename T> struct RemovePointer<T*> { using Type = T; };
	template<typename T> struct RemovePointer<T* const> { using Type = T; };
	template<typename T> struct RemovePointer<T* volatile> { using Type = T; };
	template<typename T> struct RemovePointer<T* const volatile> { using Type = T; };
	template<typename T>
	using RemovePointerType = typename RemovePointer<T>::Type;

	// 参照と const/volatile を外した型（std::remove_cvref_t の代わり）
	template<typename T>
	using RemoveCVRefType = RemoveCVType<RemoveReferenceType<T>>;

	namespace detail
	{
		template<typename T>
		auto TryAddRvalueReference(int) -> TypeIdentity<T&&>;
		template<typename T>
		auto TryAddRvalueReference(...) -> TypeIdentity<T>;
	}
	// 右辺値参照を付けた型（void などは付けられないのでそのまま）
	template<typename T>
	using AddRvalueReferenceType = typename decltype(detail::TryAddRvalueReference<T>(0))::Type;

	/// <summary>
	/// 型 T の値（評価されない文脈でだけ使う。std::declval の代わり）
	/// </summary>
	template<typename T>
	AddRvalueReferenceType<T> DeclVal() noexcept;

	//----------------------------------------
	// 条件
	//----------------------------------------

	template<bool Condition, typename T = void> struct EnableIf {};
	template<typename T> struct EnableIf<true, T> { using Type = T; };
	template<bool Condition, typename T = void>
	using EnableIfType = typename EnableIf<Condition, T>::Type;

	template<bool Condition, typename TrueT, typename FalseT> struct Conditional { using Type = TrueT; };
	template<typename TrueT, typename FalseT> struct Conditional<false, TrueT, FalseT> { using Type = FalseT; };
	template<bool Condition, typename TrueT, typename FalseT>
	using ConditionalType = typename Conditional<Condition, TrueT, FalseT>::Type;

	//----------------------------------------
	// 型の分類
	//----------------------------------------

	namespace detail
	{
		template<typename T> inline constexpr bool IsIntegralImpl = false;
		template<> inline constexpr bool IsIntegralImpl<bool> = true;
		template<> inline constexpr bool IsIntegralImpl<char> = true;
		template<> inline constexpr bool IsIntegralImpl<signed char> = true;
		template<> inline constexpr bool IsIntegralImpl<unsigned char> = true;
		template<> inline constexpr bool IsIntegralImpl<wchar_t> = true;
		template<> inline constexpr bool IsIntegralImpl<char8_t> = true;
		template<> inline constexpr bool IsIntegralImpl<char16_t> = true;
		template<> inline constexpr bool IsIntegralImpl<char32_t> = true;
		template<> inline constexpr bool IsIntegralImpl<short> = true;
		template<> inline constexpr bool IsIntegralImpl<unsigned short> = true;
		template<> inline constexpr bool IsIntegralImpl<int> = true;
		template<> inline constexpr bool IsIntegralImpl<unsigned int> = true;
		template<> inline constexpr bool IsIntegralImpl<long> = true;
		template<> inline constexpr bool IsIntegralImpl<unsigned long> = true;
		template<> inline constexpr bool IsIntegralImpl<long long> = true;
		template<> inline constexpr bool IsIntegralImpl<unsigned long long> = true;

		template<typename T> inline constexpr bool IsFloatingPointImpl = false;
		template<> inline constexpr bool IsFloatingPointImpl<float> = true;
		template<> inline constexpr bool IsFloatingPointImpl<double> = true;
		template<> inline constexpr bool IsFloatingPointImpl<long double> = true;
	}

	template<typename T>
	inline constexpr bool IsIntegralV = detail::IsIntegralImpl<RemoveCVType<T>>;

	template<typename T>
	inline constexpr bool IsFloatingPointV = detail::IsFloatingPointImpl<RemoveCVType<T>>;

	//----------------------------------------
	// 型の性質（コンパイラの組み込み関数。MSVC・clang・GCC で使える）
	//----------------------------------------

	template<typename T>
	inline constexpr bool IsEnumV = __is_enum(T);

	template<typename T>
	inline constexpr bool IsEmptyV = __is_empty(T);

	template<typename T>
	inline constexpr bool IsTriviallyCopyableV = __is_trivially_copyable(T);

	template<typename T, typename...Args>
	inline constexpr bool IsTriviallyConstructibleV = __is_trivially_constructible(T, Args...);

	template<typename T>
	inline constexpr bool IsTriviallyDestructibleV =
#if defined(_MSC_VER) || defined(__clang__)
		__is_trivially_destructible(T);
#else
		__has_trivial_destructor(T);
#endif

	template<typename T, typename...Args>
	inline constexpr bool IsConstructibleV = __is_constructible(T, Args...);

	template<typename T, typename...Args>
	inline constexpr bool IsNothrowConstructibleV = __is_nothrow_constructible(T, Args...);

	template<typename T>
	inline constexpr bool IsDefaultConstructibleV = IsConstructibleV<T>;

	template<typename T>
	inline constexpr bool IsCopyConstructibleV = IsConstructibleV<T, const T&>;

	template<typename T>
	inline constexpr bool IsMoveConstructibleV = IsConstructibleV<T, T&&>;

	template<typename T>
	inline constexpr bool IsNothrowMoveConstructibleV = IsNothrowConstructibleV<T, T&&>;

	// 値が同じならバイト列も同じになる型か（バイト列をそのままハッシュしてよいか）
	template<typename T>
	inline constexpr bool HasUniqueObjectRepresentationsV = __has_unique_object_representations(T);

	//----------------------------------------
	// 変換できるか
	//----------------------------------------

	namespace detail
	{
		template<typename To>
		void TestImplicitConvert(To) noexcept;

		// To 型の引数に From 型の値を暗黙に渡せれば変換できる
		template<typename From, typename To, typename = void>
		inline constexpr bool IsConvertibleImpl = false;
		template<typename From, typename To>
		inline constexpr bool IsConvertibleImpl<From, To, decltype(TestImplicitConvert<To>(DeclVal<From>()))> = true;
	}

	template<typename From, typename To>
	inline constexpr bool IsConvertibleV = IsSameV<RemoveCVType<To>, void>
		? IsSameV<RemoveCVType<From>, void>
		: detail::IsConvertibleImpl<From, To>;
}
