// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"

#include "PRJ_TIDE_P0/Interfaces/Environment/IWindAffectable.h"
#include "PRJ_TIDE_P0/Interfaces/Combat/IDamageable.h"

#include "BossMineHazard.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;
class UMaterialInstanceDynamic;
class UNiagaraSystem;
class UProjectileProfile;

/**
 * ショックウォール攻撃でボスが撒く地雷ギミック。
 *
 * 着地してArmedになった後、スライドパッシブの竜巻 (IWindAffectable) に巻き込まれると、
 * 一定時間巻き上げられてから所有者 (撒いたボス) へ山なりに撃ち返す。
 * 撃ち返し中は所有者だけにダメージを与える (Owner専用)。
 */
UCLASS()
class PRJ_TIDE_P0_API ABossMineHazard : public AActor, public IWindAffectable, public IDamageable
{
	GENERATED_BODY()

public:

	ABossMineHazard();

	// IDamageable: ボス自身の攻撃 (近接CommonAttack
	// /爆撃AOEなど)で起爆できるようにする
	// 起爆条件はReceiveDamage側で判定する (発動者が所有ボスのときのみ通常どおり爆発する)
	virtual EDamageResult ReceiveDamage(const FDamageInfo& DamageInfo) override;
	virtual bool CanBeDamaged() const override;

	void SetLaunchVelocity(const FVector& InVelocity);
	void SetExplosionDamage(float InDamage) { ExplosionDamage = InDamage; }
	void SetImmuneThroughShockwaveWaveId(int32 InWaveId) { ImmuneThroughShockwaveWaveId = InWaveId; }

	// 爆発時の破片散布のON/OFF。UShockwallAttackExecutionが地雷ごとにランダ
	// ムで設定する
	void SetScatterFragmentsOnExplode(bool bEnable) { bScatterFragmentsOnExplode = bEnable; }
	bool TriggerByShockwave(const FVector& ShockwaveCenter, int32 IncomingWaveId);
	FVector GetMineWorldLocation() const;

