#pragma once
// 並列でも順が変わらない振り分け。count 個の要素を「バケツ」（0 〜 bucketCount-1 の番号）ごとにまとめて並べ直すときの、
// 書く位置の表を受け持つ。並べ直した結果は、バケツの番号の順、同じバケツの中では元の番号の順（安定）。
// 実行はしない。使う側が、区画ごとの仕事を段（WorkStage）にして、好きな実行器で動かす。
//
// 流れ（区画 = 元の列を先頭から partSize 個ずつに切ったもの）：
//   utl::BucketPartition partition(allocator);
//   if (!partition.Reset(count, PART_SIZE, bucketCount)) { /* 確保できなかった */ }
//
//   // 段 1（区画ごとに並列）：バケツごとの個数を数える
//   uint32_t* counts = partition.BeginCount(part);
//   const utl::PartRange range = partition.RangeOfPart(part);
//   for (uint32_t i = range.begin; i < range.end; ++i) { ++counts[BucketOf(i)]; }
//
//   // 段の間（呼び出したスレッドで）：書く位置を割り振る
//   partition.AssignPositions();
//
//   // 段 2（区画ごとに並列）：割り振られた位置へ書く
//   uint32_t* cursors = partition.CursorsOfPart(part);
//   for (uint32_t i = range.begin; i < range.end; ++i) { out[cursors[BucketOf(i)]++] = in[i]; }
//
//   // バケツ b の要素は out の [BucketBegin(b), BucketEnd(b)) にある
//
// 決まりごと：
//  - 区画が書く位置は、バケツの順・同じバケツの中では区画の順に割り振る。区画どうしで書く場所が重ならないので並列に書けて、
//    結果はスレッド数・実行の順によらず同じ
//  - 表の大きさは 区画の個数 × バケツの個数。バケツの個数は小さく保つ（多いときは、番号の上位で大きく分けてから、その中を並べる）
//  - 作業領域は持ち続けて使い回す。前より大きい表が要るときだけ確保する
#include <cstdint>

#include "ECOSTL/Concurrency/PartRange.h"
#include "ECOSTL/Container/Vector/Vector.h"
#include "ECOSTL/Logger/Log.h"
#include "ECOSTL/Memory/Allocator/MemoryAllocator.h"

// ecoSTL の共通名前空間
namespace utl
{
	/// <summary>
	/// 並列でも順が変わらない振り分けの、書く位置の表
	/// </summary>
	class BucketPartition
	{
	public:
		/// <param name="allocator">表の確保に使うアロケータ（所有しない。nullptr なら既定のアロケータ）</param>
		explicit BucketPartition(MemoryAllocatorDelegate* allocator = nullptr)
			: cursors_(allocator)
			, bucketBegins_(allocator)
		{
		}

		/// <summary>
		/// 振り分けを始める。続けて 数える段 → AssignPositions → 書く段 の順に進める
		/// </summary>
		/// <param name="count">要素の個数（0 でもよい）</param>
		/// <param name="partSize">区画 1 つの個数（1 以上）</param>
		/// <param name="bucketCount">バケツの個数（1 以上）</param>
		/// <returns>表を確保できなければ false（そのときは区画が 0 個になり、何も振り分けない）</returns>
		[[nodiscard]] bool Reset(const uint32_t count, const uint32_t partSize, const uint32_t bucketCount)
		{
			count_ = 0;
			partSize_ = partSize;
			partCount_ = 0;
			bucketCount_ = bucketCount;
			const uint32_t partCount = CountParts(count, partSize);
			const uint64_t tableSize = static_cast<uint64_t>(partCount) * bucketCount;
			if (tableSize > UINT32_MAX || !cursors_.Resize(static_cast<uint32_t>(tableSize)) || !bucketBegins_.Resize(bucketCount + 1))
			{
				UTL_LOG(Warn | ::utl::logtype::Container, "振り分けの表を確保できなかった count={count} partCount={partCount} bucketCount={bucketCount}", count, partCount, bucketCount);
				bucketCount_ = 0;
				return false;
			}
			for (uint32_t bucket = 0; bucket <= bucketCount; ++bucket)
			{
				bucketBegins_[bucket] = 0;
			}
			count_ = count;
			partCount_ = partCount;
			return true;
		}

		uint32_t Count()const
		{
			return count_;
		}
		uint32_t PartCount()const
		{
			return partCount_;
		}
		uint32_t BucketCount()const
		{
			return bucketCount_;
		}

		/// <summary>
		/// 区画 part が受け持つ、元の列の範囲
		/// </summary>
		PartRange RangeOfPart(const uint32_t part)const
		{
			return MakePartRange(part, partSize_, count_);
		}

		/// <summary>
		/// 数える段で、区画 part の仕事が最初に呼ぶ。0 にした個数の欄（バケツの個数ぶん）を返すので、要素ごとに counts[バケツ] を 1 増やす
		/// 自分の区画の欄だけを触るので、他の区画と並列に呼んでよい
		/// </summary>
		uint32_t* BeginCount(const uint32_t part)
		{
			uint32_t* counts = cursors_.Begin() + part * bucketCount_;
			for (uint32_t bucket = 0; bucket < bucketCount_; ++bucket)
			{
				counts[bucket] = 0;
			}
			return counts;
		}

		/// <summary>
		/// 数える段が全て終わった後、書く段の前に 1 回呼ぶ。バケツの順・同じバケツの中では区画の順に、書く位置を割り振る
		/// </summary>
		void AssignPositions()
		{
			uint32_t next = 0;
			for (uint32_t bucket = 0; bucket < bucketCount_; ++bucket)
			{
				bucketBegins_[bucket] = next;
				for (uint32_t part = 0; part < partCount_; ++part)
				{
					uint32_t& cursor = cursors_[part * bucketCount_ + bucket];
					const uint32_t countInPart = cursor;
					cursor = next;
					next += countInPart;
				}
			}
			bucketBegins_[bucketCount_] = next;
		}

		/// <summary>
		/// 書く段で、区画 part の仕事が使う。cursors[バケツ] が次に書く位置で、書くたびに 1 つ進める
		/// 数えたときと同じ要素を、元の番号の順に、同じバケツへ書くこと
		/// </summary>
		uint32_t* CursorsOfPart(const uint32_t part)
		{
			return cursors_.Begin() + part * bucketCount_;
		}

		/// <summary>
		/// バケツ bucket の要素が並ぶ範囲の始まり（AssignPositions の後で使える。bucket は BucketCount() 以下で、BucketCount() なら全体の個数）
		/// </summary>
		uint32_t BucketBegin(const uint32_t bucket)const
		{
			return bucketBegins_[bucket];
		}

		/// <summary>
		/// バケツ bucket の要素が並ぶ範囲の終わり
		/// </summary>
		uint32_t BucketEnd(const uint32_t bucket)const
		{
			return bucketBegins_[bucket + 1];
		}

	private:
		uint32_t count_ = 0;
		uint32_t partSize_ = 1;
		uint32_t partCount_ = 0;
		uint32_t bucketCount_ = 0;
		// [区画 × バケツ]：数えるときは個数、書くときは次に書く位置
		Vector<uint32_t> cursors_;
		// バケツごとの始まりの位置（要素数はバケツの個数 + 1）
		Vector<uint32_t> bucketBegins_;
	};
}
