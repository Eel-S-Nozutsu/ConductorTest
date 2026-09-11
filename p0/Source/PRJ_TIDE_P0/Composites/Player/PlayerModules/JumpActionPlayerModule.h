// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "PRJ_TIDE_P0/Composites/Player/PlayerModules/TidePlayerModule.h"
#include "PRJ_TIDE_P0/Utilities/AutomaticTimer.h"
#include "JumpActionPlayerModule.generated.h"

class UNiagaraComponent;

enum class EJumpMontageState : uint8
{
	None,
	Start,		// 上昇中
	Loop,		// 落下・滞空
	End			// 着地
};

UCLASS()
class UJumpActionPlayerModule : public UTidePlayerModule
{
	GENERATED_BODY()

public:
	virtual void Initialize( ATidePlayerCharacter* InOwner ) override;
	virtual void OnModuleUpdate( float DeltaTime ) override;

	void RequestJump();

	// CanJump を無視して飛ばす（吹き飛びキャンセルなど状態タグが揃っていない状況用）。
	// bOverrideXY=true で水平速度も上書きし、吹き飛びの横方向の勢いを断ち切る
	void ForceJump( bool bOverrideXY = false );
	void OnLanded();

	// 他アクション（一閃の空中終了など）から落下ループへ引き継ぐ。
	// 呼び出し側が空中であること・MOVE_Falling であることを保証する想定
	void EnterFallingLoop();

	// 他アクション（面沿いのエリア退出など）からチャージジャンプへ引き継ぐ。Start 状態に入ることで
	// ST の終了検知・LP への遷移・着地の ED まで本モジュールが面倒を見る
	void EnterChargeJumpStart();

	// 他アクションの着地から通常のジャンプ着地（End 状態）へ引き継ぐ。これで
	// 「MoveCancelable＋移動入力で ED をキャンセルして走る」処理が働くようになる
	void EnterLandingEnd();

	void ConsumeAirJumps();

	// 竜巻ジャンプ／打ち上げの風切りトレイル（チャージダッシュの ChargeDashWind を流用）。
	// チャージダッシュ中はあちらが同じものを出しているので出さない。破棄は再生秒数切れ／着地／死亡
	void StartWindTrailEffect();
	void StopWindTrailEffect();

	void ReserveAutoDashOnLanding() { bReserveAutoDashOnLanding = true; }

	// 空中で神技を出した後の着地で、残っていた状態と予約が JUMP_ED やダッシュを暴発させるのを防ぐ
	void ClearJumpAndLandingDash()
	{
		CurrentJumpState = EJumpMontageState::None;
		bReserveAutoDashOnLanding = false;
	}

	// 吹き飛びキャンセルの緊急ジャンプは CanMove タグが付かない状態から飛ぶため、
	// この間は空中での横移動入力（エアコントロール）を特別に許可する
	bool IsHitCancelAirMove() const { return bHitCancelAirMove; }

	// 着地でリセットする。チャージ側が「ジャンプ→チャージジャンプ」を塞ぐ判定に使う
	bool HasJumpedThisAirtime() const { return bHasJumpedThisAirtime; }

private:
	bool CanJump() const;
	void ExecuteJump( bool bOverrideXY = false );
	void ResetJumpCount();
	void UpdateWindTrailEffect( float DeltaTime );

private:
	int32 CurrentJumpCount = 0;

	EJumpMontageState CurrentJumpState = EJumpMontageState::None;

	bool bReserveAutoDashOnLanding = false;

	// 竜巻内ジャンプか（チャージジャンプのモーションで遷移する）
	bool bWindJumpMotion = false;

	bool bHitCancelAirMove = false;	// 着地で解除。空中横移動の特別許可に使う

	// ST/LP/ED にチャージジャンプのモーションを使う（着地で解除）。吹き飛びやられからの復帰ジャンプと、
	// 面沿いモードのエリア退出が立てる
	bool bForceChargeJumpMotion = false;

	// CurrentJumpCount は ConsumeAirJumps でも埋まる（実際には飛んでいない）ため、
	// 「実際に飛んだ」ことだけを表す別フラグで持つ
	bool bHasJumpedThisAirtime = false;

	// 風切りトレイル（ループ）と、その再生秒数タイマー（無効なら着地まで流す）
	UPROPERTY( Transient )
	TObjectPtr<UNiagaraComponent> SpawnedWindTrailEffect;
	FAutomaticTimer WindTrailTimer;
};
