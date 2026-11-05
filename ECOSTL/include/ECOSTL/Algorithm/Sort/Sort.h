#pragma once
#include "ECOSTL/Core/Template/Template.h"
#include "ECOSTL/Memory/Allocator/MemoryAllocator.h"
#include <cstdint>

// 基準配列と関連配列を同じ並びに揃える並べ替え関数群(イントロソート=不安定、マージソート=安定)。

// ecoSTL 全体の名前空間。
namespace utl
{
	// Sort.h の内部実装。外部からは直接使わない。
	namespace detail
	{
		// 安定ソートの作業領域。MemoryAllocatorRef で確保し、範囲を出るときに返す
		class SortWorkBuffer
		{
		public:
			SortWorkBuffer(const uint32_t num, const MemoryAllocatorRef& alloc)
				: alloc_(alloc)
				, memory_(num <= UINT32_MAX / 2 ? alloc_.Allocate<uint32_t>(num * 2) : nullptr)
			{
				if (memory_ == nullptr)
				{
					UTL_LOG(Warn | ::utl::logtype::Container, "stable sort skipped: work buffer allocation failed num={num}", num);
				}
			}
			~SortWorkBuffer()
			{
				alloc_.Free(memory_);
			}
			SortWorkBuffer(const SortWorkBuffer&) = delete;
			SortWorkBuffer& operator=(const SortWorkBuffer&) = delete;

			uint32_t* Get()const
			{
				return memory_;
			}

		private:
			MemoryAllocatorRef alloc_;
			uint32_t* memory_;
		};

		// イントロソートの再帰深さ上限(2*log2(n))。超えたらヒープソートに切り替える
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

		// operator< で比較する既定の比較関数オブジェクト
		struct DefaultLess
		{
			template<typename T>
			inline bool operator()(const T& a, const T& b) const
			{
				return a < b;
			}
		};

		template<typename Compare, typename T>
		inline bool LessComp(const T& a, const T& b, Compare comp, const bool descending)
		{
			return descending ? comp(b, a) : comp(a, b);
		}

		template<typename Compare, typename T>
		inline bool GreaterComp(const T& a, const T& b, Compare comp, const bool descending)
		{
			return descending ? comp(a, b) : comp(b, a);
		}

		// 基準配列と関連配列の i 番目と j 番目を同時に入れ替える
		template<typename TargetType, typename...Types>
		inline void SwapAll(TargetType* target, const uint32_t i, const uint32_t j, Types*...arrayPtrs)
		{
			SwapValue(target[i], target[j]);
			(SwapValue(arrayPtrs[i], arrayPtrs[j]), ...);
		}

		// [left, right] 区間の挿入ソート(小区間用・安定、O(n^2))
		template<typename TargetType, typename Compare, typename...Types>
		inline void InsertionSortComp(TargetType* target, const int64_t left, const int64_t right, Compare comp, const bool descending, Types*...arrayPtrs)
		{
			for (int64_t i = left + 1; i <= right; ++i)
			{
				int64_t j = i;
				while (j > left && LessComp(target[j], target[j - 1], comp, descending))
				{
					SwapAll(target, static_cast<uint32_t>(j - 1), static_cast<uint32_t>(j), arrayPtrs...);
					--j;
				}
			}
		}

		// ヒープの start 以下を end(含む)まで根から下方向に整える
		template<typename TargetType, typename Compare, typename...Types>
		inline void SiftDownComp(TargetType* target, const uint32_t start, const uint32_t end, Compare comp, const bool descending, Types*...arrayPtrs)
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
				if (LessComp(target[swapIdx], target[child], comp, descending))
				{
					swapIdx = child;
				}
				if (child + 1 <= end && LessComp(target[swapIdx], target[child + 1], comp, descending))
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

