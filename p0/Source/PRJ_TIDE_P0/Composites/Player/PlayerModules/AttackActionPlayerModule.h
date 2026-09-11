// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "PRJ_TIDE_P0/Composites/Player/PlayerModules/TidePlayerModule.h"
#include "PRJ_TIDE_P0/Utilities/AutomaticTimer.h"
#include "PRJ_TIDE_P0/Data/Player/PlayerAttackType.h"

#include "AttackActionPlayerModule.generated.h"

class ATidePlayerCharacter;
class ULockOnTargetComponent;

UENUM( BlueprintType )
enum class EAttackState : uint8
{
	Idle,
	Attacking,	// 攻撃中（判定発生中）
	Recovery,	// 後隙（キャンセル可能、または行動不能）
};

UCLASS()
class PRJ_TIDE_P0_API UAttackActionPlayerModule : public UTidePlayerModule
{
	GENERATED_BODY()

public:
	virtual void Initialize( ATidePlayerCharacter* InOwner ) override;
	virtual void OnModuleUpdate( float DeltaTime ) override;

	// bForce=true で CanAttack/CanCombo タイミングタグの要求を迂回して強制的に攻撃を確立する。
	// 滑空セッションからの振り下ろしキャンセルなど、発動可否を呼び出し側で既に判定済みのケースで使う
	bool RequestAttack( EPlayerAttackType InType, bool bForce = false );

	void CancelAttack();

	void OnAttackHit( AActor* TargetActor, bool bIsRebounded ) override;

	// 攻撃ヒットバック（自己リコイル）を即座に打ち消す。
	// とどめヒット時に同フレームで呼ばれ、開始済みのヒットバックを見た目に出さない用途。
	void CancelHitBack();

	// 開始済みのヒットバック（後退）を前進へ差し替える。とどめ・壊れ物破壊で「前へ抜ける」用途。
	// このモジュールがヒットバックを張っていないときは何もしない（前進の二重適用を防ぐ）。
	void ApplyBreakthroughMove( const FVector& Direction, float Speed );

	bool IsAttacking() const { return CurrentState != EAttackState::Idle; }
	EAttackState GetCurrentState() const { return CurrentState; }

	int32 GetCurrentLightComboIndex() const { return CurrentLightComboIndex; }
	// 弱攻撃コンボの最大段数（DA: MaxLightComboCount）。DA 未設定時は 2 段
	int32 GetMaxLightComboCount() const;

#if !UE_BUILD_SHIPPING
	void DrawDebugImGui();
#endif

private:
	void OnStartAttack( EPlayerAttackType InType );
	void OnEndAttack();

	float PlayAttackMontage( EPlayerAttackType InType );

	// 現在のコンボ段数に対応する弱攻撃モンタージュタグ（GetChargeAttackAnimTag と同流儀）
	FName GetLightAttackAnimTag() const;

	// 弱攻撃の発動直後、前段モンタージュのタグが残留している窓か（DA: LightAttackTagResidueIgnoreTime）。
	// IsWithinChargeActionTagResidueWindow と同趣旨で、この間は次段への進行を受け付けない
	bool IsWithinLightAttackTagResidueWindow() const;

	FVector GetHomingDirection( const FVector& InDefaultDir, ULockOnTargetComponent*& OutTargetComp ) const;

	// 吸着対象へ届くようルートモーションの前進量を伸ばす倍率と、それを効かせる区間を決める
	// （DA: Action|LightAttack|Approach）。素の前進で足りないぶんだけ伸ばす（縮めない・上限クランプあり）
	void SetupLightAttackApproach( ULockOnTargetComponent* TargetComp );

	bool IsFrontBlockedByPawn() const;

private:
	EAttackState CurrentState = EAttackState::Idle;

	EPlayerAttackType CurrentAttackType = EPlayerAttackType::None;

	UPROPERTY()
	TObjectPtr<ULockOnTargetComponent> CurrentHomingTarget;

	// 攻撃の持続・後隙管理用タイマー
	FAutomaticTimer AttackTimer;

	bool bWasDashingBeforeAttack = false;	// 攻撃開始時にダッシュ中だったかどうかのフラグ

	int32 CurrentLightComboIndex = 1;

	// 弱攻撃の対象詰め倍率と、それを効かせる区間の終端（＝判定発生の秒。0 で詰めなし）。
	// OnStartAttack で段ごとに決め、判定が出るまでのルートモーションにだけ掛ける
	float LightAttackApproachScale = 1.0f;
	float LightAttackApproachEndTime = 0.0f;

	FAutomaticTimer HitBackTimer;
	FVector CurrentHitBackVelocity = FVector::ZeroVector;
	// 現在のヒットバック枠が「前進（切り抜け）」へ差し替え済みか。とどめは先読みと敵側通知で同フレームに 2 回解決される
	// ため、2 回目の適用を弾くために持つ
	bool bHitBackIsBreakthrough = false;
};
