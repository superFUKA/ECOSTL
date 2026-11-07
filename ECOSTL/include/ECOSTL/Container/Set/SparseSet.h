#pragma once
// まばらなキー(SparseKey)を密な添え字(DenceIndex)へ対応づける SparseSet。
#include "ECOSTL/Core/Template/Template.h"
#include "../Vector/Vector.h"

// ecoSTL のユーティリティ名前空間。
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

	/// <summary>
	/// まばらなキーを密集した添え字に変換するテーブル
	/// 削除するときに最後の要素を削除する要素に上書きする
	/// </summary>
	class SparseSet
	{
	public:

		//-----------------------------------
		// 作成
		//-----------------------------------

		SparseSet(MemoryAllocatorDelegate* alloc = nullptr)
			:indexToKey_(alloc)
			, keyToIndex_(alloc)
			, size_(0)
		{

		}

		SparseSet(const SparseSet& src)
			:indexToKey_(src.indexToKey_)
			, keyToIndex_(src.keyToIndex_)
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

		// ムーブ：中身を引き取り、移し元を空にする（testGame 版はコピーになっていて、移し元に中身が残っていた）
		SparseSet(SparseSet&& src)noexcept
			:indexToKey_(utl::Move(src.indexToKey_))
			, keyToIndex_(utl::Move(src.keyToIndex_))
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

		//-----------------------------------
		// 開放
		//-----------------------------------

		~SparseSet()
		{
			Reset();
		}

		// 全キーを破棄して要素数を 0 にする。Vector の要素は Clear されるので再利用不可（Clear() は NULL 埋めで残す版）。
		void Reset()
		{
			keyToIndex_.Clear();
			indexToKey_.Clear();
			size_ = 0;
		}


		//-----------------------------------
		// 追加
		//-----------------------------------

		// key を登録し、割り当てた添え字を返す。登録済みならその添え字を返す。key+1 までテーブルが伸びる（再確保あり）ので、キーが大きいとメモリを使う。
		// 確保できない、または key が NULL_VALUE なら、何も変えずに NULL_VALUE を返す
		DenceIndex Insert(const SparseKey key)
		{
			// すでに追加済みのキーならそのまま返す
			if (CheckActive(key))
			{
				return keyToIndex_[key];
			}
			if (key == NULL_VALUE)
			{
				return NULL_VALUE;
			}

			// 先に2つの表を広げ、両方広げられたときだけ登録する
			const DenceIndex index = static_cast<DenceIndex>(size_);
			if (!keyToIndex_.IsValidIndex(key) && !keyToIndex_.Resize(key + 1, NULL_VALUE))
			{
				return NULL_VALUE;
			}
			if (!indexToKey_.IsValidIndex(index) && !indexToKey_.Resize(index + 1, NULL_VALUE))
			{
				return NULL_VALUE;
			}
			++size_;
			keyToIndex_[key] = index;
			indexToKey_[index] = key;

			return index;
		}

		DenceIndex operator[](const SparseKey key)
		{
			return Insert(key);
		}

		//-----------------------------------
		// 取得
		//-----------------------------------

		// key が登録済み（有効な添え字を持つ）なら true。NULL_VALUE や範囲外は false。
		bool CheckActive(const SparseKey key)const
		{
			if (key == NULL_VALUE)
			{
				return false;
			}
			if (!keyToIndex_.IsValidIndex(key))
			{
				return false;
			}
			if (!indexToKey_.IsValidIndex(keyToIndex_[key]))
			{
				return false;
			}
			return keyToIndex_[key] != NULL_VALUE;
		}

		// key に対応する添え字。未登録・範囲外は NULL_VALUE（登録解除済みも NULL_VALUE が入っている）。
		DenceIndex GetIndex(const SparseKey key)const
		{
			if (!keyToIndex_.IsValidIndex(key))
			{
				return NULL_VALUE;
			}
			return keyToIndex_[key];
		}

		// 末尾（最後に登録された）要素の添え字。空なら NULL_VALUE。
		DenceIndex GetBackIndex()const
		{
			if (size_ == 0)
			{
				return NULL_VALUE;
			}
			return size_ - 1;
		}

		// 添え字 index に対応するキー。範囲外は NULL_VALUE。
		SparseKey GetKey(const DenceIndex index)const
		{
			if (!indexToKey_.IsValidIndex(index))
			{
				return NULL_VALUE;
			}

			return indexToKey_[index];
		}

		// 末尾要素のキー。空なら NULL_VALUE。
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


		//-----------------------------------
		// メモリ操作
		//-----------------------------------

		// 両テーブルのキャパシティを要素数にいい感じに合わせる（Vector::FitSlack。再確保あり）。
		void FitSlack()
		{
			indexToKey_.FitSlack();
			keyToIndex_.FitSlack();
		}

		// 両テーブルのキャパシティを要素数ちょうどに合わせる（Vector::Fit。再確保あり）。
		void Fit()
		{
			indexToKey_.Fit();
			keyToIndex_.Fit();
		}

		//-----------------------------------
		// 削除
		//-----------------------------------

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
		/// 2つのキーが指す添え字を入れ替える（値の場所は動かさない。ECOSTL dev から）
		/// 片方だけ登録済みなら、登録済みの側の添え字を、もう片方のキーに付け替える。両方未登録なら false
		/// </summary>
		bool SwapKeys(const SparseKey keyA, const SparseKey keyB)
		{
			const bool activeA = CheckActive(keyA);
			const bool activeB = CheckActive(keyB);
			if (!activeA && !activeB)
			{
				return false;
			}
			if (keyA == keyB)
			{
				return true;
			}
			if (activeA && activeB)
			{
				const DenceIndex indexA = keyToIndex_[keyA];
				const DenceIndex indexB = keyToIndex_[keyB];
				keyToIndex_[keyA] = indexB;
				keyToIndex_[keyB] = indexA;
				indexToKey_[indexA] = keyB;
				indexToKey_[indexB] = keyA;
				return true;
			}
			// 片方だけ登録済み：登録済みの側から未登録の側へ付け替える
			const SparseKey from = activeA ? keyA : keyB;
			const SparseKey to = activeA ? keyB : keyA;
			if (!keyToIndex_.IsValidIndex(to))
			{
				keyToIndex_.Resize(to + 1, NULL_VALUE);
				if (!keyToIndex_.IsValidIndex(to))
				{
					return false;
				}
			}
			const DenceIndex index = keyToIndex_[from];
			keyToIndex_[to] = index;
			keyToIndex_[from] = NULL_VALUE;
			indexToKey_[index] = to;
			return true;
		}

		/// <summary>
		/// 添え字 index に紐づくキーを key に付け替える（並べ替え用）
		/// key は登録済みのキーであること。付け替え前のキーの keyToIndex_ は呼び出し側が
		/// 全添え字ぶんを付け替えて整合させる
		/// </summary>
		void Rebind(const DenceIndex index, const SparseKey key)
		{
			indexToKey_[index] = key;
			keyToIndex_[key] = index;
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

		Vector<DenceIndex> keyToIndex_; // キー -> 添え字。未登録は NULL_VALUE。キーの最大値+1 の長さになる。
		// 添え字 -> キー。size_ 未満が有効、それ以降は NULL_VALUE。
		Vector<SparseKey> indexToKey_;
		// 登録したキーの数
		uint32_t size_;

	};


}