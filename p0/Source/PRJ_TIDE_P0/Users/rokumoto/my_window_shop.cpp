// my_window_shop.cpp
//
// prj_shop の状態機械を ImGui で操作するショップウィンドウ。
//
// 状態機械(2 段構成):
//   state_ui_visibility  Hide <-> [state_shop]
//   state_shop           Idle(商品選択) <-> Selecting(個数選択)
// state_shop は合成状態なので、内側の状態は is<identity<state_shop>>() で調べる。
//
// 6 つのフィーチャ(Funds/Selection/ItemList/Item/Shop_UI/Item_UI)は ShopModel が
// 多重継承でまとめて実装する。SM へ渡す ft::* 参照はすべて同じインスタンスなので、
// アクション/ガードは ShopModel のメンバを直接読み書きしている。
//
// 商品データはデータテーブルから読む(行構造は shop/ShopDataTypes.h):
//   Test_ShopData (FShopItemRow)   ItemID / 在庫 / 単価  … 行の並び順が商品一覧の順
//   Test_ItemData (FItemMasterRow) ItemID / 商品名 / 商品詳細
// 在庫は負値が無制限。所持数はテーブルに無いのでテスト用の固定値を入れている。

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "UObject/UObjectGlobals.h"

#include "PRJ_TIDE_P0/Users/rokumoto/shop/ShopDataTypes.h"

#include "imgui.h"

#include "smx/projects/prj_shop/events.h"
#include "smx/projects/prj_shop/features.h"
#include "smx/projects/prj_shop/state_shop.h"
#include "smx/projects/prj_shop/state_ui_visibility.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace app {

using namespace shop;

enum class ItemUi { Locked, Normal, Focus, QuantitySelect, SoldOut };

// 商品 1 個ぶんのデータ + 表示状態。
struct ShopItem {
    std::string id;                  // アイテムID(テーブルの突き合わせキー)
    std::string name;                // 商品名
    int         stock = 0;           // 在庫(負値 = 無制限)
    int         price = 0;           // 単価
    std::string desc;                // 商品詳細
    int         owned = 0;           // プレイヤーの所持数
    ItemUi      ui = ItemUi::Normal; // 表示状態(update で更新)

    bool unlimited() const { return stock < 0; }
    bool sold_out()  const { return !unlimited() && stock <= 0; }
    bool has_stock(int num) const { return unlimited() || stock >= num; }
};

