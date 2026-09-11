// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "ArcPathBehavior.h"
#include "PRJ_TIDE_P0/Actors/Projectile/EnemyProjectile.h"
#include "PRJ_TIDE_P0/Interfaces/Combat/IDamageable.h"
#include "PRJ_TIDE_P0/Utilities/TideCombatUtils.h"
#include "PRJ_TIDE_P0/Data/Combat/DamageInfo.h"

#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "DrawDebugHelpers.h"

namespace
{
	// デバッグ描画の表示秒数 ※1発の飛行を追い切れる程度に長く取る
	constexpr float ArcDebugDrawDuration = 8.0f;
}

void UArcPathBehavior::SetTargetLocation(const FVector& WorldLocation)
{
	CfgP3 = WorldLocation;
	bCfgTarget = true;
}

void UArcPathBehavior::SetControlPoint(const FVector& WorldLocation)
{
	CfgMidPoints = { WorldLocation };
	bCfgSingleGuide = true;
}

void UArcPathBehavior::SetControlPoints(const FVector& InP1, const FVector& InP2)
{
	CfgMidPoints = { InP1, InP2 };
	bCfgSingleGuide = false;
}

void UArcPathBehavior::SetCurveMidPoints(const TArray<FVector>& InMidPoints)
{
	CfgMidPoints = InMidPoints;
	bCfgSingleGuide = false;
}

void UArcPathBehavior::SetInitialHeadingDirection(const FVector& WorldDirection)
{
	CfgHeadingDir = WorldDirection.GetSafeNormal();
	bCfgHeading = !CfgHeadingDir.IsNearlyZero();
}

void UArcPathBehavior::SetDeploy(const FVector& InDeployLocation, float InHoverDuration)
{
	CfgDeployLoc = InDeployLocation;
	CfgHoverDur  = FMath::Max(0.0f, InHoverDuration);
	bCfgDeploy   = true;
}

void UArcPathBehavior::SetHomingHandoff(AActor* InTarget, float InHandoffRatio, float InTurnRateDegPerSec)
{
	CfgHomingTarget = InTarget;
	CfgHandoffRatio = FMath::Clamp(InHandoffRatio, 0.0f, 1.0f);
	CfgHandoffTurn  = FMath::Max(0.0f, InTurnRateDegPerSec);
}

void UArcPathBehavior::CommandDescend(const FVector& CurrentLocation, const FVector& Target,
	const TArray<FVector>& InMidPoints, float InFlightDuration)
{
	// 本発射以降は軌道が確定しているので指令を無視する (二重発射・途中差し替えを防ぐ)
	if (Phase != EArcPhase::Deploying && Phase != EArcPhase::Hovering && Phase != EArcPhase::Aiming)
	{
		return;
	}

	P3 = Target;
	MidPoints = InMidPoints;
	bSingleGuide = false;
	if (InFlightDuration > 0.0f)
	{
		FlightDuration = InFlightDuration;
	}

	// 降下の初期方位は最初の制御点 (無ければ着弾点) へ向ける。上昇時の方位保持を引きずらせない
	const FVector AimPoint = MidPoints.IsEmpty() ? P3 : MidPoints[0];
	InitialHeadingDirection = (AimPoint - CurrentLocation).GetSafeNormal();
	bInitialHeadingSet = !InitialHeadingDirection.IsNearlyZero();

	// 現在地を始点に弧を組み直して即Firingへ。以降は通常のTickFiringが走る
	StartFiringFrom(CurrentLocation);
}