	// IWindAffectable
	virtual void OnWindEnter(const FWindInfluence& Wind) override;
	virtual void OnWindTick(const FWindInfluence& Wind, float DeltaTime) override;
	virtual void OnWindExit() override;

protected:

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	UFUNCTION()
	void OnMineHit(UPrimitiveComponent* HitComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	UFUNCTION()
	void OnMineOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

private:

	enum class EMineState : uint8
	{
		Airborne,	// 投射されて飛行中
		Armed,		// 着地して起動済み (点滅)
		Captured,	// 竜巻に巻き上げられて滞空・回転中
		Launched,	// 所有者ボスへ山なりに撃ち返し中
		Exploded,
	};

	void ArmMine(const FHitResult& Hit);
	void Explode(const FVector& ImpactLocation);
	// 爆発時にOriginから360度ランダム方向へ破片 (山なり着弾の弾) を撒く
	void ScatterFragments(const FVector& Origin);
	void ApplyBlinkMaterial(float DeltaTime);

	// 接近起爆: 範囲内に敵対対象が入ったらカウント開始 → 0で起爆する (接触起爆とは別経路で併存)
	void UpdateProximityFuse(float DeltaTime);
	// 指定半径内に所有者から見て敵対するPawnがいるか
	bool HasHostileTargetWithin(float Radius);

	// 竜巻に巻き上げられている間の挙動 (中心へ引き込み + 上昇 + 旋回)
	void UpdateCapture(float DeltaTime);
	// 巻き上げ完了 → 所有者ボスへ撃ち返しを開始する
	void BeginLaunch();
	// 所有者へ山なりに飛翔 (XYは所有者へ毎フレーム追従、Zは放物線アーク)
	void UpdateLaunch(float DeltaTime);
	// 撃ち返し命中時の処理。所有者だけにダメージを与えて爆発する (Owner専用)
	void ExplodeOnOwner();

public:

	// CollisionCompをRootComponentにする
	// (ProjectileMovementのUpdatedComponentと一致させ、
	// Rootとコリジョン/見た目の位置が乖離しないようにする)
	UPROPERTY(VisibleAnywhere, Category = "Tide|Mine")
	TObjectPtr<USphereComponent> CollisionComp;

	UPROPERTY(VisibleAnywhere, Category = "Tide|Mine")
	TObjectPtr<UStaticMeshComponent> MeshComp;

	UPROPERTY(VisibleAnywhere, Category = "Tide|Mine")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	UPROPERTY(EditAnywhere, Category = "Tide|Mine|Damage")
	float ExplosionDamage = 20.0f;

	UPROPERTY(EditAnywhere, Category = "Tide|Mine|Damage")
	float ExplosionRadius = 250.0f;

	UPROPERTY(EditAnywhere, Category = "Tide|Mine|Damage")
	FGameplayTag HitReactionTag;

	UPROPERTY(EditAnywhere, Category = "Tide|Mine|Damage")
	float ContactReArmInterval = 0.2f;

	// 着地して起動するときの地面へのめり込み量
	// 0=地面に乗る(球が接地), 0.5=半分埋まる, 1=完全に埋まる
	UPROPERTY(EditAnywhere, Category = "Tide|Mine|Placement", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float GroundEmbedRatio = 0.5f;

	// 接地と判定する地面法線のZ下限 (1=真上, 0.7≒45度)
	// これ未満の急斜面/壁では着地(Arm)せず、壁に沿って滑り落ちて地面に着いてから起動する
	// 0で常に着地(従来挙動)
	UPROPERTY(EditAnywhere, Category = "Tide|Mine|Placement", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float GroundArmNormalZThreshold = 0.7f;

	UPROPERTY(EditAnywhere, Category = "Tide|Mine|Visual")
	FName BlinkParameterName = TEXT("Blink");

	// 通常 (待機) 時の点滅周期。大きいほどゆっくり点滅する
	// 接近起爆カウント中はこの値からProximityFuseFastBlinkPeriodへ向けて徐々に
	// 速くなる
	UPROPERTY(EditAnywhere, Category = "Tide|Mine|Visual")
	float BlinkPeriod = 2.0f;

	UPROPERTY(EditAnywhere, Category = "Tide|Mine|Visual")
	TObjectPtr<UNiagaraSystem> ExplosionEffect;

	// 爆発時に破片を山なりに撒き散らすか。実行時にUShockwallAttackExecutionが地雷
	// ごとに設定する
	UPROPERTY(EditAnywhere, Category = "Tide|Mine|Fragment")
	bool bScatterFragmentsOnExplode = false;

	// 破片弾プロファイル (山なり着弾する「爆撃(ベジェ弧)」ビヘイビアを設定する)
	// 爆撃技と同じ弾を流用可
	UPROPERTY(EditAnywhere, Category = "Tide|Mine|Fragment")
	TObjectPtr<UProjectileProfile> FragmentProfile;

	// 撒く破片数
	UPROPERTY(EditAnywhere, Category = "Tide|Mine|Fragment", meta = (ClampMin = "0"))
	int32 FragmentCount = 8;

	// 破片の着弾距離の最小/最大。360度ランダム方向 × この距離レンジで着弾点を散らす
	UPROPERTY(EditAnywhere, Category = "Tide|Mine|Fragment", meta = (ClampMin = "0.0"))
	float FragmentScatterRadiusMin = 200.0f;

	UPROPERTY(EditAnywhere, Category = "Tide|Mine|Fragment", meta = (ClampMin = "0.0"))
	float FragmentScatterRadiusMax = 600.0f;

	// 破片の生成高さオフセット。接地地雷だと生成直後に地面へ接触して即炸裂・消滅するため、
	// 少し上空から発射する。0だと接地型では見えないことがある
	UPROPERTY(EditAnywhere, Category = "Tide|Mine|Fragment", meta = (ClampMin = "0.0"))
	float FragmentSpawnHeightOffset = 60.0f;

	// 破片の山なり弧の高さ
	UPROPERTY(EditAnywhere, Category = "Tide|Mine|Fragment")
	float FragmentArcHeight = 400.0f;

	// 破片の飛行秒数
	UPROPERTY(EditAnywhere, Category = "Tide|Mine|Fragment", meta = (ClampMin = "0.01"))
	float FragmentFlightDuration = 1.0f;

	// 破片1発のダメージ。負値 = 地雷のExplosionDamageを流用する
	UPROPERTY(EditAnywhere, Category = "Tide|Mine|Fragment")
	float FragmentDamage = -1.0f;

	// 接近起爆: この半径内に敵対対象が入るとカウントを開始する
	UPROPERTY(EditAnywhere, Category = "Tide|Mine|Proximity", meta = (ClampMin = "0.0"))
	float ProximityFuseRadius = 600.0f;

	// 接近起爆までの秒数
	UPROPERTY(EditAnywhere, Category = "Tide|Mine|Proximity", meta = (ClampMin = "0.0"))
	float ProximityFuseDuration = 6.0f;

	// 接近起爆カウントが0に近づいたときの最速点滅周期。小さいほど速い
	UPROPERTY(EditAnywhere, Category = "Tide|Mine|Proximity", meta = (ClampMin = "0.01"))
	float ProximityFuseFastBlinkPeriod = 0.05f;

	// 巻き上げ中に中心の周りを旋回する角速度 (度/秒)。見た目の回転演出用
	UPROPERTY(EditAnywhere, Category = "Tide|Mine|Wind")
	float WindOrbitAngularSpeedDeg = 120.0f;

	// 上昇速度の倍率。竜巻のLiftSpeedに掛ける。1.0で竜巻どおり、小さくするとゆっくり上昇し、
	// 頂上到達までの時間が延びる (= その間の旋回が増える)。0で上昇せず即投擲
	UPROPERTY(EditAnywhere, Category = "Tide|Mine|Wind", meta = (ClampMin = "0.0"))
	float WindLiftSpeedMultiplier = 0.6f;

	// 撃ち返しの水平移動速度。飛行時間は「水平距離 / この速度」で決まるので、
	// 距離が変わっても見た目の速度は一定 (遠いほど時間がかかるだけ) になる
	UPROPERTY(EditAnywhere, Category = "Tide|Mine|Wind", meta = (ClampMin = "1.0"))
	float LaunchSpeed = 1400.0f;

private:

	EMineState State = EMineState::Airborne;
	FVector LaunchVelocity = FVector::ZeroVector;
	float BlinkPhase = 0.0f;		// 点滅の積分位相 (周期が変わっても不連続にならないよう位相で保持)
	double LastContactTime = -1000.0;
	int32 ImmuneThroughShockwaveWaveId = 0;

	// 接近起爆カウントの状態
	bool bProximityFuseActive = false;
	float ProximityFuseRemaining = 0.0f;

	// 竜巻 (IWindAffectable) から受け取った影響パラメータのキャッシュ
	// 巻き上げ中の挙動に使う
	FWindInfluence CapturedWind;
	float OrbitAngleDeg = 0.0f;		// 巻き上げ中の旋回角

	// 撃ち返し (Launched) 用。開始時に距離から算出してキャッシュする
	FVector LaunchStartLocation = FVector::ZeroVector;	// 撃ち返し開始位置
	float LaunchElapsed = 0.0f;							// 撃ち返し開始からの経過時間
	float LaunchDuration = 0.0f;						// 飛行時間 = 水平距離 / LaunchSpeed
	float LaunchArcHeight = 0.0f;						// 放物線アークの高さ = 水平距離に比例

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> BlinkMID = nullptr;

	FTimerHandle LifetimeTimerHandle;

};
