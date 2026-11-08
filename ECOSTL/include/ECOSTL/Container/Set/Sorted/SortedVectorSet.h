#pragma once
// 値を昇順・重複なしの配列で持つ集合(SortedVectorSet<T>)。要素数が少ない集合向け。
// 追加・削除は挿入位置以降をずらす O(N)、検索は二分探索 O(log N)。
// 昇順で持つため、同じ集合なら追加順に関係なく同じ並び・同じハッシュ値になる。
#include "ECOSTL/Container/Vector/Vector.h"
#include "ECOSTL/Algorithm/Search/BinarySearch.h"
#include "ECOSTL/Core/Hash/Hash.h"
#include <cstdint>

// ecoSTL の共通名前空間
namespace utl
{
	/// <summary>
	/// 昇順かつ重複なしで値を保持する集合
	/// T は operator< と operator== で比較でき、Hash64FromType でハッシュ化できること
	/// </summary>
	template<typename T>
	class SortedVectorSet
	{
	public:

		SortedVectorSet() = default;

		/// <summary>
		/// 使うアロケータを指定して作る（所有しない。nullptr なら既定のアロケータ）
		/// </summary>
		explicit SortedVectorSet(MemoryAllocatorDelegate* alloc)
			: items_(alloc)
		{
		}

		/// <summary>値を追加する。新たに追加されたら true</summary>
		bool Add(const T value)
		{
			uint32_t insertIndex = LowerBound(value);
			if (insertIndex < items_.Size() && items_[insertIndex] == value)
			{
				return false;
			}
			items_.Insert(insertIndex, value);
			return true;
		}

		/// <summary>値を取り除く。取り除かれたら true</summary>
		bool Remove(const T value)
		{
			uint32_t index = LowerBound(value);
			if (index >= items_.Size() || items_[index] != value)
			{
				return false;
			}
			items_.Erase(index);
			return true;
		}

		/// <summary>値を含むか</summary>
		bool Has(const T value)const
		{
			uint32_t index = LowerBound(value);
			return index < items_.Size() && items_[index] == value;
		}

		/// <summary>値が何番目にあるかを二分探索で求める。含まなければUINT32_MAX</summary>
		inline uint32_t Find(const T value)const
		{
			return utl::BinaryFind(items_.Begin(), items_.Size(), value);
		}

		void Clear()
		{
			items_.Clear();
		}

		inline uint32_t Size()const
		{
			return items_.Size();
		}

		inline bool IsEmpty()const
		{
			return items_.IsEmpty();
		}

		/// <summary>昇順に並んだ値の一覧</summary>
		inline const Vector<T>& GetItems()const
		{
			return items_;
		}

		/// <summary>集合のハッシュ値。同じ集合なら追加順に関係なく同じ値になる</summary>
		Hash64 CreateHash()const
		{
			Hash64 result = HASH_OFFSET;
			for (uint32_t index = 0; index < items_.Size(); ++index)
			{
				result = Hash64FromType(items_[index], result);
			}
			return result;
		}

		bool operator==(const SortedVectorSet& other)const
		{
			if (items_.Size() != other.items_.Size())
			{
				return false;
			}
			for (uint32_t index = 0; index < items_.Size(); ++index)
			{
				if (items_[index] != other.items_[index])
				{
					return false;
				}
			}
			return true;
		}

		bool operator!=(const SortedVectorSet& other)const
		{
			return !(*this == other);
		}

	private:

		// value以上の値が最初に現れる添え字を求める
		uint32_t LowerBound(const T value)const
		{
			return utl::LowerBound(items_.Begin(), items_.Size(), value);
		}

	private:

		// 昇順に並んだ値の一覧
		Vector<T> items_;
	};
}
