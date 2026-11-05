#pragma once
// ECOSTL・ECOECS がログを出すためのマクロ。ECOSTL のコードはこのマクロだけを使い、ロガーの実装を直接読み込まない。
// 中身は ECOCppLogger（ECOSTL の依存先）の ECO_LOG_* にそのまま転送する。
//
//   UTL_LOG_TRACE("vector grow {old} -> {new}", oldCapacity, newCapacity);   // 書式は ECOCppLogger と同じ（名前付きの {}）
//   UTL_LOG_DEBUG / UTL_LOG_INFO / UTL_LOG_WARN / UTL_LOG_ERROR / UTL_LOG_FATAL
//   UTL_ASSERT(cond, "書式", 引数...)   // 失敗したらログ・スタック・最近の値を残す（ECO_ASSERT）
//   UTL_LOG(Debug | utl::logtype::Memory, "書式", 引数...)   // 型を付けたログ（見るときに --type memory で絞れる）
//   UTL_LOG_ONCE(Warn | utl::logtype::Memory, "書式", 引数...) // 最初の1回だけ
//
// ログの型（見るときの絞り込みに使う）：
//   memory    : 確保・開放（Trace）、確保の失敗・大きさのあふれ（Error）、既定のアロケータの使用（最初の1回だけ Info）
//   container : コンテナの作業領域が確保できずに処理をあきらめた所など（Warn）
//
// - 呼び出し位置ごとに静的な情報を持ち、実行時は引数を書き写すだけ（ECOCppLogger の高速な経路）。
// - 無効にするには、ロガー側の設定 ECO_LOG_ENABLE=0（引数も評価しない）か ECO_LOG_COMPILE_LEVEL を使う。
// - ECOSTL → ECOCppLogger の一方向の依存。ロガーは ECOSTL を使わない（確保のログでロガーが ECOSTL のアロケータを使うと再入するため）。
// - 継承・仮想関数は使わない。出力先や始め方（lg::start・lg::writeOut）は、使う側（ゲーム）が決める。
#include "ECOCppLogger/ECOCppLogger.h"

#define UTL_LOG_TRACE(...) ECO_LOG_TRACE(__VA_ARGS__)
#define UTL_LOG_DEBUG(...) ECO_LOG_DEBUG(__VA_ARGS__)
#define UTL_LOG_INFO(...) ECO_LOG_INFO(__VA_ARGS__)
#define UTL_LOG_WARN(...) ECO_LOG_WARN(__VA_ARGS__)
#define UTL_LOG_ERROR(...) ECO_LOG_ERROR(__VA_ARGS__)
#define UTL_LOG_FATAL(...) ECO_LOG_FATAL(__VA_ARGS__)
#define UTL_ASSERT(...) ECO_ASSERT(__VA_ARGS__)
// MSVC の従来のプリプロセッサは __VA_ARGS__ を1つの引数として渡すため、展開をもう1段挟んで引数に分け直す
#define UTL_LOG_EXPAND_(x) x
#define UTL_LOG(...) UTL_LOG_EXPAND_(ECO_LOG(__VA_ARGS__))
#define UTL_LOG_ONCE(...) UTL_LOG_EXPAND_(ECO_LOG_ONCE(__VA_ARGS__))

namespace utl::logtype
{
	ECO_LOG_TYPE(Memory, "memory");
	ECO_LOG_TYPE(Container, "container");
}
