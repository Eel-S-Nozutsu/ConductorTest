// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "EnemyProjectile.h"
#include "Behaviors/ProjectileBehavior.h"
#include "ProjectilePoolSubsystem.h"
#include "ProjectileProfile.h"
#include "PRJ_TIDE_P0/Interfaces/Combat/IDamageable.h"
#include "PRJ_TIDE_P0/Utilities/TideCombatUtils.h"
#include "PRJ_TIDE_P0/Actors/Character/Enemy/EnemyCharacter.h"
#include "PRJ_TIDE_P0/Core/TideGameSettings.h"

#include "Components/BoxComponent.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "TimerManager.h"
#include "DrawDebugHelpers.h"

AEnemyProjectile::AEnemyProjectile()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	CollisionComp = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionComp"));
	CollisionComp->InitBoxExtent(FVector(10.0f, 10.0f, 10.0f));
	CollisionComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	// ECC_GameTraceChannel1 = Projectile (todo: PRJで方針を定める)
	CollisionComp->SetCollisionObjectType(ECC_GameTraceChannel1);
	CollisionComp->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionComp->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	CollisionComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	// WorldDynamic(可動の地面・壁・高台 / 壊れ物プロップ等)はBlock
	// ブロッキングヒットでOnHitが走り、IDamageableならダメージ、
	// そうでなければ消滅する(地面すり抜け防止)
	CollisionComp->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	CollisionComp->SetNotifyRigidBodyCollision(true);
	CollisionComp->OnComponentHit.AddDynamic(this, &AEnemyProjectile::OnHit);
	CollisionComp->OnComponentBeginOverlap.AddDynamic(this, &AEnemyProjectile::OnOverlapBegin);
	RootComponent = CollisionComp;

	TrailEffect = CreateDefaultSubobject<UNiagaraComponent>(TEXT("TrailEffect"));
	TrailEffect->SetupAttachment(RootComponent);
	TrailEffect->SetAutoActivate(true);

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->UpdatedComponent = CollisionComp;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bShouldBounce = false;
	ProjectileMovement->ProjectileGravityScale = 0.0f;
}

void AEnemyProjectile::BeginPlay()
{
	Super::BeginPlay();

	// 静的なコリジョン応答Blueprintの保存値より優先して確実に適用
	CollisionComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	CollisionComp->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Ignore);
	CollisionComp->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);

	if (bPoolManaged)
	{
		// プール生成直後発射せず待機状態にしAcquire -> ActivateProjectileを待つ
		ParkForPool();
		return;
	}

	// 非プールはそのまま発射速度には触れずにSuper::BeginPlayを呼んで独自に弾道制御するサブ
	// クラスに干渉しない
	LaunchSequence();
}

void AEnemyProjectile::ActivateProjectile()
{
	// 発射元エネミーの実行中攻撃 (FAttackEntry::HitReactionTag)
	// を取り込む。全プール弾スポーンがこの経路を通るため、
	// ここ1か所で攻撃ごとリアクションが弾へ乗る
	// 空 = 未設定なら弾BP自前のタグを尊重する
	AEnemyCharacter* OwnerEnemy = Cast<AEnemyCharacter>(GetOwner());
	TideCombatUtil::InjectAttackHitReactionTag(OwnerEnemy, HitReactionTag);

	// 「スポナー範囲外で弾をフェード」指定の攻撃なら、発射時のテリトリー中心/半径を捕捉する
	// 全プール弾がこの経路を通るので、攻撃側はFAttackEntryの1フラグを立てるだけで全弾に効く
	// テリトリーを持たない敵 (直置き = 半径0) は対象外
	bFadeOutsideTerritory = false;
	bOutOfRangeFading = false;
	OutOfRangeFadeElapsed = 0.0f;
	if (OwnerEnemy && OwnerEnemy->GetCurrentAttackFadeOutsideRange())
	{
		const float Radius = OwnerEnemy->GetPatrolRadius();
		if (Radius > 0.0f)
		{
			bFadeOutsideTerritory = true;
			TerritoryOrigin = OwnerEnemy->GetPatrolOrigin();
			TerritoryRadius = Radius;
		}
	}

	// プール再利用の復帰処理 待機状態から発射可能状態へ戻す新規スポーンではBeginPlay ->
	// LaunchSequenceを通るためこちらはプール消費側からのみ呼ばれる
	SetActorHiddenInGame(false);
	CollisionComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionComp->ClearMoveIgnoreActors();
	CollisionComp->IgnoreActorWhenMoving(GetOwner(), true);

	// トレイルをリセットして再生 (再利用時にラインが画面を横切るのを防止)
	if (TrailEffect)
	{
		TrailEffect->Activate(true);
	}

	// 再利用ではPMCのBeginPlayが再実行されないため速度を自前で与える
	if (ProjectileMovement)
	{
		ProjectileMovement->Activate();
		ProjectileMovement->Velocity = GetActorForwardVector() * InitialSpeed;
	}

	LaunchSequence();
}

