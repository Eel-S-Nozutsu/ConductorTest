// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "PRJ_TIDE_P0/Composites/Player/PlayerModules/TidePlayerModule.h"
#include "PRJ_TIDE_P0/Utilities/AutomaticTimer.h"

#include "HitReactionPlayerModule.generated.h"

class ATidePlayerCharacter;

UENUM()
enum class EHitReactionState : uint8
{
	Idle,
	Flinching,		// 通常ののけぞり中
	Blowback_Start,	// 吹き飛び上昇中 (ST)
	Blowback_Loop,	// 吹き飛び落下中 (LP)
	Blowback_End,	// 地面にダウン着地 (ED)
	Recovering		// 起き上がり中
};

UCLASS()
class UHitReactionPlayerModule : public UTidePlayerModule
{
	GENERATED_BODY()

public:
	virtual void Initialize( ATidePlayerCharacter* InOwner ) override;
	virtual void OnModuleUpdate( float DeltaTime ) override;

	// 被弾リアクションを開始する。敵と同じく HitReactionTag を参照してのけぞり（Knockback.S/M/L）／吹き飛び（Blowoff.*）を
	// 選び、NoReaction・未設定タグでは出さない。Instigator を渡すと攻撃者の方向を向く（吹き飛びの方向もこれに追従する）
	void ProcessHit( FGameplayTag ReactionTag, AActor* Instigator = nullptr );
	bool IsInvincible() const;
	bool IsImmune() const;

	// 被弾リアクション（のけぞり／吹き飛び／起き上がり）進行中か
	bool IsReacting() const { return CurrentState != EHitReactionState::Idle; }

#if !UE_BUILD_SHIPPING
	void DrawDebugImGui();
#endif

private:
	void ClearState();
	// 指定モンタージュが「実質再生し終わっている」か（別モーションに切替済み／停止／末尾到達）を判定する
	bool IsMontageFinished( class UAnimMontage* Montage ) const;

	// 吹き飛び開始から一定秒数経過後、回避／ジャンプ入力で吹っ飛びやられを即キャンセルする。
	// キャンセルが成立したら true（呼び元はその場で return する）。
	bool TryBlowbackCancel();

	// 起き上がり終了時にキャラの向きを入力方向（無入力ならカメラ正面）へ揃える。
	// 被弾時に攻撃者を向いたまま復帰すると、正面入力で DashModule の Turn が誤発火するのを防ぐ。
	void OrientForRecoveryExit();

private:
	FAutomaticTimer InvincibilityTimer;
	EHitReactionState CurrentState = EHitReactionState::Idle;

	// Flinching 中に再生しているのけぞりモンタージュ名（小/中/大）。終了判定に使う
	FName CurrentFlinchMontageName = NAME_None;

	// 吹き飛び開始からのキャンセル受付までの待機タイマー（終了したら回避／ジャンプでキャンセル可能）
	FAutomaticTimer BlowbackCancelTimer;
};
