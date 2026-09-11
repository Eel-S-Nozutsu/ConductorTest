// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "PRJ_TIDE_P0/AI/States/EnemyAIStateBase.h"
#include "EnemyAIState_Attack.generated.h"

class AEnemyCharacter;
class UAnimMontage;
class UTideAttackExecution;
struct FAttackEntry;

/**
 * 選択された攻撃を実行する。BTTask_ExecuteAttack相当。
 *
 * BTでは「正常終了 = FinishLatentTask」「中断 = AbortTask」と入口が2つあり、
 * 両方で同じ後始末 (モンタージュ停止・デリゲート解除・トークン返却・キーリセット) を
 * 書く必要があった。ステートマシンでは中断も遷移なので、出口はFinishAttack 1つに畳んである。
 *
 * 終了条件:
 *   - モンタージュ終了
 *   - asyncモード (GetMontage() == nullptr) はExecutionのFinishDelegate
 *   - State.Enemy.CanTransitionによる早期遷移
 */
UCLASS()
class PRJ_TIDE_P0_API UEnemyAIState_Attack : public UEnemyAIStateBase
{
	GENERATED_BODY()

public:

	virtual void Enter(UEnemyBrainComponent& Brain) override;
	virtual void Tick(UEnemyBrainComponent& Brain, float DeltaSeconds) override;
	virtual void Exit(UEnemyBrainComponent& Brain) override;

	// 攻撃の実行中だけ割り込みを拒む。FinishAttack後はfalseになり、
	// 優先度の再評価がそのまま次の状態へ移せる
	virtual bool IsBusy() const override { return bActive; }

	virtual FString GetDebugText() const override;

private:

	// 攻撃本体を開始する (ClearFocus → OnAttackBegin → モンタージュ再生)
	// 向き直りを挟む場合はその終了後に、挟まない場合はEnterから直接呼ぶ
	void BeginExecution();

	// 攻撃前に向き直りモーションを再生すべきなら、そのmontageを返す
	// (不要or未設定ならnullptr)。PCが正面からAISettingsの向き直り角度以上ズレている
	// ときだけ発動する
	UAnimMontage* GetTurnToTargetMontage(AEnemyCharacter* Enemy, const FAttackEntry& Entry) const;

	// 唯一の出口。攻撃終了の通知・デリゲート解除・トークン返却・要求の取り下げを行う
	void FinishAttack(bool bInterrupted);

	UFUNCTION()
	void OnMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	// OnAttackEventをActiveExecutionへ転送
	void ForwardAttackEvent(FGameplayTag Tag);

	// ActiveExecutionへ攻撃終了を1回だけ通知する
	void NotifyAttackEnd(bool bInterrupted);

	UPROPERTY(Transient)
	TObjectPtr<UEnemyBrainComponent> OwningBrain = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<AEnemyCharacter> CachedEnemy = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UAnimInstance> AnimInstance = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> AttackMontage = nullptr;

	// 攻撃本体の前に再生中の向き直りモーション
	// 終了でBeginExecutionへ繋ぐ (未使用時nullptr)
	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> TurnMontage = nullptr;

	// FAttackEntry::ExecutionClassから生成したインスタンス。攻撃中のみ有効
	UPROPERTY(Transient)
	TObjectPtr<UTideAttackExecution> ActiveExecution = nullptr;

	int32 ActiveAttackIndex = -1;

	// 攻撃を実行中か。Exitが「正常終了済みの後片付け」と「中断」を区別するのに使う
	bool bActive = false;

	// OnAttackBeginを呼んだ後true
	bool bExecutionStarted = false;

	// 攻撃本体の前の向き直りモーション再生中 (この間はTickでExecutionを触らない)
	bool bTurningToTarget = false;

	// OnAttackEnd通知済み (二重通知の抑止)
	bool bAttackEndNotified = false;

	// TryAcquireAttackTokenが成功した場合のみtrue (返却要否)
	bool bTokenAcquired = false;

	// State.Enemy.CanTransitionによる早期遷移を許すか
	static constexpr bool bAllowEarlyTransitionByTag = true;

};
