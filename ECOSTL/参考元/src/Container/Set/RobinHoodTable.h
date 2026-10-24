#pragma once
#include <cstdint>   
#include <cstring>   
#include <utility>   
#include <type_traits>
#include "src/Core/Core.hpp"
#include "src/Utility/Utility.hpp"

namespace utl
{
	using RHKey = uint64_t;
	constexpr RHKey RHKEY_NULL = UINT64_MAX;
	// リサイズするなら
	constexpr RHKey RESIZE_NULL = UINT64_MAX - 1;

	using RHIndex = uint32_t;
	constexpr RHIndex RHINDEX_NULL = UINT32_MAX;
	constexpr RHIndex RHINDEX_NOTFOUND = UINT32_MAX - 1;

	using RHID = RecyclableID;
	constexpr uint32_t RHINDEX_RESIZE = UINT32_MAX - 2;

	// 削除時は消えたキーの添え字を空にせず、削除済み扱いにする
	// キーを空にすると検索が空になったキーに反応し止まり、すでに挿入済みのキーを未挿入と勘違いする可能性があるため
	// 以上のことからRobinHoodHashクラスには削除機能必要ないかも

	class RobinHoodHash
	{
	public:

		//------------------------------------
		// 作成
		//------------------------------------

		RobinHoodHash(uint32_t cap, uint32_t maxDist = UINT32_MAX, utl::MemoryAllocatorAPI* memAlloc = nullptr)
			:indexToKey_(cap, memAlloc)
			, size_(0)
			, cap_(cap)
			, maxDist_(maxDist)
		{
			indexToKey_.Resize(cap, RHKEY_NULL);
		}

		void Create(uint32_t cap, uint32_t maxDist, utl::MemoryAllocatorAPI* memAlloc)
		{
			Clear();
			cap_ = cap;
			size_ = 0;
			maxDist_ = maxDist;
			indexToKey_.Resize(cap, RHKEY_NULL);
		}

		void Create(uint32_t cap, uint32_t maxDist = UINT32_MAX)
		{
			Create(cap, maxDist, nullptr);
		}

		//------------------------------------
		// 代入
		//------------------------------------

		/// <summary>
		/// 要素をコピーして作成するコンストラクタ
		/// </summary>
		/// <param name="src"></param>
		RobinHoodHash(const RobinHoodHash& src)
			:indexToKey_(src.indexToKey_)
			, size_(src.size_)
			, cap_(src.cap_)
		{

		}

		RobinHoodHash& operator=(const RobinHoodHash& src)
		{
			indexToKey_ = src.indexToKey_;
			size_ = src.size_;
			cap_ = src.cap_;
			return *this;
		}


		RobinHoodHash(RobinHoodHash&& src)noexcept
			: indexToKey_(src.indexToKey_)
			, size_(src.size_)
			, cap_(src.cap_)
			, maxDist_(RHKEY_NULL)
		{
			src.indexToKey_.Clear();
			src.size_ = 0;
			src.cap_ = 0;
		}

		RobinHoodHash& operator=(RobinHoodHash&& src)noexcept
		{
			indexToKey_ = Move(src.indexToKey_);
			size_ = src.size_;
			cap_ = src.cap_;

			src.indexToKey_.Clear();
			src.size_ = 0;
			src.cap_ = 0;
			return *this;
		}


		//------------------------------------
		// 開放
		//------------------------------------

		void Clear()
		{
			indexToKey_.Clear();
			size_ = 0;
			cap_ = 0;
		}

		//------------------------------------
		// 操作
		//------------------------------------

		struct InsertResult
		{
			// 添え字を奪った場合に奪った相手のキーを格納する
			// 空の添え字を見つけた場合はNULL、検索キーと同じキーを見つけたらそのキーが入る
			RHKey swapKey = RHKEY_NULL;
			// キーと紐づく添え字かつ、スワップしたキーから奪った添え字でもある
			RHIndex resultIndex = RHINDEX_NULL;
		};

