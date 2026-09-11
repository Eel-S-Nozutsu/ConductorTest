// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/Actors/Gimmick/TimedSteppingStone.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SplineComponent.h"
#include "Kismet/GameplayStatics.h"
#include "PRJ_TIDE_P0/Actors/Volume/AbyssVolume.h"
#include "PRJ_TIDE_P0/Actors/Character/Player/TidePlayerCharacter.h"
#include "PRJ_TIDE_P0/Components/Player/FallRecoveryComponent.h"

ATimedSteppingStone::ATimedSteppingStone()
{
	PrimaryActorTick.bCanEverTick = true;

	StoneMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StoneMesh"));
	StoneMesh->SetCollisionProfileName(TEXT("BlockAll"));
	StoneMesh->SetGenerateOverlapEvents(true);
	RootComponent = StoneMesh;

	PathSpline = CreateDefaultSubobject<USplineComponent>(TEXT("PathSpline"));
	PathSpline->SetupAttachment(StoneMesh);
}

void ATimedSteppingStone::BeginPlay()
{
	Super::BeginPlay();

	OriginalLocation = GetActorLocation();
	// 石が動くとパスも一緒に動いてしまうので、実行時は親から切り離して世界に固定する
	PathSpline->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
}

bool ATimedSteppingStone::IsSplineActive() const
{
	return MoveSpeed > 0.0f && PathSpline->GetSplineLength() > KINDA_SMALL_NUMBER;
}

void ATimedSteppingStone::AdvanceAlongSpline(float DeltaTime)
{
	const float Length = PathSpline->GetSplineLength();

	SplineDistance += SplineDir * MoveSpeed * DeltaTime;
	if (PathSpline->IsClosedLoop())
	{
		SplineDistance = FMath::Fmod(SplineDistance, Length);
		if (SplineDistance < 0.0f) SplineDistance += Length;
	}
	else if (SplineDistance >= Length)
	{
		SplineDistance = Length;
		SplineDir = -1;
	}
	else if (SplineDistance <= 0.0f)
	{
		SplineDistance = 0.0f;
		SplineDir = 1;
	}

	// 実行時はスプラインを親から切り離して固定済みなのでWorld座標で直接引ける
	OriginalLocation = PathSpline->GetLocationAtDistanceAlongSpline(SplineDistance, ESplineCoordinateSpace::World);
	SetActorLocation(OriginalLocation);
}

void ATimedSteppingStone::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 前段(Entangled/Freeing/Emerging)中は通常の乗り検知・沈み
	// /浮き・振動を止め、派生の駆動に委ねる
	if (State == ESteppingStoneState::Entangled
		|| State == ESteppingStoneState::Freeing
		|| State == ESteppingStoneState::Emerging)
	{
		TickPreSettled(DeltaTime);
		return;
	}

	if (!CachedPlayer.IsValid())
	{
		CachedPlayer = Cast<ATidePlayerCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
	}

	const bool bPlayerOnStone = CachedPlayer.IsValid()
		&& CachedPlayer->GetMovementBase() == StoneMesh;

	if (!bWasPlayerOnStone && bPlayerOnStone) OnPlayerLanded();
	else if (bWasPlayerOnStone && !bPlayerOnStone) OnPlayerLeft();
	bWasPlayerOnStone = bPlayerOnStone;

	// パス移動はFloatingの間(落下前の振動フェーズ含む)続ける。沈み〜浮き戻り中だけ停止
	if (IsSplineActive() && State == ESteppingStoneState::Floating)
	{
		AdvanceAlongSpline(DeltaTime);
	}

	if (bIsShaking)
	{
		ShakeElapsedTime += DeltaTime;
		const float OffsetX = FMath::Sin(ShakeElapsedTime * ShakeFrequency) * ShakeAmplitude;
		SetActorLocation(FVector(OriginalLocation.X + OffsetX, OriginalLocation.Y, GetActorLocation().Z));
	}

	if (State == ESteppingStoneState::Sinking)
	{
		FHitResult Hit;
		AddActorWorldOffset(FVector(0.0f, 0.0f, -SinkSpeed * DeltaTime), true, &Hit);
		if (Hit.bBlockingHit)
		{
			EnterSunken();
		}
	}
	else if (State == ESteppingStoneState::Rising)
	{
		// 時間ベース補間。開始高さ→OriginalLocation.ZをRiseDuration秒で
		// 到達判定はAlpha>=1なので速度指定＋IsNearlyEqualのようなfloat/double
		// 誤差で詰まらない
		RiseElapsedTime += DeltaTime;
		const float Alpha = (RiseDuration > 0.0f) ? FMath::Clamp(RiseElapsedTime / RiseDuration, 0.0f, 1.0f) : 1.0f;
		const double NewZ = FMath::Lerp(RiseStartZ, OriginalLocation.Z, static_cast<double>(Alpha));
		SetActorLocation(FVector(OriginalLocation.X, OriginalLocation.Y, NewZ));
		if (Alpha >= 1.0f)
		{
			State = ESteppingStoneState::Floating;
		}
	}
}