void UArcPathBehavior::OnLaunch(AEnemyProjectile* Projectile)
{
	// 実行時状態を初期化
	FiringDistance = 0.0f;
	FiringElapsed = 0.0f;
	HoverElapsed = 0.0f;
	AimElapsed = 0.0f;
	HomingElapsed = 0.0f;
	bFrameInitialized = false;
	FrameTangent = FVector::ForwardVector;
	FrameNormal = FVector::RightVector;
	FrameBinormal = FVector::UpVector;
	HomingDir = FVector::ForwardVector;
	HomingMoveSpeed = 0.0f;
	NoiseSeedU = FMath::FRandRange(-1000.0f, 1000.0f);
	NoiseSeedV = FMath::FRandRange(-1000.0f, 1000.0f);
	SpiralPhase = FMath::FRandRange(0.0f, 2.0f * PI);
	SpiralSign = (bRandomizeSpiralDirection && FMath::RandBool()) ? -1.0f : 1.0f;

	if (!Projectile) return;

	// 弧軌道を自前で制御するため誘導中だけPMCを止める
	if (Projectile->ProjectileMovement)
	{
		Projectile->ProjectileMovement->StopMovementImmediately();
		Projectile->ProjectileMovement->SetComponentTickEnabled(false);
	}

	const FVector LaunchLocation = Projectile->GetActorLocation();

	// 設定を実行時状態へ取り込む
	P3 = bCfgTarget ? CfgP3 : (LaunchLocation + Projectile->GetActorForwardVector() * 1000.0f);

	bInitialHeadingSet = bCfgHeading;
	InitialHeadingDirection = CfgHeadingDir;

	HomingTarget = CfgHomingTarget;
	HomingHandoffRatio = CfgHandoffRatio;
	HomingHandoffTurnRateDegPerSec = CfgHandoffTurn;

	bUseDeploy = bCfgDeploy;
	DeployLocation = CfgDeployLoc;
	DeployHoverDuration = CfgHoverDur;

	MidPoints = CfgMidPoints;
	bSingleGuide = bCfgSingleGuide;

	if (bUseDeploy)
	{
		// まずDeployLocationへ飛び待機してから本発射に移行
		// 制御点は発射開始時(StartFiringFrom)に組み立てる
		Phase = EArcPhase::Deploying;
		CurvePoints.Reset();

		const FVector ToDeploy = (DeployLocation - LaunchLocation).GetSafeNormal();
		if (!ToDeploy.IsNearlyZero())
		{
			Projectile->SetActorRotation(ToDeploy.Rotation());
		}
	}
	else
	{
		// 即発射 (弧)
		Phase = EArcPhase::Firing;
		BuildCurve(LaunchLocation);

		// 制御点確定後に弧長テーブルを作り巡航速度を確定する
		BuildArcLengthTable();

		if (bDebugDrawPath) DebugDrawPath(Projectile->GetWorld());
	}

	// 設定フラグをリセット (プール再利用で前回値を残さない)
	bCfgTarget = false;
	CfgMidPoints.Reset();
	bCfgSingleGuide = false;
	bCfgHeading = false;
	bCfgDeploy = false;
	CfgHandoffRatio = 0.0f;
	CfgHomingTarget = nullptr;
}

void UArcPathBehavior::BuildCurve(const FVector& Start)
{
	CurvePoints.Reset(MidPoints.Num() + 2);
	CurvePoints.Add(Start);

	if (MidPoints.IsEmpty())
	{
		// 指定なし: ArcHeightぶん持ち上げた素直な山なり (3次)
		const FVector Delta = P3 - Start;
		const FVector Up(0.0f, 0.0f, 1.0f);
		CurvePoints.Add(Start + Delta * (1.0f / 3.0f) + Up * ArcHeight);
		CurvePoints.Add(Start + Delta * (2.0f / 3.0f) + Up * (ArcHeight * 0.35f));
	}
	else if (bSingleGuide)
	{
		// ガイド点1個: 始点側と着弾点側から寄せた2点に展開して3次に落とす
		const FVector Guide = MidPoints[0];
		CurvePoints.Add(FMath::Lerp(Start, Guide, 0.75f));
		CurvePoints.Add(FMath::Lerp(P3, Guide, 0.75f));
	}
	else
	{
		CurvePoints.Append(MidPoints);
	}

	CurvePoints.Add(P3);
}

void UArcPathBehavior::StartFiringFrom(const FVector& Origin)
{
	BuildCurve(Origin);
	FiringElapsed = 0.0f;
	bFrameInitialized = false;
	BuildArcLengthTable();
	Phase = EArcPhase::Firing;
}

void UArcPathBehavior::BuildArcLengthTable()
{
	// ベジェを均等tでサンプルし、区間チョード長の累積を弧長近似として持つ
	ArcParamTable.Reset();
	ArcLengthTable.Reset();

	const int32 SampleCount = 24;
	ArcParamTable.Reserve(SampleCount + 1);
	ArcLengthTable.Reserve(SampleCount + 1);

	FVector Prev = EvaluatePosition(0.0f);
	float Accum = 0.0f;
	ArcParamTable.Add(0.0f);
	ArcLengthTable.Add(0.0f);

	for (int32 i = 1; i <= SampleCount; ++i)
	{
		const float t = static_cast<float>(i) / static_cast<float>(SampleCount);
		const FVector P = EvaluatePosition(t);
		Accum += FVector::Dist(Prev, P);
		ArcParamTable.Add(t);
		ArcLengthTable.Add(Accum);
		Prev = P;
	}

	TotalArcLength = Accum;
	FiringSpeed = TotalArcLength / FMath::Max(FlightDuration, KINDA_SMALL_NUMBER);
	FiringDistance = 0.0f;
}

