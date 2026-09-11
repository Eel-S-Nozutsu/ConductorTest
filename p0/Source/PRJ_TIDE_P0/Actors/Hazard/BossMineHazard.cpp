// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/Actors/Hazard/BossMineHazard.h"

#include "PRJ_TIDE_P0/Actors/Projectile/ProjectilePoolSubsystem.h"
#include "PRJ_TIDE_P0/Actors/Projectile/ProjectileProfile.h"
#include "PRJ_TIDE_P0/Actors/Projectile/EnemyProjectile.h"
#include "PRJ_TIDE_P0/Actors/Projectile/Behaviors/BallisticBehavior.h"
#include "PRJ_TIDE_P0/Actors/Projectile/Behaviors/SplashDamageBehavior.h"
#include "PRJ_TIDE_P0/Data/Combat/DamageInfo.h"
#include "PRJ_TIDE_P0/Interfaces/Combat/IDamageable.h"
#include "PRJ_TIDE_P0/Utilities/TideCombatUtils.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraFunctionLibrary.h"

ABossMineHazard::ABossMineHazard()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComp"));
	RootComponent = CollisionComp;
	CollisionComp->SetSphereRadius(24.0f);
	CollisionComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CollisionComp->SetCollisionObjectType(ECC_WorldDynamic);
	// 地形/床がWorldStatic以外のカスタムObjectTypeでも止まるよう、
	// 基本はBlockにする
	CollisionComp->SetCollisionResponseToAllChannels(ECR_Block);
	CollisionComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	CollisionComp->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	CollisionComp->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	CollisionComp->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Ignore); // Projectile
	CollisionComp->SetNotifyRigidBodyCollision(true);
	CollisionComp->SetGenerateOverlapEvents(true);
	// ボスの攻撃判定 (AnimNotifyState_CommonAttack等)
	// がIDamageableを拾う際の被弾レイヤータグ
	CollisionComp->ComponentTags.Add(TEXT("DamageLayer"));
	CollisionComp->OnComponentHit.AddDynamic(this, &ABossMineHazard::OnMineHit);
	CollisionComp->OnComponentBeginOverlap.AddDynamic(this, &ABossMineHazard::OnMineOverlap);

	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	MeshComp->SetupAttachment(CollisionComp);
	MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->UpdatedComponent = CollisionComp;
	ProjectileMovement->bRotationFollowsVelocity = true;
	// 壁/急斜面に当たったとき止まらず滑り落ちるようにする。Bounciness=0で反発はせず、
	// 接線成分(滑り)だけ残り、重力で下へ落ちていく。接地(上向き法線)ではOnMineHitが起動する
	ProjectileMovement->bShouldBounce = true;
	ProjectileMovement->Bounciness = 0.0f;
	ProjectileMovement->ProjectileGravityScale = 1.0f;
	ProjectileMovement->bForceSubStepping = true;
	ProjectileMovement->InitialSpeed = 0.0f;
	ProjectileMovement->MaxSpeed = 4000.0f;
}

void ABossMineHazard::SetLaunchVelocity(const FVector& InVelocity)
{
	LaunchVelocity = InVelocity;
	if (ProjectileMovement)
	{
		ProjectileMovement->Velocity = LaunchVelocity;
	}
}

void ABossMineHazard::BeginPlay()
{
	Super::BeginPlay();

	if (AActor* OwnerActor = GetOwner())
	{
		CollisionComp->IgnoreActorWhenMoving(OwnerActor, true);
	}

	// 同時多発スポーン時に地雷同士で押し合って浮かないよう、互いを移動衝突から除外する
	TArray<AActor*> ExistingMines;
	UGameplayStatics::GetAllActorsOfClass(this, ABossMineHazard::StaticClass(), ExistingMines);
	for (AActor* Actor : ExistingMines)
	{
		ABossMineHazard* OtherMine = Cast<ABossMineHazard>(Actor);
		if (!OtherMine || OtherMine == this || !OtherMine->CollisionComp) continue;

		CollisionComp->IgnoreActorWhenMoving(OtherMine, true);
		OtherMine->CollisionComp->IgnoreActorWhenMoving(this, true);
	}

	if (MeshComp && MeshComp->GetNumMaterials() > 0)
	{
		BlinkMID = MeshComp->CreateAndSetMaterialInstanceDynamic(0);
	}

	if (ProjectileMovement)
	{
		ProjectileMovement->Velocity = LaunchVelocity;
	}
}