		/// <summary>
		/// ハッシュ値を挿入する
		/// ロビンフッドハッシュアルゴリズムに沿って入れ替えが発生した場合、
		/// 入れ替えるのではなく結果で入れ替えが発生したことを伝えて終了する
		/// この関数を使う側は結果に応じて入れ替え後の処理をする必要がある
		/// 
		/// 結果のSwapKeyがNULL：新しいキーとして挿入に成功
		/// 結果のSwapKeyが引数と一致：すでに登録済みのキー
		/// 結果のSwapKeyが知らないキー：そのキーとのスワップが発生
		///							　使用者はそのキーを使いもう一度Insertする必要がある
		/// </summary>
		/// <param name="key"></param>
		/// <returns></returns>
		InsertResult Insert(const RHKey key)
		{
			InsertResult result;

			// すでに容量を越していたらリサイズするような結果を返す
			if (size_ == cap_)
			{
				result.swapKey = RESIZE_NULL;
				return result;
			}
			// 理想の位置を求める
			uint32_t keyHope = key % cap_;

			for (uint32_t dist = 0; dist < cap_; ++dist)
			{
				// 最長距離を超えたら失敗を返す
				if (dist >= maxDist_)
				{
					result.swapKey = RESIZE_NULL;
					return result;
				}

				RHIndex lookIndex = (keyHope + dist) % cap_;
				// 検索位置にいたキーを求める
				RHKey lookKey = indexToKey_[lookIndex];

				// 解決
				//-------------------------------

				// キーが空なら挿入して終了
				if (lookKey == RHKEY_NULL)
				{
					++size_;
					indexToKey_[lookIndex] = key;
					result.swapKey = RHKEY_NULL;
					result.resultIndex = lookIndex;
					return result;
				}
				// 同じキーを発見したら挿入済みの処理をして終了
				if (lookKey == key)
				{
					// スワップキーに引数を渡すことで一致を伝える
					result.swapKey = key;
					result.resultIndex = lookIndex;
					return result;
				}

				// 未解決
				//-------------------------------

				// 先客のキーの理想の位置と今の位置の距離を求める
				uint32_t lookHopeDist = (lookIndex + cap_ - (lookKey % cap_)) % cap_;
				// 元々あったキーのほうが近ければその位置を奪い、
				// 元々あったキーを挿入しなおすために入れ替えたハッシュ値を返す
				// こうすることで、キーの距離が均一化され高速化につながるらしい
				if (lookHopeDist < dist)
				{
					indexToKey_[lookIndex] = key;
					result.swapKey = lookKey;
					result.resultIndex = lookIndex;
					return result;
				}
			}

			// 場所が見つからなければ失敗を返す
			result.resultIndex = RHINDEX_NOTFOUND;
			return result;
		}

		RHKey GetKey(const RHIndex index)
		{
			return indexToKey_[index];
		}

		RHIndex GetIndex(const RHKey key)const
		{
			// 理想の位置を求める
			uint32_t keyHope = key % cap_;

			// 理想の位置から一つずつ離れていく
			for (uint32_t dist = 0; dist < cap_; ++dist)
			{
				// 最長距離を超えたらNULL
				if (dist >= maxDist_)
				{
					return RHINDEX_NULL;
				}

				RHIndex lookIndex = (keyHope + dist) % cap_;
				// 検索位置にいたキーを求める
				RHKey lookKey = indexToKey_[lookIndex];
				
				// 同じキーを発見したら返す
				if (lookKey == key)
				{
					return lookIndex;
				}
				
				// 空のキーなら見つからなかったのでNULLを返す
				if (lookKey == RHKEY_NULL)
				{
					return RHINDEX_NULL;
				}
			}

			// 場所が見つからなければNULLを返す
			return RHINDEX_NULL;
		}

		//------------------------------------
		// 取得
		//------------------------------------

		const Vector<RHKey>& GetIndexToKey()const
		{
			return indexToKey_;
		}

		uint32_t GetSize()
		{
			return size_;
		}

		uint32_t GetCap()
		{
			return cap_;
		}

	private:

		Vector<RHKey> indexToKey_;
		uint32_t size_;
		uint32_t cap_;
		RHKey maxDist_ = RHKEY_NULL;

	};

	/// <summary>
	/// ハッシュ値と連番で一意なIDを紐づけるテーブル
	/// </summary>
	class RobinHoodHashTable
	{
	public:

		RobinHoodHashTable(uint32_t cap = 0, uint32_t maxDist = UINT32_MAX, MemoryAllocatorAPI* alloc = nullptr)
			:rhHash_(cap, maxDist, alloc)
		{
			rhIndexToRhID_.Resize(cap);
			rhIDToRHIndex_.Resize(cap);
		}

		RHID PreInsert(const RHKey key, RobinHoodHash::InsertResult& insertResult)
		{
			// 引数のキーを挿入して添え字を取得
			insertResult = rhHash_.Insert(key);

			// すでに挿入済みだった場合は登録済みのＩＤを返して終了
			if (insertResult.swapKey == key)
			{
				return rhIndexToRhID_[insertResult.resultIndex];
			}

			RHID result;

			// リサイズの必要があれば
			if (insertResult.swapKey == RESIZE_NULL)
			{
				result.index = RHINDEX_RESIZE;
				return result;
			}

			result.index = UINT32_MAX;

			return result;
		}

