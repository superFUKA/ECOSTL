#pragma once
// 区画ごとの出力の場所。並列の段の中で「個数が前もって分からない結果」を、確保せずに集めるために使う。
//
// 考え方：
//  - 区画（段の仕事 1 つ）ごとに、結果を書く場所を 1 つ持つ。段の中では確保しない（確保はロックや失敗の元になるため）
//  - 仕事は、場所に入る分だけ書き、入らなくても個数は最後まで数えて、必要だった個数を SetNeeded で知らせる
//  - 段が終わったら、呼び出したスレッドで GrowOverflowedParts を呼ぶ。足りなかった区画の場所を広げて、その番号の一覧を作る
//  - 一覧にある区画だけ、もう一度同じ仕事をする（2 回目は必ず入る）
//  - 場所は持ち続けて使い回すので、結果の個数が落ち着けば、やり直しも確保も起きない
//
//   utl::PartBuffers<uint32_t> buffers(allocator);
//   if (!buffers.Reset(partCount)) { /* 確保できなかった */ }
//
//   // 段（区画ごとに並列）
//   uint32_t* out = buffers.BufferOfPart(part);
//   const uint32_t capacity = buffers.CapacityOfPart(part);
//   uint32_t found = 0;
//   for (...) { if (found < capacity) { out[found] = value; } ++found; }
//   buffers.SetNeeded(part, found);
//
//   // 段の後（呼び出したスレッドで）
//   if (!buffers.GrowOverflowedParts()) { /* 確保できなかった */ }
//   for (const uint32_t part : buffers.OverflowedParts()) { /* 区画 part の仕事をやり直す（段にしてもよい） */ }
//
//   // 区画 part の結果は BufferOfPart(part) の先頭から NeededOfPart(part) 個
#include <cstdint>

#include "ECOSTL/Container/Array/View/ArrayView.h"
#include "ECOSTL/Container/Vector/Vector.h"
#include "ECOSTL/Logger/Log.h"
#include "ECOSTL/Memory/Allocator/MemoryAllocator.h"

// ecoSTL の共通名前空間
namespace utl
{
	/// <summary>
	/// 区画ごとの出力の場所（段の中で確保しない可変長の出力）
	/// </summary>
	/// <typeparam name="Type">結果の要素の型（既定のコンストラクタで作れること）</typeparam>
	template<typename Type>
	class PartBuffers
	{
	public:
		/// <param name="allocator">場所の確保に使うアロケータ（所有しない。nullptr なら既定のアロケータ）</param>
		explicit PartBuffers(MemoryAllocatorDelegate* allocator = nullptr)
			: allocator_(allocator)
			, buffers_(allocator)
			, needed_(allocator)
			, overflowedParts_(allocator)
		{
		}

		/// <summary>
		/// 区画の個数を決めて、集め始める。前に広げた場所はそのまま使い回す（区画が減っても消さない）
		/// </summary>
		/// <returns>確保できなければ false（そのときは区画が 0 個になる）</returns>
		[[nodiscard]] bool Reset(const uint32_t partCount)
		{
			partCount_ = 0;
			overflowedParts_.Clear();
			bool succeeded = needed_.Resize(partCount) && buffers_.ReserveSlack(partCount) && overflowedParts_.ReserveSlack(partCount);
			// 区画が増えた分だけ、場所（まだ空）を足す
			while (succeeded && buffers_.Size() < partCount)
			{
				succeeded = buffers_.EmplaceBack(allocator_) != nullptr;
			}
			if (!succeeded)
			{
				UTL_LOG(Warn | ::utl::logtype::Container, "区画ごとの出力の場所を確保できなかった partCount={partCount}", partCount);
				return false;
			}
			for (uint32_t part = 0; part < partCount; ++part)
			{
				needed_[part] = 0;
			}
			partCount_ = partCount;
			return true;
		}

		/// <summary>
		/// 区画を 0 個にする（場所は持ち続ける）
		/// </summary>
		void Clear()
		{
			partCount_ = 0;
			overflowedParts_.Clear();
		}

		uint32_t PartCount()const
		{
			return partCount_;
		}

		/// <summary>
		/// 区画 part の結果を書く場所。自分の区画の場所だけを触るなら、他の区画と並列に書いてよい
		/// </summary>
		Type* BufferOfPart(const uint32_t part)
		{
			return buffers_[part].Begin();
		}
		const Type* BufferOfPart(const uint32_t part)const
		{
			return buffers_[part].Begin();
		}

		/// <summary>
		/// 区画 part の場所に、いま入る個数
		/// </summary>
		uint32_t CapacityOfPart(const uint32_t part)const
		{
			return buffers_[part].Size();
		}

		/// <summary>
		/// 区画 part の仕事が、必要だった個数（入らなかった分も含む）を知らせる。仕事の最後に 1 回呼ぶ
		/// </summary>
		void SetNeeded(const uint32_t part, const uint32_t needed)
		{
			needed_[part] = needed;
		}

		/// <summary>
		/// 区画 part の、必要だった個数。場所が足りていれば、これが結果の個数
		/// </summary>
		uint32_t NeededOfPart(const uint32_t part)const
		{
			return needed_[part];
		}

		/// <summary>
		/// 全ての区画の、必要だった個数の合計
		/// </summary>
		uint64_t CountNeeded()const
		{
			uint64_t total = 0;
			for (uint32_t part = 0; part < partCount_; ++part)
			{
				total += needed_[part];
			}
			return total;
		}

		/// <summary>
		/// 段が終わった後に、呼び出したスレッドで呼ぶ。場所が足りなかった区画を広げて、その番号の一覧（OverflowedParts）を作り直す
		/// 少し増えるたびにやり直さないよう、必要な個数より 4 分の 1 だけ多く取る
		/// </summary>
		/// <returns>広げられなければ false</returns>
		[[nodiscard]] bool GrowOverflowedParts()
		{
			overflowedParts_.Clear();
			for (uint32_t part = 0; part < partCount_; ++part)
			{
				const uint32_t needed = needed_[part];
				if (needed <= buffers_[part].Size())
				{
					continue;
				}
				const uint64_t grown = static_cast<uint64_t>(needed) + needed / 4 + 16;
				const uint32_t newSize = grown > UINT32_MAX ? UINT32_MAX : static_cast<uint32_t>(grown);
				if (!buffers_[part].Resize(newSize))
				{
					UTL_LOG(Warn | ::utl::logtype::Container, "区画の出力の場所を広げられなかった part={part} needed={needed}", part, needed);
					return false;
				}
				// Reset で場所を取ってあるので、ここでは確保しない
				overflowedParts_.PushBack(part);
			}
			return true;
		}

		/// <summary>
		/// 直前の GrowOverflowedParts で広げた区画の番号（小さい順）。これらの区画は、仕事をやり直す
		/// </summary>
		ArrayView<uint32_t> OverflowedParts()const
		{
			return ArrayView<uint32_t>(overflowedParts_.Begin(), overflowedParts_.Size());
		}

	private:
		MemoryAllocatorDelegate* allocator_;
		uint32_t partCount_ = 0;
		// 区画ごとの場所（要素数が、いま入る個数）
		Vector<Vector<Type>> buffers_;
		// 区画ごとの、必要だった個数
		Vector<uint32_t> needed_;
		// 場所を広げた区画の番号
		Vector<uint32_t> overflowedParts_;
	};
}
