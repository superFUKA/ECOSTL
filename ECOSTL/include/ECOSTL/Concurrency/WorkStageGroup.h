#pragma once
// 互いに独立した段(WorkStage)をいくつか、1 つの段にまとめる道具。
//  - 例：資源を作る段と、写し元に中身を書く段が互いに独立なら、同じ実行器で 1 つの並びとして取り合わせたい。
//  - まとめた段の仕事の番号は、足した順に段の仕事の番号を続けて振る(段 0 の 0..count0-1、段 1 の 0..count1-1 …)。
//    どの順に・どのスレッドで実行してもよいのは、元の段と同じ。
//  - 実行器(スレッドプール等)は持たない(WorkStage と同じく、仕事の記述だけのデータ)。グローバルな状態も確保も無い。
//  - 段は値で写して持つので、元の WorkStage の変数は捨ててよい(段の ctx が指す物は、元の段の決まりどおり終わるまで生かす)。
//  - まとめるのは独立な段だけ。段の間に順番(前の段の結果を後の段が使う)があるなら、まとめずに順に実行する。
//
//   utl::WorkStageGroup group;
//   group.Add(stageA);
//   group.Add(stageB);
//   const utl::WorkStage all = group.GetWork();  // all.count == stageA.count + stageB.count
//   all.RunSerial();                             // または実行器に渡す(group は終わるまで生かし、動かさない)
#include <cstdint>

#include "ECOSTL/Concurrency/WorkStage.h"

// ecoSTL の共通名前空間
namespace utl
{
	/// <summary>
	/// 互いに独立した段をまとめた 1 つの段
	/// </summary>
	class WorkStageGroup
	{
	public:
		/// <summary>
		/// まとめられる段の数の上限(仕事 1 つごとに段を前から探すので、少ない数を想定する)
		/// </summary>
		static constexpr uint32_t MAX_STAGES = 8;

		/// <summary>
		/// 段を足す(仕事が 0 個の段も足せる。何もしない)
		/// </summary>
		/// <returns>段の数の上限を超えた・仕事があるのに関数が無い・仕事の数の和が uint32 を超えるなら false(足さない)</returns>
		bool Add(const WorkStage& stage)
		{
			if (stageCount_ == MAX_STAGES || (stage.count != 0 && stage.body == nullptr) || stage.count > UINT32_MAX - total_)
			{
				return false;
			}
			stages_[stageCount_++] = stage;
			total_ += stage.count;
			return true;
		}

		/// <summary>
		/// まとめた段。この WorkStageGroup を、終わるまで生かし、動かさない(ctx が this を指す)。実行している間に Add・Clear しない
		/// </summary>
		WorkStage GetWork()const
		{
			WorkStage stage;
			stage.count = total_;
			stage.body = &WorkStageGroup::Run;
			stage.ctx = const_cast<WorkStageGroup*>(this);
			return stage;
		}

		/// <summary>足した段の数</summary>
		uint32_t GetStageCount()const { return stageCount_; }
		/// <summary>まとめた仕事の数(足した段の count の和)</summary>
		uint32_t GetTotalCount()const { return total_; }

		/// <summary>段を空にする(次のまとまりに使い回す)</summary>
		void Clear()
		{
			stageCount_ = 0;
			total_ = 0;
		}

	private:
		static void Run(void* ctx, uint32_t index)
		{
			const WorkStageGroup& group = *static_cast<const WorkStageGroup*>(ctx);
			for (uint32_t s = 0; s < group.stageCount_; ++s)
			{
				const WorkStage& stage = group.stages_[s];
				if (index < stage.count)
				{
					stage.body(stage.ctx, index);
					return;
				}
				index -= stage.count;
			}
		}

		WorkStage stages_[MAX_STAGES];
		uint32_t stageCount_ = 0;
		uint32_t total_ = 0;
	};
}
