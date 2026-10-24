#pragma once
#include "../Set/SparseSet.h"
#include "../Vector/ChunkVector.h"

namespace utl
{
	/// <summary>
	/// メモリの再確保をチャンクごとに行うメモリが密集するマップ
	/// ハッシュ値など、無茶な大きさのキーは使用不可
	/// </summary>
	/// <typeparam name="Type"></typeparam>
	template<typename Type>
	class ChunkDenceMap
	{
	public:

		//------------------------------------------
		// 作成
		//------------------------------------------

		/// <summary>
		/// キー管理用のメモリアロケータと要素を格納するメモリアロケータを分けて作成するコンストラクタ
		/// </summary>
		/// <param name="blockSize">チャンク一つ分の大きさ</param>
		/// <param name="alloc">キー管理用のメモリアロケータ</param>
		/// <param name="valAlloc">要素を格納するメモリを管理するメモリアロケータ</param>
		ChunkDenceMap(const uint32_t blockSize, MemoryAllocatorAPI* alloc, MemoryAllocatorAPI* valAlloc)
			:table_(alloc)
			, values_(blockSize, alloc, valAlloc)
		{

		}
		/// <summary>
		/// メモリアロケータをまとめて指定して作成するコンストラクタ
		/// </summary>
		/// <param name="blockSize">チャンク一つ分の大きさ　指定しなければキャッシュ効率のいい64が指定される</param>
		/// <param name="alloc">キー管理、要素管理に使用するアロケータ</param>
		ChunkDenceMap(const uint32_t blockSize = 64, MemoryAllocatorAPI* alloc = nullptr)
			:table_(alloc)
			, values_(blockSize, alloc, alloc)
		{

		}



		//------------------------------------------
		// 追加
		//------------------------------------------

		/// <summary>
		/// キーと紐づけて参照をもとに要素を追加する
		/// </summary>
		/// <param name="key"></param>
		/// <param name="src"></param>
		/// <returns></returns>
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

		/// <summary>
		/// キーと紐づけてコンストラクタから要素を追加する
		/// </summary>
		/// <typeparam name="...ArgTypes"></typeparam>
		/// <param name="key"></param>
		/// <param name="...args"></param>
		/// <returns></returns>
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

		/// <summary>
		/// キーと紐づく要素を削除する
		/// </summary>
		/// <param name="removeKey"></param>
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
				values_[remIndex] = std::move(values_[backIndex]);
			}

			// 末尾の要素を無効化
			values_.PopBack();
		}

		/// <summary>
		/// 要素を全て開放する
		/// </summary>
		void Clear()
		{
			table_.Reset();
			values_.Clear();
		}

		//------------------------------------------
		// メモリ操作
		//------------------------------------------

		/// <summary>
		/// メモリサイズを要素数に合わせる
		/// </summary>
		void Fit()
		{
			table_.Fit();
			values_.Fit();
		}

		/// <summary>
		/// メモリサイズを要素数に合わせていい感じのサイズに合わせる
		/// </summary>
		void FitSlack()
		{
			table_.FitSlack();
			values_.FitSlack();
		}

		//------------------------------------------
		// 取得
		//------------------------------------------

		inline constexpr Type* BeginFromChunk(const uint32_t blockIndex = 0)const
		{
			uint32_t index = table_.GetIndex(blockIndex);
			if (values_.IsContain(index))
			{
				return nullptr;
			}
			return values_[index];
		}

		/// <summary>
		/// チャンクごとに
		/// </summary>
		/// <param name="blockIndex"></param>
		/// <returns></returns>
		Type* Begin(const uint32_t blockIndex)const
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

		inline constexpr uint32_t GetBlockSize()const
		{
			return values_.GetChunkSize();
		}

		inline constexpr uint32_t GetBlockNum()const
		{
			return values_.GetChunkNum();
		}

		uint32_t Size()const
		{
			return table_.Size();
		}

		Type& operator[](const SparseKey key)
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

	private:

		SparseSet table_;
		ChunkVector<Type> values_;

	};
}