// 6 フィーチャをまとめて実装するショップの実体。
struct ShopModel
    : ft::Funds, ft::Selection, ft::ItemList, ft::Item, ft::Shop_UI, ft::Item_UI {

    std::vector<ShopItem> items;
    int  funds     = 500;   // 所持金
    int  selected  = 0;     // カーソル位置(商品番号)
    int  quantity  = 1;     // 選択中の購入個数
    bool visible   = false;
    bool selecting = false; // 個数選択中か(set_selecting で反映)
    std::string message;    // 直近の購入結果

    float alpha       = 0.0f; // フェードの現在値(0=非表示, 1=表示)
    float alphaTarget = 0.0f;

    // アルファを目標へ 0.5 秒かけて近づける(毎フレーム呼ぶ)。
    void advance_fade(float dt) {
        const float step = dt / 0.5f;
        if (alpha < alphaTarget)      alpha = std::min(alphaTarget, alpha + step);
        else if (alpha > alphaTarget) alpha = std::max(alphaTarget, alpha - step);
    }

    int  item_count() const { return (int)items.size(); }
    bool valid(int i)  const { return i >= 0 && i < item_count(); }

    // 1 個でも買えるか。false の商品はカーソルを合わせられない。
    bool buyable_one(int i) const {
        if (!valid(i)) return false;
        const ShopItem& it = items[i];
        return it.has_stock(1) && funds >= it.price;
    }

    //--------------------------------------------------------------------------
    // データテーブルからの商品リスト構築
    //--------------------------------------------------------------------------
    static constexpr const TCHAR* kItemTablePath =
        TEXT("/Game/PRJ_TIDE_P0/SMX/Shop/Test_ItemData.Test_ItemData");
    static constexpr const TCHAR* kShopTablePath =
        TEXT("/Game/PRJ_TIDE_P0/SMX/Shop/Test_ShopData.Test_ShopData");

    static constexpr int kQuantityMax = 99; // 無制限在庫のときの個数上限

    bool        items_loaded = false;
    std::string load_status;          // 読み込み件数 or エラー(ウィンドウに出す)

    // ImGui は UTF-8 の char* を取るので FString/FText はここで変換する。
    static std::string to_utf8(const FString& s) { return std::string(TCHAR_TO_UTF8(*s)); }

    // 行構造まで確かめてテーブルを返す。失敗時は out_error を埋めて nullptr。
    static UDataTable* load_table(const TCHAR* path, const UScriptStruct* row_struct,
                                  std::string& out_error) {
        UDataTable* table = LoadObject<UDataTable>(nullptr, path);
        if (!table) {
            out_error = "テーブルが見つからない: " + to_utf8(FString(path));
            return nullptr;
        }
        if (table->GetRowStruct() != row_struct) {
            out_error = "行構造が違う: " + to_utf8(FString(path));
            return nullptr;
        }
        return table;
    }

    // 所持数はテーブルに無いので並び順から適当に決める(テスト表示用)。
    static int sample_owned(int index) {
        static const int pattern[] = {3, 1, 0, 2, 0, 1, 0, 0};
        return pattern[index % (int)(sizeof(pattern) / sizeof(pattern[0]))];
    }

    // 2 つのテーブルから商品リストを作り直す(初回 update から呼ばれる)。
    void reload_items() {
        items_loaded = true;
        items.clear();
        message.clear();
        selected = 0;
        quantity = 1;

        std::string error;
        UDataTable* item_table = load_table(kItemTablePath, FItemMasterRow::StaticStruct(), error);
        UDataTable* shop_table = error.empty()
            ? load_table(kShopTablePath, FShopItemRow::StaticStruct(), error) : nullptr;
        if (!item_table || !shop_table) { load_status = error; return; }

        // ItemID -> アイテムマスター行。ItemID 未設定の行は行名を ID として扱う。
        TMap<FName, const FItemMasterRow*> master;
        for (const FName& row_name : item_table->GetRowNames()) {
            const FItemMasterRow* row =
                item_table->FindRow<FItemMasterRow>(row_name, TEXT("my_window_shop"));
            if (!row) continue;
            master.Add(row->ItemID.IsNone() ? row_name : row->ItemID, row);
        }

        // ショップ表を上から順に。ItemID が引き当てられない行は飛ばす。
        int skipped = 0;
        for (const FName& row_name : shop_table->GetRowNames()) {
            const FShopItemRow* row =
                shop_table->FindRow<FShopItemRow>(row_name, TEXT("my_window_shop"));
            const FItemMasterRow* const* found = row ? master.Find(row->ItemID) : nullptr;
            if (!found) { ++skipped; continue; }
            const FItemMasterRow& m = **found;

            ShopItem it;
            it.id    = to_utf8(row->ItemID.ToString());
            it.name  = to_utf8(m.ItemName.IsEmpty() ? row->ItemID.ToString() : m.ItemName.ToString());
            it.stock = row->Stock;
            it.price = row->Price;
            it.desc  = to_utf8(m.Description.ToString());
            it.owned = sample_owned(item_count());
            items.push_back(std::move(it));
        }

        load_status = to_utf8(FString::Printf(TEXT("商品 %d 件を読み込み(スキップ %d 件)"),
                                             item_count(), skipped));

        for (int i = 0; i < item_count(); ++i)
            if (buyable_one(i)) { selected = i; break; }
    }

    // 自分自身を別フィーチャとして渡すときの曖昧さ回避(6 個を多重継承しているため)。
    ft::Funds&           as_funds()           { return *this; }
    const ft::Funds&     as_funds()     const { return *this; }
    ft::Selection&       as_selection()       { return *this; }
    const ft::Selection& as_selection() const { return *this; }
    ft::Item&            as_item()            { return *this; }
    const ft::Item&      as_item()      const { return *this; }

    //--------------------------------------------------------------------------
    // ft::Funds (所持金)
    //--------------------------------------------------------------------------
    bool has_funds(int money) const override { return funds >= money; }
    void spend(int money) override { funds -= money; }

    //--------------------------------------------------------------------------
    // ft::Selection (購入個数を保持。判定/計算は Item へ個数を渡して委譲)
    //--------------------------------------------------------------------------
    // 上限(buyable_num)を超えたら 1 へ、1 未満なら上限へ巡回する。
    void change_quantity(int quantity_num = 1, int buyable_num = 1) override {
        int new_quantity = this->quantity + quantity_num;
        if (new_quantity > buyable_num) {
            new_quantity = 1;
        }
        else if (new_quantity < 1) {
            new_quantity = buyable_num > 0 ? buyable_num : 0;
        }
        this->quantity = new_quantity;
    }
    void reset() override { quantity = 1; }
    bool has_funds(const ft::Item& f0, const ft::Funds& f1) const override {
        return f0.has_funds(f1, quantity);
    }
    bool has_stock(const ft::Item& f0) const override {
        return f0.has_stock(quantity);
    }
    void purchase(ft::Item& f0, ft::Funds& f1) override {
        f0.purchase(f1, quantity);
        reset();
    }
    void update(ft::Item& f0, ft::Funds& f1, bool isFocused = false,
                bool isSelected = false) override {
        f0.update(f1, quantity, isFocused, isSelected);
    }

    //--------------------------------------------------------------------------
    // ft::ItemList (商品リスト / 選択中商品への操作)
    //--------------------------------------------------------------------------
    bool can_buy(const ft::Selection& f0, const ft::Funds& f1) const override {
        return f0.has_funds(as_item(), f1) && f0.has_stock(as_item());
    }
    bool has_stock(const ft::Selection& f0) const override {
        return f0.has_stock(as_item());
    }
    void focus(const ev::SelectItem& e) override {
        if (valid(e.item_num)) selected = e.item_num;
    }
    void set_selecting(bool isSelecting = false) override { selecting = isSelecting; }
    void change_quantity(const ev::ChangeQuantity& e, ft::Selection& f0,
                         ft::Funds& f1) override {
        as_item().change_quantity(e, f1, f0);
    }
    // Selection::purchase の中で個数がリセットされるので、個数は先に控える。
    void purchase(ft::Selection& f0, ft::Funds& f1) override {
        if (!valid(selected)) return;
        ShopItem& it = items[selected];
        const int count = quantity;
        f0.purchase(as_item(), f1);
        message = it.name + " を " + std::to_string(count) + "個 購入した";
    }
    // 全商品の表示状態を一括更新。
    // ft::Item / ft::Item_UI は商品番号を持たない = 1 商品ぶんしか表現できないので、
    // フィーチャを通せるのは選択中商品だけ。他はここで直接設定する。
    void update(ft::Selection& f0, ft::Funds& f1) override {
        for (int i = 0; i < item_count(); ++i) {
            ShopItem& it = items[i];
            const bool isFocused = (i == selected);
            if (isFocused) {
                f0.update(as_item(), f1, isFocused, isFocused && selecting);
            } else if (it.sold_out()) {
                it.ui = ItemUi::SoldOut;
            } else {
                it.ui = f1.has_funds(it.price) ? ItemUi::Normal : ItemUi::Locked;
            }
        }
    }

    //--------------------------------------------------------------------------
    // ft::Item (selected 商品に対する計算)
    //--------------------------------------------------------------------------
    bool has_funds(const ft::Funds& f0, int num = 1) const override {
        return valid(selected) && f0.has_funds(items[selected].price * num);
    }
    bool has_stock(int num = 1) const override {
        return valid(selected) && items[selected].has_stock(num);
    }
    void purchase(ft::Funds& f0, int num = 1) override {
        if (!valid(selected)) return;
        ShopItem& it = items[selected];
        f0.spend(it.price * num);
        if (!it.unlimited()) {      // 無制限在庫は減らさない
            it.stock -= num;
            if (it.stock < 0) it.stock = 0;
        }
        it.owned += num;
    }
    // isSelected は ItemList::update から「選択中 かつ 個数選択中」で渡る。
    void update(ft::Funds& f0, int num = 1, bool isFocused = false,
                bool isSelected = false) override {
        if (!valid(selected)) return;
        if (isFocused) {
            if (isSelected) quantityselect();
            else            focus();
        } else if (!has_stock(num)) {
            soldout();
        } else if (has_funds(f0, num)) {
            normal();
        } else {
            locked();
        }
    }
    // 買える最大個数を Funds に聞いて求め、個数の計算自体は Selection へ委譲する。
    void change_quantity(const ev::ChangeQuantity& e, ft::Funds& f0, ft::Selection& f1) override {
        if (!valid(selected)) { f1.change_quantity(e.quantity, 1); return; }
        const int price = items[selected].price;
        const int stock = items[selected].unlimited() ? kQuantityMax : items[selected].stock;
        int buyable_num = stock;
        if (price > 0) {
            for (int i = 1; i <= stock; ++i) {
                if (!f0.has_funds(price * i)) { buyable_num = i - 1; break; }
            }
        }
        f1.change_quantity(e.quantity, buyable_num);
    }

    //--------------------------------------------------------------------------
    // ft::Shop_UI
    //--------------------------------------------------------------------------
    void show() override {
        visible = true;
        alphaTarget = 1.0f;
        // カーソルを一番上の買える商品へ(無ければ先頭)。
        selected = 0;
        for (int i = 0; i < item_count(); ++i)
            if (buyable_one(i)) { selected = i; break; }
    }
    void hide() override { visible = false; alphaTarget = 0.0f; }

    //--------------------------------------------------------------------------
    // ft::Item_UI (反映先は常に選択中商品)
    //--------------------------------------------------------------------------
    void locked() override         { if (valid(selected)) items[selected].ui = ItemUi::Locked; }
    void normal() override         { if (valid(selected)) items[selected].ui = ItemUi::Normal; }
    void focus() override          { if (valid(selected)) items[selected].ui = ItemUi::Focus; }
    void quantityselect() override { if (valid(selected)) items[selected].ui = ItemUi::QuantitySelect; }
    void soldout() override        { if (valid(selected)) items[selected].ui = ItemUi::SoldOut; }
};

