#pragma once
#include <cstddef>
#include <cstdint>
#include "Utility.h"
#include "../Memory/Allocate/MemoryAllocatorAPI.h"

namespace utl
{
	namespace detail
	{
		inline int DepthLimit(uint32_t n)
		{
			int depth = 0;
			while (n > 1)
			{
				n >>= 1;
				++depth;
			}
			return depth * 2;
		}

		template<typename T>
		inline void SwapValue(T& a, T& b)
		{
			utl::Swap(a, b);
		}

		struct DefaultLess
		{
			template<typename T>
			inline bool operator()(const T& a, const T& b) const
			{
				return a < b;
			}
		};

		/// <summary>
		/// 比較関数の順序を反転する。降順は実行時の分岐ではなく、この型で選ぶ
		/// </summary>
		template<typename Compare>
		struct ReverseComp
		{
			Compare comp;

			template<typename T>
			inline bool operator()(const T& a, const T& b) const
			{
				return comp(b, a);
			}
		};

		/// <summary>
		/// 要素ではなく、メンバ変数の値を比較関数へ渡す
		/// </summary>
		template<typename TargetType, typename ValueType, typename Compare>
		struct MemberComp
		{
			ValueType TargetType::* memberPtr;
			Compare comp;

			inline bool operator()(const TargetType& a, const TargetType& b) const
			{
				return comp(a.*memberPtr, b.*memberPtr);
			}
		};

		template<typename TargetType, typename...Types>
		inline void SwapAll(TargetType* target, const uint32_t i, const uint32_t j, Types*...arrayPtrs)
		{
			SwapValue(target[i], target[j]);
			(SwapValue(arrayPtrs[i], arrayPtrs[j]), ...);
		}

		template<typename TargetType, typename Compare, typename...Types>
		inline void InsertionSortComp(TargetType* target, const int64_t left, const int64_t right, Compare comp, Types*...arrayPtrs)
		{
			for (int64_t i = left + 1; i <= right; ++i)
			{
				int64_t j = i;
				while (j > left && comp(target[j], target[j - 1]))
				{
					SwapAll(target, static_cast<uint32_t>(j - 1), static_cast<uint32_t>(j), arrayPtrs...);
					--j;
				}
			}
		}

		template<typename TargetType, typename Compare, typename...Types>
		inline void SiftDownComp(TargetType* target, const uint32_t start, const uint32_t end, Compare comp, Types*...arrayPtrs)
		{
			uint32_t root = start;
			while (true)
			{
				uint32_t child = root * 2 + 1;
				if (child > end)
				{
					return;
				}
				uint32_t swapIdx = root;
				if (comp(target[swapIdx], target[child]))
				{
					swapIdx = child;
				}
				if (child + 1 <= end && comp(target[swapIdx], target[child + 1]))
				{
					swapIdx = child + 1;
				}
				if (swapIdx == root)
				{
					return;
				}
				SwapAll(target, root, swapIdx, arrayPtrs...);
				root = swapIdx;
			}
		}

		template<typename TargetType, typename Compare, typename...Types>
		inline void HeapSortSegmentComp(TargetType* target, const uint32_t count, Compare comp, Types*...arrayPtrs)
		{
			if (count < 2)
			{
				return;
			}

			for (int64_t start = static_cast<int64_t>(count / 2) - 1; start >= 0; --start)
			{
				SiftDownComp(target, static_cast<uint32_t>(start), count - 1, comp, arrayPtrs...);
			}

			for (uint32_t end = count - 1; end > 0; --end)
			{
				SwapAll(target, 0, end, arrayPtrs...);
				SiftDownComp(target, 0, end - 1, comp, arrayPtrs...);
			}
		}