void ATimedSteppingStone::NotifyActorBeginOverlap(AActor* OtherActor)
{
	Super::NotifyActorBeginOverlap(OtherActor);

	if (State == ESteppingStoneState::Sinking && Cast<AAbyssVolume>(OtherActor))
	{
		EnterSunken(true);
	}
}

void ATimedSteppingStone::OnPlayerLanded()
{
	// 沈まない床モード: 沈下/振動タイマーを一切仕込まず、Floatingのまま
	// (MoveSpeed>0なら移動床)
	if (!bSinkOnPlayer) return;

	// 沈み/浮き待ち/浮き上がり中の(再)着地では何もしない。ここでRise予約を消すと
	// Sunkenのまま何も起動せず、パス移動が再開しなくなる(特に縦移動床で乗り基底が
	// フラつきSunken中に着地エッジが立つケース)
	if (State != ESteppingStoneState::Floating) return;

	GetWorldTimerManager().ClearTimer(RiseTimerHandle);
	GetWorldTimerManager().SetTimer(SinkTimerHandle, this, &ATimedSteppingStone::OnSinkTimerFired, SinkDelay, false);

	if (SinkDelay > ShakeDuration)
	{
		GetWorldTimerManager().SetTimer(ShakeTimerHandle, this, &ATimedSteppingStone::OnShakeTimerFired, SinkDelay - ShakeDuration, false);
	}
}

void ATimedSteppingStone::OnPlayerLeft()
{
	GetWorldTimerManager().ClearTimer(SinkTimerHandle);
	GetWorldTimerManager().ClearTimer(ShakeTimerHandle);
	StopShake();

	if (State == ESteppingStoneState::Sinking || State == ESteppingStoneState::Sunken)
	{
		GetWorldTimerManager().SetTimer(RiseTimerHandle, this, &ATimedSteppingStone::OnRiseTimerFired, RiseDelay, false);
	}
}

void ATimedSteppingStone::OnSinkTimerFired()
{
	StopShake();
	State = ESteppingStoneState::Sinking;
}

void ATimedSteppingStone::OnShakeTimerFired()
{
	bIsShaking = true;
	ShakeElapsedTime = 0.0f;
}

void ATimedSteppingStone::StopShake()
{
	bIsShaking = false;
	ShakeElapsedTime = 0.0f;
	SetActorLocation(FVector(OriginalLocation.X, OriginalLocation.Y, GetActorLocation().Z));
}

void ATimedSteppingStone::OnRiseTimerFired()
{
	State = ESteppingStoneState::Rising;
	RiseElapsedTime = 0.0f;
	RiseStartZ = GetActorLocation().Z;
}

void ATimedSteppingStone::SettleAt(const FVector& WorldLocation)
{
	// 浮上到達点を新しい定位置に据えて通常挙動へ委譲する。以降は乗ると沈み、離れるとここへ戻る
	OriginalLocation = WorldLocation;
	SetActorLocation(WorldLocation);
	State = ESteppingStoneState::Floating;
}

void ATimedSteppingStone::EnterSunken(bool bFromAbyss)
{
	State = ESteppingStoneState::Sunken;

	bool bRequestedRecovery = false;
	if (bFromAbyss && bWasPlayerOnStone && CachedPlayer.IsValid())
	{
		if (UFallRecoveryComponent* Recovery = CachedPlayer->GetFallRecoveryComponent())
		{
			Recovery->RequestRecovery();
			bRequestedRecovery = true;
		}
	}

	// 復帰を要求した場合、プレイヤーはテレポートで石から離れることが確定するので、
	// OnPlayerLeftの発火(乗り基底の外れ)に頼らずここで浮き上がりを予約する
	// これで「奈落落下 → 復帰 → 浮き上がり → パス移動再開」が確実に走る
	// プレイヤーが乗っていない通常沈下も従来どおりここで予約する
	if ((!bWasPlayerOnStone || bRequestedRecovery) && !GetWorldTimerManager().IsTimerActive(RiseTimerHandle))
	{
		GetWorldTimerManager().SetTimer(RiseTimerHandle, this, &ATimedSteppingStone::OnRiseTimerFired, RiseDelay, false);
	}
}
