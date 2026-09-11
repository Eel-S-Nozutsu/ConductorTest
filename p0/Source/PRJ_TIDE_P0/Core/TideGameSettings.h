// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/GameUserSettings.h"
#include "TideGameSettings.generated.h"

/**
 * ユーザー設定
 */
UCLASS()
class UTideGameSettings : public UGameUserSettings
{
	GENERATED_BODY()

public:

	UPROPERTY(Config) TMap<FString, bool> ImGuiWindowOpened;

	// SituationJump登録データ。書式:
	// "Name\tLevelName\tX\tY\tZ\tPitch\tYaw\tRoll"
	UPROPERTY(Config) TArray<FString> SituationJumpEntries;

	// ---- デバッグフラグ ----

	// Player
	UPROPERTY(Config) bool bDebugFlagInvincible = false;				// 無敵
	UPROPERTY(Config) bool bDebugFlagSuperArmor = false;				// スーパーアーマー
	UPROPERTY(Config) bool bDebugFlagNoDamage = false;					// ダメージ無効
	UPROPERTY(Config) bool bDebugFlagAutoDodgeSuccess = false;			// 常時回避成功
	UPROPERTY(Config) bool bDebugFlagDrawTransformCoordinate = false;	// プレイヤー座標描画
	UPROPERTY(Config) bool bDebugFlagDrawRootCoordinate = false;		// ルート座標描画
	UPROPERTY(Config) bool bDebugFlagDrawHomingArea = false;			// 攻撃吸着描画
	UPROPERTY(Config) bool bDebugGodActionInfiniteGauge = false;		// 神技ゲージ無限
	UPROPERTY(Config) bool bDebugFlagPlayerAttack9999 = false;			// プレイヤーの攻撃力を9999にする
	UPROPERTY(Config) bool bDebugFlagNoDeath = false;					// 死亡しない (ExecutePlayerDeathを呼ばない)
	UPROPERTY(Config) bool bDebugFlagSpawnTornadoInFront = false;		// B(キャンセル)で目の前にスライドパッシブの竜巻(大)を発生

	// Enemy
	UPROPERTY(Config) bool bDebugEnemyNoDetect = false;					// エネミー感知しない
	UPROPERTY(Config) bool bDebugEnemyAIStop = false;					// エネミーAI停止
	UPROPERTY(Config) bool bDebugEnemyNoDamage = false;					// エネミー被ダメージなし
	UPROPERTY(Config) bool bDebugDrawSpawnerVolumes = false;			// スポナー内外ボリューム表示
	UPROPERTY(Config) bool bDebugEnemyDodgeNavCheck = false;			// ドッジNavチェック結果を表示
	UPROPERTY(Config) bool bDebugEnemyNoPredictiveDodge = false;		// 予知回避ステップを無効化
	UPROPERTY(Config) bool bDebugEnemyBlowbackX3 = false;				// 吹き飛び発射力を3倍
	UPROPERTY(Config) bool bDebugEnemyNoAttackCooldown = false;			// 攻撃の個別クールダウンを0にする ※グローバルCDはそのまま
	UPROPERTY(Config) bool bDebugDrawAttackRange = false;				// 近中遠レンジ境界を表示
	UPROPERTY(Config) bool bDebugDrawAttackAngle = false;				// 攻撃の上下角(ピッチ)除外コーンを表示
	UPROPERTY(Config) bool bDebugEnemyShowHpText = false;				// 敵頭上HUDのHP数値テキストを表示
	UPROPERTY(Config) bool bDebugDrawRoutePath = false;					// ルート待機移動の目標線と到達距離を表示
	UPROPERTY(Config) bool bDebugDrawEnemyFOV = false;					// エネミー視野 (扇) をデカール表示
	UPROPERTY(Config) bool bDebugDrawReturnHome = false;				// 帰巣のパス検索結果(成功=緑/失敗=赤)とワープを表示
	UPROPERTY(Config) bool bDebugDrawDetectionGauge = false;			// 検知ゲージを敵の頭上に表示 (未交戦時のみ動く)

	// Combat
	UPROPERTY(Config) bool bDebugDrawAttackHitbox = false;				// 攻撃判定表示
	UPROPERTY(Config) bool bDebugDrawHaloHitbox = false;				// 光輪の当たり判定(バリア球/タッチAOE/背後コーン)表示
	UPROPERTY(Config) bool bDebugDrawBombardLanding = false;			// 爆撃着弾点表示
	UPROPERTY(Config) bool bDebugDisableHitReaction = false;			// ヒットリアクション無効
	UPROPERTY(Config) bool bDebugEnableBlowbackCollisionDamage = false;	// 吹き飛び衝突ダメージ有効
	UPROPERTY(Config) bool bDebugShowDamageNumbers = false;				// ダメージ数値をImGuiオーバーレイ表示
	UPROPERTY(Config) bool bDebugFlagDisableHitStop = false;			// ヒットストップ無効 ※HitStopUtil経由の適用を一括停止。外の演出はサポート外

	// Camera
	UPROPERTY(Config) bool bDebugFlagDisableFOVChange = false;			// FOV変更無効
	UPROPERTY(Config) bool bDebugFlagDisableLag = false;				// カメラ遅延無効
	UPROPERTY(Config) bool bDebugFlagDrawJumpFocus = false;				// ジャンプ注視点デッドゾーン描画
	UPROPERTY(Config) bool bDebugFlagDrawCameraPivotFocus = false;		// TPSカメラの回転軸(Pivot)・注視点(Focus)・カメラ位置を可視化
	UPROPERTY(Config) bool bDebugFlagEnableAirChargeDashCamera = false;	// 空中チャージダッシュのカメラ切り替え(専用寄せカメラ)を行うか ※OFFで通常カメラのまま

	// UI
	// 暫定HUD(ImGui)の表示を一括で隠す
	// 対象=プレイヤーHP/神技ゲージ/チャージ漢字/敵HP/ボスHP
	UPROPERTY(Config) bool bDebugHideUI = false;						// UI非表示
	// HUDカラーを画像モチーフ(ティール/クリーム/ラスト)へ切り替える
	// OFFで従来色へ戻す(Player/Enemy/Boss HPが対象)
	UPROPERTY(Config) bool bUseTideMotifHudColors = true;				// HUDモチーフカラー

	static UTideGameSettings* Get();

};
