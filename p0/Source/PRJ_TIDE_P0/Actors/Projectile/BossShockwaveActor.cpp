// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/Actors/Projectile/BossShockwaveActor.h"
#include "PRJ_TIDE_P0/Actors/Hazard/HaloHazard.h"
#include "PRJ_TIDE_P0/Actors/Hazard/BossMineHazard.h"
#include "PRJ_TIDE_P0/Interfaces/Combat/IDamageable.h"
#include "PRJ_TIDE_P0/Data/Combat/DamageInfo.h"
#include "PRJ_TIDE_P0/Utilities/TideCombatUtils.h"

#include "ProceduralMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/OverlapResult.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"

namespace
{
	int32 GShockwaveWaveSerial = 0;
}

ABossShockwaveActor::ABossShockwaveActor()
{
	PrimaryActorTick.bCanEverTick = true;

	RingMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("RingMesh"));
	RingMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RingMesh->SetCastShadow(false);
	RootComponent = RingMesh;
}

int32 ABossShockwaveActor::PeekNextWaveId()
{
	return GShockwaveWaveSerial + 1;
}

void ABossShockwaveActor::BeginPlay()
{
	Super::BeginPlay();

	WaveId = ++GShockwaveWaveSerial;

	if (RingMaterial)
	{
		DynamicMaterial = UMaterialInstanceDynamic::Create(RingMaterial, this);
	}
}

void ABossShockwaveActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (Phase == EPhase::Rising)
	{
		RiseElapsed += DeltaSeconds;
		// 0 -> WaveHeightに立ち上がる
		CurrentHeight = WaveHeight * FMath::Clamp((RiseDuration > 0.0f) ? RiseElapsed / RiseDuration : 1.0f, 0.0f, 1.0f);

		if (RiseElapsed >= RiseDuration)
		{
			CurrentRadius = InitialRadius;
			Phase = (HoldDuration > 0.0f) ? EPhase::Holding : EPhase::Traveling;
		}
	}
	else if (Phase == EPhase::Holding)
	{
		HoldElapsed += DeltaSeconds;
		CurrentHeight = WaveHeight;
		if (HoldElapsed >= HoldDuration)
		{
			Phase = EPhase::Traveling;
		}
	}
	else
	{
		CurrentRadius += ExpandSpeed * DeltaSeconds;

		// 指定半径までは高さを維持し、終端に向けてまとめて減衰させる
		const float HeightFadeStartRadius = FMath::Lerp(InitialRadius, MaxRadius,
			FMath::Clamp(HeightFadeStartRadiusRatio, 0.0f, 1.0f));
		const float HeightFadeRange = FMath::Max(1.0f, MaxRadius - HeightFadeStartRadius);
		const float HeightFadeProgress = FMath::Clamp(
			(CurrentRadius - HeightFadeStartRadius) / HeightFadeRange, 0.0f, 1.0f);
		CurrentHeight = WaveHeight * (1.0f - HeightFadeProgress);

		ApplyBandDamage();

		if (CurrentRadius >= MaxRadius)
		{
			Destroy();
			return;
		}
	}

	// フェード (Travelingのみ計算。それ以外は不透明)
	if (DynamicMaterial)
	{
		float Opacity = 1.0f;
		if (Phase == EPhase::Traveling)
		{
			const float FadeStartRadius = MaxRadius * FadeStartRadiusRatio;
			const float FadeRange       = MaxRadius - FadeStartRadius;
			Opacity = (FadeRange > 0.0f)
				? FMath::Clamp(1.0f - (CurrentRadius - FadeStartRadius) / FadeRange, 0.0f, 1.0f)
				: 1.0f;
		}
		DynamicMaterial->SetScalarParameterValue(OpacityParamName, Opacity);
	}

	const float DisplayRadius = (Phase == EPhase::Traveling) ? CurrentRadius : InitialRadius;
	BuildRingMesh(DisplayRadius, CurrentHeight);
}

