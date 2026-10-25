#pragma once
#include "../Container/Vector/Vector.h"

namespace utl
{
	struct RecyclableID
	{
		uint32_t index = 0;
		uint32_t version = 0;
	};

	class RecyclableIDAllocator
	{
	private:
		struct IDInfo
		{
			uint32_t version = 0;
			bool activate = false;
		};

	public:


		RecyclableIDAllocator()
			:nextGeneIndex_(0)
		{}

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
			if (recycleWaitIndices_.IsEmpty())
			{
				index = nextGeneIndex_++;
				indexToInfo_.EmplaceBack();
			}
			else
			{
				index = *recycleWaitIndices_.Back();
				recycleWaitIndices_.PopBack();
			}

			// 添え字の情報を更新する
			IDInfo& info = indexToInfo_[index];
			info.activate = true;
			version = ++info.version;

			result.*indexPtr = static_cast<IndexType>(index);
			result.*versionPtr = static_cast<VersionType>(version);
		}

		void Deallocate(const RecyclableID& id)
		{
			Deallocate(id.index, id.version);
		}

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
			recycleWaitIndices_.EmplaceBack(index);
		}

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

	private:

		// 次に生成する添え字
		uint32_t nextGeneIndex_;

		// 添え字と有効状態を紐づけて管理するコンテナ
		Vector<IDInfo> indexToInfo_;
		// リサイクル待ちの添え字を格納するコンテナ
		Vector<uint32_t> recycleWaitIndices_;

	};

}
