#pragma once
// 添え字+バージョンで世代管理する再利用可能な ID(RecyclableID)の発行器。スレッドセーフではない。
#include "ECOSTL/Container/Vector/Vector.h"

// 汎用ユーティリティ名前空間
namespace utl
{
	// 添え字とバージョンの組。開放後に同じ添え字が再利用されても、バージョンが違えば古い ID は無効になる
	struct RecyclableID
	{
		uint32_t index = 0;   // 添え字(再利用される)
		uint32_t version = 0; // 世代(確保のたびに増える。有効な ID は 1 以上)
	};

	// RecyclableID を発行・開放・有効判定する。開放した添え字はリサイクル待ちに積み、次の確保で後ろから再利用する
	class RecyclableIDAllocator
	{
	private:
		// 添え字ごとの状態(バージョンと、リサイクル待ちかどうか)
		struct IDInfo
		{
			uint32_t version = 0;    // この添え字の最新バージョン
			bool activate = false;   // 現在有効(確保中)か
		};

	public:


		RecyclableIDAllocator()
			:nextGeneIndex_(0)
		{
		}

		/// <summary>
		/// 使うアロケータを指定して作る（所有しない。nullptr なら既定のアロケータ）
		/// </summary>
		explicit RecyclableIDAllocator(MemoryAllocatorDelegate* alloc)
			:nextGeneIndex_(0)
			, indexToInfo_(alloc)
			, recycleWaitIndeces_(alloc)
		{
		}

		// ID を 1 つ発行する。リサイクル待ちがあれば再利用し、なければ新しい添え字を作る(表の拡張で確保が起きうる)
		// 確保できなければ version が 0 の ID を返す(有効な ID の version は 1 以上なので、どの ID とも一致しない)
		RecyclableID Allocate()
		{
			RecyclableID result;
			Allocate(result, &RecyclableID::index, &RecyclableID::version);
			return result;
		}

		/// <summary>
		/// 再利用可能なIDを生成し、任意の型の任意の変数にセットする
		/// </summary>
		/// <typeparam name="IDType"></typeparam>
		/// <typeparam name="IndexType"></typeparam>
		/// <typeparam name="VersionType"></typeparam>
		/// <param name="result"></param>
		/// <param name="indexPtr"></param>
		/// <param name="versionPtr"></param>
		/// <returns>確保できなければ false（result は変えず、状態も変わらない）</returns>
		template<typename IDType, typename IndexType, typename VersionType>
		bool Allocate(IDType& result, IndexType IDType::* indexPtr, VersionType IDType::* versionPtr)
		{
			uint32_t index = 0;
			uint32_t version = 0;

			// リサイクル待ちの添え字がなければ新しく生成する（表を広げられたときだけ添え字を進める）
			if (recycleWaitIndeces_.IsEmpty())
			{
				index = nextGeneIndex_;
				if (!indexToInfo_.Resize(index + 1))
				{
					return false;
				}
				++nextGeneIndex_;
			}
			else
			{
				index = *recycleWaitIndeces_.Back();
				recycleWaitIndeces_.Resize(recycleWaitIndeces_.Size() - 1);
			}

			// 添え字の情報を更新する
			IDInfo& info = indexToInfo_[index];
			info.activate = true;
			version = ++info.version;

			result.*indexPtr = static_cast<IndexType>(index);
			result.*versionPtr = static_cast<VersionType>(version);
			return true;
		}

