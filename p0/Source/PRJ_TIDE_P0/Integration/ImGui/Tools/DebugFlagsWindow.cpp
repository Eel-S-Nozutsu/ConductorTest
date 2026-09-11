// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/Integration/ImGui/Tools/DebugFlagsWindow.h"
#include "PRJ_TIDE_P0/Core/TideGameSettings.h"

#if !UE_BUILD_SHIPPING

void DebugFlagsWindow::DrawContents()
{
	UTideGameSettings* Settings = UTideGameSettings::Get();
	if (!Settings) return;

	auto SavedCheckbox = [&](const char* Label, bool& Flag)
	{
		if (ImGui::Checkbox(Label, &Flag))
		{
			Settings->SaveSettings();
		}
	};

	ImGui::SeparatorText("Player");
	ImGui::Checkbox("無敵", &Settings->bDebugFlagInvincible);
	ImGui::Checkbox("死亡しない", &Settings->bDebugFlagNoDeath);
	ImGui::Checkbox("スーパーアーマー", &Settings->bDebugFlagSuperArmor);
	ImGui::Checkbox("被ダメージなし", &Settings->bDebugFlagNoDamage);
	ImGui::Checkbox("常時回避成功", &Settings->bDebugFlagAutoDodgeSuccess);
	ImGui::Checkbox("Transform表示", &Settings->bDebugFlagDrawTransformCoordinate);
	ImGui::Checkbox("Root表示", &Settings->bDebugFlagDrawRootCoordinate);
	ImGui::Checkbox("攻撃吸着表示", &Settings->bDebugFlagDrawHomingArea);
	ImGui::Checkbox("神技ゲージ無限", &Settings->bDebugGodActionInfiniteGauge);
	ImGui::Checkbox("攻撃力9999", &Settings->bDebugFlagPlayerAttack9999);
	ImGui::Checkbox("B で目の前に竜巻(大)", &Settings->bDebugFlagSpawnTornadoInFront);

	ImGui::SeparatorText("Enemy");
	SavedCheckbox("感知しない",              Settings->bDebugEnemyNoDetect);
	if (Settings->bDebugEnemyNoDetect)
	{
		ImGui::SameLine();
		ImGui::TextDisabled("(既に感知済みの敵はリセットされない)");
	}
	SavedCheckbox("AI停止",					Settings->bDebugEnemyAIStop);
	SavedCheckbox("被ダメージなし##enemy",	Settings->bDebugEnemyNoDamage);
	SavedCheckbox("ドッジNavチェック表示",	Settings->bDebugEnemyDodgeNavCheck);
	SavedCheckbox("予知回避ステップ無効",	Settings->bDebugEnemyNoPredictiveDodge);
	SavedCheckbox("吹き飛び3倍",			Settings->bDebugEnemyBlowbackX3);
	SavedCheckbox("個別クールダウン0",		Settings->bDebugEnemyNoAttackCooldown);
	SavedCheckbox("近中遠レンジ表示",		Settings->bDebugDrawAttackRange);
	SavedCheckbox("上下角コーン表示",		Settings->bDebugDrawAttackAngle);
	SavedCheckbox("HP数値表示",				Settings->bDebugEnemyShowHpText);
	SavedCheckbox("ルート移動目標表示",		Settings->bDebugDrawRoutePath);
	SavedCheckbox("視野(扇)表示",			Settings->bDebugDrawEnemyFOV);
	SavedCheckbox("帰巣パス検索表示",		Settings->bDebugDrawReturnHome);
	SavedCheckbox("検知ゲージ表示",			Settings->bDebugDrawDetectionGauge);

	ImGui::SeparatorText("Gimmick");
	SavedCheckbox("スポナーボリューム表示",    Settings->bDebugDrawSpawnerVolumes);

	ImGui::SeparatorText("Combat");
	SavedCheckbox("攻撃判定表示",               Settings->bDebugDrawAttackHitbox);
	SavedCheckbox("光輪判定表示",               Settings->bDebugDrawHaloHitbox);
	SavedCheckbox("爆撃着弾点表示",             Settings->bDebugDrawBombardLanding);
	SavedCheckbox("ヒットリアクション無効",     Settings->bDebugDisableHitReaction);
	SavedCheckbox("吹き飛び衝突ダメージ有効",   Settings->bDebugEnableBlowbackCollisionDamage);
	SavedCheckbox("ヒットストップ無効",         Settings->bDebugFlagDisableHitStop);
	if (Settings->bDebugFlagDisableHitStop)
	{
		ImGui::SameLine();
		ImGui::TextDisabled("(HitStopUtil経由のみ。神技/とどめ等の独自スローは対象外)");
	}

	ImGui::SeparatorText("Camera");
	ImGui::Checkbox("FOV変更無効", &Settings->bDebugFlagDisableFOVChange);
	ImGui::Checkbox("カメラ遅延無効", &Settings->bDebugFlagDisableLag);
	ImGui::Checkbox("ジャンプ注視点デッドゾーン描画", &Settings->bDebugFlagDrawJumpFocus);
	ImGui::Checkbox("回転軸/注視点 描画", &Settings->bDebugFlagDrawCameraPivotFocus);
	if (Settings->bDebugFlagDrawCameraPivotFocus)
	{
		ImGui::SameLine();
		ImGui::TextDisabled("(緑=回転軸 水色=注視点 黄=カメラ 白=原点)");
	}
	ImGui::Checkbox("空中ダッシュカメラ切り替え有効", &Settings->bDebugFlagEnableAirChargeDashCamera);

	ImGui::SeparatorText("UI");
	SavedCheckbox("UI非表示", Settings->bDebugHideUI);
	if (Settings->bDebugHideUI)
	{
		ImGui::SameLine();
		ImGui::TextDisabled("(プレイヤーHP/神技ゲージ/チャージ漢字/敵HP/ボスHP)");
	}
	SavedCheckbox("HUDモチーフカラー", Settings->bUseTideMotifHudColors);
	SavedCheckbox("ダメージ数値表示", Settings->bDebugShowDamageNumbers);
	if (Settings->bDebugShowDamageNumbers)
	{
		ImGui::SameLine();
		ImGui::TextDisabled("(文字=Tide色/輪郭: 敵=黒 PL=紅 30以上=黄)");
	}
	if (Settings->bUseTideMotifHudColors)
	{
		ImGui::SameLine();
		ImGui::TextDisabled("(画像モチーフ：ティール/クリーム/ラスト。OFFで従来色)");
	}

}

#endif // !UE_BUILD_SHIPPING
