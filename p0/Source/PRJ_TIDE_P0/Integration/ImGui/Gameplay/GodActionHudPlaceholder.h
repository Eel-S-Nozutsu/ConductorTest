// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#pragma once

#if !UE_BUILD_SHIPPING

#include "CoreMinimal.h"

class APlayerController;

/**
 * 神技 HUD の暫定表示。正式 UI が未実装の間、ImGui でゲージ・ロックオンレティクル等を描画しておく。
 *
 * 正式 UI を実装したら、このファイル（.h / .cpp）を削除し、TidePlayerCharacter::UpdateUI() からの
 * Draw() 呼び出し 1 行を外せば撤去は完了する。
 */
namespace GodActionHudPlaceholder
{
	// HUD 描画に必要な神技モジュールの状態スナップショット
	struct FGodActionHudData
	{
		float GaugeRate = 0.0f;			// 0.0〜1.0
		bool bGaugeFull = false;		// 発動可能か
		bool bLockingOn = false;		// マルチロックオン中か
		bool bSlashing = false;			// 一閃発動中か
		int32 LockedCount = 0;			// 現在のロックオン数
		int32 MaxLockOnCount = 0;		// 現ギアでの最大ロックオン数
		TArray<FVector> LockedTargetWorldLocations;	// ロックオン中ターゲットのワールド座標

		// --- 神鳥検証モード（3択神技メニュー） ---
		bool bGodArtSelecting = false;	// 構え中。セレクタを一回り大きくし、真ん中を持ち上げて山なりにする
		int32 SelectedArt = 0;			// カーソル（0=戯れ / 1=導き / 2=憑依）
		bool bFaceButtonSelect = false;	// X/Y/B 直接選択モード。アイコンにボタン記号を出し ◀▶ は出さない
		bool bBirdAway = false;			// 共有の神鳥が出払い中。戯/導をグレーアウトする（憑依は対象外）

		// --- ゲージ蓄積演出（エネルギー玉）---
		// StartWorldLoc を投影し、Progress でゲージへ補間描画する
		struct FOrbView
		{
			FVector StartWorldLoc = FVector::ZeroVector;
			float Progress = 0.0f;		// 0..1
			float Lateral = 0.0f;		// 発生点→ゲージの線に対する横ぶれ（+/-で線の左右に分かれる）
			float RadiusScale = 1.0f;	// 半径倍率（BoostGimmick 経路など目立たせたい玉で >1）
			float AlphaScale = 1.0f;	// 不透明度倍率（同上）
		};
		TArray<FOrbView> Orbs;
		float OrbRadius = 14.0f;		// 基本半径（px@1080p。解像度でスケール）
		float OrbCurveStrength = 0.35f;	// 飛行カーブの膨らみ（始点→終点距離に対する張り出し比）
		float OrbFlashTime = 0.45f;		// ゲージ増加フラッシュの長さ（秒）
		float OrbAlpha = 0.5f;			// 0〜1。小さいほど半透明
	};

	// World は点滅アニメ用の時間、PC はワールド→スクリーン投影に使う。
	// bWantVisible が false のときは out アニメを再生しながら消えるので、消え切るまで毎フレーム呼び続けること
	void Draw( const UWorld* World, APlayerController* PC, const FGodActionHudData& Data, bool bWantVisible );
}

#endif // !UE_BUILD_SHIPPING
