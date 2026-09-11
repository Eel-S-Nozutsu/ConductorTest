// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameplayTagContainer.h"
#include "NiagaraSystem.h"
#include "NiagaraComponent.h"
#include "Components/BoxComponent.h"
#include "EnemyProjectile.generated.h"

class UProjectileBehavior;
class UProjectilePoolSubsystem;
class UProjectileProfile;
enum class EDamageResult : uint8;

enum class EProjectileEndReason : uint8
{
	None,
	Hit,
	LifeSpanExpired,
	Despawned
};

/**
 * 敵が発射する弾
 */
UCLASS()
class PRJ_TIDE_P0_API AEnemyProjectile : public AActor
{
	GENERATED_BODY()

public:

	AEnemyProjectile();

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	// 寿命切れ プール管理ならプールへ返却し非管理なら従来どおり破棄
	virtual void LifeSpanExpired() override;

	UFUNCTION()
	virtual void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
		const FHitResult& SweepResult);

	// Pawnとのオーバーラップで消滅させてよいか(既定=true)。持続ダメージ弾のサブクラスなどが
	// falseをoverrideすると、接触では消滅せずMaxLifeTimeまで生存し続ける
	virtual bool ShouldConsumeOnOverlap(AActor* OtherActor, const FHitResult& SweepResult) const { return true; }

	// 地形(WorldStatic/WorldDynamic)へのブロッキングヒットで消滅させてよいか
	// (既定=true)。物理的な衝突自体
	// (ProjectileMovementのBlock応答)は防がずその場に止まるだけになる
	virtual bool ShouldConsumeOnTerrainHit(AActor* OtherActor, const FHitResult& Hit) const { return true; }

	// このアクタ自身がTickを必要とするか(既定=false)
	// FireProjectileでのTick有効化判定に使う
	// Behaviors側のWantsTick()とは別に、サブクラス自身が毎フレーム処理
	// (持続ダメージ弾のTick等)を行いたい場合はこれをoverrideしてtrueを返すこと
	// 忘れるとBehavior構成やデバッグ表示の有無次第でTickが呼ばれたり呼ばれなかったりする不具合
	// になる
	virtual bool WantsProjectileTick() const { return false; }

	// ダメージ通知を試みる。戻り値falseは回避された
	// (ヒットを無視して飛行を続ける)
	// OutResultに判定結果を返す(ビヘイビアのOnHitへ渡す)
	// bIsDamageOverTime=trueで継続ダメージ扱いにし、
	// 毎ティックの被弾リアクション・カメラ揺れ連発を避ける
	bool TryDamageActor(AActor* OtherActor, const FHitResult& HitInfo, EDamageResult& OutResult, bool bIsDamageOverTime = false);

	// サブクラス固有のプロファイル項目を適用する (InitFromProfileの末尾で呼ばれる)
	// 既定では何もしない
	virtual void ApplyExtraProfileParams(UProjectileProfile* Profile) {}

public:

	// 衝突判定
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<UBoxComponent> CollisionComp = nullptr;

	// 弾の移動
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement = nullptr;

	// 常駐効果コンポーネント 本体形状もメッシュレンダラで対応想定
	UPROPERTY(VisibleAnywhere, Category = "Tide")
	TObjectPtr<UNiagaraComponent> TrailEffect = nullptr;

	// ダメージ量
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tide")
	float Damage = 10.0f;

	// 被弾側へ与えるヒットリアクションタグ。空=リアクション無し
	// ※発射時に敵の実行中攻撃タグで上書きされる
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tide", meta = (Categories = "HitReaction"))
	FGameplayTag HitReactionTag;

	// 発射速度
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tide")
	float InitialSpeed = 300.0f;

	// 最大生存秒数 ※コアリダイレクト対策でMaxLifeTimeと命名
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tide")
	float MaxLifeTime = 5.0f;

	// スポーン後の待機秒数。0で即発射 ※待機中は他のカウント停止
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tide", meta = (ClampMin = "0.0"))
	float SpawnHoldDuration = 0.0f;

	// スポナー範囲外フェードの所要秒数。攻撃側 (FAttackEntry)でフェードONの弾でのみ使う
	// 0で即消滅。フェード開始と同時にコリジョンを切るので、範囲外では当たらない
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tide", meta = (ClampMin = "0.0"))
	float OutOfRangeFadeDuration = 0.3f;

	// 常駐効果コンポーネントを返す
	UNiagaraComponent* GetPassiveEffectComponent() const { return TrailEffect; }

	// 直近の消滅がヒットによるものかどうかを返す
	bool WasHit() const { return bWasHit; }
	// 直近の消滅理由を返す
	EProjectileEndReason GetEndReason() const { return EndReason; }
	// 現在発射中か (消滅後/プール待機中はfalse)
	bool IsActive() const { return bActive; }

	// 挙動モジュール群
	UPROPERTY(VisibleInstanceOnly, Transient, Category = "Tide")
	TArray<TObjectPtr<UProjectileBehavior>> Behaviors;

	// ---- プロファイル / プール / ライフサイクル ----

	// プロファイルからパラメータを適用しビヘイビアを弾へ複製
	// 同一プロファイルでの再利用時は複製済みを使い回す
	void InitFromProfile(UProjectileProfile* Profile);

	// 適用中のプロファイル (プール返却時のキーに使用)
	UProjectileProfile* GetProfile() const { return CachedProfileTemplate; }

	// 複製済みビヘイビアから指定型を探す
	// (InitFromProfile後ActivateProjectile前に呼ぶこと)
	template <typename T>
	T* FindBehavior() const
	{
		for (UProjectileBehavior* Behavior : Behaviors)
		{
			if (T* Typed = Cast<T>(Behavior)) return Typed;
		}
		return nullptr;
	}

	// 発射する (プロパティ注入後に呼ぶ)
	virtual void ActivateProjectile();
	// SpawnHoldDurationによる待機中の弾を、時間経過を待たずに即発射へ切り替える
	// (待機中でなければ何もしない)。頭上待機からの狙い撃ち解放など、外部イベント駆動の
	// 発射タイミング制御に使う
	void ReleaseHold();
	// この弾をプールが管理することを示す
	void SetPoolOwner(UProjectilePoolSubsystem* Pool);
	// プールへ返却される際の無効化処理
	void DeactivateForPool();
	// 外部から強制的に消滅させる (プール管理ならプールへ返却し非管理なら破棄)
	void Despawn();

