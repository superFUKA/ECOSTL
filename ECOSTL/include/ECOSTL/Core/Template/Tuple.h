#pragma once
// 型の並び（TypeList・TypeAt）と、値の組 Tuple（std::tuple の代わり）。
// 継承は使わない（継承ゼロの方針）。Tuple<H, R...> は「先頭の値 head」と「残りの Tuple tail」をメンバーとして持つ。
// Get<I> は再帰をたどるだけなので、インライン展開されれば std::tuple と同じく余計な処理は残らない。
#include "ECOSTL/Core/Template/Template.h"

// 汎用ユーティリティ名前空間
namespace utl
{
	//----------------------------
	// 型の並び
	//----------------------------

	// 型の並び（値は持たない。関数の引数の型の一覧などに使う）
	template<typename...Types>
	struct TypeList
	{
		static constexpr decltype(sizeof(0)) SIZE = sizeof...(Types);
	};

	namespace detail
	{
		template<decltype(sizeof(0)) Index, typename...Types>
		struct TypeAtImpl;
		template<typename Head, typename...Rest>
		struct TypeAtImpl<0, Head, Rest...>
		{
			using Type = Head;
		};
		template<decltype(sizeof(0)) Index, typename Head, typename...Rest>
		struct TypeAtImpl<Index, Head, Rest...>
		{
			using Type = typename TypeAtImpl<Index - 1, Rest...>::Type;
		};

		// Types の中で T が最初に現れる位置（無ければ sizeof...(Types)）
		template<typename T, typename...Types>
		inline constexpr decltype(sizeof(0)) IndexOfImpl = 0;
		template<typename T, typename Head, typename...Rest>
		inline constexpr decltype(sizeof(0)) IndexOfImpl<T, Head, Rest...> = IsSameV<T, Head> ? 0 : 1 + IndexOfImpl<T, Rest...>;
	}

	// 型の並びの Index 番目の型
	template<decltype(sizeof(0)) Index, typename...Types>
	using TypeAt = typename detail::TypeAtImpl<Index, Types...>::Type;

	// TypeList・Tuple の Index 番目の型（std::tuple_element_t の代わり）
	template<decltype(sizeof(0)) Index, typename List>
	struct ElementOf;
	template<decltype(sizeof(0)) Index, template<typename...> class List, typename...Types>
	struct ElementOf<Index, List<Types...>>
	{
		using Type = TypeAt<Index, Types...>;
	};
	template<decltype(sizeof(0)) Index, typename List>
	using ElementOfType = typename ElementOf<Index, List>::Type;

	//----------------------------
	// 値の組
	//----------------------------

	template<typename...Types>
	struct Tuple;

	template<>
	struct Tuple<>
	{
	};

	template<typename Head, typename...Rest>
	struct Tuple<Head, Rest...>
	{
		Head head{};  // 先頭の値（値初期化される）
		Tuple<Rest...> tail{};  // 残りの値

		constexpr Tuple() = default;

		// 全ての値を渡して作る（数が合うときだけ。コピー・ムーブのコンストラクタと取り違えないため）
		template<typename HeadArg, typename...RestArgs, typename = EnableIfType<sizeof...(RestArgs) == sizeof...(Rest) && !IsSameV<RemoveCVRefType<HeadArg>, Tuple>>>
		constexpr Tuple(HeadArg&& headArg, RestArgs&&...restArgs)
			: head(Forward<HeadArg>(headArg))
			, tail(Forward<RestArgs>(restArgs)...)
		{
		}
	};

	namespace detail
	{
		template<decltype(sizeof(0)) Index>
		struct TupleGet
		{
			template<typename TupleType>
			static constexpr auto& Get(TupleType& tuple)noexcept
			{
				return TupleGet<Index - 1>::Get(tuple.tail);
			}
		};
		template<>
		struct TupleGet<0>
		{
			template<typename TupleType>
			static constexpr auto& Get(TupleType& tuple)noexcept
			{
				return tuple.head;
			}
		};
	}

	/// <summary>
	/// Index 番目の値（std::get<Index> の代わり）
	/// </summary>
	template<decltype(sizeof(0)) Index, typename...Types>
	constexpr TypeAt<Index, Types...>& Get(Tuple<Types...>& tuple)noexcept
	{
		return detail::TupleGet<Index>::Get(tuple);
	}
	template<decltype(sizeof(0)) Index, typename...Types>
	constexpr const TypeAt<Index, Types...>& Get(const Tuple<Types...>& tuple)noexcept
	{
		return detail::TupleGet<Index>::Get(tuple);
	}

	/// <summary>
	/// 型 T の値（std::get<T> の代わり）。T は組の中に1つだけであること
	/// </summary>
	template<typename T, typename...Types>
	constexpr T& Get(Tuple<Types...>& tuple)noexcept
	{
		constexpr decltype(sizeof(0)) index = detail::IndexOfImpl<T, Types...>;
		static_assert(index < sizeof...(Types), "型が組の中にありません");
		return Get<index>(tuple);
	}
	template<typename T, typename...Types>
	constexpr const T& Get(const Tuple<Types...>& tuple)noexcept
	{
		constexpr decltype(sizeof(0)) index = detail::IndexOfImpl<T, Types...>;
		static_assert(index < sizeof...(Types), "型が組の中にありません");
		return Get<index>(tuple);
	}

	/// <summary>
	/// 値から組を作る（std::make_tuple の代わり。参照は外して値として持つ）
	/// </summary>
	template<typename...Args>
	constexpr Tuple<RemoveCVRefType<Args>...> MakeTuple(Args&&...args)
	{
		return Tuple<RemoveCVRefType<Args>...>(Forward<Args>(args)...);
	}
}