struct MyWindowShop {
    ShopModel model;
    // 同じインスタンスを 6 フィーチャとして渡す(Funds, Selection, ItemList, Item, Shop_UI, Item_UI)。
    state_ui_visibility sm{model, model, model, model, model, model};

    // 合成状態の中(state_shop)の状態を調べる。
    template <class TState>
    bool in_shop() {
        return sm.is<sml::aux::identity<sm::state_shop>>(sml::state<TState>);
    }
    bool is_hidden()    { return sm.is(sml::state<sm::state_ui_visibility::Hide>); }
    bool is_idle()      { return in_shop<sm::state_shop::Idle>(); }
    bool is_selecting() { return in_shop<sm::state_shop::Selecting>(); }

    const char* state_name() {
        if (is_hidden())    return "Hide (非表示)";
        if (is_selecting()) return "Selecting (個数選択)";
        if (is_idle())      return "Idle (商品選択)";
        return "?";
    }

    static ImU32 ui_color(ItemUi ui) {
        switch (ui) {
            case ItemUi::SoldOut:        return IM_COL32(150, 150, 150, 255);
            case ItemUi::Locked:         return IM_COL32(150, 110, 110, 255);
            case ItemUi::Focus:          return IM_COL32(120, 200, 255, 255);
            case ItemUi::QuantitySelect: return IM_COL32(255, 220, 100, 255);
            case ItemUi::Normal:
            default:                     return IM_COL32(235, 235, 235, 255);
        }
    }