		/// <summary>
		/// まとめて生成する
		/// リサイクル待ちの添え字を先に使い、足りない分は連続した新しい添え字をまとめて確保する
		/// </summary>
		/// <param name="num">生成する数</param>
		/// <param name="onAllocate">生成した添え字とバージョンを受け取る関数 (uint32_t index, uint32_t version)</param>
		/// <returns>確保できなければ false（1つも生成せず、onAllocate も呼ばない）</returns>
		template<typename Func>
		bool AllocateMany(const uint32_t num, Func&& onAllocate)
		{
			// 新しい添え字が要るなら、先に表を広げる（失敗したら何も生成しない）
			const uint32_t recycleNum = recycleWaitIndeces_.Size();
			const uint32_t newNum = num > recycleNum ? num - recycleNum : 0;
			if (newNum > 0 && !indexToInfo_.Resize(nextGeneIndex_ + newNum))
			{
				return false;
			}

			uint32_t allocated = 0;
			// リサイクル待ちの添え字を使う
			while (allocated < num && !recycleWaitIndeces_.IsEmpty())
			{
				uint32_t index = *recycleWaitIndeces_.Back();
				recycleWaitIndeces_.PopBack();
				IDInfo& info = indexToInfo_[index];
				info.activate = true;
				onAllocate(index, ++info.version);
				++allocated;
			}

			// 残りは、先に広げた表の新しい添え字を連続して使う
			const uint32_t beginIndex = nextGeneIndex_;
			nextGeneIndex_ += newNum;
			IDInfo* infos = indexToInfo_.Begin();
			for (uint32_t index = beginIndex; index < nextGeneIndex_; ++index)
			{
				IDInfo& info = infos[index];
				info.activate = true;
				onAllocate(index, ++info.version);
			}
			return true;
		}

		/// <summary>添え字の表の容量を確保する(確保済みの添え字の数 + これから確保する数。確保で伸びないようにする)</summary>
		/// <returns>確保できなければ false</returns>
		bool ReserveIndices(const uint32_t num)
		{
			return indexToInfo_.ReserveSlack(num);
		}

		/// <summary>まとめて開放するために、リサイクル待ちの容量を今の数 + num 確保する</summary>
		/// <returns>確保できなければ false</returns>
		bool ReserveRecycle(const uint32_t num)
		{
			return recycleWaitIndeces_.ReserveSlack(recycleWaitIndeces_.Size() + num);
		}

		bool Deallocate(const RecyclableID& id)
		{
			return Deallocate(id.index, id.version);
		}

		// ID を開放してリサイクル待ちに積む
		// すでに無効な ID なら何もせず false。リサイクル待ちを広げられなければ、ID は有効なまま false
		bool Deallocate(const uint32_t index, const uint32_t version)
		{
			if (!CheckActive(index, version))
			{
				return false;
			}

			// 先にリサイクル待ちへ積み、積めたときだけ添え字を無効にする
			if (recycleWaitIndeces_.EmplaceBack(index) == nullptr)
			{
				return false;
			}
			indexToInfo_[index].activate = false;
			return true;
		}

		/// <summary>
		/// 有効な添え字を開放せずにそのまま再利用する
		/// バージョンを上げるため古いIDは無効になり、新しいバージョンのIDとして有効なまま使える
		/// 開放してすぐ同じ数を生成する場合に、リサイクル待ちを経由せずに済む
		/// </summary>
		/// <returns>新しいバージョン 無効なIDなら0</returns>
		uint32_t Renew(const uint32_t index, const uint32_t version)
		{
			if (!CheckActive(index, version))
			{
				return 0;
			}
			return ++indexToInfo_[index].version;
		}

		// index/version が最新かつ確保中の ID か(O(1))
		bool CheckActive(const uint32_t index, const uint32_t version)const
		{
			// まだ生成されてない添え字なら無効
			if (indexToInfo_.Size() <= index)
			{
				return false;
			}

			// 添え字の最新バージョンで無ければ無効
			const IDInfo& info = indexToInfo_[index];
			if (info.version != version)
			{
				return false;
			}

			// 添え字の有効状態を返す
			return info.activate;
		}

		bool CheckActive(const RecyclableID& id)const
		{
			return CheckActive(id.index, id.version);
		}

		/// <summary>
		/// 添え字の最新のバージョンを取得する
		/// </summary>
		/// <returns>まだ生成されていない添え字なら0</returns>
		uint32_t GetVersion(const uint32_t index)const
		{
			if (indexToInfo_.Size() <= index)
			{
				return 0;
			}
			return indexToInfo_[index].version;
		}

	private:

		// 次に生成する添え字
		uint32_t nextGeneIndex_; // 新規に発行する添え字(これまでに作った添え字の総数と等しい)

		// 添え字と有効状態を紐づけて管理するコンテナ
		Vector<IDInfo> indexToInfo_;
		// リサイクル待ちの添え字を格納するコンテナ
		Vector<uint32_t> recycleWaitIndeces_;

	};

}