void UArcPathBehavior::DebugDrawPath(const UWorld* World) const
{
	if (!World) return;

	UWorld* MutableWorld = const_cast<UWorld*>(World);

	// 基準ベジェ (揺らぎを乗せる前の芯)
	const int32 SampleCount = 48;
	FVector Prev = EvaluatePosition(0.0f);
	for (int32 i = 1; i <= SampleCount; ++i)
	{
		const FVector Point = EvaluatePosition(static_cast<float>(i) / SampleCount);
		DrawDebugLine(MutableWorld, Prev, Point, FColor::Green, false, ArcDebugDrawDuration, 0, 3.0f);
		Prev = Point;
	}

	// 制御多角形。曲線がどちらへ引っ張られているかはこの折れ線で読む
	for (int32 i = 0; i < CurvePoints.Num() - 1; ++i)
	{
		DrawDebugLine(MutableWorld, CurvePoints[i], CurvePoints[i + 1],
			FColor(80, 80, 80), false, ArcDebugDrawDuration, 0, 1.0f);
	}

	// 始点 = 白 / 中間制御点 = 水色 (番号付き) / 着弾点 = 赤
	for (int32 i = 0; i < CurvePoints.Num(); ++i)
	{
		const bool bIsEnd = (i == 0) || (i == CurvePoints.Num() - 1);
		const FColor Color = (i == 0) ? FColor::White
			: (i == CurvePoints.Num() - 1) ? FColor::Red : FColor::Cyan;

		DrawDebugSphere(MutableWorld, CurvePoints[i], bIsEnd ? 40.0f : 60.0f, 8,
			Color, false, ArcDebugDrawDuration);
		DrawDebugString(MutableWorld, CurvePoints[i] + FVector(0.0f, 0.0f, 80.0f),
			FString::Printf(TEXT("P%d"), i), nullptr, Color, ArcDebugDrawDuration);
	}
}

float UArcPathBehavior::ParamAtDistance(float Dist) const
{
	const int32 Num = ArcLengthTable.Num();
	if (Num < 2 || TotalArcLength <= KINDA_SMALL_NUMBER)
	{
		return 1.0f;
	}

	Dist = FMath::Clamp(Dist, 0.0f, TotalArcLength);

	// 累積弧長テーブルを二分探索し、区間内を線形補間してtを返す
	int32 Lo = 0;
	int32 Hi = Num - 1;
	while (Lo + 1 < Hi)
	{
		const int32 Mid = (Lo + Hi) / 2;
		if (ArcLengthTable[Mid] <= Dist) Lo = Mid; else Hi = Mid;
	}

	const float L0 = ArcLengthTable[Lo];
	const float L1 = ArcLengthTable[Hi];
	const float Alpha = (L1 - L0 > KINDA_SMALL_NUMBER) ? (Dist - L0) / (L1 - L0) : 0.0f;
	return FMath::Lerp(ArcParamTable[Lo], ArcParamTable[Hi], Alpha);
}

void UArcPathBehavior::OnTick(AEnemyProjectile* Projectile, float DeltaSeconds)
{
	if (!Projectile) return;

	switch (Phase)
	{
	case EArcPhase::Deploying: TickDeploy(Projectile, DeltaSeconds); return;
	case EArcPhase::Hovering: TickHover(Projectile, DeltaSeconds); return;
	case EArcPhase::Aiming: TickAim(Projectile, DeltaSeconds); return;
	case EArcPhase::Homing: TickHoming(Projectile, DeltaSeconds); return;
	case EArcPhase::Inertial: return;
	default: break;
	}

	TickFiring(Projectile, DeltaSeconds);
}