void ABossMineHazard::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	switch (State)
	{
	case EMineState::Armed:
		ApplyBlinkMaterial(DeltaTime);
		UpdateProximityFuse(DeltaTime);
		break;
	case EMineState::Captured:
		UpdateCapture(DeltaTime);
		break;
	case EMineState::Launched:
		UpdateLaunch(DeltaTime);
		break;
	default:
		break;
	}
}

void ABossMineHazard::OnMineHit(UPrimitiveComponent* HitComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (State != EMineState::Airborne) return;
	if (OtherActor == GetOwner()) return;
	if (!Hit.bBlockingHit) return;

	// 接地面 (上向き法線) のときだけ起動する。壁/急斜面 (法線が寝ている) では起動せず、
	// PMCの滑り (Bounciness 0) と重力で壁に沿って落とし、地面に着いてから起動させる
	if (Hit.ImpactNormal.Z < GroundArmNormalZThreshold) return;

	ArmMine(Hit);
}

void ABossMineHazard::OnMineOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// 撃ち返し中は所有者ボスに触れたら爆発 (途中で追いついた場合の保険。通常は到達時に爆発)
	if (State == EMineState::Launched)
	{
		if (OtherActor == GetOwner())
		{
			ExplodeOnOwner();
		}
		return;
	}

	if (State != EMineState::Armed) return;
	if (!OtherActor || OtherActor == this) return;
	if (!Cast<APawn>(OtherActor)) return;

	const double Now = GetWorld()->GetTimeSeconds();
	if (Now - LastContactTime < ContactReArmInterval) return;
	LastContactTime = Now;

	const FVector ImpactLocation = SweepResult.bBlockingHit
		? FVector(SweepResult.ImpactPoint)
		: GetMineWorldLocation();
	Explode(ImpactLocation);
}

bool ABossMineHazard::TriggerByShockwave(const FVector& ShockwaveCenter, int32 IncomingWaveId)
{
	if (State != EMineState::Armed) return false;
	if (IncomingWaveId <= ImmuneThroughShockwaveWaveId) return false;

	const FVector MineLoc = GetMineWorldLocation();
	const FVector ToMine = (MineLoc - ShockwaveCenter).GetSafeNormal2D();
	const FVector Impact = MineLoc - ToMine * 10.0f;
	Explode(Impact);
	return true;
}

void ABossMineHazard::ArmMine(const FHitResult& Hit)
{
	if (State != EMineState::Airborne) return;

	State = EMineState::Armed;
	BlinkPhase = 0.0f;

	// 着地瞬間に位置をスナップする。GroundEmbedRatioで地面へのめり込み量を決める
	// 中心オフセット = 半径 * (1 - 2*Ratio): Ratio 0=中心が地表+半径
	// (上に乗る), 0.5=中心が地表(半分埋まる), 1=中心が地表-半径(完全に埋まる)
	if (CollisionComp && Hit.bBlockingHit)
	{
		const float Radius = CollisionComp->GetScaledSphereRadius();
		const float CenterOffset = Radius * (1.0f - 2.0f * GroundEmbedRatio);
		const FVector SnapLocation = Hit.ImpactPoint + Hit.ImpactNormal * CenterOffset;
		CollisionComp->SetWorldLocation(SnapLocation, false, nullptr, ETeleportType::TeleportPhysics);
	}
	if (ProjectileMovement)
	{
		ProjectileMovement->StopMovementImmediately();
		ProjectileMovement->SetComponentTickEnabled(false);
	}

	if (CollisionComp)
	{
		CollisionComp->SetWorldRotation(FRotator::ZeroRotator);
	}
}

