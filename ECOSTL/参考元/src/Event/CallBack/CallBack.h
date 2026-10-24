#pragma once

namespace utl
{
	template<typename... FuncArgTypes>
	class ChainCallBackHolder;

	/// <summary>
	/// コールバックを表すクラス
	/// </summary>
	/// <typeparam name="...FuncArgTypes"></typeparam>
	template<typename...FuncArgTypes>
	class ChainCallBack
	{
		using FuncType = void(*)(void*, const FuncArgTypes&...);
	public:
		ChainCallBack()
			: func_(nullptr)
			, data_(nullptr)
			, front_(nullptr)
		{
		}

		ChainCallBack(void* data, FuncType func)
			: func_(func)
			, data_(data)
			, front_(nullptr)
		{
		}

		/// <summary>
		/// コールバックを作成する
		/// </summary>
		/// <param name="func"></param>
		/// <param name="ptr"></param>
		inline void Create(void* data, FuncType func)
		{
			data_ = data;
			func_ = func;
		}

		/// <summary>
		/// コールバックを呼び出す
		/// 追加された順で呼ばれる
		/// </summary>
		/// <param name="...args"></param>
		inline void Call(const FuncArgTypes&...args)
		{
			if (front_ != nullptr)
			{
				front_->Call(args...);
			}
			if (func_ != nullptr)
			{
				func_(data_, args...);
			}
		}

		/// <summary>
		/// コールバックの連結をクリアする
		/// </summary>
		/// <param name="...args"></param>
		inline void Clear()
		{
			ChainCallBack<FuncArgTypes...>* frontPtr = front_;
			front_ = nullptr;
			if (frontPtr != nullptr)
			{
				frontPtr->Clear();
			}
		}

		/// <summary>
		/// 一つ前のコールバックを取得
		/// </summary>
		/// <returns></returns>
		const ChainCallBack<FuncArgTypes...>* GetFront()const
		{
			return front_;
		}

	private:
		friend class ChainCallBackHolder<FuncArgTypes...>;

		/// <summary>
		/// 一つ前のコールバックを登録
		/// </summary>
		/// <param name="callback"></param>
		inline void SetFront(ChainCallBack<FuncArgTypes...>* callback)
		{
			if (front_ == nullptr)
			{
				front_ = callback;
			}
			else
			{
				front_->SetFront(callback);
			}
		}

	private:

		// コールバックで呼ばれる処理
		FuncType func_;
		// コールバックを呼ぶ際に渡す独自データのアドレス
		void* data_;
		// 一つ前に登録されたコールバックのアドレス
		ChainCallBack<FuncArgTypes...>* front_;

	};

	/// <summary>
	/// コールバックを収集するクラス
	/// </summary>
	/// <typeparam name="...FuncArgTypes"></typeparam>
	template<typename...FuncArgTypes>
	class ChainCallBackHolder
	{
	public:
		ChainCallBackHolder()
			: back_(nullptr)
		{
		}

		/// <summary>
		/// コールバックを登録する
		/// 登録済みのコールバックを渡すと循環するため注意
		/// </summary>
		/// <param name="callback"></param>
		inline void Register(ChainCallBack<FuncArgTypes...>* callback)
		{
			if (callback == nullptr)
			{
				return;
			}
			if (callback->GetFront() != nullptr)
			{
				return;
			}

			if (back_ == nullptr)
			{
				back_ = callback;
			}
			else
			{
				callback->SetFront(back_);
				back_ = callback;
			}
		}

		/// <summary>
		/// コールバックを呼ぶ
		/// </summary>
		/// <param name="...args"></param>
		inline void Call(const FuncArgTypes&...args)
		{
			if (back_ == nullptr)
			{
				return;
			}
			back_->Call(args...);
		}

		inline void Clear()
		{
			ChainCallBack<FuncArgTypes...>* current = back_;
			while (current != nullptr)
			{
				ChainCallBack<FuncArgTypes...>* front = current->front_;
				current->front_ = nullptr;
				current = front;
			}
			back_ = nullptr;
		}

	private:

		// 一番最後に登録されたコールバック
		ChainCallBack<FuncArgTypes...>* back_;
	};
}