void AEnemyProjectile::LaunchSequence()
{
	bActive = true;
	bWasHit = false;
	EndReason = EProjectileEndReason::None;

	// 発射時点のビヘイビアをスナップショットし、生成時フックOnSpawnを呼ぶ
	// 常駐VFX等のビジュアルがここで起動し、待機中も本体が表示される
	RebuildActiveBehaviors();
	for (UProjectileBehavior* Behavior : ActiveBehaviors)
	{
		if (Behavior) Behavior->OnSpawn(this);
	}

	// SpawnHoldDurationは技側が1発ごとに注入する値
	// ここで消費し0に戻して、プール再利用時に次弾へ待機が漏れないようにする
	// (SetTargetLocation等と同じper-shot思想)
	const float HoldDuration = SpawnHoldDuration;
	SpawnHoldDuration = 0.0f;

	if (HoldDuration > 0.0f)
	{
		// 待機中は移動/寿命を停止。移動系ビヘイビアはまだ起動しない (待機中に動かさない)
		// Tickも無効のまま (有効だと未起動の移動系OnTickが走ってしまう)
		if (ProjectileMovement)
		{
			ProjectileMovement->StopMovementImmediately();
			ProjectileMovement->Deactivate();
		}
		GetWorldTimerManager().SetTimer(HoldTimerHandle, this,
			&AEnemyProjectile::LaunchAfterHold, HoldDuration, false);
		return;
	}

	FireProjectile();
}

void AEnemyProjectile::LaunchAfterHold()
{
	// 待機完了 → 発射 (ビジュアルはLaunchSequenceで起動済み)
	FireProjectile();
}

void AEnemyProjectile::ReleaseHold()
{
	if (!GetWorldTimerManager().IsTimerActive(HoldTimerHandle)) return;

	GetWorldTimerManager().ClearTimer(HoldTimerHandle);
	FireProjectile();
}

void AEnemyProjectile::FireProjectile()
{
	SetLifeSpan(MaxLifeTime);

	if (ProjectileMovement)
	{
		ProjectileMovement->Activate();
		ProjectileMovement->Velocity = GetActorForwardVector() * InitialSpeed;
	}

	// 発射時フックOnLaunchを呼ぶ (移動・狙い等の起動)
	// ビジュアル系はOnSpawn側で起動済みで、OnLaunchは既定空なので二重起動しない
	bool bAnyWantsTick = false;
	for (UProjectileBehavior* Behavior : ActiveBehaviors)
	{
		if (!Behavior) continue;
		Behavior->OnLaunch(this);
		bAnyWantsTick |= Behavior->WantsTick();
	}

	// WantsTickが1つでもtrueを返すか、サブクラス自身がTickを必要とするなら駆動
	bool bTickForDebug = false;
#if !UE_BUILD_SHIPPING
	// 当たり判定デバッグ表示中は、Tick不要な弾 (物理放物線等)
	// でもTickを回して毎フレーム描画する
	bTickForDebug = UTideGameSettings::Get()->bDebugDrawAttackHitbox;
#endif
	SetActorTickEnabled(bAnyWantsTick || bTickForDebug || WantsProjectileTick());
}

void AEnemyProjectile::RebuildActiveBehaviors()
{
	// 発射時点のBehaviorsのスナップショットを取る
	ActiveBehaviors.Reset();
	for (UProjectileBehavior* Behavior : Behaviors)
	{
		if (Behavior) ActiveBehaviors.Add(Behavior);
	}
}

void AEnemyProjectile::InitFromProfile(UProjectileProfile* Profile)
{
	if (!Profile) return;

	Damage = Profile->Damage;
	InitialSpeed = Profile->InitialSpeed;
	MaxLifeTime = Profile->MaxLifeTime;

	if (ProjectileMovement)
	{
		ProjectileMovement->InitialSpeed = InitialSpeed;
		ProjectileMovement->MaxSpeed = InitialSpeed;
	}

	if (!Profile->CollisionExtent.IsNearlyZero() && CollisionComp)
	{
		CollisionComp->SetBoxExtent(Profile->CollisionExtent, false);
	}

	// プロファイルのビヘイビアを自前インスタンスへ複製する
	// (共有インスタンスの実行時状態衝突を防止)
	// 同一プロファイルでの再利用時は複製済みを使い回す
	if (CachedProfileTemplate != Profile)
	{
		CachedProfileTemplate = Profile;
		Behaviors.Reset();
		if (Profile->Movement)
		{
			Behaviors.Add(DuplicateObject<UProjectileBehavior>(Profile->Movement, this));
		}
		for (UProjectileBehavior* Effect : Profile->Effects)
		{
			if (Effect) Behaviors.Add(DuplicateObject<UProjectileBehavior>(Effect, this));
		}
	}

	ApplyExtraProfileParams(Profile);
}