		template<typename TargetType, typename Compare, typename...Types>
		inline void IntroSortComp(TargetType* target, const int64_t left, const int64_t right, int depthLimit, Compare comp, Types*...arrayPtrs)
		{
			int64_t l = left;
			int64_t r = right;

			while (l < r)
			{
				const int64_t range = r - l + 1;
				if (range <= 16)
				{
					InsertionSortComp(target, l, r, comp, arrayPtrs...);
					return;
				}

				if (depthLimit <= 0)
				{
					const uint32_t count = static_cast<uint32_t>(range);
					HeapSortSegmentComp(target + l, count, comp, (arrayPtrs + l)...);
					return;
				}

				--depthLimit;

				const int64_t mid = l + (r - l) / 2;
				const TargetType& a = target[l];
				const TargetType& b = target[mid];
				const TargetType& c = target[r];
				const TargetType pivot = comp(a, b)
					? (comp(b, c) ? b : (comp(a, c) ? c : a))
					: (comp(a, c) ? a : (comp(b, c) ? c : b));

				int64_t i = l;
				int64_t j = r;
				while (i <= j)
				{
					while (comp(target[i], pivot))
					{
						++i;
					}
					while (comp(pivot, target[j]))
					{
						--j;
					}

					if (i <= j)
					{
						SwapAll(target, static_cast<uint32_t>(i), static_cast<uint32_t>(j), arrayPtrs...);
						++i;
						--j;
					}
				}

				// 小さい側を再帰し、大きい側をループで処理してスタックを浅く保つ
				const int64_t leftSize = j - l;
				const int64_t rightSize = r - i;
				if (leftSize < rightSize)
				{
					if (l < j)
					{
						IntroSortComp(target, l, j, depthLimit, comp, arrayPtrs...);
					}
					l = i;
				}
				else
				{
					if (i < r)
					{
						IntroSortComp(target, i, r, depthLimit, comp, arrayPtrs...);
					}
					r = j;
				}
			}
		}

		/// <summary>
		/// 添え字の範囲を安定に挿入ソートする
		/// </summary>
		template<typename TargetType, typename Compare>
		inline void InsertionSortIndicesComp(const TargetType* target, uint32_t* indices, const int64_t left, const int64_t right, Compare comp)
		{
			for (int64_t i = left + 1; i < right; ++i)
			{
				const uint32_t key = indices[i];
				int64_t j = i;
				while (j > left && comp(target[key], target[indices[j - 1]]))
				{
					indices[j] = indices[j - 1];
					--j;
				}
				indices[j] = key;
			}
		}

		template<typename TargetType, typename Compare>
		inline void MergeIndicesComp(const TargetType* target, uint32_t* indices, uint32_t* temp, const int64_t left, const int64_t mid, const int64_t right, Compare comp)
		{
			// すでに整列済みなら、併合を省く
			if (!comp(target[indices[mid]], target[indices[mid - 1]]))
			{
				return;
			}
			int64_t i = left;
			int64_t j = mid;
			int64_t k = left;
			while (i < mid && j < right)
			{
				// 右が厳密に前に来るときだけ右を取る（安定）
				if (comp(target[indices[j]], target[indices[i]]))
				{
					temp[k++] = indices[j++];
				}
				else
				{
					temp[k++] = indices[i++];
				}
			}
			while (i < mid) temp[k++] = indices[i++];
			while (j < right) temp[k++] = indices[j++];
			for (int64_t t = left; t < right; ++t) indices[t] = temp[t];
		}

		template<typename TargetType, typename Compare>
		inline void MergeSortIndicesComp(const TargetType* target, uint32_t* indices, uint32_t* temp, const int64_t left, const int64_t right, Compare comp)
		{
			if (right - left <= 16)
			{
				InsertionSortIndicesComp(target, indices, left, right, comp);
				return;
			}
			const int64_t mid = left + (right - left) / 2;
			MergeSortIndicesComp(target, indices, temp, left, mid, comp);
			MergeSortIndicesComp(target, indices, temp, mid, right, comp);
			MergeIndicesComp(target, indices, temp, left, mid, right, comp);
		}

		/// <summary>
		/// indicesの並びになるよう、基準配列と関連する配列を入れ替える
		/// </summary>
		/// <param name="pos">num個の作業領域</param>
		template<typename TargetType, typename...Types>
		inline void ApplyPermutation(TargetType* target, const uint32_t* indices, uint32_t* pos, const uint32_t num, Types*...arrayPtrs)
		{
			for (uint32_t i = 0; i < num; ++i)
			{
				pos[indices[i]] = i;
			}

			for (uint32_t i = 0; i < num; ++i)
			{
				while (pos[i] != i)
				{
					const uint32_t j = pos[i];
					SwapAll(target, i, j, arrayPtrs...);
					const uint32_t temp = pos[i];
					pos[i] = pos[j];
					pos[j] = temp;
				}
			}
		}