    // 列レイアウト(ヘッダーと行で共有する X 位置)。
    // リストの child は WindowPadding=0 なので、左余白は kMargin で自前に確保する。
    // 名前より右は kNameW からの相対。名前幅を変えたいときは kNameW だけ触る。
    static constexpr float kMargin = 10.0f;  // 枠左端と内容の間の余白
    static constexpr float kNameW  = 240.0f; // 名前クリック領域の幅
    static constexpr float kQtyX   = kMargin + kNameW + 5.0f; // 個数選択 < n >
    static constexpr float kOwnedX = kQtyX + 90.0f;           // 所持数
    static constexpr float kPriceX = kOwnedX + 85.0f;         // 金額
    static constexpr float kListW  = kPriceX + 95.0f;         // 商品一覧の幅

    // 列見出し。リスト枠内の非スクロール領域に描く。
    void draw_list_header() {
        ImGui::SetCursorPosX(kMargin); ImGui::TextDisabled("Item");
        ImGui::SameLine(kOwnedX); ImGui::TextDisabled("Owned");
        ImGui::SameLine(kPriceX); ImGui::TextDisabled("Price");
    }

    // 1 商品ぶんの行。列は [名前][(個数選択)][所持数][金額]。
    void draw_item_row(int i) {
        ShopItem& it = model.items[i];
        const bool sel      = (i == model.selected);
        const bool qty_here = is_selecting() && sel; // この行で個数選択中か

        ImGui::PushID(i);
        ImGui::PushStyleColor(ImGuiCol_Text, ui_color(it.ui));

        // 名前。クリックで操作できるのは Idle 中のみ、かつ買える商品のみ。
        // Selecting 中は他商品の見た目が変わらないよう、ホバーしないテキストで描く。
        ImGui::SetCursorPosX(kMargin);
        if (is_idle()) {
            ImGui::BeginDisabled(!model.buyable_one(i));
            if (ImGui::Selectable(it.name.c_str(), sel, ImGuiSelectableFlags_None, ImVec2(kNameW, 0))) {
                if (sel) sm.process_event(ev::Decide{});      // 選択中を再クリックで決定
                else     sm.process_event(ev::SelectItem{i}); // 別商品ならカーソル移動
            }
            ImGui::EndDisabled();
        } else {
            ImGui::TextUnformatted(it.name.c_str());
        }

        if (qty_here) {
            ImGui::SameLine(kQtyX);
            if (ImGui::SmallButton("<")) sm.process_event(ev::ChangeQuantity{-1});
            ImGui::SameLine();
            ImGui::Text("%d", model.quantity);
            ImGui::SameLine();
            if (ImGui::SmallButton(">")) sm.process_event(ev::ChangeQuantity{+1});
        }

        ImGui::SameLine(kOwnedX);
        ImGui::Text("%d", it.owned);

        // 個数選択中の行だけ単価 × 個数、それ以外は単価。
        ImGui::SameLine(kPriceX);
        if (it.sold_out())
            ImGui::TextUnformatted("SOLD OUT");
        else
            ImGui::Text("%d G", qty_here ? it.price * model.quantity : it.price);

        ImGui::PopStyleColor();
        ImGui::PopID();
    }

