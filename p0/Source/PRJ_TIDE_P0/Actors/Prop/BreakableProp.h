// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "NiagaraSystem.h"
#include "PRJ_TIDE_P0/Interfaces/Combat/IDamageable.h"
#include "BreakableProp.generated.h"

class AWindZone;

/**
 * Chaos Geometry Collectionを使った壊れ物プロップ。
 * 攻撃コリジョン(AnimNotifyState_CommonAttack)でヒットを受け取り、
 * Field SystemでGCクラスターStrainを与えて砕く。
 */
UCLASS()
class PRJ_TIDE_P0_API ABreakableProp : public AActor, public IDamageable
{
	GENERATED_BODY()

public:

	ABreakableProp();

	void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	// IDamageable
	virtual EDamageResult ReceiveDamage(const FDamageInfo& DamageInfo) override;
	virtual bool CanBeDamaged() const override;

public:

	// 壊れるジオメトリコレクション
	UPROPERTY(VisibleAnywhere, Category = "Tide|Prop")
	TObjectPtr<class UGeometryCollectionComponent> GCComp;

	// Root。コリジョン・ビジュアル・DamageStageパラメータ管理を担う
	// ※Simple Collisionを設定したStaticMeshをBPでセットすること
	UPROPERTY(VisibleAnywhere, Category = "Tide|Prop")
	TObjectPtr<UStaticMeshComponent> VisualMesh;

	// Field System(Strain/力場の送出元)
	UPROPERTY(VisibleAnywhere, Category = "Tide|Prop")
	TObjectPtr<class UFieldSystemComponent> FieldComp;

	// 攻撃吸着のターゲット部位。ロックオン(ObjectType=Pawn検索)には拾われず、
	// 攻撃吸着(ECC_Pawnチャンネル検索)にだけ拾われる。位置はBPで調整可
	UPROPERTY(VisibleAnywhere, Category = "Tide|Prop")
	TObjectPtr<class ULockOnTargetComponent> LockOnTargetComp;

	// 耐久値 ※1で一撃破壊
	UPROPERTY(EditAnywhere, Category = "Tide|Prop")
	int32 MaxHP = 1;

	// ONで落下(空中チャージ)攻撃でのみ壊れる判定はFDamageInfo.AttackTypeTag
	// == AttackType.Player.AirChargeAttack /
	// AttackType.Player.AirChargeDashを使用
	UPROPERTY(EditAnywhere, Category = "Tide|Prop")
	bool bOnlyBreakByAirChargeAttack = false;

	// ONで無傷のまま剛体として物理シミュレートする(床が抜けたら落下・転倒する)
	// 対象StaticMeshにSimple Collisionが必須。普段動いてほしくないピラー等は、
	// BPのPhysics > ConstraintsでXY移動と回転をロックすると真下にだけ落ちる
	UPROPERTY(EditAnywhere, Category = "Tide|Prop|Physics")
	bool bSimulatePhysicsWhenIntact = false;

	// GCのDamage Thresholdを超えるStrain量 ※GCアセット側の値より大きくすること
	UPROPERTY(EditAnywhere, Category = "Tide|Prop|Physics")
	float BreakStrainMagnitude = 1000000.0f;

	// Strain/力場の影響半径
	UPROPERTY(EditAnywhere, Category = "Tide|Prop|Physics")
	float BreakFieldRadius = 200.0f;

	// 破片を吹き飛ばす力の強さ
	UPROPERTY(EditAnywhere, Category = "Tide|Prop|Physics")
	float BreakScatterForce = 200000.0f;

	// 破片が完全に消えるまでの秒数 ※0で消えない
	UPROPERTY(EditAnywhere, Category = "Tide|Prop|Physics")
	float DebrisLifetime = 2.0f;

	// DebrisLifetime終了前にフェードを始める秒数 ※0でフェードなし
	UPROPERTY(EditAnywhere, Category = "Tide|Prop|Physics")
	float FadeDuration = 1.0f;

	// フェードに使うマテリアルのOpacityスカラーパラメータ名
	UPROPERTY(EditAnywhere, Category = "Tide|Prop|Physics")
	FName FadeParameterName = TEXT("Opacity");

	// ヒット時エフェクト(オプション) ※耐久が残っているとき
	UPROPERTY(EditAnywhere, Category = "Tide|Prop|VFX")
	TObjectPtr<UNiagaraSystem> HitEffect;

	// 破壊時エフェクト(オプション)
	UPROPERTY(EditAnywhere, Category = "Tide|Prop|VFX")
	TObjectPtr<UNiagaraSystem> BreakEffect;

	// ヒビ段階を渡すマテリアルパラメータ名 ※ARTと合わせること
	UPROPERTY(EditAnywhere, Category = "Tide|Prop|Damage")
	FName DamageStageParameterName = TEXT("DamageStage");