		template<typename TargetType, typename Compare, typename...Types>
		inline void SortImpl(TargetType* target, const uint32_t num, Compare comp, const bool descending, Types*...arrayPtrs)
		{
			const int depthLimit = DepthLimit(num);
			const int64_t right = static_cast<int64_t>(num) - 1;
			if (descending)
			{
				IntroSortComp(target, 0, right, depthLimit, ReverseComp<Compare>{ comp }, arrayPtrs...);
			}
			else
			{
				IntroSortComp(target, 0, right, depthLimit, comp, arrayPtrs...);
			}
		}

		template<typename TargetType, typename Compare, typename...Types>
		inline bool StableSortImpl(TargetType* target, const uint32_t num, MemoryAllocatorRef alloc, Compare comp, const bool descending, Types*...arrayPtrs)
		{
			// indicesとtempを1回の確保でまとめる。併合後のtempは作業領域として再利用する
			if (num > UINT32_MAX / 2)
			{
				return false;
			}
			uint32_t* buffer = alloc.Allocate<uint32_t>(num * 2);
			if (buffer == nullptr)
			{
				return false;
			}
			uint32_t* indices = buffer;
			uint32_t* temp = buffer + num;
			for (uint32_t i = 0; i < num; ++i) indices[i] = i;
			if (descending)
			{
				MergeSortIndicesComp(target, indices, temp, 0, static_cast<int64_t>(num), ReverseComp<Compare>{ comp });
			}
			else
			{
				MergeSortIndicesComp(target, indices, temp, 0, static_cast<int64_t>(num), comp);
			}
			ApplyPermutation(target, indices, temp, num, arrayPtrs...);
			alloc.Free(buffer);
			return true;
		}
	}

	/// <summary>
	/// 基準配列の値で並べ替え、関連する配列も同じ並びにします（不安定ソート）。
	/// </summary>
	/// <typeparam name="TargetType">基準配列の要素型</typeparam>
	/// <typeparam name="...Types">同時に並べ替える配列の要素型</typeparam>
	/// <param name="ratgetBegingPtr">基準配列の先頭ポインタ</param>
	/// <param name="num">要素数</param>
	/// <param name="descending">true で降順（比較結果を反転）</param>
	/// <param name="...arrayPtrs">同時に並べ替える配列の先頭ポインタ群（要素数は num と同じ）</param>
	template<typename TargetType, typename...Types>
	inline void SortArray(TargetType* ratgetBegingPtr, const uint32_t num, const bool descending = false, Types*...arrayPtrs)
	{
		if (ratgetBegingPtr == nullptr || num < 2)
		{
			return;
		}
		detail::SortImpl(ratgetBegingPtr, num, detail::DefaultLess{}, descending, arrayPtrs...);
	}

	/// <summary>
	/// 比較関数で順序を決め、関連する配列も同じ並びにします（不安定ソート）。
	/// </summary>
	/// <param name="ratgetBegingPtr">基準配列の先頭ポインタ</param>
	/// <param name="num">要素数</param>
	/// <param name="comp">比較関数（a が b より前に並ぶとき true）</param>
	/// <param name="descending">true で降順（comp の順序を反転）</param>
	/// <param name="...arrayPtrs">同時に並べ替える配列の先頭ポインタ群（要素数は num と同じ）</param>
	template<typename TargetType, typename Compare, typename...Types>
	inline void SortArrayCmp(TargetType* ratgetBegingPtr, const uint32_t num, Compare comp, const bool descending = false, Types*...arrayPtrs)
	{
		if (ratgetBegingPtr == nullptr || num < 2)
		{
			return;
		}
		detail::SortImpl(ratgetBegingPtr, num, comp, descending, arrayPtrs...);
	}

	/// <summary>
	/// 指定したメンバ変数の値で配列を並べ替えます（不安定ソート）。
	/// </summary>
	/// <typeparam name="TargetType">配列要素の型</typeparam>
	/// <typeparam name="ValueType">メンバ変数の型</typeparam>
	/// <param name="targetPtr">配列の先頭ポインタ</param>
	/// <param name="num">要素数</param>
	/// <param name="memberPtr">比較に使うメンバ変数へのポインタ</param>
	/// <param name="descending">true で降順（比較結果を反転）</param>
	template<typename TargetType, typename ValueType>
	inline void SortMember(TargetType* targetPtr, const uint32_t num, ValueType TargetType::* memberPtr, const bool descending = false)
	{
		if (targetPtr == nullptr || num < 2)
		{
			return;
		}
		detail::SortImpl(targetPtr, num, detail::MemberComp<TargetType, ValueType, detail::DefaultLess>{ memberPtr, detail::DefaultLess{} }, descending);
	}