    void update() {
        if (!model.items_loaded) model.reload_items(); // 商品データは初回だけ読む
        model.advance_fade(ImGui::GetIO().DeltaTime);

        ImGui::Begin("prj_shop Shop");

        ImGui::Text("State: %s", state_name());
        if (!model.load_status.empty())
            ImGui::TextDisabled("%s", model.load_status.c_str());
        ImGui::Separator();

        // 状態が Hide でフェードも終わっているときだけ開くボタン。
        // フェードアウト中(alpha>0)は下の内容を薄れさせながら描き続ける。
        const bool fully_hidden = is_hidden() && model.alpha <= 0.001f;
        if (fully_hidden) {
            ImGui::TextUnformatted("ショップは閉じています。");
            if (ImGui::Button("Show (ショップを開く)"))
                sm.process_event(ev::Show{});
            if (!model.message.empty())
                ImGui::Text("前回: %s", model.message.c_str());
            ImGui::End();
            return;
        }

        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, model.alpha); // 以降にフェードを適用

        const ImGuiStyle& style = ImGui::GetStyle();
        const float row_h  = ImGui::GetTextLineHeightWithSpacing();
        const float list_w = kListW;

        // 所持金。テキスト 1 行ぶんの高さちょうどにしてスクロールバーを出さない。
        const float funds_h = ImGui::GetTextLineHeight() + style.WindowPadding.y * 2.0f;
        ImGui::BeginChild("funds_box", ImVec2(list_w, funds_h), true, ImGuiWindowFlags_NoScrollbar);
        ImGui::Text("所持金: %d G", model.funds);
        ImGui::EndChild();