	// ヒビ小に切り替わるHP割合
	UPROPERTY(EditAnywhere, Category = "Tide|Prop|Damage", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CrackSmallThreshold = 0.8f;

	// ヒビ大に切り替わるHP割合
	UPROPERTY(EditAnywhere, Category = "Tide|Prop|Damage", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CrackLargeThreshold = 0.3f;

	// ヒビ小のパラメータ値 ※マテリアル側の設定に合わせること
	UPROPERTY(EditAnywhere, Category = "Tide|Prop|Damage")
	float CrackSmallStageValue = 1.0f;

	// ヒビ大のパラメータ値 ※マテリアル側の設定に合わせること
	UPROPERTY(EditAnywhere, Category = "Tide|Prop|Damage")
	float CrackLargeStageValue = 2.0f;

	// ダメージ段階切り替え時の振動秒数
	UPROPERTY(EditAnywhere, Category = "Tide|Prop|Damage")
	float DamageShakeDuration = 0.3f;

	// 振動の周波数 (rad/s)
	UPROPERTY(EditAnywhere, Category = "Tide|Prop|Damage")
	float DamageShakeFrequency = 40.0f;

	// 振動の振幅
	UPROPERTY(EditAnywhere, Category = "Tide|Prop|Damage")
	float DamageShakeAmplitude = 3.0f;

	// 破壊時にドロップするアクタークラス(オプション)
	UPROPERTY(EditAnywhere, Category = "Tide|Prop|Drop")
	TSubclassOf<AActor> DropClass;

	// ドロップ数
	UPROPERTY(EditAnywhere, Category = "Tide|Prop|Drop")
	int32 DropCount = 1;

	// ドロップの散らばり半径
	UPROPERTY(EditAnywhere, Category = "Tide|Prop|Drop")
	float DropScatterRadius = 80.0f;

	// 破壊時に上に乗っているポーンを吹き飛ばすか
	// 足場が消えても自然落下に頼らず確定で吹き飛びリアクションさせる
	// (ナビ拘束された敵が地面へスナップするのを避ける)
	UPROPERTY(EditAnywhere, Category = "Tide|Prop|Blowoff")
	bool bBlowoffActorsOnBreak = true;

	// 吹き飛ばしに使うリアクションタグ(既定HitReaction.Blowoff)
	// ※受け手のHitReactionComponentが解釈
	UPROPERTY(EditAnywhere, Category = "Tide|Prop|Blowoff")
	FGameplayTag BlowoffReactionTag;

	// 吹き飛ばしと同時に与えるダメージ ※0で演出のみ
	UPROPERTY(EditAnywhere, Category = "Tide|Prop|Blowoff")
	float BlowoffDamage = 0.0f;

	// 検出範囲のマージン。VisualMeshのバウンズに対し水平/上方向に広げて、
	// 縁や天面に立つポーンを拾う
	UPROPERTY(EditAnywhere, Category = "Tide|Prop|Blowoff")
	float BlowoffDetectPadding = 150.0f;

	// ONで砕けるヒットのときプレイヤーをノックバックさせない(敵にとどめを刺したときと同じ挙動)
	// ※耐久が残るヒットは敵と同様にノックバックする
	UPROPERTY(EditAnywhere, Category = "Tide|Prop")
	bool bSuppressPlayerKnockbackOnBreak = true;

	UPROPERTY(EditAnywhere, Category = "Tide|Debug")
	TObjectPtr<AWindZone> WindZone;

private:

	// ReceiveDamageの受付ゲート。壊れない攻撃(敵の攻撃・落下攻撃限定など)を弾く
	bool AcceptsDamage(const FDamageInfo& DamageInfo) const;

	void Break(const FVector& ImpactLocation, const FVector& AttackDirection);
	void BlowoffActorsOnTop();
	void ApplyScatterForce();
	void StartFade();
	void OnDamageStageChanged(float NewStage);
	void OnDamageShakeEnd();

	void DebugDrawWindExclusion();

	int32 CurrentHP = 0;
	FVector CachedImpactLocation = FVector::ZeroVector;
	// ラジアル散布の原点を攻撃者側にオフセットするための方向
	FVector CachedAttackDirection = FVector::ZeroVector;

	bool bFading = false;
	float FadeElapsed = 0.0f;
	TArray<TObjectPtr<UMaterialInstanceDynamic>> FadeMIDs;

	float CurrentDamageStage = 0.0f;

	bool bDamageShaking = false;
	float DamageShakeElapsed = 0.0f;
	FVector ShakeBaseLocation = FVector::ZeroVector;

	FTimerHandle ScatterTimerHandle;
	FTimerHandle FadeStartTimerHandle;
	FTimerHandle DamageShakeTimerHandle;

	bool bShowDebugWindExclusion = false;

};
