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
		template<typename IDType, typename IndexType, typename VersionType>
		void Allocate(IDType& result, IndexType IDType::* indexPtr, VersionType IDType::* versionPtr)
		{
			uint32_t index = 0;
			uint32_t version = 0;

			// リサイクル待ちの添え字がなければ新しく生成して終了
			if (recycleWaitIndeces_.IsEmpty())
			{
				index = nextGeneIndex_++;
				indexToInfo_.Resize(index + 1);
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
		}

		/// <summary>
		/// まとめて生成する
		/// リサイクル待ちの添え字を先に使い、足りない分は連続した新しい添え字をまとめて確保する
		/// </summary>
		/// <param name="num">生成する数</param>
		/// <param name="onAllocate">生成した添え字とバージョンを受け取る関数 (uint32_t index, uint32_t version)</param>
		template<typename Func>
		void AllocateMany(const uint32_t num, Func&& onAllocate)
		{
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

			// 残りは新しい添え字を連続して生成し、情報の配列の拡張を一度で済ませる
			uint32_t remaining = num - allocated;
			if (remaining == 0)
			{
				return;
			}
			uint32_t beginIndex = nextGeneIndex_;
			nextGeneIndex_ += remaining;
			indexToInfo_.Resize(nextGeneIndex_);
			IDInfo* infos = indexToInfo_.Begin();
			for (uint32_t index = beginIndex; index < nextGeneIndex_; ++index)
			{
				IDInfo& info = infos[index];
				info.activate = true;
				onAllocate(index, ++info.version);
			}
		}

		/// <summary>
		/// まとめて開放するためにリサイクル待ちの容量を確保する
		/// </summary>
		/// <summary>添え字の表の容量を確保する(確保済みの添え字の数 + これから確保する数。確保で伸びないようにする)</summary>
		void ReserveIndices(const uint32_t num)
		{
			indexToInfo_.ReserveSlack(num);
		}

		void ReserveRecycle(const uint32_t num)
		{
			recycleWaitIndeces_.ReserveSlack(recycleWaitIndeces_.Size() + num);
		}

		void Deallocate(const RecyclableID& id)
		{
			Deallocate(id.index, id.version);
		}

		// ID を開放してリサイクル待ちに積む。すでに無効な ID は何もしない
		void Deallocate(const uint32_t index, const uint32_t version)
		{
			// すでに無効なIDならなにもしない
			if (!CheckActive(index, version))
			{
				return;
			}

			// 添え字を無効にし、添え字をリサイクル待ちリストに追加
			IDInfo& info = indexToInfo_[index];
			info.activate = false;
			recycleWaitIndeces_.EmplaceBack(index);

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