		void PostInsert(const RHKey key, RobinHoodHash::InsertResult& insertResult, RHID nID, RHID swapID)
		{
			// 空きが見つかるまでスワップしてきたキーを再挿入し続ける
			// スワップしてきたキーは乗っ取られているため重複は起きない
			//RHID swapID = rhIndexToRhID_[insertResult.resultIndex];
			RHID oldID = swapID;
			while (insertResult.swapKey != RHKEY_NULL)
			{
				swapID = oldID;

				insertResult = rhHash_.Insert(insertResult.swapKey);

				oldID = rhIndexToRhID_[insertResult.resultIndex];
				rhIndexToRhID_[insertResult.resultIndex] = swapID;
				rhIDToRHIndex_[swapID.index] = insertResult.resultIndex;
			}

			//新しいＩＤを発行してkeyを登録する
			RHIndex keyIndex = rhHash_.GetIndex(key);
			// GetIndexがNULLを返す場合は異常なので処理しない
			if (keyIndex == RHINDEX_NULL)
			{
				return;
			}
			rhIndexToRhID_[keyIndex] = nID;
			rhIDToRHIndex_[nID.index] = keyIndex;
		}

		RHID Insert(const RHKey key)
		{
			RobinHoodHash::InsertResult insertResult;

			if (rhHash_.GetCap() == rhHash_.GetSize())
			{
				ReHash(static_cast<uint32_t>(static_cast<float>(rhHash_.GetCap()) * 1.5f));
			}

			RHID keyID = PreInsert(key, insertResult);

			if (keyID.index == RHINDEX_RESIZE)
			{
				ReHash(static_cast<uint32_t>(static_cast<float>(rhHash_.GetCap()) * 1.5f));
				// リサイズ後に再度PreInsertを実行
				keyID = PreInsert(key, insertResult);
			}



			// すでにIDが作成済みなら
			if (keyID.index != UINT32_MAX)
			{
				// 有効なIDならそのまま返す
				if (rhIDAllocator_.CheckActive(keyID))
				{
					return keyID;
				}
				// 削除済みのIDなら新しいIDを発行し、紐づけなおす
				RHID nID = rhIDAllocator_.Allocate();
				rhIndexToRhID_[insertResult.resultIndex] = nID;
				rhIDToRHIndex_[nID.index] = insertResult.resultIndex;
				return nID;
			}

			// 新しいIDを作成する
			keyID = rhIDAllocator_.Allocate();
			RHID swapID = rhIndexToRhID_[insertResult.resultIndex];

			PostInsert(key, insertResult, keyID, swapID);

			return keyID;
		}

		/// <summary>
		/// メモリサイズを変更する
		/// </summary>
		/// <param name="size"></param>
		void ReHash(uint32_t newCap)
		{
			// 旧コンテナ	の所有権を移動
			RobinHoodHash oldHash(Move(rhHash_));
			Vector<RHID> oldIndexToRhID(Move(rhIndexToRhID_));
			Vector<RHIndex>	oldIDToRHIndex(Move(rhIDToRHIndex_));

			// ハッシュを再確保
			rhHash_.Create(newCap);
			rhIndexToRhID_.Resize(newCap);
			rhIDToRHIndex_.Resize(newCap);

			for (uint32_t i = 0; i < oldHash.GetIndexToKey().Size(); i++)
			{
				RHID oldID = oldIndexToRhID[i];

				// すでに無効になったIDなら無視
				if (!rhIDAllocator_.CheckActive(oldID))
				{
					continue;
				}

				RHKey oldKey = oldHash.GetIndexToKey()[i];

				// 
				if (oldKey == RHKEY_NULL)
				{
					continue;
				}

				RobinHoodHash::InsertResult insertResult;


				RHID nID = PreInsert(oldKey, insertResult);

				// IDが存在しないか削除済みなら次
				if (nID.index != UINT32_MAX)
				{
					continue;
				}

				if (insertResult.resultIndex >= rhIndexToRhID_.Size())
				{
					continue;
				}
				RHID swapID = rhIndexToRhID_[insertResult.resultIndex];

				PostInsert(oldKey, insertResult, oldID, swapID);
			}
		}

		void Remove(const RHKey nKey)
		{
			RHIndex index = rhHash_.GetIndex(nKey);
			if (!rhIndexToRhID_.IsContain(index))
			{
				return;
			}
			RHID id = rhIndexToRhID_[index];
			rhIDAllocator_.Deallocate(id);
		}

		bool IsActive(const RHKey nKey)
		{
			RHIndex index = rhHash_.GetIndex(nKey);
			RHID id = rhIndexToRhID_[index];
			return rhIDAllocator_.CheckActive(id);
		}

		RHID Find(const RHKey nKey)const
		{
			RHIndex index = rhHash_.GetIndex(nKey);
			if (!rhIndexToRhID_.IsContain(index))
			{
				return RHID();
			}
			return rhIndexToRhID_[index];
		}

		RHID* GetIndexToID()
		{
			return &rhIndexToRhID_[0];
		}

		RHIndex* GetIDToIndex()
		{
			return &rhIDToRHIndex_[0];
		}

		bool CheckActive(RHID id)const
		{
			return rhIDAllocator_.CheckActive(id);
		}


	private:

		RobinHoodHash rhHash_;

		Vector<RHID> rhIndexToRhID_;
		Vector<RHIndex> rhIDToRHIndex_;

		RecyclableIDAllocator rhIDAllocator_;
	};
}