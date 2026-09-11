// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/Actors/Hazard/CircusRainConductor.h"

#include "PRJ_TIDE_P0/Actors/Character/Enemy/EnemyCharacter.h"
#include "PRJ_TIDE_P0/Actors/Projectile/EnemyProjectile.h"
#include "PRJ_TIDE_P0/Actors/Projectile/Behaviors/ArcPathBehavior.h"
#include "PRJ_TIDE_P0/Utilities/TideCombatUtils.h"

#include "Components/DecalComponent.h"
#include "Components/SceneComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "DrawDebugHelpers.h"

ACircusRainConductor::ACircusRainConductor()
{
	PrimaryActorTick.bCanEverTick = true;

	// 見た目は持たないが、
	// Rootが無いとtransformを持てず一部処理が警告を出すので空Rootを置く
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
}

void ACircusRainConductor::Initialize(AEnemyCharacter* InEnemy, const FConfig& InConfig, float SafetyLifeSpan)
{
	Enemy = InEnemy;
	Config = InConfig;

	// 全弾の降下とデカール消化で自然にDestroyするが、取りこぼした時のための保険を張る
	if (SafetyLifeSpan > 0.0f)
	{
		SetLifeSpan(SafetyLifeSpan);
	}
}

void ACircusRainConductor::AddDescent(AEnemyProjectile* Missile, float DescendAtTime)
{
	if (!Missile) return;

	FScheduledDescent Descent;
	Descent.Missile = Missile;
	Descent.DescendAtTime = FMath::Max(0.0f, DescendAtTime);
	Descents.Add(Descent);
}

void ACircusRainConductor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	Elapsed += DeltaTime;

	for (FScheduledDescent& Descent : Descents)
	{
		if (Descent.bCommanded || Elapsed < Descent.DescendAtTime) continue;

		// 消えた弾 (被弾/寿命) はスキップして予約だけ消化済みにする
		if (AEnemyProjectile* Missile = Descent.Missile.Get())
		{
			CommandDescent(Missile);
		}
		Descent.bCommanded = true;
		++CommandedCount;
	}

	UpdateDecals(DeltaTime);

	// 全弾の降下を出し切り、予兆デカールも消化しきったら用済みなので自壊する
	if (CommandedCount >= Descents.Num() && PendingDecals.Num() == 0)
	{
		Destroy();
	}
}

void ACircusRainConductor::CommandDescent(AEnemyProjectile* Missile)
{
	UArcPathBehavior* Arc = Missile->FindBehavior<UArcPathBehavior>();
	if (!Arc) return;

	const FVector MissilePos = Missile->GetActorLocation();

	// 降下開始の「今」のPC位置を基準に着弾点を確定する。これで直進もジグザグも追従できる
	// PCが取れない場合は弾の真下へ落とす (追従を諦めるが降雨自体は成立させる)
	FVector Center = MissilePos;
	if (const AActor* Target = TideCombatUtil::GetTarget(Enemy.Get()))
	{
		Center = Target->GetActorLocation() + Target->GetVelocity() * Config.PredictLeadTime;
	}

	const float Radius = Config.ScatterRadius * FMath::Sqrt(FMath::FRand());
	const float Angle = FMath::FRandRange(0.0f, 360.0f);
	const FVector Offset = FVector::ForwardVector.RotateAngleAxis(Angle, FVector::UpVector) * Radius;
	const FVector Impact = ProjectToGround(Center + Offset);

	// ホバー位置 → 着弾点の降下弧。始点の接線を水平寄りに保つため、Mid1は頂点から
	// 「水平に着弾方向へ抜けつつ上へ持ち上げた」山越え点に置く。ここを着弾点方向へ下げて置くと
	// 降下開始で真下へガクッと向いてしまうので、必ず始点基準の水平成分 + 上げで作る
	// Mid2は着弾点の真上に置き、最後は垂直に近い角度で落とす。螺旋/ノイズはプロファイル側が乗せる
	const FVector Up(0.0f, 0.0f, 1.0f);
	const FVector Flat(Impact.X - MissilePos.X, Impact.Y - MissilePos.Y, 0.0f);
	const FVector Mid1 = MissilePos + Flat * 0.33f + Up * Config.DescentSwoopHeight;
	const FVector Mid2 = Impact + Up * Config.ImpactApproachHeight;
	const TArray<FVector> MidPoints = { Mid1, Mid2 };

	Arc->CommandDescend(MissilePos, Impact, MidPoints, Config.DescentFlightDuration);

	AddImpactDecal(Impact);

	if (Config.bDebugDraw && GetWorld())
	{
		DrawDebugSphere(GetWorld(), Impact, 60.0f, 12, FColor::Yellow, false, 3.0f);
		DrawDebugLine(GetWorld(), MissilePos, Impact, FColor::Orange, false, 3.0f, 0, 2.0f);
	}
}

