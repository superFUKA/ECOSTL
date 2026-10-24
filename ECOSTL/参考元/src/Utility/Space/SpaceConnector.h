#pragma once
#include <cinttypes>
#include "../../Container/Vector/Vector.h"
#include "../../Core/Core.hpp"

namespace utl
{
	// 空間一つ分のデータ
	struct SpaceData
	{
		SpaceData()
			: beginPosition(UINT32_MAX)
			, length(UINT32_MAX)
		{
		}

		SpaceData(uint32_t srcPositin, uint32_t srcLength)
			: beginPosition(srcPositin)
			, length(srcLength)
		{
		}

		uint32_t EndPosition()const
		{
			return beginPosition + length;
		}

		bool IsValid()const
		{
			return beginPosition != UINT32_MAX && length != UINT32_MAX;
		}

		uint32_t beginPosition;
		uint32_t length;
	};

	/// <summary>
	/// 空間を収集して結合するクラス
	/// </summary>
	class SpaceConnector
	{
	public:
		SpaceConnector()
			: length_(0)
		{
		}

		/// <summary>
		/// 空きスペースを追加する
		/// </summary>
		/// <param name="beginPositin"></param>
		/// <param name="length"></param>
		void Insert(const uint32_t beginPositin, const uint32_t length)
		{
			SpaceData insertData(beginPositin, length);
			SpaceData* preData = nullptr;
			SpaceData* nextData = nullptr;
			// 挿入場所を探す
			uint32_t currentIndex = 0;
			for (currentIndex = 0; currentIndex < vSpaces_.Size(); currentIndex++)
			{
				if (beginPositin < vSpaces_[currentIndex].beginPosition)
				{
					break;
				}
			}
			// 前のデータと結合できるか確認
			bool isPreMerged = false;
			if (currentIndex > 0)
			{
				preData = &vSpaces_[currentIndex - 1];
				if (preData->EndPosition() >= insertData.beginPosition)
				{
					uint32_t preBeginPosition = utl::Min(insertData.beginPosition, preData->beginPosition);
					uint32_t preLength = utl::Max(insertData.EndPosition(), preData->EndPosition()) - preBeginPosition;
					preData->beginPosition = preBeginPosition;
					preData->length = preLength;
					insertData = *preData;
					isPreMerged = true;
				}
			}
			// 次のデータと結合できるか確認
			bool isNextMerged = false;
			if (currentIndex < vSpaces_.Size())
			{
				nextData = &vSpaces_[currentIndex];
				// 結合したら、次の要素を削除して合体
				if (insertData.EndPosition() >= nextData->beginPosition)
				{
					uint32_t nextBeginPosition = utl::Min(insertData.beginPosition, nextData->beginPosition);
					uint32_t nextLength = utl::Max(insertData.EndPosition(), nextData->EndPosition()) - nextBeginPosition;
					nextData->length = nextLength;
					nextData->beginPosition = nextBeginPosition;
					isNextMerged = true;
				}
			}
			// 結合しなかったら新規挿入
			if (!isPreMerged && !isNextMerged)
			{
				vSpaces_.Insert(currentIndex, insertData);
			}
			// 両方結合したら次の要素を削除
			else if (isPreMerged && isNextMerged)
			{
				preData->length = nextData->length;
				vSpaces_.Erase(currentIndex);
			}

			length_ += length;
		}

		/// <summary>
		/// 指定した長さの空間を末尾から取り出し、取り出せた長さを返す
		/// </summary>
		/// <param name="result"></param>
		/// <param name="needLength"></param>
		/// <returns></returns>
		inline uint32_t Take(utl::Vector<SpaceData>& result, const uint32_t needLength)
		{
			if (needLength == 0)
			{
				return 0;
			}

			uint32_t dataNum = 0;
			uint32_t takedLength = 0;
			for (uint32_t i = vSpaces_.Size(); i > 0; --i)
			{
				SpaceData& currentData = vSpaces_[i - 1];
				uint32_t takeLength = utl::Min(currentData.length, needLength - takedLength);

				SpaceData* takedData = result.PushBack();
				takedData->length = takeLength;
				takedData->beginPosition = currentData.EndPosition() - takeLength;

				currentData.length -= takeLength;
				if (currentData.length == 0)
				{
					++dataNum;
				}

				takedLength += takeLength;
				if (takedLength >= needLength)
				{
					break;
				}
			}

			length_ -= takedLength;
			for (uint32_t index = 0; index < dataNum; ++index)
			{
				vSpaces_.PopBack();
			}

			return takedLength;
		}

		/// <summary>
		/// 空間を全て取り出す
		/// </summary>
		/// <param name="result"></param>
		inline void TakeAll(utl::Vector<SpaceData>& result)
		{
			result = utl::Move(vSpaces_);
			vSpaces_.Clear();
			length_ = 0;
		}

		/// <summary>
		/// 空間がなければtrue
		/// </summary>
		/// <returns></returns>
		inline bool Empty()const
		{
			return vSpaces_.Empty();
		}

		/// <summary>
		/// 有効な空間の長さ
		/// </summary>
		/// <returns></returns>
		uint32_t Length()const
		{
			return length_;
		}

		SpaceConnector& operator=(SpaceConnector&& src) noexcept
		{
			vSpaces_ = utl::Move(src.vSpaces_);
			length_ = src.length_;
			src.length_ = 0;
			return *this;
		}

	private:

		utl::Vector<SpaceData> vSpaces_;
		uint32_t length_;

	};
}
