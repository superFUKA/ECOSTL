#pragma once
#include "../Vector/Vector.h"

namespace utl
{
	/// <summary>
	/// まばらなキー
	/// </summary>
	using SparseKey = uint32_t;

	/// <summary>
	/// 密集した添え字
	/// </summary>
	using DenceIndex = uint32_t;

	// NullのDenceKey,DenceIndexを表す定数
	constexpr uint32_t NULL_VALUE = UINT32_MAX;


	class SparseSet
	{
	public:
		// 作成
		//----------------------

		SparseSet(MemoryAllocatorRef allocator)
			: ketToIndex_(allocator)
			, indexToKey_(allocator)
			, size_(0)
		{
		}


	private:

		// キーと添え字を紐づけるコンテナ
		Vector<DenceIndex> ketToIndex_;
		// 添え字とキーを紐づけるコンテナ
		Vector<SparseKey> indexToKey_;
		// 登録したキーの数
		uint32_t size_;
	};

}