void AEnemyProjectile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	for (UProjectileBehavior* Behavior : ActiveBehaviors)
	{
		if (Behavior) Behavior->OnTick(this, DeltaSeconds);

		// ビヘイビアがOnTick内で自身を消滅/プール返却した場合
		// "Array has changed during ranged-for iteration"の防止
		if (!bActive) break;
	}

	// スポナー範囲外フェード。テリトリー円 (2D) を出たらフェード開始し、フェード完了で消滅する
	// 弧攻撃などが一時的に境界を跨ぐことは想定していないので、
	// 有効化する攻撃は選ぶこと (FAttackEntry)
	if (bActive && bFadeOutsideTerritory)
	{
		if (bOutOfRangeFading)
		{
			OutOfRangeFadeElapsed += DeltaSeconds;
			if (OutOfRangeFadeElapsed >= OutOfRangeFadeDuration)
			{
				Despawn();
				return;
			}
		}
		else if (FVector::Dist2D(GetActorLocation(), TerritoryOrigin) > TerritoryRadius)
		{
			BeginOutOfRangeFade();
			if (!bActive) return; // 即消し (FadeDuration<=0) で返却済みなら以降触らない
		}
	}

#if !UE_BUILD_SHIPPING
	// 弾自体の当たり判定(＝ダメージエリア)を可視化する。飛行中ずっと弾に追従して描く
	if (bActive && CollisionComp && UTideGameSettings::Get()->bDebugDrawAttackHitbox)
	{
		DrawDebugBox(GetWorld(), CollisionComp->GetComponentLocation(),
			CollisionComp->GetScaledBoxExtent(), CollisionComp->GetComponentQuat(),
			FColor::Orange, false, -1.0f, 0, 1.5f);
	}
#endif
}

void AEnemyProjectile::SetPoolOwner(UProjectilePoolSubsystem* Pool)
{
	OwningPool   = Pool;
	bPoolManaged = (Pool != nullptr);
}

void AEnemyProjectile::BeginOutOfRangeFade()
{
	// 場外では当てない。コリジョンを即切ってから見た目だけ余韻を残して消す
	CollisionComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// 静かに消す。終了エフェクト(デスVFX/炸裂
	// /カメラ揺れ)を出さないようOnExpireを抑止する (OnDeactivateの後始末はそのまま走る)
	bSuppressEndEffects = true;

	// 即消し指定ならそのまま返却する
	if (OutOfRangeFadeDuration <= 0.0f)
	{
		Despawn();
		return;
	}

	// トレイルの新規発生を止め、残存パーティクルの寿命で自然に消えるのに任せる
	// (ParkForPoolと同じ流儀)。FadeElapsedが
	// OutOfRangeFadeDurationに達したらTick側でDespawn
	if (TrailEffect)
	{
		TrailEffect->Deactivate();
	}
	bOutOfRangeFading = true;
	OutOfRangeFadeElapsed = 0.0f;
}

void AEnemyProjectile::DeactivateForPool()
{
	FireEndLifeHooks();
	ParkForPool();
}

void AEnemyProjectile::ParkForPool()
{
	// 発射状態を解除して待機させる (座標は次のAcquireで設定)
	SetActorTickEnabled(false);
	if (ProjectileMovement)
	{
		ProjectileMovement->StopMovementImmediately();
		ProjectileMovement->Deactivate();
	}
	if (TrailEffect)
	{
		TrailEffect->Deactivate();
	}
	CollisionComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetActorHiddenInGame(true);

	// 寿命/待機タイマーが返却後に発火しないよう全クリア
	SetLifeSpan(0.0f);
	GetWorldTimerManager().ClearAllTimersForObject(this);

	// 範囲外フェードの状態を次のAcquireへ持ち越さない
	// (コリジョンはActivateProjectileで戻す)
	bFadeOutsideTerritory = false;
	bOutOfRangeFading = false;
	OutOfRangeFadeElapsed = 0.0f;
	bSuppressEndEffects = false;

	ActiveBehaviors.Reset();
}

void AEnemyProjectile::FireEndLifeHooks()
{
	if (!bActive) return; // 二重発火防止 (Release後にワールド破棄でEndPlayが来るとか)
	bActive = false;

	// 演出は静かな消滅 (範囲外フェード等) では出さないが、後始末は必ず通す
	// 掴んだプレイヤーの解放など、抑止すると外部に副作用が残り続けるものがあるため
	if (!bSuppressEndEffects)
	{
		for (UProjectileBehavior* Behavior : ActiveBehaviors)
		{
			if (Behavior) Behavior->OnExpire(this);
		}
	}

	for (UProjectileBehavior* Behavior : ActiveBehaviors)
	{
		if (Behavior) Behavior->OnDeactivate(this);
	}
}