void UArcPathBehavior::TickDeploy(AEnemyProjectile* Projectile, float DeltaSeconds)
{
	const FVector Pos = Projectile->GetActorLocation();
	const FVector ToDeploy = DeployLocation - Pos;
	const float Dist = ToDeploy.Size();
	if (Dist <= FMath::Max(1.0f, DeployAcceptRadius))
	{
		Projectile->SetActorLocation(DeployLocation);
		HoverBaseLocation = DeployLocation;
		HoverElapsed = 0.0f;
		Phase = EArcPhase::Hovering;
		return;
	}

	const FVector Dir = ToDeploy / Dist;
	const float Step = FMath::Min(Dist, FMath::Max(0.0f, DeploySpeed) * DeltaSeconds);
	if (!MoveWithSweep(Projectile, Pos + Dir * Step))
	{
		return;
	}
	Projectile->SetActorRotation(Dir.Rotation());
}

void UArcPathBehavior::TickHover(AEnemyProjectile* Projectile, float DeltaSeconds)
{
	HoverElapsed += DeltaSeconds;

	const float Bob = FMath::Sin(HoverElapsed * 2.0f * PI * 1.5f) * FMath::Max(0.0f, DeployHoverBobAmplitude);
	if (!MoveWithSweep(Projectile, HoverBaseLocation + FVector(0.0f, 0.0f, Bob)))
	{
		return;
	}

	if (HoverElapsed >= DeployHoverDuration)
	{
		if (AimDuration > 0.0f)
		{
			AimElapsed = 0.0f;
			Phase = EArcPhase::Aiming;
		}
		else
		{
			StartFiringFrom(Projectile->GetActorLocation());
			if (bDebugDrawPath) DebugDrawPath(Projectile->GetWorld());
		}
	}
}

void UArcPathBehavior::TickAim(AEnemyProjectile* Projectile, float DeltaSeconds)
{
	AimElapsed += DeltaSeconds;

	// 発射前なので曲線はまだ組み立てていない。最初の制御点 (無ければ着弾点) を向く
	const FVector AimPoint = MidPoints.IsEmpty() ? P3 : MidPoints[0];
	const FVector ToAim = (AimPoint - Projectile->GetActorLocation()).GetSafeNormal();
	if (!ToAim.IsNearlyZero())
	{
		Projectile->SetActorRotation(FMath::RInterpConstantTo(
			Projectile->GetActorRotation(), ToAim.Rotation(), DeltaSeconds, FMath::Max(0.0f, AimRotationRate)));
	}

	if (AimElapsed >= FMath::Max(0.0f, AimDuration))
	{
		StartFiringFrom(Projectile->GetActorLocation());
		if (bDebugDrawPath) DebugDrawPath(Projectile->GetWorld());
	}
}

FVector UArcPathBehavior::HomingTargetCenter(AEnemyProjectile* Projectile) const
{
	const AActor* Target = HomingTarget.Get();
	if (!Target) return Projectile->GetActorLocation();
	FVector Center = Target->GetActorLocation();
	if (const ACharacter* C = Cast<ACharacter>(Target))
	{
		if (C->GetCapsuleComponent())
		{
			Center.Z += C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * 0.5f;
		}
	}
	return Center;
}

void UArcPathBehavior::TickHoming(AEnemyProjectile* Projectile, float DeltaSeconds)
{
	const FVector Pos = Projectile->GetActorLocation();

	HomingElapsed += DeltaSeconds;

	if (HomingElapsed >= FMath::Max(0.5f, HomingMaxDuration) || !HomingTarget.IsValid())
	{
		EnterInertialFlight(Projectile, HomingDir * FMath::Max(1.0f, HomingMoveSpeed));
		return;
	}

	const FVector ToTarget = HomingTargetCenter(Projectile) - Pos;
	const float Dist = ToTarget.Size();
	const FVector DesiredDir = ToTarget / FMath::Max(Dist, KINDA_SMALL_NUMBER);
	const float MaxRad = FMath::DegreesToRadians(HomingHandoffTurnRateDegPerSec) * DeltaSeconds;
	const float Dot = FMath::Clamp(FVector::DotProduct(HomingDir, DesiredDir), -1.0f, 1.0f);
	const float Angle = FMath::Acos(Dot);
	if (Angle > MaxRad)
	{
		FVector Axis = FVector::CrossProduct(HomingDir, DesiredDir);
		if (!Axis.Normalize())
		{
			Axis = FVector::UpVector;
		}
		HomingDir = FQuat(Axis, MaxRad).RotateVector(HomingDir).GetSafeNormal();
	}
	else
	{
		HomingDir = DesiredDir;
	}

	const float Speed = FMath::Max(1.0f, HomingMoveSpeed);
	const FVector NewPos = Pos + HomingDir * Speed * DeltaSeconds;

	if (!MoveWithSweep(Projectile, NewPos))
	{
		return;
	}

	Projectile->SetActorRotation(HomingDir.Rotation());
}