void ABossMineHazard::Explode(const FVector& ImpactLocation)
{
	if (State == EMineState::Exploded) return;
	State = EMineState::Exploded;

	GetWorldTimerManager().ClearTimer(LifetimeTimerHandle);
	SetActorEnableCollision(false);
	const FVector MineLoc = GetMineWorldLocation();

	TArray<AActor*> OverlapActors;
	UKismetSystemLibrary::SphereOverlapActors(
		this,
		MineLoc,
		ExplosionRadius,
		{ UEngineTypes::ConvertToObjectType(ECC_Pawn) },
		nullptr,
		{ GetOwner(), this },
		OverlapActors);

	for (AActor* Actor : OverlapActors)
	{
		if (!Actor) continue;
		if (!TideCombatUtil::IsHostileTo(GetOwner(), Actor)) continue;

		IDamageable* Damageable = Cast<IDamageable>(Actor);
		if (!Damageable) continue;

		FDamageInfo DamageInfo;
		DamageInfo.BaseDamage = ExplosionDamage;
		DamageInfo.Instigator = GetOwner();
		DamageInfo.HitReactionTag = HitReactionTag;
		DamageInfo.HitResult.ImpactPoint = ImpactLocation;
		DamageInfo.HitResult.ImpactNormal = (Actor->GetActorLocation() - ImpactLocation).GetSafeNormal();
		Damageable->ReceiveDamage(DamageInfo);
	}

	if (ExplosionEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, ExplosionEffect, MineLoc);
	}

	// フラグが立っていれば破片を山なりに撒く
	// (Destroy前に発射: 破片は別アクタなので地雷消滅後も飛ぶ)
	if (bScatterFragmentsOnExplode)
	{
		ScatterFragments(MineLoc);
	}

	Destroy();
}

void ABossMineHazard::ScatterFragments(const FVector& Origin)
{
	UWorld* World = GetWorld();
	if (!World || !FragmentProfile || FragmentCount <= 0) return;

	UProjectilePoolSubsystem* Pool = World->GetSubsystem<UProjectilePoolSubsystem>();
	if (!Pool) return;

	const float RMin = FMath::Min(FragmentScatterRadiusMin, FragmentScatterRadiusMax);
	const float RMax = FMath::Max(FragmentScatterRadiusMin, FragmentScatterRadiusMax);
	const float Dmg  = (FragmentDamage >= 0.0f) ? FragmentDamage : ExplosionDamage;

	// 破片の所有者/発動者は地雷の所有者 (ボス)。敵対判定とダメージ帰属をボス基準にする
	APawn* OwnerPawn = Cast<APawn>(GetOwner());

	for (int32 i = 0; i < FragmentCount; ++i)
	{
		// 360度ランダム方向 × ランダム距離の着弾点 (高さは地雷と同じ = 地面)
		const float AngleDeg = FMath::FRandRange(0.0f, 360.0f);
		const float Radius   = FMath::FRandRange(RMin, RMax);
		const FVector Dir    = FRotator(0.0f, AngleDeg, 0.0f).Vector();
		FVector Target       = Origin + Dir * Radius;
		Target.Z             = Origin.Z;

		// 接地地雷だと生成直後に地面へ接触して即消滅するので少し上空から発射する
		const FVector SpawnLoc = Origin + FVector(0.0f, 0.0f, FragmentSpawnHeightOffset);
		const FTransform SpawnTM(FRotator::ZeroRotator, SpawnLoc);
		AEnemyProjectile* Frag = Pool->Acquire(FragmentProfile, SpawnTM, GetOwner(), OwnerPawn);
		if (!Frag) continue;

		// 破片ダメージは地雷側の値を適用する(飛行パラメータ・弧高はプロファイル側)
		Frag->Damage = Dmg;
		// 重力放物線で着弾点(地面)へ落とす
		if (UBallisticBehavior* Bombard = Frag->FindBehavior<UBallisticBehavior>())
		{
			Bombard->SetTargetLocation(Target);
		}
		// 範囲ダメージはEffect (USplashDamageBehavior) 側
		if (USplashDamageBehavior* Splash = Frag->FindBehavior<USplashDamageBehavior>())
		{
			Splash->SplashDamage = Dmg;
		}
		Frag->ActivateProjectile();
	}
}