void AEnemyProjectile::Despawn()
{
	// 既に待機(プール返却済み)なら二重返却しない
	if (!bActive) return;
	EndReason = EProjectileEndReason::Despawned;
	FinishLife();
}

void AEnemyProjectile::FinishLife()
{
	// プール管理ならプールへ返却しそうでなければ破棄
	if (OwningPool.IsValid())
	{
		OwningPool->Release(this);
	}
	else
	{
		Destroy();
	}
}

void AEnemyProjectile::LifeSpanExpired()
{
	if (OwningPool.IsValid())
	{
		// プール管理 寿命切れでも破棄せず返却
		EndReason = EProjectileEndReason::LifeSpanExpired;
		FinishLife();
		return;
	}

	EndReason = EProjectileEndReason::LifeSpanExpired;
	Super::LifeSpanExpired();
}

void AEnemyProjectile::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// アクティブなまま破棄される場合に終了処理を行うプール返却済みならbActive=falseでスキップ
	// される
	FireEndLifeHooks();

	Super::EndPlay(EndPlayReason);
}

bool AEnemyProjectile::TryDamageActor(AActor* OtherActor, const FHitResult& HitInfo, EDamageResult& OutResult, bool bIsDamageOverTime)
{
	OutResult = EDamageResult::Immune;

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OtherActor) return true;

	// 敵対しない相手にはダメージを与えない
	if (!TideCombatUtil::IsHostileTo(OwnerActor, OtherActor)) return true;

	IDamageable* Damageable = Cast<IDamageable>(OtherActor);
	if (!Damageable) return true;

	FDamageInfo DamageInfo;
	DamageInfo.BaseDamage        = Damage;
	DamageInfo.HitResult         = HitInfo;
	DamageInfo.Instigator        = OwnerActor;
	DamageInfo.HitReactionTag    = HitReactionTag;
	DamageInfo.bIsDamageOverTime = bIsDamageOverTime;

	OutResult = Damageable->ReceiveDamage(DamageInfo);
	if (OutResult == EDamageResult::Evaded)
	{
		return false; // 回避された場合は貫通を継続
	}

	return true;
}

void AEnemyProjectile::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	// 既に消費済み (プール返却済み等) なら無視する
	// 同一フレームに複数のヒット/オーバーラップが届くとFinishLifeが二重に走り、
	// プールへ二重返却 -> 空きリスト重複 -> 1つの弾を複数ショットが取り合う不具合になる
	if (!bActive) return;

	// 環境ジオメトリ(WorldStatic/WorldDynamic)や壊れ物プロップへの
	// BlockingHit。敵対IDamageableならダメージを与え、
	// 回避されなければ消滅
	if (!OtherActor || OtherActor == GetOwner()) return;

	// サブクラスが消滅を拒否したら(持続ダメージ弾など)地形ヒット自体を無視して生存を続ける
	if (!ShouldConsumeOnTerrainHit(OtherActor, Hit)) return;

	// ビヘイビアがこの当たりでの消滅を拒否したら当たり自体を無視して飛行を継続
	bool bConsume = true;
	for (UProjectileBehavior* Behavior : ActiveBehaviors)
	{
		if (Behavior) bConsume = Behavior->ShouldConsumeOnHit(this, Hit, bConsume);
	}
	if (!bConsume) return;

	EDamageResult DamageResult = EDamageResult::Immune;
	if (!TryDamageActor(OtherActor, Hit, DamageResult)) return;

	for (UProjectileBehavior* Behavior : ActiveBehaviors)
	{
		if (Behavior) Behavior->OnHit(this, OtherActor, Hit, DamageResult);
	}

	bWasHit = true;
	EndReason = EProjectileEndReason::Hit;
	FinishLife();
}

void AEnemyProjectile::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// 既に消費済み (プール返却済み等) なら無視する (OnHitと同様に二重返却を防ぐ)
	if (!bActive) return;

	// Pawnとのオーバーラップ 敵対しなければ貫通させる
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OtherActor || OtherActor == OwnerActor) return;
	if (!TideCombatUtil::IsHostileTo(OwnerActor, OtherActor)) return;

	// サブクラスが消滅を拒否したら(持続ダメージ弾など)当たり自体を無視して生存を続ける
	if (!ShouldConsumeOnOverlap(OtherActor, SweepResult)) return;

	EDamageResult DamageResult = EDamageResult::Immune;
	if (!TryDamageActor(OtherActor, SweepResult, DamageResult)) return;

	for (UProjectileBehavior* Behavior : ActiveBehaviors)
	{
		if (Behavior) Behavior->OnHit(this, OtherActor, SweepResult, DamageResult);
	}

	bWasHit = true;
	EndReason = EProjectileEndReason::Hit;
	FinishLife();
}