void UArcPathBehavior::EnterInertialFlight(AEnemyProjectile* Projectile, const FVector& InitialVelocity)
{
	if (!Projectile)
	{
		return;
	}

	if (Projectile->ProjectileMovement)
	{
		const float Speed = InitialVelocity.Size();
		if (Speed > KINDA_SMALL_NUMBER)
		{
			Projectile->ProjectileMovement->InitialSpeed = Speed;
			Projectile->ProjectileMovement->MaxSpeed = Speed;
		}
		Projectile->ProjectileMovement->SetComponentTickEnabled(true);
		Projectile->ProjectileMovement->Activate();
		Projectile->ProjectileMovement->Velocity = InitialVelocity;
		Projectile->ProjectileMovement->UpdateComponentVelocity();
	}

	Phase = EArcPhase::Inertial;
}

bool UArcPathBehavior::MoveWithSweep(AEnemyProjectile* Projectile, const FVector& NewLocation)
{
	if (!Projectile)
	{
		return false;
	}

	FHitResult SweepHit;
	Projectile->SetActorLocation(NewLocation, true, &SweepHit);
	return !SweepHit.bBlockingHit;
}

void UArcPathBehavior::TickFiring(AEnemyProjectile* Projectile, float DeltaSeconds)
{
	const FVector PrevPos = Projectile->GetActorLocation();

	// 弧長を等速で進める (linear-tと違い曲線上の速度が一定になり終端減速が起きない)
	FiringElapsed += DeltaSeconds;
	FiringDistance += FiringSpeed * DeltaSeconds;

	const bool bReachedEnd = (TotalArcLength <= KINDA_SMALL_NUMBER) || (FiringDistance >= TotalArcLength);
	const float t = bReachedEnd ? 1.0f : ParamAtDistance(FiringDistance);

	const FVector Tangent = EvaluateTangent(t).GetSafeNormal();
	if (!Tangent.IsNearlyZero())
	{
		if (!bFrameInitialized) InitializeFrame(Tangent);
		else                    UpdateFrame(Tangent);
	}

	const FVector BasePos = EvaluatePosition(t);
	FVector NewPos = BasePos + EvaluateWeaveOffset(t);
	if (bReachedEnd)
	{
		NewPos = P3;
	}

	if (!MoveWithSweep(Projectile, NewPos))
	{
		return;
	}

	// 実際に通った線。基準ベジェ (緑) との差が螺旋とノイズの乗せぶんになる
	if (bDebugDrawPath)
	{
		DrawDebugLine(Projectile->GetWorld(), PrevPos, NewPos, FColor::Yellow,
			false, ArcDebugDrawDuration, 0, 4.0f);
	}

	// 姿勢は実移動方向へ追従 射出直後は初期方位を保持
	FVector MoveDir = (NewPos - PrevPos).GetSafeNormal();
	if (MoveDir.IsNearlyZero())
	{
		MoveDir = FrameTangent;
	}

	const bool bUseHoldHeading = bInitialHeadingSet && (FiringElapsed < FMath::Max(HeadingHoldTime, 0.0f));
	const FVector DesiredDir = bUseHoldHeading ? InitialHeadingDirection : MoveDir;

	FVector CurrentForward = Projectile->GetActorForwardVector().GetSafeNormal();
	if (CurrentForward.IsNearlyZero())
	{
		CurrentForward = DesiredDir;
	}

	const float MaxTurnRadians = FMath::DegreesToRadians(FMath::Max(TurnRateDegPerSec, 0.0f)) * DeltaSeconds;
	FVector FinalForward = DesiredDir;
	if (MaxTurnRadians > KINDA_SMALL_NUMBER)
	{
		const float Dot = FMath::Clamp(FVector::DotProduct(CurrentForward, DesiredDir), -1.0f, 1.0f);
		const float Angle = FMath::Acos(Dot);
		if (Angle > MaxTurnRadians)
		{
			FVector Axis = FVector::CrossProduct(CurrentForward, DesiredDir);
			if (Axis.Normalize())
			{
				FinalForward = FQuat(Axis, MaxTurnRadians).RotateVector(CurrentForward).GetSafeNormal();
			}
			else
			{
				FinalForward = CurrentForward;
			}
		}
	}

	FVector UpForRotation = FrameBinormal - FVector::DotProduct(FrameBinormal, FinalForward) * FinalForward;
	if (!UpForRotation.Normalize())
	{
		UpForRotation = FVector::UpVector - FVector::DotProduct(FVector::UpVector, FinalForward) * FinalForward;
		if (!UpForRotation.Normalize())
		{
			UpForRotation = FVector::RightVector;
		}
	}

	Projectile->SetActorRotation(FRotationMatrix::MakeFromXZ(FinalForward, UpForRotation).Rotator());

	// 飛行割合が閾値を超えたら弧誘導をやめて対象へホーミングへ切り替え
	// 等速なので巡航速度をそのまま渡せば継ぎ目で減速しない
	if (HomingHandoffRatio > 0.0f && HomingTarget.IsValid()
		&& FiringDistance >= HomingHandoffRatio * TotalArcLength)
	{
		HomingDir = (NewPos - PrevPos).GetSafeNormal();
		if (HomingDir.IsNearlyZero())
		{
			HomingDir = FrameTangent;
		}
		HomingMoveSpeed = FiringSpeed;
		Phase = EArcPhase::Homing;
		return;
	}

	if (bReachedEnd)
	{
		const float HandoffSpeed = FMath::Max(FiringSpeed, Projectile->InitialSpeed);
		EnterInertialFlight(Projectile, FinalForward.GetSafeNormal() * HandoffSpeed);
	}
}