EDamageResult ABossMineHazard::ReceiveDamage(const FDamageInfo& DamageInfo)
{
	if (State == EMineState::Exploded) return EDamageResult::Immune;

	// 「ボス自身の攻撃」でのみ起爆する。発動者がこの地雷を撒いた所有ボスのときだけ反応し、
	// プレイヤーの攻撃では起爆させない (誤って壊させない)
	if (DamageInfo.Instigator.Get() != GetOwner()) return EDamageResult::Immune;

	Explode(GetMineWorldLocation());
	return EDamageResult::Hit;
}

bool ABossMineHazard::CanBeDamaged() const
{
	return State != EMineState::Exploded;
}

void ABossMineHazard::ApplyBlinkMaterial(float DeltaTime)
{
	if (!BlinkMID) return;

	// 常時 (待機中) は点滅しない。接近起爆カウントが始まってから点滅する
	if (!bProximityFuseActive)
	{
		BlinkPhase = 0.0f;
		BlinkMID->SetScalarParameterValue(BlinkParameterName, 0.0f);
		return;
	}

	// カウント中は残り時間が0に近づくほど周期を短くして点滅を加速させる
	const float Alpha = FMath::Clamp(ProximityFuseRemaining / FMath::Max(0.01f, ProximityFuseDuration), 0.0f, 1.0f);
	const float FastPeriod = FMath::Max(0.01f, ProximityFuseFastBlinkPeriod);
	const float Period = FMath::Lerp(FastPeriod, FMath::Max(0.01f, BlinkPeriod), Alpha);

	// 周期が変わっても点滅が飛ばないよう、位相を積分して進める
	BlinkPhase += (2.0f * PI * DeltaTime) / Period;
	const float BlinkValue = 0.5f + 0.5f * FMath::Sin(BlinkPhase);
	BlinkMID->SetScalarParameterValue(BlinkParameterName, BlinkValue);
}

void ABossMineHazard::UpdateProximityFuse(float DeltaTime)
{
	if (State != EMineState::Armed) return;

	// まだ開始していなければ、範囲内に敵対対象が入った時点でカウントを開始する
	if (!bProximityFuseActive)
	{
		if (HasHostileTargetWithin(ProximityFuseRadius))
		{
			bProximityFuseActive = true;
			ProximityFuseRemaining = FMath::Max(0.01f, ProximityFuseDuration);
		}
		return;
	}

	// 一度開始したら離れても継続 (キャンセルしない)。0で起爆
	ProximityFuseRemaining -= DeltaTime;
	if (ProximityFuseRemaining <= 0.0f)
	{
		Explode(GetMineWorldLocation());
	}
}

bool ABossMineHazard::HasHostileTargetWithin(float Radius)
{
	if (Radius <= 0.0f) return false;

	TArray<AActor*> OverlapActors;
	UKismetSystemLibrary::SphereOverlapActors(
		this,
		GetMineWorldLocation(),
		Radius,
		{ UEngineTypes::ConvertToObjectType(ECC_Pawn) },
		nullptr,
		{ GetOwner(), this },
		OverlapActors);

	for (AActor* Actor : OverlapActors)
	{
		if (!Actor) continue;
		if (TideCombatUtil::IsHostileTo(GetOwner(), Actor)) return true;
	}
	return false;
}

void ABossMineHazard::OnWindEnter(const FWindInfluence& Wind)
{
	// 着地して起動済みの地雷だけが竜巻に巻き上げられる
	if (State != EMineState::Armed) return;

	State = EMineState::Captured;
	CapturedWind = Wind;

	// 竜巻に巻き込まれたら接近起爆のカウントは停止する
	bProximityFuseActive = false;
	ProximityFuseRemaining = 0.0f;

	// 旋回の起点を現在の中心からの方位に合わせ、滑らかに回り始める
	// 位置はCollisionComp基準で扱う
	// (ProjectileMovementが動かすのはRootではなくCollisionCompのため)
	const FVector Offset = GetMineWorldLocation() - Wind.Center;
	OrbitAngleDeg = FMath::RadiansToDegrees(FMath::Atan2(Offset.Y, Offset.X));

	// 巻き上げ中は自前で位置を駆動するため、投射移動は止めておく
	// (Armed時点で停止済みだが念のため)
	if (ProjectileMovement)
	{
		ProjectileMovement->StopMovementImmediately();
		ProjectileMovement->SetComponentTickEnabled(false);
	}
}

