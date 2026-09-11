// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GameplayTagContainer.h"
#include "AITypes.h"
#include "PRJ_TIDE_P0/Data/Tags/TideGameplayTags.h"
#include "TideAttackExecution.generated.h"

class AEnemyCharacter;
class UAnimMontage;
class UAnimInstance;
struct FPathFollowingResult;

/**
 * 攻撃実行ロジックの基底クラス
 * 
 * 攻撃ごとに生成され、攻撃終了で破棄される。実行時状態のリセットは不要
 */
UCLASS(Abstract, Blueprintable)
class PRJ_TIDE_P0_API UTideAttackExecution : public UObject
{
	GENERATED_BODY()

public:

	virtual UAnimMontage* GetMontage() const { return nullptr; }
	virtual void OnAttackBegin(AEnemyCharacter* Enemy) {}
	virtual void OnAttackTick(AEnemyCharacter* Enemy, float DeltaTime) {}
	virtual void OnAttackEvent(AEnemyCharacter* Enemy, FGameplayTag EventTag) {}
	virtual void OnAttackEnd(AEnemyCharacter* Enemy) {}

	// 攻撃選択時の発動可否。falseを返すと抽選対象から除外される ※CDO経由で問い合わせる
	// NOTE: P0では暫定で攻撃ロジックに発動条件を持たせている。将来は汎用の条件機構へ移す想定
	virtual bool CanActivate(const AEnemyCharacter* Enemy) const { return true; }

	TFunction<void(bool bSuccess)> FinishDelegate;

	// OnAttackEndが中断由来か。中断時は割り込み側 (リアクション等) が
	// 行動抑止を握っているので、ここからAI状態に触らないための判別に使う
	bool bAttackInterrupted = false;

};

/**
 * 標準的なモンタージュ攻撃
 * 
 * 攻撃判定はAnimNotifyで解決
 */
UCLASS(meta = (DisplayName = "モンタージュ攻撃"))
class PRJ_TIDE_P0_API UMontageAttackExecution : public UTideAttackExecution
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere)
	TObjectPtr<UAnimMontage> Montage;

	virtual UAnimMontage* GetMontage() const override { return Montage; }

};

/**
 * 接近→攻撃
 * 
 * 指定距離までAI移動した後に攻撃モンタージュを再生
 * GiveUpRangeを超えた場合は移動を中止してそのまま失敗
 * GetMontage()はnullptrを返すasyncモードで動作しFinishDelegateで呼び出し側に完了を通知
 */
UCLASS(meta = (DisplayName = "接近→攻撃"))
class PRJ_TIDE_P0_API UApproachMontageAttackExecution : public UTideAttackExecution
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere)
	TObjectPtr<UAnimMontage> AttackMontage;

	// この距離以内に入ったら攻撃モンタージュを再生
	UPROPERTY(EditAnywhere)
	float ApproachRadius = 200.0f;

	// この距離を超えたら諦める ※0で無効
	UPROPERTY(EditAnywhere)
	float GiveUpRange = 0.0f;

	virtual UAnimMontage* GetMontage() const override { return nullptr; }
	virtual void OnAttackBegin(AEnemyCharacter* Enemy) override;
	virtual void OnAttackTick(AEnemyCharacter* Enemy, float DeltaTime) override;
	virtual void OnAttackEnd(AEnemyCharacter* Enemy) override;

private:

	void PlayAttack();
	void OnMoveRequestFinished(FAIRequestID RequestID, const FPathFollowingResult& Result);

	UFUNCTION()
	void OnApproachMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	TWeakObjectPtr<AEnemyCharacter> CachedEnemy;
	TWeakObjectPtr<UAnimInstance> CachedAnimInstance;
	FAIRequestID ActiveMoveRequestID;

	bool bReachedTarget = false;

};
