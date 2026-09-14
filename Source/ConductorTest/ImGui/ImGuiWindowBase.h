#pragma once

#include "CoreMinimal.h"
#include "ImGuiCommon.h"
#include "ImGuiWindowBase.generated.h"

/**
 * メインメニューのカテゴリ
 * ChatGptに聞いて適当に追加したので、ここは要相談です
 */
UENUM(BlueprintType)
enum class EImGuiMenuCategory : uint8
{
	// プレイヤー、敵、アイテム、進行状況など、ゲーム内容に関わるもの
	Gameplay,

	// ワールド時間、天候、日照角、マップ設定など
	World,

	// AI 状態、ブラックボード、行動ツリー、NPCデバッグ
	AI,

	// HUD 表示切り替え、デバッグオーバーレイ、UI レイアウトの確認など
	UI,

	// ポストエフェクト、ライト設定、LOD、シャドウ、SSRなど
	Graphics,

	// BGM・SE再生、ボリューム調整、サウンドキューの確認など
	Audio,

	// 物理設定、衝突デバッグ、ヒットボックスの表示など
	Physics,

	// 自作のエディタツール、バッチ処理、データ出力など
	Tools,

	// セーブ・ロード、時間操作、グローバル設定など
	System,

	Max UMETA(Hidden)
};

/**
 * ImGui
 */
class ImGuiWindowBase
{
public:
	virtual ~ImGuiWindowBase() {}

	// これを継承先で実装する
	virtual const char* GetWindowName() const = 0;
	virtual EImGuiMenuCategory GetCategory() const = 0;
	virtual void DrawContents() = 0;
	virtual void OnOpen() {}
	virtual void OnClose() {}

	// 本体の外に別ウィンドウを出したい場合に使う。
	// DrawContents は折りたたむと呼ばれないので、そこに書くと畳んだ瞬間に消える
	virtual void DrawExtraWindows() {}

	void Tick()
	{
		if (bIsOpen && !bWasOpen) OnOpen();
		if (bIsOpen)
		{
			if (ImGui::Begin(GetWindowName(), &bIsOpen))
			{
				DrawContents();
			}
			ImGui::End();

			// 入れ子のBeginを避けるため本体を閉じてから呼ぶ
			DrawExtraWindows();
		}
		if (!bIsOpen && bWasOpen) OnClose();
		bWasOpen = bIsOpen;
	}

	void TickOnce()
	{
		static thread_local TMap<ImGuiWindowBase*, bool> WasOpen;
		const bool Prev = WasOpen.FindRef(this);
		if (bIsOpen && !Prev) OnOpen();
		if (!bIsOpen && Prev) OnClose();
		WasOpen.Add(this, bIsOpen);
	}

	UGameInstance* GameInstance = nullptr;
	bool bIsOpen = false;
	bool bWasOpen = false; // 前フレームの開閉状態

};
