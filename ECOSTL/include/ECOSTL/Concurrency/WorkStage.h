#pragma once
// 並列にできる仕事の 1 段(WorkStage)。実行器(スレッドプール等)には依存しない、仕事の記述だけのデータ。
//  - 1 段は count 個の互いに独立した仕事からなる。仕事 i は body(ctx, i) で実行する。
//  - 段の中の仕事は、どの順に・どのスレッドで実行してもよい。全て終わってから、次の段へ進む(段の間が依存関係)。
//  - 仕事を作る側(例: ECS)は段を返すだけで、実行は使う側がスレッドプール等と組み合わせて行う。
#include <cstdint>

// ecoSTL の共通名前空間
namespace utl
{
	/// <summary>
	/// 並列にできる仕事の 1 段
	/// </summary>
	struct WorkStage
	{
		using Body = void (*)(void* ctx, uint32_t index);

		uint32_t count = 0;   // 仕事の数
		Body body = nullptr;  // 仕事 1 つを実行する関数
		void* ctx = nullptr;  // body に渡すデータ(所有しない。段を返した側が、段が終わるまで生かす)

		/// <summary>呼び出したスレッドで、仕事を 0..count-1 の順に全て実行する</summary>
		void RunSerial()const
		{
			for (uint32_t index = 0; index < count; ++index)
			{
				body(ctx, index);
			}
		}
	};

	/// <summary>
	/// 段の並びを返す操作(Next(WorkStage&) を持つ型)を、呼び出したスレッドで最後まで実行する
	/// 操作は Next で次の段を返し(段の間の直列の処理は Next の中で行う)、終わったら false を返すこと
	/// </summary>
	template<typename Operation>
	void RunStagesSerial(Operation& operation)
	{
		WorkStage stage;
		while (operation.Next(stage))
		{
			stage.RunSerial();
		}
	}
}