		// 先頭から count 個をヒープソートする(イントロソートの深さ超過時の退避、O(n log n)・追加確保なし)
		template<typename TargetType, typename Compare, typename...Types>
		inline void HeapSortSegmentComp(TargetType* target, const uint32_t count, Compare comp, const bool descending, Types*...arrayPtrs)
		{
			if (count < 2)
			{
				return;
			}

			for (int64_t start = static_cast<int64_t>(count / 2) - 1; start >= 0; --start)
			{
				SiftDownComp(target, static_cast<uint32_t>(start), count - 1, comp, descending, arrayPtrs...);
			}

			for (uint32_t end = count - 1; end > 0; --end)
			{
				SwapAll(target, 0, end, arrayPtrs...);
				SiftDownComp(target, 0, end - 1, comp, descending, arrayPtrs...);
			}
		}

		// [left, right] 区間のイントロソート(クイックソート+ヒープソート退避+小区間は挿入ソート)。小さい側を再帰し大きい側をループにして再帰深さを抑える
		template<typename TargetType, typename Compare, typename...Types>
		inline void IntroSortComp(TargetType* target, const int64_t left, const int64_t right, int depthLimit, Compare comp, const bool descending, Types*...arrayPtrs)
		{
			int64_t l = left;
			int64_t r = right;

			while (l < r)
			{
				const int64_t range = r - l + 1;
				if (range <= 16)
				{
					InsertionSortComp(target, l, r, comp, descending, arrayPtrs...);
					return;
				}

				if (depthLimit <= 0)
				{
					const uint32_t count = static_cast<uint32_t>(range);
					HeapSortSegmentComp(target + l, count, comp, descending, (arrayPtrs + l)...);
					return;
				}

				--depthLimit;

				const int64_t mid = l + (r - l) / 2;
				const TargetType& a = target[l];
				const TargetType& b = target[mid];
				const TargetType& c = target[r];
				const TargetType pivot = LessComp(a, b, comp, descending)
					? (LessComp(b, c, comp, descending) ? b : (LessComp(a, c, comp, descending) ? c : a))
					: (LessComp(a, c, comp, descending) ? a : (LessComp(b, c, comp, descending) ? c : b));

				int64_t i = l;
				int64_t j = r;
				while (i <= j)
				{
					while (LessComp(target[i], pivot, comp, descending))
					{
						++i;
					}
					while (GreaterComp(target[j], pivot, comp, descending))
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

				const int64_t leftSize = j - l;
				const int64_t rightSize = r - i;
				if (leftSize < rightSize)
				{
					if (l < j)
					{
						IntroSortComp(target, l, j, depthLimit, comp, descending, arrayPtrs...);
					}
					l = i;
				}
				else
				{
					if (i < r)
					{
						IntroSortComp(target, i, r, depthLimit, comp, descending, arrayPtrs...);
					}
					r = j;
				}
			}
		}

		// メンバ変数で比較する挿入ソート([left, right] 区間・安定)
		template<typename TargetType, typename ValueType, typename Compare>
		inline void InsertionSortMemberComp(TargetType* target, const int64_t left, const int64_t right, ValueType TargetType::* memberPtr, Compare comp, const bool descending)
		{
			for (int64_t i = left + 1; i <= right; ++i)
			{
				int64_t j = i;
				while (j > left && LessComp(target[j].*memberPtr, target[j - 1].*memberPtr, comp, descending))
				{
					SwapValue(target[j - 1], target[j]);
					--j;
				}
			}
		}

		// メンバ変数で比較するヒープの下方向整形
		template<typename TargetType, typename ValueType, typename Compare>
		inline void SiftDownMemberComp(TargetType* target, const uint32_t start, const uint32_t end, ValueType TargetType::* memberPtr, Compare comp, const bool descending)
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
				if (LessComp(target[swapIdx].*memberPtr, target[child].*memberPtr, comp, descending))
				{
					swapIdx = child;
				}
				if (child + 1 <= end && LessComp(target[swapIdx].*memberPtr, target[child + 1].*memberPtr, comp, descending))
				{
					swapIdx = child + 1;
				}
				if (swapIdx == root)
				{
					return;
				}
				SwapValue(target[root], target[swapIdx]);
				root = swapIdx;
			}
		}