FVector ACircusRainConductor::ProjectToGround(const FVector& Location) const
{
	UWorld* World = GetWorld();
	if (!World) return Location;

	FVector Grounded = Location;

	FHitResult FloorHit;
	FCollisionQueryParams FloorParams;
	if (Enemy.IsValid())
	{
		FloorParams.AddIgnoredActor(Enemy.Get());
	}

	const FVector TraceFrom(Location.X, Location.Y, Location.Z + Config.GroundTraceUp);
	const FVector TraceTo(Location.X, Location.Y, Location.Z - Config.GroundTraceDown);
	if (World->LineTraceSingleByChannel(FloorHit, TraceFrom, TraceTo, ECC_WorldStatic, FloorParams))
	{
		Grounded.Z = FloorHit.ImpactPoint.Z;
	}
	return Grounded;
}

void ACircusRainConductor::AddImpactDecal(const FVector& Location)
{
	UWorld* World = GetWorld();
	const float LeadTime = Config.DescentFlightDuration;
	if (!Config.DecalMaterial || LeadTime <= 0.0f || !World) return;

	const FVector DecalSize(Config.DecalDepth, Config.DecalRadius, Config.DecalRadius);

	// 寿命はLeadTime。着弾までに自然消滅するので取りこぼしても残骸にならない
	UDecalComponent* Decal = UGameplayStatics::SpawnDecalAtLocation(
		World, Config.DecalMaterial, DecalSize, Location, FRotator(-90.0f, 0.0f, 0.0f), LeadTime);
	if (!Decal) return;

	FImpactDecal Pending;
	Pending.RemainingTime = LeadTime;
	Pending.TotalTime = FMath::Max(0.0001f, LeadTime);
	Pending.Decal = Decal;
	if (UMaterialInstanceDynamic* MID = Decal->CreateDynamicMaterialInstance())
	{
		MID->SetScalarParameterValue(Config.DecalProgressParam, 0.0f);
		Pending.DecalMID = MID;
	}

	PendingDecals.Add(Pending);
}

void ACircusRainConductor::UpdateDecals(float DeltaTime)
{
	for (int32 i = PendingDecals.Num() - 1; i >= 0; --i)
	{
		FImpactDecal& Pending = PendingDecals[i];
		Pending.RemainingTime -= DeltaTime;

		const float Progress = FMath::Clamp(
			(Pending.TotalTime - Pending.RemainingTime) / Pending.TotalTime, 0.0f, 1.0f);
		if (UMaterialInstanceDynamic* MID = Pending.DecalMID.Get())
		{
			MID->SetScalarParameterValue(Config.DecalProgressParam, Progress);
		}

		if (Pending.RemainingTime <= 0.0f)
		{
			if (UDecalComponent* Decal = Pending.Decal.Get())
			{
				Decal->DestroyComponent();
			}
			PendingDecals.RemoveAt(i);
		}
	}
}