void ABossMineHazard::OnWindTick(const FWindInfluence& Wind, float DeltaTime)
{
	// 実際の移動はTick内のUpdateCaptureで行う。ここでは追従用に最新の影響を控えておく
	// (竜巻が移動・拡縮しても中心へ寄せ続けられるように)
	if (State != EMineState::Captured) return;
	CapturedWind = Wind;
}

void ABossMineHazard::OnWindExit()
{
	// 巻き上げ中に範囲外へ出ても (上昇して竜巻の高さを抜ける等)、すぐには解放しない
	// 撃ち返しの発火はあくまでWindSpinDurationの経過で行うため、
	// UpdateCaptureはキャッシュした最後の影響
	// (CapturedWind)を使って巻き上げを継続し、時間が来たら撃ち返す
	// (竜巻が途中で消えた場合も、最後の中心を基準に巻き上げ切ってから撃ち返す挙動になる)
}

void ABossMineHazard::UpdateCapture(float DeltaTime)
{
	if (!CollisionComp) return;

	// 旋回の見た目チューニング (竜巻サイズから導出するので手動プロパティは持たない)
	constexpr float OrbitRadiusRatio = 0.7f;		// 底 (巻き込み始め) の軌道半径 = 竜巻半径 * この割合
	constexpr float MinOuterRadius = 60.0f;			// 竜巻が極小でも底ではこれだけは回る
	constexpr float OrbitRadiusInterpSpeed = 5.0f;	// 目標半径へ寄せる速さ (巻き込み始めの段差をならす)

	const FVector Current = GetMineWorldLocation();
	const FVector Center = CapturedWind.Center;

	// 垂直: 竜巻の柱の上端 (頂上) まで上昇させる
	// ColumnTopZは範囲ボリュームから来るので必ず有限
	// 上昇速度は竜巻のLiftSpeedに地雷側の倍率を掛けて調整する
	const float ApexZ = CapturedWind.ColumnTopZ;
	const float LiftSpeed = CapturedWind.LiftSpeed * WindLiftSpeedMultiplier;
	float NewZ = Current.Z + LiftSpeed * DeltaTime;
	// 上昇できない設定 (実効LiftSpeed <= 0) でも止まらないよう、
	// その場合は即頂上扱いにする
	const bool bReachedApex = (NewZ >= ApexZ) || (LiftSpeed <= 0.0f);
	if (bReachedApex)
	{
		NewZ = ApexZ;
	}

	// 水平: ソフトクリーム状に巻き込む。高さが上がるほど軌道半径を絞り、頂上で中心 (半径0)
	// に収束させる。底では竜巻半径いっぱいの外周を回り、上昇に連れて内側へ巻き込まれていく
	const float ColumnHeight = FMath::Max(1.0f, ApexZ - Center.Z);
	const float HeightAlpha = FMath::Clamp((NewZ - Center.Z) / ColumnHeight, 0.0f, 1.0f);
	const float OuterRadius = FMath::Max(MinOuterRadius, CapturedWind.ColumnRadius * OrbitRadiusRatio);
	const float TargetRadius = FMath::Lerp(OuterRadius, 0.0f, HeightAlpha);
	FVector Offset = Current - Center;
	Offset.Z = 0.0f;
	const float Radius = FMath::FInterpTo(Offset.Size(), TargetRadius, DeltaTime, OrbitRadiusInterpSpeed);
	OrbitAngleDeg += WindOrbitAngularSpeedDeg * DeltaTime;
	const float AngleRad = FMath::DegreesToRadians(OrbitAngleDeg);
	const FVector NewXY = Center + FVector(Radius * FMath::Cos(AngleRad), Radius * FMath::Sin(AngleRad), 0.0f);

	CollisionComp->SetWorldLocation(FVector(NewXY.X, NewXY.Y, NewZ), false, nullptr, ETeleportType::TeleportPhysics);

	// 頂上に到達したら滞空させず即投擲へ遷移 (上昇 → 撃ち返しを途切れなく繋ぐ)
	if (bReachedApex)
	{
		BeginLaunch();
	}
}