		// メンバ変数で比較するヒープソート(深さ超過時の退避)
		template<typename TargetType, typename ValueType, typename Compare>
		inline void HeapSortSegmentMemberComp(TargetType* target, const uint32_t count, ValueType TargetType::* memberPtr, Compare comp, const bool descending)
		{
			if (count < 2)
			{
				return;
			}

			for (int64_t start = static_cast<int64_t>(count / 2) - 1; start >= 0; --start)
			{
				SiftDownMemberComp(target, static_cast<uint32_t>(start), count - 1, memberPtr, comp, descending);
			}

			for (uint32_t end = count - 1; end > 0; --end)
			{
				SwapValue(target[0], target[end]);
				SiftDownMemberComp(target, 0, end - 1, memberPtr, comp, descending);
			}
		}

		// メンバ変数で比較するイントロソート([left, right] 区間)
		template<typename TargetType, typename ValueType, typename Compare>
		inline void IntroSortMemberComp(TargetType* target, const int64_t left, const int64_t right, int depthLimit, ValueType TargetType::* memberPtr, Compare comp, const bool descending)
		{
			int64_t l = left;
			int64_t r = right;

			while (l < r)
			{
				const int64_t range = r - l + 1;
				if (range <= 16)
				{
					InsertionSortMemberComp(target, l, r, memberPtr, comp, descending);
					return;
				}

				if (depthLimit <= 0)
				{
					const uint32_t count = static_cast<uint32_t>(range);
					HeapSortSegmentMemberComp(target + l, count, memberPtr, comp, descending);
					return;
				}

				--depthLimit;

				const int64_t mid = l + (r - l) / 2;
				const ValueType& a = target[l].*memberPtr;
				const ValueType& b = target[mid].*memberPtr;
				const ValueType& c = target[r].*memberPtr;
				const ValueType pivot = LessComp(a, b, comp, descending)
					? (LessComp(b, c, comp, descending) ? b : (LessComp(a, c, comp, descending) ? c : a))
					: (LessComp(a, c, comp, descending) ? a : (LessComp(b, c, comp, descending) ? c : b));

				int64_t i = l;
				int64_t j = r;
				while (i <= j)
				{
					while (LessComp(target[i].*memberPtr, pivot, comp, descending))
					{
						++i;
					}
					while (GreaterComp(target[j].*memberPtr, pivot, comp, descending))
					{
						--j;
					}

					if (i <= j)
					{
						SwapValue(target[i], target[j]);
						++i;
						--j;
					}
				}

				const int64_t leftSize = j - l;
				const int64_t rightSize = r - i;
				if (leftSize < rightSize)
				{
					if (l < j)
					{
						IntroSortMemberComp(target, l, j, depthLimit, memberPtr, comp, descending);
					}
					l = i;
				}
				else
				{
					if (i < r)
					{
						IntroSortMemberComp(target, i, r, depthLimit, memberPtr, comp, descending);
					}
					r = j;
				}
			}
		}

		// indices を [left, mid) と [mid, right) の2つの整列済み列として target の値でマージし、temp 経由で indices に戻す(同値は左側優先で安定)
		template<typename TargetType, typename Compare>
		inline void MergeIndicesComp(TargetType* target, uint32_t* indices, uint32_t* temp, const int64_t left, const int64_t mid, const int64_t right, Compare comp, const bool descending)
		{
			int64_t i = left;
			int64_t j = mid;
			int64_t k = left;
			while (i < mid && j < right)
			{
				const TargetType& a = target[indices[i]];
				const TargetType& b = target[indices[j]];
				const bool ab = LessComp(a, b, comp, descending);
				const bool ba = LessComp(b, a, comp, descending);
				if (ab || !ba)
				{
					temp[k++] = indices[i++];
				}
				else
				{
					temp[k++] = indices[j++];
				}
			}
			while (i < mid) temp[k++] = indices[i++];
			while (j < right) temp[k++] = indices[j++];
			for (int64_t t = left; t < right; ++t) indices[t] = temp[t];
		}

