// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ProjectileProfile.generated.h"

class AEnemyProjectile;
class UProjectileBehavior;

/**
 * 弾の種類をデータとして定義するDataAsset
 *
 * パラメータと飛び方/効果を表す
 * ビヘイビアの合成を1つのアセットに持つ これにより弾ごとにC++サブクラスやBPを
 * 作らずにアセット上でモジュールを組み合わせるだけで新しい弾を定義可能
 *
 * AEnemyProjectile::InitFromProfile()が値を適用しビヘイビアを弾へ複製する
 */
UCLASS(BlueprintType)
class PRJ_TIDE_P0_API UProjectileProfile : public UDataAsset
{
	GENERATED_BODY()

public:

	// 生成する弾アクタ 未設定なら呼び出し側がAEnemyProjectileを使用
	UPROPERTY(EditAnywhere, Category = "Projectile")
	TSubclassOf<AEnemyProjectile> ProjectileActorClass;

	// 基本ダメージ 攻撃側が上書きしなければこの値を使用
	UPROPERTY(EditAnywhere, Category = "Projectile")
	float Damage = 10.0f;

	// 初速
	UPROPERTY(EditAnywhere, Category = "Projectile")
	float InitialSpeed = 300.0f;

	// 最大生存秒数
	UPROPERTY(EditAnywhere, Category = "Projectile")
	float MaxLifeTime = 5.0f;

	// 当たり判定ボックスの半分 0以下なら弾アクタ既定の判定を使用
	UPROPERTY(EditAnywhere, Category = "Projectile", meta = (ClampMin = "0.0"))
	FVector CollisionExtent = FVector::ZeroVector;

	// ドットダメージを与える秒間隔。APersistentProjectile等、
	// 対応するアクタのみが使用する
	UPROPERTY(EditAnywhere, Category = "Projectile", meta = (ClampMin = "0.01"))
	float DotTickInterval = 0.5f;

	// ドットダメージでヒットリアクションを取らせるか
	// APersistentProjectile等、対応するアクタのみが使用する
	// false(既定)なら継続ダメージ扱いにしヒットリアクション・カメラ揺れを毎ティック抑制する
	UPROPERTY(EditAnywhere, Category = "Projectile")
	bool bDotUsesHitReaction = false;

	// 飛び方 (排他1個) nullなら直進 (ProjectileMovement任せ)
	UPROPERTY(EditAnywhere, Instanced, Category = "Behaviors")
	TObjectPtr<UProjectileBehavior> Movement;

	// 効果 (複数可) 常駐/接触時効果やカメラ振動など
	UPROPERTY(EditAnywhere, Instanced, Category = "Behaviors")
	TArray<TObjectPtr<UProjectileBehavior>> Effects;

};
