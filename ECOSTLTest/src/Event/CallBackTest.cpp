// ecoSTL ChainCallBack の試験（旧 testGameBench の CallbackBench の試験部分）。
// 二重登録で循環しない・個別解除できる・登録順・複数スレッドから並行に Call。
#include <gtest/gtest.h>

#include <atomic>
#include <cstdint>
#include <thread>
#include <vector>

#include "ECOSTL/Event/CallBack/CallBack.h"

namespace {
struct Ev { uint32_t a, b; };   // 試験用のイベント（a / b は中身に意味のないペイロード）
using CB = utl::ChainCallBack<Ev>;
using Holder = utl::ChainCallBackHolder<Ev>;

// 受け手が呼ばれた回数（n）と呼ばれた順（order / orderN）を記録する。
struct Rec { int n[8] = {}; int order[64]; int orderN = 0; };
// 番号 I の受け手。呼ばれた回数と順序を Rec に記録する。
template <int I>
void On(void* u, const Ev&) { Rec* r = static_cast<Rec*>(u); ++r->n[I]; if (r->orderN < 64) r->order[r->orderN++] = I; }

// 二重登録で循環しない・個別解除できる・登録順に呼ばれる、といった振る舞いを確かめる。
TEST(ChainCallBack, Behavior) {
    const Ev ev{ 1, 2 };
    {   // 二重登録（1つだけ → 自己ループだった）
        Rec r; Holder h; CB a(&r, &On<0>);
        h.Register(&a);
        const bool again = h.Register(&a);
        h.Call(ev);
        EXPECT_TRUE(!again && r.n[0] == 1 && h.Count() == 1) << "二重登録: 1つの受け手を2回登録しても1回だけ呼ばれる";
    }
    {   // 二重登録（先頭を再登録 → A→B→A の循環だった）
        Rec r; Holder h; CB a(&r, &On<0>), b(&r, &On<1>);
        h.Register(&a); h.Register(&b); h.Register(&a);
        h.Call(ev);
        EXPECT_TRUE(r.n[0] == 1 && r.n[1] == 1 && r.order[0] == 0 && r.order[1] == 1) << "二重登録: 先頭の再登録で循環しない・順序が保たれる";
    }
    {   // 同じ関数+同じ data の別オブジェクト
        Rec r; Holder h; CB a(&r, &On<0>), a2(&r, &On<0>);
        h.Register(&a); h.Register(&a2);
        h.Call(ev);
        EXPECT_TRUE(r.n[0] == 1 && h.Count() == 1) << "二重登録: 同じ(関数, data)の別の受け手は無視";
    }
    {   // 個別解除
        Rec r; Holder h; CB a(&r, &On<0>), b(&r, &On<1>), c(&r, &On<2>), d(&r, &On<3>);
        h.Register(&a); h.Register(&b); h.Register(&c); h.Register(&d);
        const bool m = h.Unregister(&b);      // 途中
        const bool f = h.Unregister(&a);      // 先頭
        const bool l = d.Unregister();        // 末尾（受け手から）
        const bool none = h.Unregister(&b);   // 解除済み
        h.Call(ev);
        EXPECT_TRUE(m && f && l && !none && r.n[0] == 0 && r.n[1] == 0 && r.n[2] == 1 && r.n[3] == 0 && h.Count() == 1) << "個別解除: 途中・先頭・末尾を外せて残りだけ呼ばれる";
        EXPECT_TRUE(h.Register(&a) && h.Register(&d) && h.Count() == 3) << "個別解除: 解除後に再登録できる";
        r.orderN = 0; h.Call(ev);
        EXPECT_TRUE(r.orderN == 3 && r.order[0] == 2 && r.order[1] == 0 && r.order[2] == 3) << "登録順: 古い順に呼ばれる(c, a, d)";
    }
    {   // 破棄の安全
        Rec r; Holder h; CB a(&r, &On<0>);
        { CB tmp(&r, &On<1>); h.Register(&tmp); h.Register(&a); }
        h.Call(ev);
        EXPECT_TRUE(r.n[1] == 0 && r.n[0] == 1 && h.Count() == 1) << "破棄: 受け手が先に消えても連鎖が壊れない";
        CB late(&r, &On<2>);
        { Holder h2; h2.Register(&late); }
        EXPECT_TRUE(!late.IsRegistered() && h.Register(&late)) << "破棄: Holder が先に消えても受け手が未登録に戻る";
    }
    {   // 別の Holder
        Rec r; Holder h1, h2; CB a(&r, &On<0>);
        h1.Register(&a);
        EXPECT_TRUE(!h2.Register(&a) && h2.Count() == 0 && h1.Count() == 1) << "別の Holder に登録済みの受け手は登録しない";
    }
    {   // Clear
        Rec r; Holder h; CB a(&r, &On<0>), b(&r, &On<1>);
        h.Register(&a); h.Register(&b); h.Clear();
        h.Call(ev);
        EXPECT_TRUE(r.n[0] == 0 && r.n[1] == 0 && h.Register(&a) && h.Register(&b)) << "Clear: 全部外れ、再登録できる";
    }
    {   // 並行 Call
        Rec r[4]; Holder h; CB a(&r[0], &On<0>), b(&r[0], &On<1>), c(&r[0], &On<2>);
        (void)r;
        static std::atomic<int> total{ 0 };
        total = 0;
        auto fn = [](void*, const Ev&) { total.fetch_add(1, std::memory_order_relaxed); };
        CB x(nullptr, fn), y(&total, fn), z(&r, fn);
        Holder hh; hh.Register(&x); hh.Register(&y); hh.Register(&z);
        std::vector<std::thread> th;
        for (int t = 0; t < 4; ++t) th.emplace_back([&] { for (int i = 0; i < 100000; ++i) hh.Call(ev); });
        for (auto& t : th) t.join();
        EXPECT_TRUE(total.load() == 4 * 100000 * 3) << "並行 Call: 4スレッド×10万回×3受け手の数が合う";
    }
}

// 登録のハンドルと、受け手・ホルダーのサイズが想定どおりか確かめる。
TEST(ChainCallBack, HandleAndSize) {
    const Ev ev{ 1, 2 };
    Rec r; Holder h;
    const uint32_t a = h.Add(&r, &On<0>), b = h.Add(&r, &On<1>), c = h.Add(&r, &On<2>);
    EXPECT_TRUE(a && b && c && a != b && h.Add(&r, &On<0>) == 0 && h.Add(&r, nullptr) == 0) << "Add: 番号を返し、二重・null は 0";
    EXPECT_TRUE(h.Remove(b) && !h.Remove(b) && h.Count() == 2);
    r.orderN = 0; h.Call(ev);
    EXPECT_TRUE(r.orderN == 2 && r.order[0] == 0 && r.order[1] == 2) << "Remove: 登録順が保たれる";
    // 満杯(Capacity 個まで)
    utl::ChainCallBackHolderN<2, Ev> small;
    EXPECT_TRUE(small.Add(&r, &On<0>) && small.Add(&r, &On<1>) && small.Add(&r, &On<2>) == 0) << "満杯なら 0";
    // 受け手が Create 前後・登録中にコピーされても値が保たれる
    CB x(&r, &On<3>), y;
    h.Register(&x);
    y = x;
    EXPECT_TRUE(!y.IsRegistered() && h.Register(&y) == false) << "登録中の受け手のコピーは未登録で、同じ(関数,data)は無視";
    EXPECT_LE(sizeof(CB), 24u) << "受け手のバイト数";
}
}  // namespace
