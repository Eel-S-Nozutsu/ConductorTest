// my_window_rokumoto_vending_machine.cpp
//
// rokumoto_vending_machine の状態機械 VendingMachine を ImGui 上で操作する自販機ウィンドウ。
// 実際の機能実装(Deposit/Slots/Slot/DispensingSlot)は implements/vending_machine.h のものを使う。
//
// 遷移仕様(初期状態: Standby):
//   Standby + ev::Deposit[is_ready]           / deposit               -> Ready
//   Ready   + ev::Deposit                     / deposit, update
//   Ready   + ev::Refund                      / refund                -> Standby
//   Ready   + ev::Buy[can_buy]                / buy                   -> Check
//   Check   [!has_deposit]                                            -> Standby
//   Check   (else)                                                    -> Ready
//
// ft::SlotUI は features.h のインターフェースのみが定義されており具象実装が無いので、
// このウィンドウ向けに SlotUIForImGui を実装し、各スロットの購入可否(購入可/待機/売切れ)を
// 色分け表示する(my_window_shop.cpp の ItemUi と同じ作法)。

#include "imgui.h"

#include "smx/features/state_log.h"
#include "smx/projects/rokumoto_vending_machine/events.h"
#include "smx/projects/rokumoto_vending_machine/features.h"
#include "smx/projects/rokumoto_vending_machine/features_builtin.h"
#include "smx/projects/rokumoto_vending_machine/vending_machine.h"
#include "smx/projects/rokumoto_vending_machine/implements/vending_machine.h"

#include <memory>

namespace app {

// Unity Build で他ファイル(例: my_window_shop.cpp)と同一の翻訳単位に結合された際、
// "using namespace vending_machine;" で持ち込む vending_machine::ft / ev などが
// 他ファイルの同名ネスト名前空間(shop::ft 等)と衝突しないよう、この using namespace
// とファイル内部の実装は vending_detail に隔離する。
namespace vending_detail {

using namespace rokumoto;
using namespace vending_machine;

// ft::SlotUI の実装。
// activate/standby/no_stock/set_info は Slots::update から状態遷移のタイミングでのみ呼ばれるので、
// ここでは通知された内容(表示状態・商品情報)を記録するだけにとどめる。実際の ImGui 描画は
// draw() が受け持つが、draw() はフレームループ(update())から毎フレーム明示的に呼ばれる、という点で
// 通知(activate 等)とは呼び出しのタイミングが異なる。
// set_info(ev::ProductInfo) 経由で商品情報を受け取るため、implements::Slot/Slots への依存はない。
struct SlotUIForImGui: ft::SlotUI {
    enum class State { Standby, Active, NoStock } state = State::Standby;
    ev::ProductInfo info{"", 0, 0};

    void activate() override  { state = State::Active; }   // 購入可能状態
    void standby() override   { state = State::Standby; }  // 待機状態
    void no_stock() override  { state = State::NoStock; }  // 在庫切れ
    void set_info(const ev::ProductInfo& e) override { info = e; } // 商品情報の通知

    ImU32 color() const
    {
        switch (state) {
            case State::Active:   return IM_COL32(120, 200, 255, 255); // 購入可能
            case State::NoStock:  return IM_COL32(150, 150, 150, 255); // 売り切れ
            case State::Standby:
            default:               return IM_COL32(235, 235, 235, 255); // 待機
        }
    }

    // 1 スロット分の行を描画する。押されたら sm へ ev::Buy{index} を送る。
    void draw(int index, VendingMachine& sm) const
    {
        ImGui::PushID(index);
        ImGui::PushStyleColor(ImGuiCol_Text, color());
        ImGui::Text("%-8s %4d yen  stock:%d", info.name.c_str(), info.price, info.stock);
        ImGui::PopStyleColor();

        ImGui::SameLine();
        const bool buyable = (state == State::Active);
        ImGui::BeginDisabled(!buyable);
        if (ImGui::Button("購入 (Buy)"))
            sm.process_event(ev::Buy{index});
        ImGui::EndDisabled();
        ImGui::PopID();
    }
};

// 状態機械とその機能実装一式(implements/vending_machine.h)をまとめて保持するモデル。
struct RokumotoVendingModel {
    Deposit deposit;
    Slots slots;
    DispensingSlot dispensing;
    StateLog log;
    VendingMachine sm{deposit, slots, dispensing, log};

    RokumotoVendingModel()
    {
        // サンプルの商品構成。在庫0の商品を混ぜて売切れ表示を確認できるようにする。
        slots.slots_.reserve(4);
        slots.slots_.emplace_back("Water",  100, 5);
        slots.slots_.emplace_back("Cola",   150, 3);
        slots.slots_.emplace_back("Coffee", 130, 4);
        slots.slots_.emplace_back("Tea",    120, 0);

        // 各スロットに ImGui 向けの SlotUI 実装を差し込む(p_ui がその差し込み口)。
        for (auto& slot: slots.slots_)
            slot.p_ui = std::make_shared<SlotUIForImGui>();

        // sm の初期 on_entry 時点ではスロットが未構築だったため、ここで表示状態を
        // 一度手動更新しておく(以後は状態遷移のたびに Slots::update が呼ばれる)。
        slots.update(deposit);
    }
};

struct MyWindowRokumotoVendingMachine {
    RokumotoVendingModel model;
    int pending = 100; // 次に投入する金額(UI で編集)

    void update()
    {
        ImGui::Begin("rokumoto_vending_machine State Machine");

        ImGui::Text("State: %s", model.log.state().c_str());
        ImGui::Text("投入金額: %d 円", model.deposit.deposit_amount);
        ImGui::Text("返金累計: %d 円", model.deposit.refund_amount);
        ImGui::Separator();

        ImGui::TextUnformatted("投入する金額:");
        if (ImGui::Button("-10")) pending -= 10;
        ImGui::SameLine();
        if (ImGui::Button("+10")) pending += 10;
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120);
        ImGui::InputInt("yen##deposit", &pending, 10, 100);
        if (pending < 0) pending = 0;

        if (ImGui::Button("投入 (Deposit)"))
            model.sm.process_event(ev::Deposit{pending});
        ImGui::SameLine();
        if (ImGui::Button("返金 (Refund)"))
            model.sm.process_event(ev::Refund{});

        ImGui::Separator();
        ImGui::TextUnformatted("商品一覧:");
        for (size_t i = 0; i < model.slots.slots_.size(); ++i) {
            auto ui = std::static_pointer_cast<SlotUIForImGui>(model.slots.slots_[i].p_ui);
            ui->draw(static_cast<int>(i), model.sm);
        }

        ImGui::Separator();
        ImGui::TextUnformatted("購入履歴:");
        for (const auto& name: model.dispensing.purchases())
            ImGui::BulletText("%s", name.c_str());

        ImGui::End();
    }
};

} // namespace vending_detail

void my_window_rokumoto_vending_machine()
{
    static vending_detail::MyWindowRokumotoVendingMachine w;
    w.update();
}

} // namespace app