void ABossShockwaveActor::BuildRingMesh(float DisplayRadius, float Height)
{
	const int32 Seg = FMath::Max(8, RingSegments);
	const float HalfW = RingThickness * 0.5f;
	const FTransform Xf = GetActorTransform();
	const FVector ActorLoc = Xf.GetLocation();

	TArray<FVector> Verts;
	TArray<int32> Tris;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FLinearColor> Colors;
	TArray<FProcMeshTangent> Tangents;

	// 1セグメントあたり4頂点 (内下/外下/内上/外上)
	Verts.Reserve((Seg + 1) * 4);
	Normals.Reserve((Seg + 1) * 4);
	UVs.Reserve((Seg + 1) * 4);
	Tangents.Reserve((Seg + 1) * 4);
	Tris.Reserve(Seg * 18);

	for (int32 i = 0; i <= Seg; ++i)
	{
		const float Angle = (2.0f * PI * static_cast<float>(i)) / static_cast<float>(Seg);
		const FVector Dir(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f);
		const FVector Circum(-FMath::Sin(Angle), FMath::Cos(Angle), 0.0f);

		// セグメント中心線のXYで1回だけ地面トレースし、底辺の高さに使う (1区間は平らに保つ)
		const FVector CenterXY = ActorLoc + Dir * DisplayRadius;
		const float GroundZ = GroundZAt(FVector2D(CenterXY.X, CenterXY.Y));

		const FVector InnerXY = ActorLoc + Dir * (DisplayRadius - HalfW);
		const FVector OuterXY = ActorLoc + Dir * (DisplayRadius + HalfW);

		const FVector InnerBottom(InnerXY.X, InnerXY.Y, GroundZ);
		const FVector OuterBottom(OuterXY.X, OuterXY.Y, GroundZ);
		const FVector InnerTop(InnerXY.X, InnerXY.Y, GroundZ + Height);
		const FVector OuterTop(OuterXY.X, OuterXY.Y, GroundZ + Height);

		Verts.Add(Xf.InverseTransformPosition(InnerBottom)); // +0
		Verts.Add(Xf.InverseTransformPosition(OuterBottom)); // +1
		Verts.Add(Xf.InverseTransformPosition(InnerTop));    // +2
		Verts.Add(Xf.InverseTransformPosition(OuterTop));    // +3

		// 共有頂点のため法線は上向きで代表させる (発光/Unlitマテリアル前提)
		Normals.Add(FVector::UpVector);
		Normals.Add(FVector::UpVector);
		Normals.Add(FVector::UpVector);
		Normals.Add(FVector::UpVector);

		const float U = static_cast<float>(i) / static_cast<float>(Seg);
		UVs.Add(FVector2D(U, 0.0f)); // 内下
		UVs.Add(FVector2D(U, 0.0f)); // 外下
		UVs.Add(FVector2D(U, 1.0f)); // 内上
		UVs.Add(FVector2D(U, 1.0f)); // 外上

		const FProcMeshTangent Tan(Xf.InverseTransformVectorNoScale(Circum), false);
		Tangents.Add(Tan);
		Tangents.Add(Tan);
		Tangents.Add(Tan);
		Tangents.Add(Tan);
	}

	auto AddQuad = [&Tris](int32 A, int32 B, int32 C, int32 D)
	{
		Tris.Add(A); Tris.Add(B); Tris.Add(C);
		Tris.Add(A); Tris.Add(C); Tris.Add(D);
	};

	for (int32 s = 0; s < Seg; ++s)
	{
		const int32 B0 = 4 * s;       // このセグメント
		const int32 B1 = 4 * (s + 1); // 次のセグメント

		const int32 IB0 = B0 + 0, OB0 = B0 + 1, IT0 = B0 + 2, OT0 = B0 + 3;
		const int32 IB1 = B1 + 0, OB1 = B1 + 1, IT1 = B1 + 2, OT1 = B1 + 3;

		// 天面/外壁/内壁 (底面は地面に接するため省略)
		AddQuad(IT0, OT0, OT1, IT1); // 天面
		AddQuad(OB0, OT0, OT1, OB1); // 外壁
		AddQuad(IB0, IB1, IT1, IT0); // 内壁
	}

	RingMesh->CreateMeshSection_LinearColor(0, Verts, Tris, Normals, UVs, Colors, Tangents, false);

	if (DynamicMaterial)
	{
		RingMesh->SetMaterial(0, DynamicMaterial);
	}
}

float ABossShockwaveActor::GroundZAt(const FVector2D& WorldXY, const AActor* AlsoIgnore) const
{
	const UWorld* World = GetWorld();
	const float BaseZ = GetActorLocation().Z;
	if (!World) return BaseZ;

	const FVector TraceStart(WorldXY.X, WorldXY.Y, BaseZ + GroundTraceUpDistance);
	const FVector TraceEnd(WorldXY.X, WorldXY.Y, BaseZ - GroundTraceDownDistance);

	// bTraceComplex = true: 地面がコンプレックスコリジョンでも拾えるようにする
	FCollisionQueryParams Params(FName(TEXT("ShockwaveGround")), true, this);
	if (const AActor* MyOwner = GetOwner()) Params.AddIgnoredActor(MyOwner);
	if (AlsoIgnore) Params.AddIgnoredActor(AlsoIgnore);
	if (APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(World, 0))
	{
		Params.AddIgnoredActor(PlayerPawn);

		TArray<AActor*> AttachedActors;
		PlayerPawn->GetAttachedActors(AttachedActors, true);
		for (AActor* AttachedActor : AttachedActors)
		{
			if (AttachedActor)
			{
				Params.AddIgnoredActor(AttachedActor);
			}
		}

		if (AController* PlayerController = PlayerPawn->GetController())
		{
			Params.AddIgnoredActor(PlayerController);
		}
	}

	TArray<FHitResult> Hits;
	if (World->LineTraceMultiByChannel(Hits, TraceStart, TraceEnd, ECC_Visibility, Params))
	{
		for (const FHitResult& Hit : Hits)
		{
			AActor* HitActor = Hit.GetActor();
			if (HitActor && Cast<ABossMineHazard>(HitActor))
			{
				continue;
			}

			// 地面はPawnをブロックする。カットシーントリガー等のPawnを通す
			// クエリ専用ボリュームは地面ではないので底辺の基準にしない
			const UPrimitiveComponent* HitComp = Hit.GetComponent();
			if (!HitComp || HitComp->GetCollisionResponseToChannel(ECC_Pawn) != ECR_Block)
			{
				continue;
			}

			return Hit.ImpactPoint.Z;
		}
	}
	return BaseZ;
}

