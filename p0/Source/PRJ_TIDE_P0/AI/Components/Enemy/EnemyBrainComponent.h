// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PRJ_TIDE_P0/AI/States/EnemyAIStateTypes.h"
#include "PRJ_TIDE_P0/Data/Enemy/EnemyDataAsset.h"
#include "EnemyBrainComponent.generated.h"

class UEnemyAIStateBase;
class AEnemyAIController;
class AEnemyCharacter;
class UEnemyBattleComponent;
class UEnemyDataAsset;
class ATidePlayerController;

/**
 * BehaviorTreeに代わる敵AIのフロー制御。AIControllerに載る。
 *
 * 毎ティックSelectDesiredState() で優先度順に状態を選び直し、現在の状態が割り込みを
 * 許すなら遷移する。これがBTのSelector + Observer abortsの置き換えにあたる。
 *
 * 「行動できない」はReactという状態であって外部スイッチではないため、
 * StopLogic / RestartLogicの対管理(9種の理由が1つの非参照カウントなスイッチを
 * 奪い合う)は発生しない。詳細はDocs/EnemyAIRedesign.md。
 */
UCLASS()
class PRJ_TIDE_P0_API UEnemyBrainComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UEnemyBrainComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Possess後にAIControllerから呼ぶ。状態インスタンスを生成する
	void StartLogic();

	void StopLogicForDeath();

	// ---- 状態から使う窓口 ----

	AEnemyAIController* GetAIController() const;
	AEnemyCharacter* GetEnemy() const;
	AActor* GetTargetActor() const;
	const UEnemyDataAsset* GetEnemyData() const;
	UEnemyBattleComponent* GetBattleComponent() const;

	bool IsBoss() const { return bIsBoss; }

	// テリトリー。半径0=テリトリー無し
	// 徘徊圏(Inner)は非交戦時の行動範囲、交戦圏(Outer)は交戦中の追跡上限
	FVector GetPatrolOrigin() const;
	float GetPatrolRadius() const;
	float GetCombatRadius() const;

	// ---- 移動 ----完了はメッセージ待ちではなくIsMoveInProgress()
	// のポーリングで見る。毎ティック優先度を評価する作りなので、完了通知を待つ必要がない

	// bCanStrafe=trueで「移動方向へ体を向けない」
	// 注視(SetFocus)が向きを握るのでターゲットを向いたまま横移動できる
	// bStrictAcceptance=trueで到達判定からエージェント半径を除く
	// 既定falseのMoveToは「AcceptanceRadius+カプセル半径」で停止し、
	// Dist2D判定側と半径を揃えても一致しないため
	bool RequestMoveTo(const FVector& Goal, float AcceptanceRadius = 50.0f, bool bCanStrafe = false,
		bool bStrictAcceptance = false);
	bool RequestMoveToActor(AActor* Goal, float AcceptanceRadius = 50.0f, bool bCanStrafe = false);
	void StopMovement();
	bool IsMoveInProgress() const;

	// 移動速度をDAの値で設定する。状態のEnterで1回呼ぶ
	void ApplyMoveSpeed(EEnemySpeedType Speed);

	// ステートマシンが稼働中か (StartLogic済みか)
	// 未稼働中に問い合わせた側が誤って動作を進めないためのガード
	bool IsRunning() const { return CurrentStateId != EEnemyAIState::None; }

	// UEnemyAIState_StrafeがEnter/Exitで更新する
	// AnimInstanceはこれを読むだけで自前の距離判定は持たない (条件がズレると足滑りするため)
	void SetStrafing(bool bInStrafing) { bIsStrafing = bInStrafing; }
	bool IsStrafing() const { return bIsStrafing; }

	// ---- 攻撃要求 ----

	// ImGuiのデバッグ強制攻撃と、火炎放射の協力攻撃の徴集が呼ぶ
	// その場で攻撃状態へ入れられる (待ちの間接が無い)
	void RequestForceAttack(int32 AttackIndex);

	void RequestCounterAttack(int32 AttackIndex);

	// Attack状態が終了時に呼ぶ。いま実行中の要求1つだけを取り下げる
	// これでAttackが選ばれ続けなくなる = 状態は自分の条件を無効化して終わる
	void ConsumeActiveAttackRequest();

	// 死亡など、全要求をまとめて捨てる場合
	void ClearAllAttackRequests();

	int32 GetActiveAttackIndex() const;

	// いま実行中の要求が強制攻撃か (トークン制限をスキップする判定に使う)
	bool IsActiveAttackForced() const { return ForceAttackIndex >= 0; }

	// ---- デバッグ ----

	EEnemyAIState GetCurrentStateId() const { return CurrentStateId; }
	FString GetCurrentStateDebugText() const;

private:

	// 優先度順に条件を評価して、いま入るべき状態を返す。優先度の定義はここ1箇所だけ
	EEnemyAIState SelectDesiredState();

	bool CanInterruptCurrentState(EEnemyAIState Desired) const;

	void TransitionTo(EEnemyAIState Next);

	UEnemyAIStateBase* FindState(EEnemyAIState Id) const;

	// 一定間隔で攻撃抽選を回す。状態を問わず走らせる
	void TickPickAttack(float DeltaSeconds);

	// AIDirectorの戦闘参加者リストを実態に合わせる。ターゲットの有無が参加条件
	void SyncCombatantRegistration();

	// 参加者リストから確実に降りる。死亡・破棄の両方から呼ぶ
	void LeaveCombatantRegistry();

	// ---- 条件判定 ----いずれもヒステリシスのため前回結果を持つ
	// (閾値の往復でパタつくのを防ぐ)

	bool IsReacting() const;

	// カットシーン中か。PCのシネマモードをポーリングで見る (毎ティックbool 1個の読み取りで軽い)
	bool IsCinematicSuspended() const;

	// 自身またはターゲットが行動圏外か。境界は交戦中=交戦圏(Outer)、非交戦=徘徊圏(Inner)
	bool IsOutsidePatrolArea() const;
	bool IsTooClose();
	bool IsInEngageRange();
	bool IsTooFar();

	UPROPERTY(Transient)
	TMap<EEnemyAIState, TObjectPtr<UEnemyAIStateBase>> States;

	UPROPERTY(Transient)
	TObjectPtr<UEnemyAIStateBase> CurrentState = nullptr;

	EEnemyAIState CurrentStateId = EEnemyAIState::None;

	// StartLogicでキャッシュ。TideCharacter側はprotected保持のため
	// FindComponentByClassで引く
	UPROPERTY(Transient)
	TObjectPtr<class UHitReactionComponent> HitReaction = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<class UStatusComponent> Status = nullptr;

	// シネマモード問い合わせ先。StartLogicでキャッシュし、nullなら遅延で引き直す
	// (敵がPC生成前にスポーンした場合の保険)。所有関係はないので弱参照で持つ
	mutable TWeakObjectPtr<ATidePlayerController> CachedPlayerController;

	// 攻撃要求 (-1 = 要求なし)
	int32 PendingAttackIndex = -1;
	int32 CounterAttackIndex = -1;
	int32 ForceAttackIndex   = -1;

	// ヒステリシス用の前回結果
	bool bWasTooClose     = false;
	bool bWasInEngageRange = false;
	bool bWasTooFar       = false;

	// 攻撃抽選の間隔カウンタ
	float PickAttackAccum = 0.0f;

	bool bIsBoss = false;

	bool bIsStrafing = false;

};
