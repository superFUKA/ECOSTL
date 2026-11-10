#pragma once
// 並んだ count 個を、先頭から partSize 個ずつの「区画」に切る計算。
// 段（WorkStage）の仕事 1 つに区画 1 つを受け持たせるときに使う。
//
//   stage.count = utl::CountParts(count, PART_SIZE);            // 段の仕事の数
//   // 仕事 part の中で
//   const utl::PartRange range = utl::MakePartRange(part, PART_SIZE, count);
//   for (uint32_t i = range.begin; i < range.end; ++i) { ... }
//
// 区画の切り方を個数だけで決めるので、スレッド数によらず同じ分け方になる（結果を決定的にしやすい）。
#include <cstdint>

// ecoSTL の共通名前空間
namespace utl
{
	/// <summary>
	/// 区画 1 つが受け持つ範囲 [begin, end)
	/// </summary>
	struct PartRange
	{
		uint32_t begin = 0;
		uint32_t end = 0;

		constexpr uint32_t Size()const
		{
			return end - begin;
		}
	};

	/// <summary>
	/// count 個を partSize 個ずつに切ったときの区画の個数（最後の区画は partSize 個より少ないことがある）
	/// </summary>
	/// <param name="partSize">区画 1 つの個数（1 以上）</param>
	constexpr uint32_t CountParts(const uint32_t count, const uint32_t partSize)
	{
		return count == 0 ? 0 : (count - 1) / partSize + 1;
	}

	/// <summary>
	/// part 番目の区画が受け持つ範囲。part は CountParts(count, partSize) 未満
	/// </summary>
	constexpr PartRange MakePartRange(const uint32_t part, const uint32_t partSize, const uint32_t count)
	{
		const uint32_t begin = part * partSize;
		// begin + partSize が 32 ビットを超えないよう、残りの個数と比べる
		const uint32_t end = (count - begin > partSize) ? begin + partSize : count;
		return PartRange{ begin, end };
	}
}