void ABossShockwaveActor::ApplyBandDamage()
{
	UWorld* World = GetWorld();
	if (!World) return;

	const FVector Center = GetActorLocation();
	const float HalfW = RingThickness * 0.5f;
	const float InnerRadius = CurrentRadius - HalfW;

	// リング外縁までの球でPawnとWorldDynamic (光輪) を集め、2D距離でバンド内に絞る
	FCollisionObjectQueryParams ObjParams;
	ObjParams.AddObjectTypesToQuery(ECC_Pawn);
	ObjParams.AddObjectTypesToQuery(ECC_WorldDynamic);

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);
	if (AActor* MyOwner = GetOwner()) Params.AddIgnoredActor(MyOwner);

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, Center, FQuat::Identity, ObjParams,
		FCollisionShape::MakeSphere(CurrentRadius + HalfW), Params);

	for (const FOverlapResult& Result : Overlaps)
	{
		AActor* Other = Result.GetActor();
		if (!Other || HitActors.Contains(Other)) continue;

		FVector OtherWorldLocation = Other->GetActorLocation();
		if (const ABossMineHazard* MineForLocation = Cast<ABossMineHazard>(Other))
		{
			OtherWorldLocation = MineForLocation->GetMineWorldLocation();
		}

		// まだリング内縁の内側 (穴の中) なら波が到達していない
		if (FVector::Dist2D(OtherWorldLocation, Center) < InnerRadius) continue;

		// 配置された光輪はショックウェーブで破砕する
		if (AHaloHazard* Hazard = Cast<AHaloHazard>(Other))
		{
			HitActors.Add(Other);
			const FVector ShatterDir = (OtherWorldLocation - Center).GetSafeNormal2D();
			Hazard->Shatter(Center, ShatterDir);
			continue;
		}

		if (ABossMineHazard* Mine = Cast<ABossMineHazard>(Other))
		{
			if (Mine->TriggerByShockwave(Center, WaveId))
			{
				HitActors.Add(Other);
			}
			continue;
		}

		if (!TideCombatUtil::IsHostileTo(GetOwner(), Other)) continue;
		IDamageable* Damageable = Cast<IDamageable>(Other);
		if (!Damageable) continue;

		// ジャンプ回避: PCの足元が波の天面 (足元の地面 + CurrentHeight)
		// を越えていたらスキップ。HitActorsには追加せず、
		// 波が通り過ぎる前に着地すれば後続Tickで当たり得る
		// 地面トレースはOther (PC) 自身を無視する。さもないとPCのカプセルを地面と誤認する
		const FVector OtherLoc = Other->GetActorLocation();
		const float WaveTopZ = GroundZAt(FVector2D(OtherLoc.X, OtherLoc.Y), Other) + CurrentHeight;
		const float FootZ = OtherLoc.Z - Other->GetSimpleCollisionHalfHeight();
		const bool bClearedByJump = FootZ > WaveTopZ;

		if (bDebugDrawJumpCheck)
		{
			// 波の天面 (黄) とPC足元 (緑=回避/赤=ヒット) を描画
			DrawDebugSphere(World, FVector(OtherLoc.X, OtherLoc.Y, WaveTopZ), 30.0f, 8, FColor::Yellow, false, 2.0f);
			DrawDebugSphere(World, FVector(OtherLoc.X, OtherLoc.Y, FootZ), 30.0f, 8,
				bClearedByJump ? FColor::Green : FColor::Red, false, 2.0f);
		}

		if (bClearedByJump) continue;

		HitActors.Add(Other);

		FDamageInfo DamageInfo;
		DamageInfo.BaseDamage     = Damage;
		DamageInfo.Instigator     = GetOwner();
		DamageInfo.HitReactionTag = HitReactionTag;

		Damageable->ReceiveDamage(DamageInfo);
	}
}
