// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "SpawnAppearance.generated.h"

class AEnemyCharacter;
class UAnimMontage;
class ATideSpawnPoint;

/**
 * 敵の登場演出ストラテジー基底クラス。
 * Points[0]=スポーン位置, Points[1]=着地点 など、用途はサブクラスが定義する。
 */
UCLASS(Abstract, EditInlineNew, DefaultToInstanced)
class PRJ_TIDE_P0_API USpawnAppearance : public UObject
{
	GENERATED_BODY()

public:

	virtual void Execute(AEnemyCharacter* Enemy, const TArray<FTransform>& Points) {}

};

/**
 * スポーン位置にそのまま出現する (演出なし)
 */
UCLASS(meta = (DisplayName = "即時スポーン"))
class PRJ_TIDE_P0_API UInstantSpawnAppearance : public USpawnAppearance
{
	GENERATED_BODY()

public:

	virtual void Execute(AEnemyCharacter* Enemy, const TArray<FTransform>& Points) override {}

};

/**
 * 崖上などの高所からジャンプして着地点に降り立つ登場演出
 */
UCLASS(meta = (DisplayName = "崖ジャンプ登場"))
class PRJ_TIDE_P0_API UCliffJumpAppearance : public USpawnAppearance
{
	GENERATED_BODY()

public:

	virtual void Execute(AEnemyCharacter* Enemy, const TArray<FTransform>& Points) override;

	// 着地目標。レベル上に配置したATideSpawnPointを指定する
	UPROPERTY(EditAnywhere)
	TObjectPtr<ATideSpawnPoint> LandingPoint;

	// スポーン位置のZを基準とした頂点高さ。距離によらず常に同じ高さで飛ぶ
	UPROPERTY(EditAnywhere, meta = (ClampMin = "1.0"))
	float PeakHeightOffset = 400.0f;

	// 空中で再生するモンタージュ。未設定で省略
	UPROPERTY(EditAnywhere)
	TObjectPtr<UAnimMontage> JumpMontage;

	// 着地時に再生するモンタージュ。未設定で省略
	UPROPERTY(EditAnywhere)
	TObjectPtr<UAnimMontage> LandMontage;

private:

	void TickJump();
	void FinishJump();
	void RestartAI();

	UFUNCTION()
	void OnLandMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	TWeakObjectPtr<AEnemyCharacter> CachedEnemy;

	FVector JumpStartPos;
	FVector JumpInitVel;
	float   JumpGravityZ  = 0.0f;
	float   JumpStartTime = 0.0f;
	float   JumpDuration  = 0.0f;
	FTimerHandle JumpTickHandle;

};
