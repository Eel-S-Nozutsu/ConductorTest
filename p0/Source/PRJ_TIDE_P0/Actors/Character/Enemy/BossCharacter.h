// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PRJ_TIDE_P0/Actors/Character/Enemy/EnemyCharacter.h"
#include "BossCharacter.generated.h"

class UBossPhaseComponent;
class UPartDestructionComponent;
class ULockOnTargetComponent;
class ATidePlayerCharacter;

/**
 * ボス敵の基底クラス
 * フェーズ管理・部位破壊・AttackEvent配線を担う
 */
UCLASS(Abstract)
class PRJ_TIDE_P0_API ABossCharacter : public AEnemyCharacter
{
	GENERATED_BODY()

public:

	ABossCharacter();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void DrawImGuiInspector() override;

	// ボスは暫定HUDでボス用HPゲージを使用
	virtual bool UsesBossHpGauge() const override { return true; }

	UBossPhaseComponent* GetBossPhaseComponent() const { return BossPhaseComponent; }

	// プレッシャーゲージ現在値。UEnemyAIState_BossIdleが参照
	int32 GetStepPressure() const { return StepPressure; }

	// プレッシャーゲージをリセット。ステップ発火時にUEnemyAIState_BossIdleが呼ぶ
	void ResetStepPressure() { StepPressure = 0; }

	// 旋回開始。TickがTargetYawへRotationRate(度/秒)で回し、
	// TurnMontage終了/中断か目標到達で自動停止
	void BeginTurn(UAnimMontage* TurnMontage, float TargetYaw, float InRotationRate);

	// Ph1足場ジャンプ: 指定座標へのジャンプをキューへ積む
	// ABossJumpTriggerがPC通過時に呼ぶ
	// ジャンプ中でなければ即開始し、複数は1足場ずつ順に消化(PCが先行しても順に追いつく)
	void EnqueuePlatformJump(const FVector& TargetLocation);

	// 部位破壊/連動リアクションのモンタージュ再生中はtrue (殴れる窓の遅延再硬化判定に使う)
	virtual bool IsPlayingPartReaction() const override { return ActivePartReactionMontage != nullptr; }

protected:

	UPROPERTY(VisibleAnywhere, Category = "Tide|Components")
	TObjectPtr<UBossPhaseComponent> BossPhaseComponent = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Tide|Components")
	TObjectPtr<UPartDestructionComponent> PartDestructionComponent = nullptr;

	// OnAttackEventをBossPhaseComponentに転送
	// 派生クラスはSuperを呼んだ後に固有タグを処理すること
	virtual void HandleAttackEvent(FGameplayTag Tag);

	// 攻撃中は破壊部位の光輪復活を保留する (生え直すと技の分岐と食い違うため)
	virtual void SetExecutingAttack(bool bInExecuting) override;

private:

	void OnPhaseChanged(int32 OldPhase, int32 NewPhase);
	void OnPartDestroyedResponse(FName PartTag);
	void OnPartComboDestroyedResponse(int32 ComboIndex);

	// 攻撃実行中にスキップすべき連動リアクションか(片足やられが攻撃を上書きするのを防ぐ)
	// bSkipWhenExecutingAttackが立ち、かつ攻撃実行中でtrue
	bool ShouldSkipComboReaction(int32 ComboIndex) const;

	UFUNCTION()
	void OnPartReactionMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	// 条件付き部位(FPartEntry::bRequiresDownOrGodLock)の
	// bIsTargetableを毎フレーム更新
	// 複数部位破壊ダウン中orプレイヤー神技ロックオン中だけtrue
	void UpdateConditionalPartTargetable();

	// BeginPlayで収集する条件付き部位のLockOnTarget
	UPROPERTY()
	TArray<TObjectPtr<ULockOnTargetComponent>> ConditionalLockOnTargets;

	// 神技ロックオン状態を毎フレーム問い合わせるプレイヤー参照(遅延取得)
	TWeakObjectPtr<ATidePlayerCharacter> CachedPlayer;

	TObjectPtr<UAnimMontage> ActivePartReactionMontage = nullptr;

	// 連動リアクション(ダウン)終了時に発動する強制攻撃インデックス(-1=なし)
	// OnPartComboDestroyedResponseで設定
	// →OnPartReactionMontageEndedで消費
	int32 PendingForceAttackOnReactionEnd = -1;

	// ヒット回数の蓄積。OnDamageReceivedごとに+1、ステップ発火時にリセット
	int32 StepPressure = 0;

	UFUNCTION()
	void OnDamageReceivedForStep(const struct FDamageInfo& DamageInfo);

	bool  bIsTurning       = false;
	float TurnTargetYaw    = 0.0f;
	float TurnRotationRate = 180.0f;
	TObjectPtr<UAnimMontage> ActiveTurnMontage = nullptr;

	// --- Ph1足場ジャンプ移動 ---

	// ジャンプの進行状態
	// None:非ジャンプ / WindUp:踏ん張り再生中(離陸合図待ち)
	// Airborne:放物線で着地点へ移動中
	enum class EPlatformJumpPhase : uint8 { None, WindUp, Airborne };

	// キュー先頭ターゲットへのジャンプ(踏ん張り)を開始
	void BeginPlatformJump(const FVector& Target);

	// 離陸合図を受けて放物線移動を開始(WindUp→Airborne)
	void LaunchPlatformJump();

	// Tickから呼ぶジャンプ更新(踏ん張りタイムアウト監視＋放物線移動)
	void TickPlatformJump(float DeltaSeconds);

	// 着地確定。次ジャンプがあれば起動し、なければ通常状態へ戻す
	void FinishPlatformJump();

	EPlatformJumpPhase PlatformJumpPhase = EPlatformJumpPhase::None;
	FVector PlatformJumpStart   = FVector::ZeroVector;
	FVector PlatformJumpTarget  = FVector::ZeroVector;
	float   PlatformJumpElapsed = 0.0f;
	float   PlatformJumpWindUp  = 0.0f;
	TArray<FVector> PendingJumpTargets;

};
