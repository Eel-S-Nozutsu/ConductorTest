// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "Camera/CameraShakeBase.h"
#include "AttackParameterRow.generated.h"

// プレイヤー専用の攻撃パラメータ
USTRUCT( BlueprintType )
struct FPlayerAttackParameterRow : public FTableRowBase
{
	GENERATED_BODY()

	// 攻撃の種類を表すタグ
	UPROPERTY( EditAnywhere, meta = ( Categories = "AttackType.Player" ) )
	FGameplayTag AttackTypeTag;
	// チャージギアレベル
	UPROPERTY( EditAnywhere )
	int32 GearLevel = 1;
	// ダメージ倍率（基礎攻撃力*技ごとの倍率=基礎ダメージ量）
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Damage" )
	float DamageMultiplier = 1.0f;

	// 敵の地上リアクションタグ
	UPROPERTY( EditAnywhere, meta = ( Categories = "HitReaction" ) )
	FGameplayTag EnemyReactionTag;
	// 敵の空中リアクションタグ
	UPROPERTY( EditAnywhere, meta = ( Categories = "HitReaction" ) )
	FGameplayTag EnemyAirReactionTag;
	// 部位耐久削り値
	UPROPERTY( EditAnywhere )
	float StaggerDamage = 0.0f;

	// 神技ゲージ加算量（この攻撃がヒットしたときに溜まる量）。行は AttackTypeTag + GearLevel で分かれるため、
	// 通常攻撃・チャージ各ギアそれぞれに個別設定できる。0 なら加算なし
	UPROPERTY( EditAnywhere, Category = "GodAction" )
	float GodActionGaugeGain = 5.0f;

	// ヒットストップを使用するかどうか
	UPROPERTY( EditAnywhere, Category = "HitStop" )
	bool bUseHitStop = true;
	// ヒットストップ持続時間（秒）
	UPROPERTY( EditAnywhere, Category = "HitStop", meta = ( EditCondition = "bUseHitStop" ) )
	float HitStopDuration = 0.1f;
	// ヒットストップ時の TimeDilation（0.0 = 完全停止）
	UPROPERTY( EditAnywhere, Category = "HitStop", meta = ( EditCondition = "bUseHitStop", ClampMin = "0.0", ClampMax = "1.0" ) )
	float HitStopDilation = 0.1f;

	// 汎用ヒットエフェクト種類(0=無し, 1=小, 2=中, 3=大)
	UPROPERTY( EditAnywhere, Category = "HitEffect" )
	int32 HitEffectType = 1;

	// 攻撃ヒット時に再生するカメラ揺れアセット（未設定で揺れなし）
	UPROPERTY( EditAnywhere, Category = "CameraShake" )
	TSubclassOf<UCameraShakeBase> HitCameraShake;
	// カメラ揺れの強度倍率
	UPROPERTY( EditAnywhere, Category = "CameraShake", meta = ( ClampMin = "0.0" ) )
	float HitCameraShakeScale = 1.0f;

	// ガードブレイク効果があるかどうか

};

// 敵攻撃パラメータ
USTRUCT( BlueprintType )
struct FEnemyAttackParameterRow : public FTableRowBase
{
	GENERATED_BODY()

	// 攻撃の種類を表すタグ (AnimNotifyState_CommonAttack の AttackTypeTag と一致する行を使用)
	UPROPERTY( EditAnywhere, meta = ( Categories = "AttackType.Enemy" ) )
	FGameplayTag AttackTypeTag;

	// ダメージ倍率（基礎攻撃力*技ごとの倍率=基礎ダメージ量）
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Damage" )
	float DamageMultiplier = 1.0f;

	// ヒットストップを使用するかどうか
	UPROPERTY( EditAnywhere, Category = "HitStop" )
	bool bUseHitStop = true;
	// ヒットストップ持続時間（秒）
	UPROPERTY( EditAnywhere, Category = "HitStop", meta = ( EditCondition = "bUseHitStop" ) )
	float HitStopDuration = 0.1f;
	// ヒットストップ時の TimeDilation（0.0 = 完全停止）
	UPROPERTY( EditAnywhere, Category = "HitStop", meta = ( EditCondition = "bUseHitStop", ClampMin = "0.0", ClampMax = "1.0" ) )
	float HitStopDilation = 0.1f;
};
