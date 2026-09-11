// my_window_abc.cpp
//
// sample_abc プロジェクト(src/json/sample_abc.json 由来)の状態機械 state_abc を
// ImGui 上で手動操作するためのデバッグウィンドウ。
//
// 【state_abc の遷移仕様】
//   初期状態: AAA
//     AAA + ev::BBB                       / set_message -> BBB
//     BBB + ev::CCC                       / set_message -> CCC
//     CCC + ev::AAA [check_count(0)]      / set_message -> AAA   (ガード付き)
//   ※ CCC から AAA への遷移だけはガード check_count が true のときのみ成立する。
//
// 【sample(my_window.cpp)との違い】
//   sample は組み込みフィーチャ StateLog を持ち log.state() で状態名を取れるが、
//   sample_abc にはそれが無い。よって
//     - フィーチャ feature_abc は自前で実装する(下の FeatureAbc)
//     - 現在状態は boost::sml の sm.is(sml::state<...>) で判定する
//   という2点が異なる。

#include "imgui.h"

#include "smx/projects/sample_abc/events.h"    // ev::AAA / ev::BBB / ev::CCC
#include "smx/projects/sample_abc/features.h"  // ft::feature_abc(実装すべきインターフェース)
#include "smx/projects/sample_abc/state_abc.h" // state_abc(状態機械本体)

namespace app {

//------------------------------------------------------------------------------
// feature_abc の実装
//   state_abc は ft::feature_abc への参照を要求する。ここではその具象実装を用意し、
//   状態機械のアクション(set_message)とガード(check_count)の中身を定義する。
//------------------------------------------------------------------------------
struct FeatureAbc: prj_abc::ft::feature_abc {
    // set_message アクションで渡された直近のメッセージ。
    // (生成コード上、遷移時の message は空文字なので通常は "" のまま)
    string_type message;

    // check_count ガードが返す値。CCC + ev::AAA の遷移可否を UI から制御するための
    // フラグで、true なら遷移成立・false なら遷移がブロックされる。
    bool allow_count = true;

    // ガード: CCC 状態で ev::AAA を受けたときに評価される。
    //   引数 e   … 発火したイベント(未使用)
    //   引数 value … json の guards.params で定義された既定値付きパラメータ
    bool check_count(const prj_abc::ev::AAA& e, int value = 0) const override {
        return allow_count;
    }

    // アクション: 各遷移で呼ばれ、メッセージを保持するだけの実装。
    void set_message(string_type msg) override {
        message = msg;
    }
};

//------------------------------------------------------------------------------
// デバッグウィンドウ本体
//------------------------------------------------------------------------------
struct MyWindowAbc {
    // フィーチャ実装と状態機械。sm は ft への参照を保持するので、
    // ft より後(かつ同一オブジェクト内)に宣言して寿命を合わせる。
    FeatureAbc ft;
    prj_abc::state_abc sm{ft};

    // 現在の状態名を文字列で返す。
    // StateLog が無いため、boost::sml の is() を使ってどの状態にいるかを問い合わせる。
    const char* state_name() const {
        using namespace prj_abc;
        if (sm.is(sml::state<sm::state_abc::AAA>)) return "AAA";
        if (sm.is(sml::state<sm::state_abc::BBB>)) return "BBB";
        if (sm.is(sml::state<sm::state_abc::CCC>)) return "CCC";
        return "?"; // 想定外(通常は到達しない)
    }

    // 毎フレーム呼ばれる描画・操作処理。
    void update()
    {
        using namespace prj_abc;
        ImGui::Begin("state_abc State Machine");

        // 現在状態の表示。
        ImGui::Text("Current State: %s", state_name());
        ImGui::Separator();

        // ev::AAA は count メンバ(json の members)を持つイベントなので、
        // 送信前に値を入力できるようにしておく。static でフレーム間の値を保持する。
        static int count = 0;
        ImGui::InputInt("count", &count);

        // 各イベントの送信ボタン。押された(true が返った)フレームで process_event する。
        // 遷移表に無い組み合わせ(例: AAA で ev::CCC)は boost::sml が無視するため、
        // どの状態でどのボタンを押しても安全。
        if (ImGui::Button("ev::AAA"))
            sm.process_event(ev::AAA{count});
        if (ImGui::Button("ev::BBB"))
            sm.process_event(ev::BBB{});
        if (ImGui::Button("ev::CCC"))
            sm.process_event(ev::CCC{});

        ImGui::Separator();

        // CCC + ev::AAA のガード check_count を切り替える。
        // OFF にすると CCC のまま留まり、ガード付き遷移の挙動を確認できる。
        ImGui::Checkbox("check_count (guard)", &ft.allow_count);

        // set_message で保持された直近メッセージの表示。
        ImGui::Text("message: \"%s\"", ft.message.c_str());

        ImGui::End();
    }
};

//------------------------------------------------------------------------------
// main.cpp から毎フレーム呼ばれるエントリ関数。
//   ウィンドウ状態を static で保持し、フレームをまたいで同一インスタンスを使い回す。
//------------------------------------------------------------------------------
void my_window_abc()
{
    static MyWindowAbc w;
    w.update();
}

} // namespace app