	/// <summary>
	/// 指定したメンバ変数と比較関数で並べ替えます（不安定ソート）。
	/// </summary>
	/// <param name="targetPtr">配列の先頭ポインタ</param>
	/// <param name="num">要素数</param>
	/// <param name="memberPtr">比較に使うメンバ変数へのポインタ</param>
	/// <param name="comp">比較関数（a が b より前に並ぶとき true）</param>
	/// <param name="descending">true で降順（comp の順序を反転）</param>
	template<typename TargetType, typename ValueType, typename Compare>
	inline void SortMemberCmp(TargetType* targetPtr, const uint32_t num, ValueType TargetType::* memberPtr, Compare comp, const bool descending = false)
	{
		if (targetPtr == nullptr || num < 2)
		{
			return;
		}
		detail::SortImpl(targetPtr, num, detail::MemberComp<TargetType, ValueType, Compare>{ memberPtr, comp }, descending);
	}

	/// <summary>
	/// 基準配列の値で安定に並べ替え、関連する配列も同じ並びにします。
	/// </summary>
	/// <param name="ratgetBegingPtr">基準配列の先頭ポインタ</param>
	/// <param name="num">要素数</param>
	/// <param name="alloc">作業用のメモリを確保するアロケータ（必須）</param>
	/// <param name="descending">true で降順（比較結果を反転）</param>
	/// <param name="...arrayPtrs">同時に並べ替える配列の先頭ポインタ群（要素数は num と同じ）</param>
	/// <returns>並べ替えたらtrue。作業用のメモリを確保できなければ何もせずfalse</returns>
	template<typename TargetType, typename...Types>
	inline bool StableSortArray(TargetType* ratgetBegingPtr, const uint32_t num, const MemoryAllocatorRef& alloc, const bool descending = false, Types*...arrayPtrs)
	{
		if (ratgetBegingPtr == nullptr || num < 2)
		{
			return true;
		}
		return detail::StableSortImpl(ratgetBegingPtr, num, alloc, detail::DefaultLess{}, descending, arrayPtrs...);
	}

	/// <summary>
	/// 比較関数で安定に並べ替え、関連する配列も同じ並びにします。
	/// </summary>
	/// <param name="ratgetBegingPtr">基準配列の先頭ポインタ</param>
	/// <param name="num">要素数</param>
	/// <param name="alloc">作業用のメモリを確保するアロケータ（必須）</param>
	/// <param name="comp">比較関数（a が b より前に並ぶとき true）</param>
	/// <param name="descending">true で降順（comp の順序を反転）</param>
	/// <param name="...arrayPtrs">同時に並べ替える配列の先頭ポインタ群（要素数は num と同じ）</param>
	/// <returns>並べ替えたらtrue。作業用のメモリを確保できなければ何もせずfalse</returns>
	template<typename TargetType, typename Compare, typename...Types>
	inline bool StableSortArrayCmp(TargetType* ratgetBegingPtr, const uint32_t num, const MemoryAllocatorRef& alloc, Compare comp, const bool descending = false, Types*...arrayPtrs)
	{
		if (ratgetBegingPtr == nullptr || num < 2)
		{
			return true;
		}
		return detail::StableSortImpl(ratgetBegingPtr, num, alloc, comp, descending, arrayPtrs...);
	}