        ImGui::Spacing();

        // 左: 商品一覧。見出しを枠内に固定し、その下だけをスクロールさせる(7 行ぶん)。
        const float list_h = row_h * 7.0f              // スクロール領域
                           + row_h                     // 見出し
                           + style.ItemSpacing.y * 2.0f
                           + style.FramePadding.y * 2.0f;

        // SameLine の X は window 原点基準で、先頭要素は padding 込みで始まる。
        // 見出し(外枠)と行(内側 scroll)で列 X を揃えるため両方の padding を 0 にする。
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::BeginChild("item_list", ImVec2(list_w, list_h), true);
        draw_list_header();
        ImGui::Separator();
        ImGui::BeginChild("item_scroll", ImVec2(0.0f, 0.0f), false);
        for (int i = 0; i < model.item_count(); ++i)
            draw_item_row(i);
        ImGui::EndChild();
        ImGui::EndChild();
        ImGui::PopStyleVar();

        ImGui::SameLine();

        // 右: 選択中商品の詳細(アイコン枠 + 商品名 + 説明)。
        ImGui::BeginChild("item_detail", ImVec2(260.0f, list_h), true);
        if (model.valid(model.selected)) {
            const ShopItem& it = model.items[model.selected];
            // ImDrawList への直接描画は PushStyleVar(Alpha) が効かないので、
            // 枠色のアルファに手動でフェードを掛ける。
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const ImVec2 p = ImGui::GetCursorScreenPos();
            const float box = row_h * 1.6f;
            const int   a   = (int)(200 * model.alpha);
            dl->AddRect(p, ImVec2(p.x + box, p.y + box), IM_COL32(200, 200, 200, a), 3.0f);
            ImGui::Dummy(ImVec2(box, box));
            ImGui::SameLine();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(it.name.c_str());
            ImGui::Separator();
            ImGui::TextWrapped("%s", it.desc.c_str());
        }
        ImGui::EndChild();

        ImGui::Separator();

        // 操作ボタン。決定の意味は状態で変わる。
        //   Idle:      決定 = 個数選択へ / キャンセル = 閉じる
        //   Selecting: 決定 = 購入       / キャンセル = 商品選択へ戻る
        // Disabled 条件は SM 側のガードと同じ判定を使う。
        if (is_selecting()) {
            const bool buyable = model.can_buy(model.as_selection(), model.as_funds());
            ImGui::BeginDisabled(!buyable);
            if (ImGui::Button("決定 (購入)")) sm.process_event(ev::Decide{});
            ImGui::EndDisabled();
            ImGui::SameLine();
            if (ImGui::Button("キャンセル (商品選択へ)")) sm.process_event(ev::Cancel{});
            ImGui::SameLine();
            const ShopItem& si = model.items[model.selected];
            if (si.unlimited())
                ImGui::Text("在庫 無制限 ／ 個数 %d ／ 合計 %d G",
                            model.quantity, si.price * model.quantity);
            else
                ImGui::Text("在庫 %d ／ 個数 %d ／ 合計 %d G",
                            si.stock, model.quantity, si.price * model.quantity);
        } else { // Idle
            const bool has = model.as_item().has_stock(1);
            ImGui::BeginDisabled(!has);
            if (ImGui::Button("決定 (個数選択へ)")) sm.process_event(ev::Decide{});
            ImGui::EndDisabled();
            ImGui::SameLine();
            if (ImGui::Button("キャンセル (閉じる)")) sm.process_event(ev::Cancel{});
        }

        if (!model.message.empty()) {
            ImGui::Spacing();
            ImGui::Text("%s", model.message.c_str());
        }

        ImGui::Spacing();
        ImGui::TextDisabled("商品をクリックで選択 / 選択中の商品を再クリックで決定");

        ImGui::PopStyleVar(); // Alpha
        ImGui::End();
    }
};

void my_window_shop() {
    static MyWindowShop w;
    w.update();
}

} // namespace app
