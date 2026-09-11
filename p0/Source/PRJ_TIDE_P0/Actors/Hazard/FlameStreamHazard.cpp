// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/Actors/Hazard/FlameStreamHazard.h"

#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/Pawn.h"
#if !UE_BUILD_SHIPPING
#include "DrawDebugHelpers.h"
#endif

#include "PRJ_TIDE_P0/Core/TideGameSettings.h"
#include "PRJ_TIDE_P0/Interfaces/Combat/IDamageable.h"
#include "PRJ_TIDE_P0/Data/Combat/DamageInfo.h"
#include "PRJ_TIDE_P0/Utilities/HitStopUtils.h"
#include "PRJ_TIDE_P0/Utilities/TideCombatUtils.h"

AFlameStreamHazard::AFlameStreamHazard()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	StreamVFX = CreateDefaultSubobject<UNiagaraComponent>(TEXT("StreamVFX"));
	StreamVFX->SetupAttachment(RootComponent);
	StreamVFX->bAutoActivate = false;
}

void AFlameStreamHazard::BeginPlay()
{
	Super::BeginPlay();

	// SetFinalRangeが呼ばれないまま使われても、見た目がBPの設定値と揃うようにしておく
	FinalRange = Range;
}

void AFlameStreamHazard::SetFinalRange(float InRange)
{
	FinalRange = FMath::Max(0.0f, InRange);

	// 伸びを使わない場合はこれで判定も確定する。使う場合は直後にSetRangeが上書きする
	Range = FinalRange;

	UpdateVFXLength();
}

void AFlameStreamHazard::SetRange(float InRange)
{
	// 伸びの途中経過。見た目は伸びきった長さのままにしたいのでUpdateVFXLengthは呼ばない
	Range = FMath::Clamp(InRange, 0.0f, FinalRange);
}

void AFlameStreamHazard::Activate()
{
	if (bActive) return;
	bActive = true;

	// 噴射ごとにヒット律速をリセット(入った瞬間から即ヒットで刻み直す)
	NextHitTimes.Reset();

	// 見た目アセットが入っていれば再生(未設定でもデバッグ描画で成立確認できる)
	if (StreamVFX && StreamVFX->GetAsset())
	{
		StreamVFX->Activate(true);
	}

	// 再生開始のフレームから正しい長さで出す(NiagaraはActivateでパラメータが初期値へ戻る)
	PushedVFXLength = -1.0f;
	UpdateVFXLength();

	SetActorTickEnabled(true);
}

void AFlameStreamHazard::Deactivate()
{
	if (!bActive) return;
	bActive = false;

	if (StreamVFX)
	{
		StreamVFX->Deactivate();
	}

	SetActorTickEnabled(false);
}

void AFlameStreamHazard::SetExternalYaw(float InWorldYaw)
{
	bHasExternalYaw = true;
	ExternalYaw = InWorldYaw;
}

void AFlameStreamHazard::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 発射元(Owner)が失われたら自壊する(攻撃中に敵が破棄された等、アタッチ元消失に備える)
	if (!GetOwner())
	{
		Destroy();
		return;
	}

	// roll無し・pitch水平固定で向きを確定する。以降の判定/デバッグ描画/VFXがこの向きを使う
	// 外部ヨー指定があればそれを優先(位置はソケット追従・方向はプログラム制御)
	// なければソケット追従で入ったpitch/rollを捨て、現在の前方の水平方向(yaw)だけを残す
	if (bHasExternalYaw)
	{
		SetActorRotation(FRotator(0.0f, ExternalYaw, 0.0f));
	}
	else if (bKeepHorizontal)
	{
		const float Yaw = GetActorForwardVector().Rotation().Yaw;
		SetActorRotation(FRotator(0.0f, Yaw, 0.0f));
	}

	DamageTargetsInBox();

#if !UE_BUILD_SHIPPING
	//if (UTideGameSettings::Get()->bDebugDrawAttackHitbox)
	if (true)// いったんNiagaraの代わりにデバッグ描画でやっておく
	{
		FVector BoxCenter, HalfExtent;
		FQuat BoxRot;
		GetBoxGeometry(BoxCenter, HalfExtent, BoxRot);
		DrawDebugBox(GetWorld(), BoxCenter, HalfExtent, BoxRot, FColor::Red, false, -1.0f, 0, 1.5f);
	}
