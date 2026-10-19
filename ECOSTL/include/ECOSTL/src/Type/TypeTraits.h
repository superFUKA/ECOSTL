#pragma once

namespace utl
{
	//----------------------------------------
	// 基本
	//----------------------------------------

	/// <summary>
	/// 型に結びついた定数
	/// </summary>
	template<typename T, T V>
	struct IntegralConstant
	{
		static constexpr T Value = V;
	};

	using TrueType = IntegralConstant<bool, true>;
	using FalseType = IntegralConstant<bool, false>;

	/// <summary>
	/// 型をそのまま返す
	/// </summary>
	template<typename T>
	struct TypeIdentity
	{
		using Type = T;
	};

	template<typename T>
	using TypeIdentityType = typename TypeIdentity<T>::Type;

	//----------------------------------------
	// 判定
	//----------------------------------------

	/// <summary>
	/// 2つの型が同じか
	/// </summary>
	template<typename T, typename U>
	struct IsSame : FalseType {};
	template<typename T>
	struct IsSame<T, T> : TrueType {};

	template<typename T, typename U>
	inline constexpr bool IsSameV = IsSame<T, U>::Value;

	/// <summary>
	/// 左辺値参照か
	/// </summary>
	template<typename T>
	struct IsLvalueReference : FalseType {};
	template<typename T>
	struct IsLvalueReference<T&> : TrueType {};

	template<typename T>
	inline constexpr bool IsLvalueReferenceV = IsLvalueReference<T>::Value;

	//----------------------------------------
	// 変換
	//----------------------------------------

	/// <summary>
	/// 参照を外す
	/// </summary>
	template<typename T>
	struct RemoveReference { using Type = T; };
	template<typename T>
	struct RemoveReference<T&> { using Type = T; };
	template<typename T>
	struct RemoveReference<T&&> { using Type = T; };

	template<typename T>
	using RemoveReferenceType = typename RemoveReference<T>::Type;

	/// <summary>
	/// constを外す
	/// </summary>
	template<typename T>
	struct RemoveConst { using Type = T; };
	template<typename T>
	struct RemoveConst<const T> { using Type = T; };

	template<typename T>
	using RemoveConstType = typename RemoveConst<T>::Type;

	/// <summary>
	/// volatileを外す
	/// </summary>
	template<typename T>
	struct RemoveVolatile { using Type = T; };
	template<typename T>
	struct RemoveVolatile<volatile T> { using Type = T; };

	template<typename T>
	using RemoveVolatileType = typename RemoveVolatile<T>::Type;

	/// <summary>
	/// constとvolatileを外す
	/// </summary>
	template<typename T>
	struct RemoveCV { using Type = RemoveConstType<RemoveVolatileType<T>>; };

	template<typename T>
	using RemoveCVType = typename RemoveCV<T>::Type;

	/// <summary>
	/// 右辺値参照を付ける
	/// </summary>
	namespace detail
	{
		template<typename T>
		auto TryAddRvalueReference(int) -> TypeIdentity<T&&>;
		template<typename T>
		auto TryAddRvalueReference(...) -> TypeIdentity<T>;
	}

	template<typename T>
	struct AddRvalueReference : decltype(detail::TryAddRvalueReference<T>(0)) {};

	template<typename T>
	using AddRvalueReferenceType = typename AddRvalueReference<T>::Type;

	/// <summary>
	/// 型の値を作ったことにする
	/// </summary>
	template<typename T>
	AddRvalueReferenceType<T> DeclVal() noexcept;

	//----------------------------------------
	// 条件
	//----------------------------------------

	/// <summary>
	/// 条件が真のときだけ型を持つ
	/// </summary>
	template<bool Condition, typename T = void>
	struct EnableIf {};
	template<typename T>
	struct EnableIf<true, T> { using Type = T; };

	template<bool Condition, typename T = void>
	using EnableIfType = typename EnableIf<Condition, T>::Type;

	/// <summary>
	/// 条件で型を選ぶ
	/// </summary>
	template<bool Condition, typename TrueT, typename FalseT>
	struct Conditional { using Type = TrueT; };
	template<typename TrueT, typename FalseT>
	struct Conditional<false, TrueT, FalseT> { using Type = FalseT; };

	template<bool Condition, typename TrueT, typename FalseT>
	using ConditionalType = typename Conditional<Condition, TrueT, FalseT>::Type;

	//----------------------------------------
	// 変換できるか
	//----------------------------------------

	namespace detail
	{
		template<typename To>
		void TestImplicitConvert(To) noexcept;

		template<typename From, typename To, typename = void>
		struct IsConvertibleImpl : FalseType {};

		// To型の引数にFrom型の値を暗黙に渡せれば変換できる
		template<typename From, typename To>
		struct IsConvertibleImpl<From, To, decltype(TestImplicitConvert<To>(DeclVal<From>()))> : TrueType {};
	}

	/// <summary>
	/// FromからToへ暗黙に変換できるか
	/// Toがvoidなら、Fromもvoidのときだけ真
	/// </summary>
	template<typename From, typename To>
	struct IsConvertible
		: ConditionalType<IsSameV<RemoveCVType<To>, void>,
			IsSame<RemoveCVType<From>, void>,
			detail::IsConvertibleImpl<From, To>>
	{};

	template<typename From, typename To>
	inline constexpr bool IsConvertibleV = IsConvertible<From, To>::Value;
}