FVector UArcPathBehavior::EvaluatePosition(float t) const
{
	const int32 Num = CurvePoints.Num();
	if (Num == 0) return FVector::ZeroVector;
	if (Num == 1) return CurvePoints[0];

	// de Casteljau: 隣接点をtで補間する操作を点が1つになるまで繰り返す
	// 制御点は多くても数個なのでインライン確保でヒープに触らない
	TArray<FVector, TInlineAllocator<8>> Work;
	Work.Append(CurvePoints);
	for (int32 Step = 1; Step < Num; ++Step)
	{
		for (int32 i = 0; i < Num - Step; ++i)
		{
			Work[i] = FMath::Lerp(Work[i], Work[i + 1], t);
		}
	}
	return Work[0];
}

FVector UArcPathBehavior::EvaluateTangent(float t) const
{
	const int32 Num = CurvePoints.Num();
	if (Num < 2) return FVector::ForwardVector;

	// 微分は「隣接点の差」を制御点とする1つ低い次数のベジェ (係数は次数ぶん)
	TArray<FVector, TInlineAllocator<8>> Work;
	Work.Reserve(Num - 1);
	for (int32 i = 0; i < Num - 1; ++i)
	{
		Work.Add(CurvePoints[i + 1] - CurvePoints[i]);
	}

	const int32 DerivNum = Work.Num();
	for (int32 Step = 1; Step < DerivNum; ++Step)
	{
		for (int32 i = 0; i < DerivNum - Step; ++i)
		{
			Work[i] = FMath::Lerp(Work[i], Work[i + 1], t);
		}
	}
	return Work[0] * static_cast<float>(Num - 1);
}

void UArcPathBehavior::InitializeFrame(const FVector& Tangent)
{
	FrameTangent = Tangent.GetSafeNormal();
	FVector ReferenceUp = FVector::UpVector;
	if (FMath::Abs(FVector::DotProduct(FrameTangent, ReferenceUp)) > 0.98f)
	{
		ReferenceUp = FVector::ForwardVector;
	}

	FrameNormal = FVector::CrossProduct(ReferenceUp, FrameTangent).GetSafeNormal();
	if (FrameNormal.IsNearlyZero())
	{
		FrameNormal = FVector::RightVector;
	}
	FrameBinormal = FVector::CrossProduct(FrameTangent, FrameNormal).GetSafeNormal();
	bFrameInitialized = true;
}