#endif
}

void AFlameStreamHazard::UpdateVFXLength()
{
	if (!StreamVFX || !StreamVFX->GetAsset() || VFXLengthParamName.IsNone()) return;

	// 見るのはFinalRange。判定側のRangeは予兆で伸びる途中の値なので使わない
	const float Length = FinalRange * VFXLengthScale;
	if (FMath::IsNearlyEqual(Length, PushedVFXLength)) return;

	// NS側のUserパラメータへ流す。マテリアルのLengthへはNS内で配線する
	// (C++からNiagara描画のマテリアルを直接触る手段が無いため、必ずUser経由になる)
	StreamVFX->SetVariableFloat(VFXLengthParamName, Length);
	PushedVFXLength = Length;
}

void AFlameStreamHazard::GetBoxGeometry(FVector& OutCenter, FVector& OutHalfExtent, FQuat& OutRot) const
{
	// ローカルX=前方, Y=左右幅, Z=上下高さ
	// ボックス中心は原点から前方へRange/2進めた位置
	const FVector Forward = GetActorForwardVector();
	OutCenter = GetActorLocation() + Forward * (Range * 0.5f);
	OutHalfExtent = FVector(Range * 0.5f, Width * 0.5f, Height * 0.5f);
	// 向きはyawのみ(Tickで平坦化済みだが、水平ボックスを保証するためyawだけで組む)
	OutRot = FRotator(0.0f, GetActorRotation().Yaw, 0.0f).Quaternion();
}

void AFlameStreamHazard::DamageTargetsInBox()
{
	UWorld* World = GetWorld();
	if (!World) return;

	const double Now = World->GetTimeSeconds();
	const float Interval = FMath::Max(0.01f, DamageInterval);

	FVector BoxCenter, HalfExtent;
	FQuat BoxRot;
	GetBoxGeometry(BoxCenter, HalfExtent, BoxRot);

	// 拡散しない一定幅のビーム。ボックスに実際にコリジョンが重なる相手だけを拾う
	// 相手の中心点ではなくカプセル形状で重なり判定するため、ビームの端でも正しく命中
	// /回避が成立する
	FCollisionObjectQueryParams ObjQuery = FCollisionObjectQueryParams::AllDynamicObjects;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(FlameStreamHazard), false, this);
	Params.AddIgnoredActor(GetOwner());

	const FCollisionShape Shape = FCollisionShape::MakeBox(HalfExtent);

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, BoxCenter, BoxRot, ObjQuery, Shape, Params);

	// 同一アクターに複数コンポーネントがヒットしても1回に絞る
	TSet<AActor*> Processed;
	for (const FOverlapResult& Ov : Overlaps)
	{
		AActor* Other = Ov.GetActor();
		if (!Other || Other == this || Other == GetOwner()) continue;
		if (Processed.Contains(Other)) continue;
		if (!TideCombatUtil::IsHostileTo(GetOwner(), Other)) continue;

		IDamageable* Damageable = Cast<IDamageable>(Other);
		if (!Damageable) continue;

		Processed.Add(Other);

		// 同じ相手にはDamageInterval間隔で刻む(初回=次回時刻0で即ヒット)
		double& NextTime = NextHitTimes.FindOrAdd(Other);
		if (Now < NextTime) continue;
		NextTime = Now + Interval;

		FDamageInfo DamageInfo;
		DamageInfo.BaseDamage        = Damage;
		DamageInfo.Instigator        = GetOwner();
		DamageInfo.HitReactionTag    = HitReactionTag;
		DamageInfo.bIsDamageOverTime = false;	// 通常ダメージ(被弾リアクション・カメラ揺れを出す)

		const EDamageResult Result = Damageable->ReceiveDamage(DamageInfo);
		if (Result != EDamageResult::Hit) continue;

		// ヒットが通った相手にだけ演出(無敵・回避では出さない)
		if (HitEffect)
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, HitEffect, Other->GetActorLocation());
		}

		// ヒットストップはプレイヤーのみ
		if (bUseHitStop)
		{
			const APawn* Pawn = Cast<APawn>(Other);
			if (Pawn && Pawn->IsPlayerControlled())
			{
				HitStopUtil::ApplyHitStop(Other, HitStopDuration, HitStopDilation);
			}
		}
	}
}
