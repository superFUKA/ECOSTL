#pragma once
// 決まった回数だけ確保に成功し、その後は失敗する（nullptr を返す）アロケータ。
// 「確保できなかったとき」の動きを試すテストのための道具。
//
//   utl::FailingAllocator failing(2);             // 2 回までは成功、3 回目から失敗（中身は既定のアロケータ）
//   utl::Vector<int> v(failing.GetAPI());
//   EXPECT_FALSE(...);                            // 3 回目の確保が要る操作が、失敗を正しく伝えるかを確かめる
//
// 何回目の確保で失敗させても壊れないことを確かめるには、成功の回数を 0 から順に増やして同じ操作を繰り返す。
#include <cstdint>
#include "MemoryAllocator.h"

// 汎用ユーティリティ名前空間
namespace utl
{
	/// <summary>
	/// 決まった回数だけ確保に成功し、その後は失敗するアロケータ。開放はいつでもできる
	/// 回数は普通の変数で数えるので、複数のスレッドから同時に確保しないこと
	/// </summary>
	class FailingAllocator
	{
	public:
		/// <param name="successCount">成功させる確保の回数（0 なら最初から失敗）</param>
		/// <param name="inner">実際に確保するアロケータ(所有しない)。nullptr なら既定のアロケータ（MemoryAllocatorRef と同じ）</param>
		explicit FailingAllocator(const uint32_t successCount, MemoryAllocatorDelegate* inner = nullptr)
			: inner_(inner)
			, remaining_(successCount)
		{
			api_.Register<FailingAllocator, &FailingAllocator::Allocate, &FailingAllocator::Free>(this);
		}

		// 自分自身を指す API を持つので、コピー・ムーブはしない
		FailingAllocator(const FailingAllocator&) = delete;
		FailingAllocator& operator=(const FailingAllocator&) = delete;

		/// <summary>
		/// コンテナなどに渡す API。このアロケータが生きている間だけ使える
		/// </summary>
		MemoryAllocatorDelegate* GetAPI()
		{
			return &api_;
		}

		/// <summary>
		/// あと何回、確保に成功するか
		/// </summary>
		uint32_t Remaining()const
		{
			return remaining_;
		}

		/// <summary>
		/// 成功させる確保の回数を決め直す（ここから数え直す）
		/// </summary>
		void SetRemaining(const uint32_t successCount)
		{
			remaining_ = successCount;
		}

		/// <summary>
		/// 失敗させた確保の回数
		/// </summary>
		uint32_t FailedCount()const
		{
			return failedCount_;
		}

		// MemoryAllocatorDelegate から呼ばれる確保
		void* Allocate(const uint32_t size, const uint32_t align)
		{
			if (remaining_ == 0)
			{
				++failedCount_;
				return nullptr;
			}
			--remaining_;
			return inner_.Allocate(size, align);
		}

		// MemoryAllocatorDelegate から呼ばれる開放
		bool Free(void* memory)
		{
			return inner_.Free(memory);
		}

	private:
		MemoryAllocatorRef inner_;     // 実際に確保するアロケータ
		MemoryAllocatorDelegate api_;  // 自分を登録した API
		uint32_t remaining_;           // あと何回成功するか
		uint32_t failedCount_ = 0;     // 失敗させた回数
	};
}
