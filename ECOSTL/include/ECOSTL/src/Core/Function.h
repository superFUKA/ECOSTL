#pragma once
#include "Utility.h"

namespace utl
{
	template<typename>
	class Function; // プライマリは未定義

	/// <summary>
	/// 関数ポインタ相当の軽量な呼び出し可能オブジェクト
	/// 動的なメモリ確保はしない。メンバ関数を束ねた場合、対象のインスタンスは所有しない
	/// </summary>
	/// <typeparam name="R">戻り値の型</typeparam>
	/// <typeparam name="Args">引数の型</typeparam>
	template<typename R, typename... Args>
	class Function<R(Args...)>
	{
	public:
		// 作成
		//----------------------

		/// <summary>
		/// 空の状態で作成する。呼び出してはならない
		/// </summary>
		Function() noexcept = default;

		Function(decltype(nullptr)) noexcept
		{
		}

		/// <summary>
		/// 関数ポインタに暗黙変換できるもの（関数、静的メンバ関数、キャプチャなしのラムダ）から作成する
		/// </summary>
		/// <param name="f">呼び出す関数</param>
		template<typename F, EnableIfType<IsConvertibleV<F, R(*)(Args...)>, int> = 0>
		Function(F f) noexcept
			: func_(f)
		{
		}

		Function(const Function&) noexcept = default;
		Function& operator=(const Function&) noexcept = default;

		/// <summary>
		/// メンバ関数とインスタンスを束ねて作成する
		/// </summary>
		/// <typeparam name="C">クラス</typeparam>
		/// <typeparam name="Method">呼び出すメンバ関数</typeparam>
		/// <param name="instance">呼び出す対象。寿命は呼び出し側が管理する</param>
		template<typename C, R(C::* Method)(Args...)>
		static Function Bind(C* instance) noexcept
		{
			return Function(instance, [](void* o, Args... as) -> R
				{
					return (static_cast<C*>(o)->*Method)(Forward<Args>(as)...);
				});
		}

		/// <summary>
		/// constメンバ関数とインスタンスを束ねて作成する
		/// </summary>
		template<typename C, R(C::* Method)(Args...) const>
		static Function Bind(const C* instance) noexcept
		{
			return Function(const_cast<C*>(instance), [](void* o, Args... as) -> R
				{
					return (static_cast<const C*>(o)->*Method)(Forward<Args>(as)...);
				});
		}

		// 呼び出し
		//----------------------

		/// <summary>
		/// 呼び出す。空の状態で呼んではならない
		/// </summary>
		R operator()(Args... args) const
		{
			if (stub_)
			{
				return stub_(obj_, Forward<Args>(args)...);
			}
			return func_(Forward<Args>(args)...);
		}

		/// <summary>
		/// 呼び出せる状態か
		/// </summary>
		explicit operator bool() const noexcept
		{
			return func_ != nullptr || stub_ != nullptr;
		}

	private:
		using Stub = R(*)(void*, Args...);

		Function(void* obj, Stub stub) noexcept
			: obj_(obj)
			, stub_(stub)
		{
		}

		R(*func_)(Args...) = nullptr;
		void* obj_ = nullptr;
		Stub stub_ = nullptr;
	};
}
