#pragma once
// 値を昇順・重複なしで、固定の大きさの配列に持つ集合(FixedSortedSet<T, Capacity>)。動的確保を一切しない値の型。
// 小さな集合を値として作ってコピーする用途向け（例: アーキタイプを構成する型の集合）。
// 追加・削除は挿入位置以降をずらす O(N)、検索は二分探索 O(log N)。同じ集合なら追加順に関係なく同じ並び・同じハッシュ値になる。
// Capacity を超えて追加しようとすると、assert で止まり false を返す（上限は使う側が定数で決める）。
#include <cassert>
#include <cstdint>
#include "ECOSTL/Container/Array/View/ArrayView.h"
#include "ECOSTL/Core/Hash/Hash.h"

// 汎用ユーティリティ名前空間
namespace utl
{
	/// <summary>
	/// 昇順かつ重複なしで値を保持する、固定容量の集合
	/// T は operator< と operator== で比較でき、Hash64FromType でハッシュ化できる、平凡にコピーできる型であること
	/// </summary>
	template<typename T, uint32_t Capacity>
	class FixedSortedSet
	{
		static_assert(Capacity > 0, "Capacity は 1 以上");

	public:

		/// <summary>値を追加する。新たに追加されたら true。既にあるとき、または満杯のとき false（満杯は assert でも知らせる）</summary>
		bool Add(const T value)
		{
			const uint32_t insertIndex = LowerBound(value);
			if (insertIndex < size_ && items_[insertIndex] == value)
			{
				return false;
			}
			if (size_ >= Capacity)
			{
				assert(false && "FixedSortedSet の容量を超えました");
				return false;
			}
			for (uint32_t index = size_; index > insertIndex; --index)
			{
				items_[index] = items_[index - 1];
			}
			items_[insertIndex] = value;
			++size_;
			return true;
		}

		/// <summary>値を取り除く。取り除かれたら true</summary>
		bool Remove(const T value)
		{
			const uint32_t index = LowerBound(value);
			if (index >= size_ || items_[index] != value)
			{
				return false;
			}
			for (uint32_t i = index + 1; i < size_; ++i)
			{
				items_[i - 1] = items_[i];
			}
			--size_;
			return true;
		}

		/// <summary>値を含むか</summary>
		bool Has(const T value)const
		{
			const uint32_t index = LowerBound(value);
			return index < size_ && items_[index] == value;
		}

		/// <summary>値が何番目にあるかを二分探索で求める。含まなければUINT32_MAX</summary>
		inline uint32_t Find(const T value)const
		{
			const uint32_t index = LowerBound(value);
			return index < size_ && items_[index] == value ? index : UINT32_MAX;
		}

		inline void Clear()
		{
			size_ = 0;
		}

		inline uint32_t Size()const
		{
			return size_;
		}

		inline bool IsEmpty()const
		{
			return size_ == 0;
		}

		inline bool IsFull()const
		{
			return size_ >= Capacity;
		}

		static constexpr uint32_t GetCapacity()
		{
			return Capacity;
		}

		/// <summary>昇順に並んだ値の一覧（この集合が変わる・破棄されるまで有効）</summary>
		inline ArrayView<T> GetItems()const
		{
			return ArrayView<T>(items_, size_);
		}

		/// <summary>集合のハッシュ値。同じ集合なら追加順に関係なく同じ値になる</summary>
		Hash64 CreateHash()const
		{
			Hash64 result = HASH_OFFSET;
			for (uint32_t index = 0; index < size_; ++index)
			{
				result = Hash64FromType(items_[index], result);
			}
			return result;
		}

		bool operator==(const FixedSortedSet& other)const
		{
			if (size_ != other.size_)
			{
				return false;
			}
			for (uint32_t index = 0; index < size_; ++index)
			{
				if (items_[index] != other.items_[index])
				{
					return false;
				}
			}
			return true;
		}

		bool operator!=(const FixedSortedSet& other)const
		{
			return !(*this == other);
		}

	private:

		// value以上の値が最初に現れる添え字を求める
		uint32_t LowerBound(const T value)const
		{
			uint32_t left = 0;
			uint32_t right = size_;
			while (left < right)
			{
				const uint32_t mid = left + ((right - left) >> 1);
				if (items_[mid] < value)
				{
					left = mid + 1;
				}
				else
				{
					right = mid;
				}
			}
			return left;
		}

	private:

		T items_[Capacity] = {};  // 昇順に並んだ値（先頭から size_ 個が有効）
		uint32_t size_ = 0;  // 要素数
	};
}