	/// <summary>
	/// 指定したメンバ変数の値で安定に並べ替えます。
	/// </summary>
	/// <param name="targetPtr">配列の先頭ポインタ</param>
	/// <param name="num">要素数</param>
	/// <param name="alloc">作業用のメモリを確保するアロケータ（必須）</param>
	/// <param name="memberPtr">比較に使うメンバ変数へのポインタ</param>
	/// <param name="descending">true で降順（比較結果を反転）</param>
	/// <returns>並べ替えたらtrue。作業用のメモリを確保できなければ何もせずfalse</returns>
	template<typename TargetType, typename ValueType>
	inline bool StableSortMember(TargetType* targetPtr, const uint32_t num, const MemoryAllocatorRef& alloc, ValueType TargetType::* memberPtr, const bool descending = false)
	{
		if (targetPtr == nullptr || num < 2)
		{
			return true;
		}
		return detail::StableSortImpl(targetPtr, num, alloc, detail::MemberComp<TargetType, ValueType, detail::DefaultLess>{ memberPtr, detail::DefaultLess{} }, descending);
	}

	/// <summary>
	/// 指定したメンバ変数と比較関数で安定に並べ替えます。
	/// </summary>
	/// <param name="targetPtr">配列の先頭ポインタ</param>
	/// <param name="num">要素数</param>
	/// <param name="alloc">作業用のメモリを確保するアロケータ（必須）</param>
	/// <param name="memberPtr">比較に使うメンバ変数へのポインタ</param>
	/// <param name="comp">比較関数（a が b より前に並ぶとき true）</param>
	/// <param name="descending">true で降順（comp の順序を反転）</param>
	/// <returns>並べ替えたらtrue。作業用のメモリを確保できなければ何もせずfalse</returns>
	template<typename TargetType, typename ValueType, typename Compare>
	inline bool StableSortMemberCmp(TargetType* targetPtr, const uint32_t num, const MemoryAllocatorRef& alloc, ValueType TargetType::* memberPtr, Compare comp, const bool descending = false)
	{
		if (targetPtr == nullptr || num < 2)
		{
			return true;
		}
		return detail::StableSortImpl(targetPtr, num, alloc, detail::MemberComp<TargetType, ValueType, Compare>{ memberPtr, comp }, descending);
	}

	/// <summary>
	/// 配列を並べ替えます（不安定ソート）。
	/// </summary>
	/// <param name="beginPtr">先頭ポインタ</param>
	/// <param name="num">要素数</param>
	/// <param name="descending">true で降順（比較結果を反転）</param>
	template<typename T>
	inline void Sort(T* beginPtr, const uint32_t num, const bool descending = false)
	{
		SortArray(beginPtr, num, descending);
	}

	/// <summary>
	/// 比較関数で並べ替えます（不安定ソート）。
	/// </summary>
	/// <param name="beginPtr">先頭ポインタ</param>
	/// <param name="num">要素数</param>
	/// <param name="comp">比較関数（a が b より前に並ぶとき true）</param>
	/// <param name="descending">true で降順（comp の順序を反転）</param>
	template<typename T, typename Compare>
	inline void Sort(T* beginPtr, const uint32_t num, Compare comp, const bool descending = false)
	{
		SortArrayCmp(beginPtr, num, comp, descending);
	}

	/// <summary>
	/// 配列を安定に並べ替えます。
	/// </summary>
	/// <param name="beginPtr">先頭ポインタ</param>
	/// <param name="num">要素数</param>
	/// <param name="alloc">作業用のメモリを確保するアロケータ（必須）</param>
	/// <param name="descending">true で降順（比較結果を反転）</param>
	/// <returns>並べ替えたらtrue。作業用のメモリを確保できなければ何もせずfalse</returns>
	template<typename T>
	inline bool StableSort(T* beginPtr, const uint32_t num, const MemoryAllocatorRef& alloc, const bool descending = false)
	{
		return StableSortArray(beginPtr, num, alloc, descending);
	}

	/// <summary>
	/// 比較関数で安定に並べ替えます。
	/// </summary>
	/// <param name="beginPtr">先頭ポインタ</param>
	/// <param name="num">要素数</param>
	/// <param name="alloc">作業用のメモリを確保するアロケータ（必須）</param>
	/// <param name="comp">比較関数（a が b より前に並ぶとき true）</param>
	/// <param name="descending">true で降順（comp の順序を反転）</param>
	/// <returns>並べ替えたらtrue。作業用のメモリを確保できなければ何もせずfalse</returns>
	template<typename T, typename Compare>
	inline bool StableSort(T* beginPtr, const uint32_t num, const MemoryAllocatorRef& alloc, Compare comp, const bool descending = false)
	{
		return StableSortArrayCmp(beginPtr, num, alloc, comp, descending);
	}
}
