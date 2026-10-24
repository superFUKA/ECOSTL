#pragma once
#include "../Set/SparseSet.h"

namespace utl
{
	/// <summary>
	/// メモリが密集するマップ
	/// ハッシュ値など、無茶な大きさのキーは使用不可
	/// </summary>
	/// <typeparam name="Type"></typeparam>
	template<typename Type>
	class DenceMap
	{
	public:
		//------------------------------------------
		// 作成
		//------------------------------------------

		/// <summary>
		/// 
		/// </summary>
		/// <param name="alloc"></param>
		DenceMap(MemoryAllocatorAPI* alloc = nullptr)
			:table_(alloc)
			, values_(alloc)
		{

		}
		/// <summary>
		/// キャパシティとメモリアロケータを指定して作成するコンストラクタ
		/// </summary>
		/// <param name="capacity">キャパシティ</param>
		/// <param name="alloc">全ての動的に確保するメモリを提供するアロケータ</param>
		DenceMap(const uint32_t capacity, MemoryAllocatorAPI* alloc = nullptr)
			:table_(alloc)
			, values_(capacity, alloc)
		{

		}
		/// <summary>
		/// キャパシティと雑多な用途で使用するメモリアロケータと要素を格納するのに使うメモリアロケータを指定して作成するコンストラクタ
		/// </summary>
		/// <param name="capacity">キャパシティ</param>
		/// <param name="utlAlloc">キーと添え字の紐づけなど、雑多な用途で使用するメモリを提供するアロケータ</param>
		/// <param name="valueAlloc">要素のインスタンスを格納するメモリを提供するアロケータ</param>
		DenceMap(const uint32_t capacity, MemoryAllocatorAPI* utlAlloc, MemoryAllocatorAPI* valueAlloc)
			:table_(utlAlloc)
			, values_(capacity, valueAlloc)
		{

		}

		//------------------------------------------
		// 追加
		//------------------------------------------


		Type* Insert(const SparseKey key, const Type& src)
		{
			DenceIndex index = NULL_VALUE;

			// すでにキーが追加されていたら上書きする
			if (table_.CheckActive(key))
			{
				index = table_[key];
				values_[index] = src;
				return &values_[index];
			}

			index = table_.Insert(key);
			// 要素をコンテナに追加
			// 有効な要素は前詰めで配置されるため、末尾に追加すればいい
			return values_.EmplaceBack(src);
		}


		template<typename...ArgTypes>
		Type* Insert(const SparseKey key, ArgTypes&&...args)
		{
			DenceIndex index = NULL_VALUE;

			// すでにキーが追加されていたら上書きする
			if (table_.CheckActive(key))
			{
				index = table_[key];
				values_[index] = Type(args...);
				return &values_[index];
			}

			index = table_.Insert(key);
			// 要素をコンテナに追加
			// 有効な要素は前詰めで配置されるため、末尾に追加すればいい
			return values_.EmplaceBack(args...);
		}

		//------------------------------------------
		// 削除
		//------------------------------------------

		void Remove(const SparseKey removeKey)
		{
			// すでに無効なら何もしない
			if (!table_.CheckActive(removeKey))
			{
				return;
			}

			DenceIndex remIndex = table_.GetIndex(removeKey);

			// テーブルからキーを削除し、場所が入れ替わったキーと添え字を取得
			DenceIndex backIndex = table_.Remove(removeKey);

			// 削除する要素の末尾の要素を上書き
			if (remIndex != backIndex)
			{
				values_[remIndex] = utl::Move(values_[backIndex]);
			}

			// 末尾の要素を無効化
			values_.PopBack();
		}

		void Clear()
		{
			table_.Reset();
			values_.Clear();
		}

		//------------------------------------------
		// メモリ操作
		//------------------------------------------

		void Fit()
		{
			table_.Fit();
			values_.Fit();
		}

		void FitSlack()
		{
			table_.FitSlack();
			values_.FitSlack();
		}

		//------------------------------------------
		// 取得
		//------------------------------------------

		Type* Begin()const
		{
			return values_.Begin();
		}

		Type* Get(const SparseKey key)const
		{
			if (!table_.CheckActive(key))
			{
				return nullptr;
			}
			DenceIndex index = table_.GetIndex(key);
			return &values_[index];
		}

		size_t Size()const
		{
			return table_.Size();
		}

		Type& GetOrAdd(const SparseKey key)
		{
			DenceIndex index = NULL_VALUE;
			if (table_.CheckActive(key))
			{
				index = table_[key];
				return values_[index];
			}

			index = table_.Insert(key);
			// 要素をコンテナに追加
			// 有効な要素は前詰めで配置されるため、末尾に追加すればいい
			return *values_.EmplaceBack();
		}

		Type& operator[](const SparseKey key)
		{
			return GetOrAdd(key);
		}

		DenceMap& operator=(DenceMap&& src)
		{
			table_ = utl::Move(src.table_);
			values_ = utl::Move(src.values_);
			return *this;
		}


		bool CheckActive(const SparseKey key)const
		{
			return table_.CheckActive(key);
		}

		const Vector<SparseKey>& GetKeys()const
		{
			return table_.GetKeys();
		}

		const uint32_t GetKeyNum()const
		{
			return table_.Size();
		}


	private:

		SparseSet table_;

		Vector<Type> values_;

	};
}

