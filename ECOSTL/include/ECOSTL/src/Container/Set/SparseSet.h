#pragma once
#include "../Vector/Vector.h"

namespace utl
{
	/// <summary>
	/// まばらなキー
	/// </summary>
	using SparseKey = uint32_t;

	/// <summary>
	/// 密集した添え字
	/// </summary>
	using DenceIndex = uint32_t;

	// NullのDenceKey,DenceIndexを表す定数
	constexpr uint32_t NULL_VALUE = UINT32_MAX;


	class SparseSet
	{
	public:
		// 作成
		//----------------------

		SparseSet(MemoryAllocatorRef allocator)
			: keyToIndex_(allocator)
			, indexToKey_(allocator)
			, size_(0)
		{
		}
		SparseSet(const SparseSet& src)
			:keyToIndex_(src.keyToIndex_)
			, indexToKey_(src.indexToKey_)
			, size_(src.size_)
		{
		}
		SparseSet& operator=(const SparseSet& src)
		{
			Reset();
			indexToKey_ = src.indexToKey_;
			keyToIndex_ = src.keyToIndex_;
			size_ = src.size_;
			return *this;
		}

		SparseSet(SparseSet&& src)noexcept
			:keyToIndex_(utl::Move(src.keyToIndex_))
			, indexToKey_(utl::Move(src.indexToKey_))
			, size_(src.size_)
		{
			src.size_ = 0;
		}
		SparseSet& operator=(SparseSet&& src)noexcept
		{
			indexToKey_ = utl::Move(src.indexToKey_);
			keyToIndex_ = utl::Move(src.keyToIndex_);
			size_ = src.size_;
			src.size_ = 0;
			return *this;
		}

		// 開放
		//----------------------

		~SparseSet()
		{
			Reset();
		}

		void Reset()
		{
			keyToIndex_.Clear();
			indexToKey_.Clear();
			size_ = 0;
		}

		// 追加
		//----------------------

		DenceIndex Insert(const SparseKey key)
		{
			// すでに追加済みのキーならそのまま返す
			const DenceIndex index = TryGetIndex(key);
			if (index != NULL_VALUE)
			{
				return index;
			}
			// 未追加なら新しいキーを割り当てる
			return AppendNew(key);
		}

		/// <summary>
		/// 未追加と分かっているキーを追加する
		/// 追加済みのキーを渡してはいけない
		/// </summary>
		DenceIndex AppendNew(const SparseKey key)
		{
			assert(!CheckActive(key));

			// 添え字を求めた後に要素数を増加
			const DenceIndex index = size_++;

			if (!keyToIndex_.IsValidIndex(key))
			{
				keyToIndex_.Resize(key + 1, NULL_VALUE);
			}
			if (!indexToKey_.IsValidIndex(index))
			{
				indexToKey_.Resize(index + 1, NULL_VALUE);
			}
			keyToIndex_[key] = index;
			indexToKey_[index] = key;

			return index;
		}

		DenceIndex operator[](const SparseKey key)
		{
			return Insert(key);
		}

		// 取得
		//----------------------

		bool CheckActive(const SparseKey key)const
		{
			return keyToIndex_.IsValidIndex(key) && keyToIndex_[key] != NULL_VALUE;
		}

		/// <summary>
		/// 有効なキーの添え字を返す。無効ならNULL_VALUE
		/// </summary>
		DenceIndex TryGetIndex(const SparseKey key)const
		{
			if (!keyToIndex_.IsValidIndex(key))
			{
				return NULL_VALUE;
			}
			return keyToIndex_[key];
		}

		DenceIndex GetIndex(const SparseKey key)const
		{
			if (!keyToIndex_.IsValidIndex(key))
			{
				return NULL_VALUE;
			}
			return keyToIndex_[key];
		}

		DenceIndex GetBackIndex()const
		{
			if (size_ == 0)
			{
				return NULL_VALUE;
			}
			return size_ - 1;
		}

		SparseKey GetKey(const DenceIndex index)const
		{
			if (!indexToKey_.IsValidIndex(index))
			{
				return NULL_VALUE;
			}

			return indexToKey_[index];
		}

		SparseKey GetBackKey()const
		{
			if (size_ == 0)
			{
				return NULL_VALUE;
			}

			return indexToKey_[size_ - 1];
		}


		size_t Size()const
		{
			return size_;
		}

		const Vector<SparseKey>& GetKeys()const
		{
			return indexToKey_;
		}


		// メモリ操作
		//----------------------

		void Fit()
		{
			indexToKey_.Fit();
			keyToIndex_.Fit();
		}

		// 削除
		//----------------------

		/// <summary>
		/// キーを削除する
		/// 帰ってきた添え字が有効なら、
		/// </summary>
		/// <param name="removeKey"></param>
		/// <returns></returns>
		DenceIndex Remove(const SparseKey removeKey)
		{
			if (!CheckActive(removeKey))
			{
				return NULL_VALUE;
			}

			const DenceIndex removeIndex = GetIndex(removeKey);

			const DenceIndex backIndex = GetBackIndex();
			const SparseKey backKey = GetBackKey();

			// 削除する要素の添え字と最後の要素の添え字が異なれば、
			if (removeIndex != backIndex)
			{
				// 削除する要素に最後の要素を張り付ける
				indexToKey_[removeIndex] = backKey;
				keyToIndex_[backKey] = removeIndex;
			}


			// 削除する要素のキーと添え字を無効化
			keyToIndex_[removeKey] = NULL_VALUE;
			indexToKey_[backIndex] = NULL_VALUE;
			--size_;

			// 位置が移動したキーの添え字を返す
			return removeIndex;
		}

		/// <summary>
		/// 登録したキーを全て削除する
		/// </summary>
		void Clear()
		{
			for (uint32_t index = 0; index < size_; ++index)
			{
				keyToIndex_[indexToKey_[index]] = NULL_VALUE;
				indexToKey_[index] = NULL_VALUE;
			}
			size_ = 0;
		}

	private:

		// キーと添え字を紐づけるコンテナ
		Vector<DenceIndex> keyToIndex_;
		// 添え字とキーを紐づけるコンテナ
		Vector<SparseKey> indexToKey_;
		// 登録したキーの数
		uint32_t size_;
	};
}
