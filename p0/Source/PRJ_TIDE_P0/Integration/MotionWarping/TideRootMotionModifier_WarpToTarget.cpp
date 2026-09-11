// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "TideRootMotionModifier_WarpToTarget.h"

#include "PRJ_TIDE_P0/Actors/Character/Enemy/EnemyCharacter.h"

#include "AIController.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "MotionWarpingComponent.h"

namespace
{
	const FName TideAutoWarpTargetName(TEXT("TideAutoTarget"));
}

UTideRootMotionModifier_WarpToTarget::UTideRootMotionModifier_WarpToTarget(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	WarpTargetName = TideAutoWarpTargetName;
}

bool UTideRootMotionModifier_WarpToTarget::ResolveWarpTarget()
{
	UMotionWarpingComponent* Warping = GetOwnerComponent();
	AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(GetActorOwner());
	if (!Warping || !Enemy) return false;

	const AActor* Target = Enemy->GetTargetActor();
	if (!Target)
	{
#if !UE_BUILD_SHIPPING
		if (bDrawDebug && GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1, 0.0f, FColor::Red, TEXT("WarpToTarget: BB TargetActor が空"));
		}
#endif
		return false;
	}

	const FVector EnemyLoc = Enemy->GetActorLocation();
	const FVector ToTargetVec = FVector::VectorPlaneProject(Target->GetActorLocation() - EnemyLoc, FVector::UpVector);
	const FVector ToTarget = ToTargetVec.GetSafeNormal();
	if (ToTarget.IsNearlyZero()) return false;

	const FVector Forward = Enemy->GetActorForwardVector().GetSafeNormal2D();

	// コーン外(当たらない角度)ならワープせず素の動きを出す
	const float AngleToTarget = FMath::RadiansToDegrees(
		FMath::Acos(FMath::Clamp(FVector::DotProduct(Forward, ToTarget), -1.0f, 1.0f)));
	if (MaxWarpAngle > 0.0f && AngleToTarget > MaxWarpAngle)
	{
#if !UE_BUILD_SHIPPING
		if (bDrawDebug && GEngine)
			GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Yellow,
				FString::Printf(TEXT("WarpToTarget: コーン外 (angle=%.0f > %.0f) ワープ停止"), AngleToTarget, MaxWarpAngle));
#endif
		return false;
	}

	// 接触点で止めるため手前へStopDistanceだけ引く。Zは敵の足元に合わせ上下させない
	// bProjectToForward: ワープ先を正面線上に射影し、横成分を殺して前後の伸縮だけにする
	FVector WarpLoc;
	if (bProjectToForward)
	{
		const float ForwardDist = FVector::DotProduct(ToTargetVec, Forward);
		WarpLoc = EnemyLoc + Forward * FMath::Max(ForwardDist - StopDistance, 0.0f);
	}
	else
	{
		WarpLoc = EnemyLoc + ToTarget * FMath::Max(ToTargetVec.Size() - StopDistance, 0.0f);
	}
	WarpLoc.Z = EnemyLoc.Z;

	const FRotator WarpRot(0.0f, FRotationMatrix::MakeFromX(ToTarget).Rotator().Yaw + YawOffset, 0.0f);

	Warping->AddOrUpdateWarpTargetFromLocationAndRotation(WarpTargetName, WarpLoc, WarpRot);

#if !UE_BUILD_SHIPPING
	if (bDrawDebug && GEngine)
	{
		DrawDebugSphere(Enemy->GetWorld(), WarpLoc, 20.0f, 12, FColor::Green, false, 0.0f, 0, 1.5f);
		DrawDebugDirectionalArrow(Enemy->GetWorld(), WarpLoc, WarpLoc + WarpRot.Vector() * 80.0f,
			30.0f, FColor::Cyan, false, 0.0f, 0, 2.0f);
		GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Green,
			FString::Printf(TEXT("WarpToTarget: dist=%.0f stop=%.0f angle=%.0f yaw=%.0f"),
				ToTargetVec.Size(), StopDistance, AngleToTarget, WarpRot.Yaw));
	}
#endif
	return true;
}

void UTideRootMotionModifier_WarpToTarget::Update(const FMotionWarpingUpdateContext& Context)
{
	// Superがこの名前でターゲットを引くので、その前に最新のターゲットを注入する
	// 解決できなければターゲットを消す (Superはターゲット不在として素のrootmotionを流す)
	if (!ResolveWarpTarget())
	{
		if (UMotionWarpingComponent* Warping = GetOwnerComponent())
			Warping->RemoveWarpTarget(WarpTargetName);
	}

	Super::Update(Context);
}
