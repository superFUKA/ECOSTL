#pragma once
#include <cstdint>

namespace utl
{
	/// <summary>
	/// AlignedAllocateで確保するときの最小のアライメント
	/// アライメントを指定しなくても、この境界には合わせる
	/// </summary>
	inline constexpr uint32_t StandardMinAlign = 16;

	/// <summary>
	/// アライメントを合わせてメモリを確保する
	/// alignは2の累乗（0ならStandardMinAlign）。確保できなければnullptrを返す
	/// 確保したメモリは必ずAlignedFreeで開放する（freeでは開放できない）
	/// </summary>
	/// <param name="size">確保するバイト数</param>
	/// <param name="align">合わせる境界のバイト数</param>
	/// <returns></returns>
	[[nodiscard]] void* AlignedAllocate(uint32_t size, uint32_t align = 0);

	/// <summary>
	/// AlignedAllocateで確保したメモリを開放する
	/// nullptrなら何もしない
	/// </summary>
	/// <param name="allocated"></param>
	void AlignedFree(void* allocated);
}