private:

	// 発射シーケンス (生成時ビジュアルの起動 + 待機判定)
	void LaunchSequence();
	// 発射本体 (寿命/速度/移動系ビヘイビア起動)。待機なしなら即時、ありなら待機後に呼ぶ
	void FireProjectile();
	// BehaviorsのスナップショットをActiveBehaviorsに取り発射中のライフサイクルで回
	// す
	void RebuildActiveBehaviors();
	// 待機後に発射する (SpawnHoldDuration経過で呼ぶ)
	void LaunchAfterHold();
	// 消滅/返却の共通入口 (プール管理ならReleaseし非管理ならDestroy)
	void FinishLife();
	// 消滅フック (アクティブな間に1度だけ発火)。演出のOnExpireは抑止されうるが
	// 後始末のOnDeactivateは必ず通す
	void FireEndLifeHooks();
	// 発射状態を解除して待機させる (移動停止/コリジョン無効/非表示/タイマークリア)
	void ParkForPool();

	// スポナー範囲外フェードを開始する。コリジョンを即切って場外での被弾を止め、
	// OutOfRangeFadeDurationかけてトレイルを消してからDespawnする
	// (0なら即Despawn)
	void BeginOutOfRangeFade();

	// 発射ごとに構築する実行ビヘイビア
	UPROPERTY(Transient) TArray<TObjectPtr<UProjectileBehavior>> ActiveBehaviors;
	// 直近に適用したプロファイル
	UPROPERTY(Transient) TObjectPtr<UProjectileProfile> CachedProfileTemplate;

	TWeakObjectPtr<UProjectilePoolSubsystem> OwningPool; // 所属プール (プール管理時のみ有効)
	FTimerHandle HoldTimerHandle;

	bool bWasHit = false; // 直近の消滅がヒットによるものか
	bool bActive = false; // 現在発射中かどうか
	bool bPoolManaged = false; // プールが管理する弾かどうか
	EProjectileEndReason EndReason = EProjectileEndReason::None;

	// ---- スポナー範囲外フェード
	// (ActivateProjectileで発射元の攻撃指定から有効化)----発射時のテリトリー中心
	// /半径を捕捉して持つ (発射元が飛行中に死んでも境界を保てる)
	// TerritoryRadius <= 0は無効 (直置き敵などterritoryを持たない弾)
	bool bFadeOutsideTerritory = false;
	FVector TerritoryOrigin = FVector::ZeroVector;
	float TerritoryRadius = 0.0f;

	// フェード進行状態
	bool bOutOfRangeFading = false;
	float OutOfRangeFadeElapsed = 0.0f;

	// 静かな消滅フラグ
	// 範囲外フェードのように「爆発/デスVFX/カメラ揺れ等の終了エフェクトを一切出さずスッと消したい」
	// ときに立てる。演出フックのOnExpireのみをスキップし、後始末のOnDeactivateは抑止しない
	bool bSuppressEndEffects = false;

};
