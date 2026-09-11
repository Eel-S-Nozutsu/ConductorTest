// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "PRJ_TIDE_P0/Composites/Player/PlayerModules/SlidePassiveSubModule.h"

#include "SlidePassiveGustSubModule.generated.h"

class UNiagaraComponent;

// スライドパッシブ：突風。足元で左右交互の切り返し（S 字。回数制で 3 なら左右左）を描くと発動し、
// 「風まとい」VFX をまとわせてチャージギアを最大へ引き上げ、次の1回のチャージアクションへ範囲攻撃バフを付与する。
//
// 検出は親から配信される符号付き旋回量を弧ごとに積算し、「片側へ十分旋回した弧」が一定サンプル窓内で
// 逆向きに連続するごとに数え、GustRequiredArcCount 本で成立する（最初の弧の向きは左右どちらでもよい）。
// bGustUseStickFlickTrigger が ON の間は軌跡検出を止め、「左スティック↓→↑」入力で成立させる（検証用）
UCLASS()
class USlidePassiveGustSubModule : public USlidePassiveSubModule
{
	GENERATED_BODY()

public:
	virtual bool IsEnabled() const override;
	// チャージ中に描き切ったときだけ成立させる（猶予中は竜巻のみ）
	virtual bool CanDetectDuringChargeDashGrace() const override { return false; }
	virtual void OnTrackingStarted() override;
	virtual void OnSampleAdded( const FVector& Foot, float SignedTurnDeltaDeg ) override;
	virtual void OnTrajectoryReset() override;
	virtual void OnUpdate( float DeltaTime ) override;

#if !UE_BUILD_SHIPPING
	virtual void DrawDebugImGui() override;
#endif

private:
	void TriggerGust();			// 風まとい VFX ＋ ギアMAX ＋ 次アクションバフ arm
	void StopGustCloak();
	void SpawnGustFootMark();	// バフ中、足元に追従させる小さな印
	void StopGustFootMark();
	void ResetDetection();
	void UpdateStickFlickDetection( float DeltaTime );	// bGustUseStickFlickTrigger 時のみ
	void ResetStickFlick();

private:
	// --- S 字検出ステート ---
	FVector LastFoot = FVector::ZeroVector;
	bool bHasLastFoot = false;

	int32 CurrentSegSign = 0;	// 現在の弧の旋回方向（+1/-1、0=未定）
	float SegTurnDeg = 0.0f;	// 現在の弧の符号付き積算旋回量
	float SegDistance = 0.0f;	// 現在の弧の積算移動距離（ジッタ除け）

	// 前の弧と逆向きに成立した本数を数え、GustRequiredArcCount 本で発動する（2=左右/右左、3=左右左 …）
	int32 ArcChainCount = 0;		// 0=未確定
	int32 LastArcSign = 0;			// 直近に数えた弧の方向（次は逆向きを要求）
	int32 SamplesSinceLastArc = 0;	// 窓判定用

	// --- ↓→↑ 入力検出ステート（検証フラグ用）---
	// スティック1回転（倒したまま円周を回る）を弾くため、↓と↑の間にニュートラル通過を要求する
	enum class EStickFlickPhase : uint8 { WaitDown, WaitNeutral, WaitUp };
	EStickFlickPhase StickFlickPhase = EStickFlickPhase::WaitDown;
	float StickFlickWindowTimer = 0.0f;		// ↓検知後の残り猶予時間
	float StickFlickMaxLateral = 0.0f;		// ↓検知後に観測した横入力の最大値（回転の検出用）

#if !UE_BUILD_SHIPPING
	float TriggerFlashTimer = 0.0f;	// 発動フィードバック（デバッグ表示）
#endif

	// 再発動時に張り替える
	UPROPERTY( Transient )
	TObjectPtr<UNiagaraComponent> GustCloakVFX;

	// バフ消費時は寿命でフェードアウトする
	UPROPERTY( Transient )
	TObjectPtr<UNiagaraComponent> GustFootMarkVFX;

	float GustFootMarkSpawnTime = 0.0f;		// 経過秒の算出用
	float GustFootMarkLifeTime = 0.0f;		// HOLD 保持の再延長で伸びる。正規化年齢の算出基準
	bool bGustFootMarkFadingOut = false;	// 多重指示防止
	float GustFootMarkFadeTimer = 0.0f;		// 尽きたら破棄する（焼き込み寿命アセットの保険）
};