void UArcPathBehavior::UpdateFrame(const FVector& Tangent)
{
	const FVector NewTangent = Tangent.GetSafeNormal();
	if (NewTangent.IsNearlyZero())
	{
		return;
	}

	const float Dot = FMath::Clamp(FVector::DotProduct(FrameTangent, NewTangent), -1.0f, 1.0f);
	const FVector Axis = FVector::CrossProduct(FrameTangent, NewTangent);
	const float SmallTolerance = 1.0e-4f;
	if (Axis.SizeSquared() > SmallTolerance)
	{
		const FVector RotationAxis = Axis.GetSafeNormal();
		const float Angle = FMath::Acos(Dot);
		const FQuat DeltaRot(RotationAxis, Angle);
		FrameNormal = DeltaRot.RotateVector(FrameNormal).GetSafeNormal();
	}

	FrameTangent = NewTangent;
	FrameBinormal = FVector::CrossProduct(FrameTangent, FrameNormal).GetSafeNormal();
	if (FrameBinormal.IsNearlyZero())
	{
		InitializeFrame(FrameTangent);
		return;
	}
	FrameNormal = FVector::CrossProduct(FrameBinormal, FrameTangent).GetSafeNormal();
}

FVector UArcPathBehavior::EvaluateWeaveOffset(float t) const
{
	const bool bUseNoise = WeaveNoiseAmplitude > KINDA_SMALL_NUMBER && WeaveNoiseFrequency > KINDA_SMALL_NUMBER;
	const bool bUseSpiral = SpiralAmplitude > KINDA_SMALL_NUMBER && FMath::Abs(SpiralTurns) > KINDA_SMALL_NUMBER;
	if (!bUseNoise && !bUseSpiral)
	{
		return FVector::ZeroVector;
	}

	// 両端でゼロ・中央で最大の包絡。始点と着弾点は必ず曲線上に乗るので、揺らしても着弾がぶれない
	float Envelope = FMath::Pow(FMath::Sin(PI * t), FMath::Max(WeaveEnvelopePower, 1.0f));

	// 終盤の収束。対称な包絡だけだと着弾間際まで揺れが残るので、指定割合から先を追加で絞る
	if (WeaveSettleRatio < 1.0f)
	{
		Envelope *= 1.0f - FMath::SmoothStep(WeaveSettleRatio, 1.0f, t);
	}

	if (Envelope <= KINDA_SMALL_NUMBER)
	{
		return FVector::ZeroVector;
	}

	FVector Offset = FVector::ZeroVector;

	if (bUseNoise)
	{
		const float uNoise = FMath::PerlinNoise1D(t * WeaveNoiseFrequency + NoiseSeedU);
		const float vNoise = FMath::PerlinNoise1D(t * WeaveNoiseFrequency + NoiseSeedV);
		Offset += (FrameNormal * uNoise + FrameBinormal * vNoise) * WeaveNoiseAmplitude;
	}

	if (bUseSpiral)
	{
		// 回転最小化フレーム上で角度を等速に回す。フレームがねじれないので一定ピッチの螺旋になる
		const float Theta = 2.0f * PI * SpiralTurns * SpiralSign * t + SpiralPhase;
		Offset += (FrameNormal * FMath::Cos(Theta) + FrameBinormal * FMath::Sin(Theta)) * SpiralAmplitude;
	}

	return Offset * Envelope;
}

void UArcPathBehavior::OnExpire(AEnemyProjectile* Projectile)
{
	if (!Projectile || Projectile->WasHit() || Projectile->GetEndReason() != EProjectileEndReason::LifeSpanExpired)
	{
		return;
	}

	if (SplashRadius <= KINDA_SMALL_NUMBER || SplashDamage <= 0.0f)
	{
		return;
	}

	AActor* OwnerActor = Projectile->GetOwner();
	const FVector Center = Projectile->GetActorLocation();

	TArray<AActor*> OverlapActors;
	UKismetSystemLibrary::SphereOverlapActors(Projectile, Center, SplashRadius,
		{ UEngineTypes::ConvertToObjectType(ECC_Pawn),
		  UEngineTypes::ConvertToObjectType(ECC_WorldDynamic) },
		nullptr, { OwnerActor }, OverlapActors);

	for (AActor* Actor : OverlapActors)
	{
		if (!TideCombatUtil::IsHostileTo(OwnerActor, Actor)) continue;

		IDamageable* Damageable = Cast<IDamageable>(Actor);
		if (!Damageable) continue;

		FDamageInfo Info;
		Info.BaseDamage = SplashDamage;
		Info.Instigator = OwnerActor;
		Info.HitReactionTag = Projectile->HitReactionTag;
		Info.HitResult.ImpactPoint = Actor->GetActorLocation();
		Info.HitResult.ImpactNormal = (Actor->GetActorLocation() - Center).GetSafeNormal();
		Damageable->ReceiveDamage(Info);
	}
}