		// indices(要素番号の並び)をトップダウンのマージソートで並べ替える。target 自体は動かさない
		template<typename TargetType, typename Compare>
		inline void MergeSortIndicesComp(TargetType* target, uint32_t* indices, uint32_t* temp, const int64_t left, const int64_t right, Compare comp, const bool descending)
		{
			if (right - left <= 1)
			{
				return;
			}
			const int64_t mid = left + (right - left) / 2;
			MergeSortIndicesComp(target, indices, temp, left, mid, comp, descending);
			MergeSortIndicesComp(target, indices, temp, mid, right, comp, descending);
			MergeIndicesComp(target, indices, temp, left, mid, right, comp, descending);
		}

		// indices が示す並びになるよう target と関連配列を置換サイクルで並べ替える(pos は num 個の作業領域。呼び出し側が渡す)
		template<typename TargetType, typename...Types>
		inline void ApplyPermutation(TargetType* target, uint32_t* indices, uint32_t* pos, const uint32_t num, Types*...arrayPtrs)
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

		// ApplyPermutation の関連配列なし版
		template<typename TargetType>
		inline void ApplyPermutationSingle(TargetType* target, uint32_t* indices, uint32_t* pos, const uint32_t num)
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
					SwapValue(target[i], target[j]);
					const uint32_t temp = pos[i];
					pos[i] = pos[j];
					pos[j] = temp;
				}
			}
		}

		// メンバ変数で比較する MergeIndicesComp
		template<typename TargetType, typename ValueType, typename Compare>
		inline void MergeIndicesMemberComp(TargetType* target, uint32_t* indices, uint32_t* temp, const int64_t left, const int64_t mid, const int64_t right, ValueType TargetType::* memberPtr, Compare comp, const bool descending)
		{
			int64_t i = left;
			int64_t j = mid;
			int64_t k = left;
			while (i < mid && j < right)
			{
				const ValueType& a = target[indices[i]].*memberPtr;
				const ValueType& b = target[indices[j]].*memberPtr;
				const bool ab = LessComp(a, b, comp, descending);
				const bool ba = LessComp(b, a, comp, descending);
				if (ab || !ba)
				{
					temp[k++] = indices[i++];
				}
				else
				{
					temp[k++] = indices[j++];
				}
			}
			while (i < mid) temp[k++] = indices[i++];
			while (j < right) temp[k++] = indices[j++];
			for (int64_t t = left; t < right; ++t) indices[t] = temp[t];
		}

		// メンバ変数で比較する MergeSortIndicesComp
		template<typename TargetType, typename ValueType, typename Compare>
		inline void MergeSortIndicesMemberComp(TargetType* target, uint32_t* indices, uint32_t* temp, const int64_t left, const int64_t right, ValueType TargetType::* memberPtr, Compare comp, const bool descending)
		{
			if (right - left <= 1)
			{
				return;
			}
			const int64_t mid = left + (right - left) / 2;
			MergeSortIndicesMemberComp(target, indices, temp, left, mid, memberPtr, comp, descending);
			MergeSortIndicesMemberComp(target, indices, temp, mid, right, memberPtr, comp, descending);
			MergeIndicesMemberComp(target, indices, temp, left, mid, right, memberPtr, comp, descending);
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
		const int depthLimit = detail::DepthLimit(num);
		detail::IntroSortComp(ratgetBegingPtr, 0, static_cast<int64_t>(num - 1), depthLimit, detail::DefaultLess{}, descending, arrayPtrs...);
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
		const int depthLimit = detail::DepthLimit(num);
		detail::IntroSortComp(ratgetBegingPtr, 0, static_cast<int64_t>(num - 1), depthLimit, comp, descending, arrayPtrs...);
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
		const int depthLimit = detail::DepthLimit(num);
		detail::IntroSortMemberComp(targetPtr, 0, static_cast<int64_t>(num - 1), depthLimit, memberPtr, detail::DefaultLess{}, descending);
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
		const int depthLimit = detail::DepthLimit(num);
		detail::IntroSortMemberComp(targetPtr, 0, static_cast<int64_t>(num - 1), depthLimit, memberPtr, comp, descending);
	}

	/// <summary>
	/// 基準配列の値で安定に並べ替え、関連する配列も同じ並びにします。
	/// </summary>
	/// <param name="ratgetBegingPtr">基準配列の先頭ポインタ</param>
	/// <param name="num">要素数</param>
	/// <param name="alloc">作業領域（要素数の2倍の uint32_t）の確保に使うアロケータ。確保できなければ並びを変えずに false を返す</param>
	/// <param name="descending">true で降順（比較結果を反転）</param>
	/// <param name="...arrayPtrs">同時に並べ替える配列の先頭ポインタ群（要素数は num と同じ）</param>
	template<typename TargetType, typename...Types>
	inline bool StableSortArray(TargetType* ratgetBegingPtr, const uint32_t num, const MemoryAllocatorRef& alloc, const bool descending = false, Types*...arrayPtrs)
	{
		if (ratgetBegingPtr == nullptr || num < 2)
		{
			return true;
		}
		// 作業領域(indices と temp で 2*num 個)はアロケータ経由で1回だけ確保する。temp は並べ替えの後、位置の表として使い回す
		detail::SortWorkBuffer work(num, alloc);
		if (work.Get() == nullptr)
		{
			return false;  // 作業領域を確保できなかった（並びは変えていない）
		}
		uint32_t* indices = work.Get();
		uint32_t* temp = indices + num;
		for (uint32_t i = 0; i < num; ++i) indices[i] = i;
		detail::MergeSortIndicesComp(ratgetBegingPtr, indices, temp, 0, static_cast<int64_t>(num), detail::DefaultLess{}, descending);
		detail::ApplyPermutation(ratgetBegingPtr, indices, temp, num, arrayPtrs...);
		return true;
	}

	/// <summary>
	/// 比較関数で安定に並べ替え、関連する配列も同じ並びにします。
	/// </summary>
	/// <param name="ratgetBegingPtr">基準配列の先頭ポインタ</param>
	/// <param name="num">要素数</param>
	/// <param name="alloc">作業領域（要素数の2倍の uint32_t）の確保に使うアロケータ。確保できなければ並びを変えずに false を返す</param>
	/// <param name="comp">比較関数（a が b より前に並ぶとき true）</param>
	/// <param name="descending">true で降順（comp の順序を反転）</param>
	/// <param name="...arrayPtrs">同時に並べ替える配列の先頭ポインタ群（要素数は num と同じ）</param>
	template<typename TargetType, typename Compare, typename...Types>
	inline bool StableSortArrayCmp(TargetType* ratgetBegingPtr, const uint32_t num, const MemoryAllocatorRef& alloc, Compare comp, const bool descending = false, Types*...arrayPtrs)
	{
		if (ratgetBegingPtr == nullptr || num < 2)
		{
			return true;
		}
		// 作業領域(indices と temp で 2*num 個)はアロケータ経由で1回だけ確保する。temp は並べ替えの後、位置の表として使い回す
		detail::SortWorkBuffer work(num, alloc);
		if (work.Get() == nullptr)
		{
			return false;  // 作業領域を確保できなかった（並びは変えていない）
		}
		uint32_t* indices = work.Get();
		uint32_t* temp = indices + num;
		for (uint32_t i = 0; i < num; ++i) indices[i] = i;
		detail::MergeSortIndicesComp(ratgetBegingPtr, indices, temp, 0, static_cast<int64_t>(num), comp, descending);
		detail::ApplyPermutation(ratgetBegingPtr, indices, temp, num, arrayPtrs...);
		return true;
	}

	/// <summary>
	/// 指定したメンバ変数の値で安定に並べ替えます。
	/// </summary>
	/// <param name="targetPtr">配列の先頭ポインタ</param>
	/// <param name="num">要素数</param>
	/// <param name="alloc">作業領域（要素数の2倍の uint32_t）の確保に使うアロケータ。確保できなければ並びを変えずに false を返す</param>
	/// <param name="memberPtr">比較に使うメンバ変数へのポインタ</param>
	/// <param name="descending">true で降順（比較結果を反転）</param>
	template<typename TargetType, typename ValueType>
	inline bool StableSortMember(TargetType* targetPtr, const uint32_t num, const MemoryAllocatorRef& alloc, ValueType TargetType::* memberPtr, const bool descending = false)
	{
		if (targetPtr == nullptr || num < 2)
		{
			return true;
		}
		// 作業領域(indices と temp で 2*num 個)はアロケータ経由で1回だけ確保する。temp は並べ替えの後、位置の表として使い回す
		detail::SortWorkBuffer work(num, alloc);
		if (work.Get() == nullptr)
		{
			return false;  // 作業領域を確保できなかった（並びは変えていない）
		}
		uint32_t* indices = work.Get();
		uint32_t* temp = indices + num;
		for (uint32_t i = 0; i < num; ++i) indices[i] = i;
		detail::MergeSortIndicesMemberComp(targetPtr, indices, temp, 0, static_cast<int64_t>(num), memberPtr, detail::DefaultLess{}, descending);
		detail::ApplyPermutationSingle(targetPtr, indices, temp, num);
		return true;
	}

	/// <summary>
	/// 指定したメンバ変数と比較関数で安定に並べ替えます。
	/// </summary>
	/// <param name="targetPtr">配列の先頭ポインタ</param>
	/// <param name="num">要素数</param>
	/// <param name="alloc">作業領域（要素数の2倍の uint32_t）の確保に使うアロケータ。確保できなければ並びを変えずに false を返す</param>
	/// <param name="memberPtr">比較に使うメンバ変数へのポインタ</param>
	/// <param name="comp">比較関数（a が b より前に並ぶとき true）</param>
	/// <param name="descending">true で降順（comp の順序を反転）</param>
	template<typename TargetType, typename ValueType, typename Compare>
	inline bool StableSortMemberCmp(TargetType* targetPtr, const uint32_t num, const MemoryAllocatorRef& alloc, ValueType TargetType::* memberPtr, Compare comp, const bool descending = false)
	{
		if (targetPtr == nullptr || num < 2)
		{
			return true;
		}
		// 作業領域(indices と temp で 2*num 個)はアロケータ経由で1回だけ確保する。temp は並べ替えの後、位置の表として使い回す
		detail::SortWorkBuffer work(num, alloc);
		if (work.Get() == nullptr)
		{
			return false;  // 作業領域を確保できなかった（並びは変えていない）
		}
		uint32_t* indices = work.Get();
		uint32_t* temp = indices + num;
		for (uint32_t i = 0; i < num; ++i) indices[i] = i;
		detail::MergeSortIndicesMemberComp(targetPtr, indices, temp, 0, static_cast<int64_t>(num), memberPtr, comp, descending);
		detail::ApplyPermutationSingle(targetPtr, indices, temp, num);
		return true;
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
	/// <param name="alloc">作業領域（要素数の2倍の uint32_t）の確保に使うアロケータ。確保できなければ並びを変えずに false を返す</param>
	/// <param name="descending">true で降順（比較結果を反転）</param>
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
	/// <param name="alloc">作業領域（要素数の2倍の uint32_t）の確保に使うアロケータ。確保できなければ並びを変えずに false を返す</param>
	/// <param name="comp">比較関数（a が b より前に並ぶとき true）</param>
	/// <param name="descending">true で降順（comp の順序を反転）</param>
	template<typename T, typename Compare>
	inline bool StableSort(T* beginPtr, const uint32_t num, const MemoryAllocatorRef& alloc, Compare comp, const bool descending = false)
	{
		return StableSortArrayCmp(beginPtr, num, alloc, comp, descending);
	}
}
