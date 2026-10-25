#pragma once
#include "../../Set/SparseSet.h"

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
		// 作成
		//----------------------

		/// <summary>
		/// 使用するメモリアロケータを指定して作成する
		/// </summary>
		/// <param name="alloc">全ての動的に確保するメモリを提供するアロケータ。指定しなければC++標準の手段で確保する</param>
		explicit DenceMap(const MemoryAllocatorRef& alloc = MemoryAllocatorRef())
			: table_(alloc)
			, values_(alloc)
		{
		}

		/// <summary>
		/// キャパシティとメモリアロケータを指定して作成する
		/// </summary>
		/// <param name="capacity">要素のキャパシティ</param>
		/// <param name="alloc">全ての動的に確保するメモリを提供するアロケータ</param>
		DenceMap(const uint32_t capacity, const MemoryAllocatorRef& alloc = MemoryAllocatorRef())
			: table_(alloc)
			, values_(capacity, alloc)
		{
		}

		/// <summary>
		/// キャパシティと、雑多な用途のアロケータ、要素を格納するアロケータを指定して作成する
		/// </summary>
		/// <param name="capacity">要素のキャパシティ</param>
		/// <param name="utlAlloc">キーと添え字の紐づけなど、雑多な用途で使用するメモリを提供するアロケータ</param>
		/// <param name="valueAlloc">要素のインスタンスを格納するメモリを提供するアロケータ</param>
		DenceMap(const uint32_t capacity, const MemoryAllocatorRef& utlAlloc, const MemoryAllocatorRef& valueAlloc)
			: table_(utlAlloc)
			, values_(capacity, valueAlloc)
		{
		}

		DenceMap(const DenceMap& src) = default;
		DenceMap& operator=(const DenceMap& src) = default;

		DenceMap(DenceMap&& src)noexcept
			: table_(Move(src.table_))
			, values_(Move(src.values_))
		{
		}
		DenceMap& operator=(DenceMap&& src)noexcept
		{
			if (this == &src)
			{
				return *this;
			}
			table_ = Move(src.table_);
			values_ = Move(src.values_);
			return *this;
		}

		// 追加
		//----------------------

		/// <summary>
		/// キーに紐づけて要素を追加する
		/// すでにキーが追加されていたら上書きする
		/// </summary>
		/// <param name="key"></param>
		/// <param name="...args">要素のコンストラクタ引数</param>
		/// <returns>追加した要素。確保できなければnullptr</returns>
		template<typename...ArgTypes>
		Type* Insert(const SparseKey key, ArgTypes&&...args)
		{
			const DenceIndex index = table_.TryGetIndex(key);
			if (index != NULL_VALUE)
			{
				Type& value = values_[index];
				value = Type(Forward<ArgTypes>(args)...);
				return &value;
			}

			// 有効な要素は前詰めで配置されるため、末尾に追加すればいい
			Type* value = values_.EmplaceBack(Forward<ArgTypes>(args)...);
			if (value == nullptr)
			{
				return nullptr;
			}
			table_.AppendNew(key);
			return value;
		}

		// キーの入れ替え
		//----------------------

		/// <summary>
		/// 2つのキーに紐づく要素を入れ替える
		/// 要素の配置は変えず、キーとの紐づけだけを更新するため、要素の大きさによらず低コスト
		/// 片方のキーだけ有効なら、その要素をもう片方のキーへ付け替える
		/// 要素のアドレスは変わらない
		/// </summary>
		/// <returns>入れ替えたらtrue。両方無効ならfalse</returns>
		bool SwapKeys(const SparseKey keyA, const SparseKey keyB)
		{
			return table_.SwapKeys(keyA, keyB);
		}

		// 削除
		//----------------------

		/// <summary>
		/// キーに紐づく要素を削除する
		/// 無効なキーなら何もしない
		/// </summary>
		/// <param name="removeKey"></param>
		void Remove(const SparseKey removeKey)
		{
			if (!table_.CheckActive(removeKey))
			{
				return;
			}

			const DenceIndex removeIndex = table_.GetIndex(removeKey);
			const DenceIndex backIndex = table_.GetBackIndex();

			table_.Remove(removeKey);

			// 削除する要素に末尾の要素を移す
			if (removeIndex != backIndex)
			{
				values_[removeIndex] = Move(values_[backIndex]);
			}
			values_.PopBack();
		}

		/// <summary>
		/// 全ての要素を削除する
		/// メモリは確保したままにする
		/// </summary>
		void Clear()
		{
			table_.Clear();
			values_.Clear();
		}

		// メモリ操作
		//----------------------

		/// <summary>
		/// キャパシティを要素数に合わせる
		/// </summary>
		void Fit()
		{
			table_.Fit();
			values_.Fit();
		}

		// 取得
		//----------------------

		/// <summary>
		/// 要素の先頭
		/// 要素は前詰めで配置される
		/// </summary>
		Type* Begin()
		{
			return values_.Begin();
		}
		const Type* Begin()const
		{
			return values_.Begin();
		}

		/// <summary>
		/// キーに紐づく要素を取得する
		/// </summary>
		/// <returns>無効なキーならnullptr</returns>
		Type* Get(const SparseKey key)
		{
			const DenceIndex index = table_.TryGetIndex(key);
			return index == NULL_VALUE ? nullptr : &values_[index];
		}
		const Type* Get(const SparseKey key)const
		{
			const DenceIndex index = table_.TryGetIndex(key);
			return index == NULL_VALUE ? nullptr : &values_[index];
		}

		/// <summary>
		/// キーに紐づく要素を取得する。なければデフォルトの要素を追加して返す
		/// </summary>
		Type& GetOrAdd(const SparseKey key)
		{
			const DenceIndex index = table_.TryGetIndex(key);
			if (index != NULL_VALUE)
			{
				return values_[index];
			}

			Type* value = values_.EmplaceBack();
			assert(value != nullptr && "メモリを確保できませんでした");
			table_.AppendNew(key);
			return *value;
		}

		Type& operator[](const SparseKey key)
		{
			return GetOrAdd(key);
		}

		bool CheckActive(const SparseKey key)const
		{
			return table_.CheckActive(key);
		}

		/// <summary>
		/// 要素数
		/// </summary>
		uint32_t Size()const
		{
			return static_cast<uint32_t>(table_.Size());
		}

		/// <summary>
		/// 要素の添え字順のキー
		/// 末尾に無効値(NULL_VALUE)を含むことがあるため、有効なのは Size() 個まで
		/// </summary>
		const Vector<SparseKey>& GetKeys()const
		{
			return table_.GetKeys();
		}

	private:

		// キーと要素の添え字を紐づけるコンテナ
		SparseSet table_;
		// 前詰めで配置される要素
		Vector<Type> values_;
	};
}