void ABossMineHazard::BeginLaunch()
{
	if (State == EMineState::Exploded) return;

	State = EMineState::Launched;
	LaunchStartLocation = GetMineWorldLocation();
	LaunchElapsed = 0.0f;

	// 飛行時間とアーク高さを「開始時の水平距離」から決める
	// ・飛行時間 = 水平距離 / LaunchSpeed     → 速度は距離非依存
	// (遠いほど時間がかかるだけ)  ・アーク高さ = 水平距離 * 一定比率        →
	// 距離が変わっても同じ山なり形状になるこれで「遠いと爆速で飛ぶ」現象を無くし、
	// 距離によらず綺麗な放物線にする
	constexpr float ArcHeightRatio = 0.45f;	// 山なりの高さ / 水平距離。大きいほど高く打ち上がる
	const AActor* OwnerActor = GetOwner();
	const FVector TargetLoc = OwnerActor ? OwnerActor->GetActorLocation() : LaunchStartLocation;
	const float HorizontalDist = FVector(TargetLoc.X - LaunchStartLocation.X, TargetLoc.Y - LaunchStartLocation.Y, 0.0f).Size();

	LaunchDuration = FMath::Max(0.05f, HorizontalDist / FMath::Max(1.0f, LaunchSpeed));
	LaunchArcHeight = HorizontalDist * ArcHeightRatio;
}

void ABossMineHazard::UpdateLaunch(float DeltaTime)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		// 撒いたボスが既に消えていれば撃ち返す相手がいないので不発で消す
		Destroy();
		return;
	}

	LaunchElapsed += DeltaTime;
	const float Alpha = FMath::Clamp(LaunchElapsed / LaunchDuration, 0.0f, 1.0f);

	// XYは所有者へ毎フレーム追従 (= 水平方向のホーミング)、Zは放物線アークを重ねて山なりにする
	// XYが追従で曲がりつつZが放物線に従うので、直線で突っ込む弾道にはならない
	// 飛行時間・アーク高さはBeginLaunchで距離から算出済み (距離非依存の一定速度・一定形状)
	const FVector Target = OwnerActor->GetActorLocation();
	FVector NewLoc = FMath::Lerp(LaunchStartLocation, Target, Alpha);
	const float Arc = 4.0f * LaunchArcHeight * Alpha * (1.0f - Alpha);
	NewLoc.Z += Arc;

	if (CollisionComp)
	{
		CollisionComp->SetWorldLocation(NewLoc, false, nullptr, ETeleportType::TeleportPhysics);
	}

	if (Alpha >= 1.0f)
	{
		ExplodeOnOwner();
	}
}

void ABossMineHazard::ExplodeOnOwner()
{
	if (State == EMineState::Exploded) return;
	State = EMineState::Exploded;

	GetWorldTimerManager().ClearTimer(LifetimeTimerHandle);
	SetActorEnableCollision(false);
	const FVector MineLoc = GetMineWorldLocation();

	// Owner専用: 撒いたボス本体だけにダメージを与える (周囲への巻き込みはしない)
	if (AActor* OwnerActor = GetOwner())
	{
		if (IDamageable* Damageable = Cast<IDamageable>(OwnerActor))
		{
			// 撃ち返しは「竜巻でプレイヤーが跳ね返した」ものなので、ダメージ帰属は竜巻の所有者
			// (= 発生させたプレイヤー) にする。取得できなければ従来どおりボス自身を入れる
			AActor* Redirector = CapturedWind.Source.IsValid() ? CapturedWind.Source->GetOwner() : nullptr;
			AActor* DamageInstigator = Redirector ? Redirector : OwnerActor;

			FDamageInfo DamageInfo;
			DamageInfo.BaseDamage = ExplosionDamage;
			DamageInfo.Instigator = DamageInstigator;
			DamageInfo.HitReactionTag = HitReactionTag;
			DamageInfo.HitResult.ImpactPoint = MineLoc;
			DamageInfo.HitResult.ImpactNormal = (OwnerActor->GetActorLocation() - MineLoc).GetSafeNormal();
			Damageable->ReceiveDamage(DamageInfo);
		}
	}

	if (ExplosionEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, ExplosionEffect, MineLoc);
	}

	Destroy();
}

FVector ABossMineHazard::GetMineWorldLocation() const
{
	return CollisionComp ? CollisionComp->GetComponentLocation() : GetActorLocation();
}
