// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "HaloHazard.generated.h"

class UStaticMeshComponent;
class UGeometryCollectionComponent;
class UFieldSystemComponent;
class UMaterialInstanceDynamic;
class UNiagaraSystem;

/**
 * 配置型の光輪ハザード。
 * Deploy() で開始位置から上へ発射し、上空から目標地点へ落下して着地する。
 * 着地後は静止して接触ダメージを与える。Shatter() でChaos破砕する
 * (ショックウォール攻撃などから呼ばれる)。
 *
 * 当たり判定はVisualMesh (SM) のシンプルコリジョンを使う。中心の穴をくぐって避ける
 * 遊びにするため、SM側でシンプルコリジョンをリング状 (凸包の集合) にオーサリングすること
 * (複雑コリジョン = trimeshはオーバーラップを生成しないので不可)。
 *
 * 向きの規約: 穴の軸 = アクターのローカル +X。SMはデフォルトで横向きのため、
 * VisualMeshをBPで回転させて穴の軸をローカル +Xに合わせ、リングを立てること。
 *
 * Chaos破砕はABreakablePropと同じGeometryCollection + Field方式。
 * GCCompのRestCollectionはBPで設定すること。
 *
 * Ph1の刺さり光輪 / Ph2の事前配置 / Ph3の配置攻撃で共通利用する。
 */
UCLASS()
class PRJ_TIDE_P0_API AHaloHazard : public AActor
{
	GENERATED_BODY()

public:

	AHaloHazard();

	// 開始位置から上空へ発射し、目標地点へ落下させる。着地後に接触ダメージが有効になる
	// アクターの向き (穴の軸) はスポーン時のRotationで決める
	// StartDelay > 0のとき、その秒数だけ非表示で待機してから発射する (順次落下用)
	void Deploy(const FVector& StartLocation, const FVector& TargetLocation, float StartDelay = 0.0f);

	// Chaos破砕する。衝撃位置と攻撃方向を指定して破片の飛散方向を決める
	void Shatter(const FVector& ImpactLocation, const FVector& AttackDirection);

	// 自身の位置を衝撃位置とした簡易破砕
	void Shatter();

protected:

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	UFUNCTION()
	void OnHazardOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

public:

	// Root。アクターの向き (穴の軸 = ローカル +X) はこのRootの回転で決まる
	UPROPERTY(VisibleAnywhere, Category = "Tide|Halo")
	TObjectPtr<USceneComponent> SceneRoot;

	// 光輪の見た目 + 当たり判定。SMのシンプルコリジョンをリング状にオーサリングすること
	// 横向きメッシュを立てるため、BPでこの相対回転を調整し穴の軸をRootのローカル +Xに合わせる
	UPROPERTY(VisibleAnywhere, Category = "Tide|Halo")
	TObjectPtr<UStaticMeshComponent> VisualMesh;

	// Chaos破片。BPでRestCollectionを設定する
	UPROPERTY(VisibleAnywhere, Category = "Tide|Halo")
	TObjectPtr<UGeometryCollectionComponent> GCComp;

	// Fieldの送出元 (Strain/力場)
	UPROPERTY(VisibleAnywhere, Category = "Tide|Halo")
	TObjectPtr<UFieldSystemComponent> FieldComp;

	// 接触ダメージ量
	UPROPERTY(EditAnywhere, Category = "Tide|Halo|Damage")
	float Damage = 15.0f;

	// 接触時のリアクション種別
	UPROPERTY(EditAnywhere, Category = "Tide|Halo|Damage")
	FGameplayTag HitReactionTag;

	// 同一アクターへ再ダメージを許可するまでの間隔秒数
	UPROPERTY(EditAnywhere, Category = "Tide|Halo|Damage")
	float DamageReArmInterval = 0.5f;

	// 上空へ発射しきるまでの秒数
	UPROPERTY(EditAnywhere, Category = "Tide|Halo|Deploy")
	float RiseDuration = 0.25f;

	// 上空から目標地点へ落下する秒数
	UPROPERTY(EditAnywhere, Category = "Tide|Halo|Deploy")
	float FallDuration = 0.4f;

	// 目標地点の上空に取る頂点の高さ
	UPROPERTY(EditAnywhere, Category = "Tide|Halo|Deploy")
	float ApexHeight = 500.0f;

	// 着地後の寿命秒数 ※0で破砕まで残り続ける
	UPROPERTY(EditAnywhere, Category = "Tide|Halo|Deploy")
	float ActiveLifetime = 0.0f;

	// GCのDamage Thresholdを超えるStrain量 ※GCアセット側の値より大きくすること
	UPROPERTY(EditAnywhere, Category = "Tide|Halo|Break")
	float BreakStrainMagnitude = 1000000.0f;

	// Strain/力場の影響半径
	UPROPERTY(EditAnywhere, Category = "Tide|Halo|Break")
	float BreakFieldRadius = 200.0f;

	// 破片を吹き飛ばす力の強さ
	UPROPERTY(EditAnywhere, Category = "Tide|Halo|Break")
	float BreakScatterForce = 200000.0f;

	// 破片が完全に消えるまでの秒数 ※0で消えない
	UPROPERTY(EditAnywhere, Category = "Tide|Halo|Break")
	float DebrisLifetime = 2.0f;

	// DebrisLifetime終了前にフェードを始める秒数 ※0でフェードなし
	UPROPERTY(EditAnywhere, Category = "Tide|Halo|Break")
	float FadeDuration = 1.0f;

	// フェードに使うマテリアルのスカラーパラメータ名 ※0=表示/1=消滅のディゾルブ規約
	UPROPERTY(EditAnywhere, Category = "Tide|Halo|Break")
	FName FadeParameterName = TEXT("Dissolve");

	// 破砕時エフェクト ※任意
	UPROPERTY(EditAnywhere, Category = "Tide|Halo|VFX")
	TObjectPtr<UNiagaraSystem> BreakEffect;

private:

	enum class EState : uint8 { Deploying, Active, Shattered };

	// StartDelay経過後に発射モーションを開始する
	void BeginDeployMotion();
	void EnterActive();
	void ApplyScatterForce();
	void StartFade();

	EState State = EState::Deploying;

	FVector DeployStart  = FVector::ZeroVector;
	FVector DeployApex   = FVector::ZeroVector;
	FVector DeployTarget = FVector::ZeroVector;
	float   DeployElapsed = 0.0f;

	// 接触ダメージの再アーム管理
	TMap<TWeakObjectPtr<AActor>, double> LastDamageTimes;

	// 破砕後のフェード
	bool  bFading = false;
	float FadeElapsed = 0.0f;
	TArray<TObjectPtr<UMaterialInstanceDynamic>> FadeMIDs;

	FVector CachedImpactLocation  = FVector::ZeroVector;
	FVector CachedAttackDirection = FVector::ForwardVector;

	FTimerHandle DeployDelayTimerHandle;
	FTimerHandle ScatterTimerHandle;
	FTimerHandle FadeStartTimerHandle;
	FTimerHandle LifetimeTimerHandle;

};
