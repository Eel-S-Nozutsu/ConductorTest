// Copyright (c) 2026, I.Fukunaka EelGameStudio, Inc. All Rights Reserved.

#include "WindPullBehavior.h"
#include "PRJ_TIDE_P0/Actors/Projectile/EnemyProjectile.h"
#include "PRJ_TIDE_P0/Interfaces/Environment/IWindAffectable.h"

#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"

void UWindPullBehavior::OnSpawn(AEnemyProjectile* Projectile)
{
	// プール再利用に備え実行時状態を初期化。
	bPlayerCaptured = false;
	CachedPlayer = nullptr;
	EndCaptureEffect();
}

void UWindPullBehavior::OnLaunch(AEnemyProjectile* Projectile)
{
	if (!Projectile) return;

	CachedPlayer = UGameplayStatics::GetPlayerPawn(Projectile, 0);
}

void UWindPullBehavior::OnTick(AEnemyProjectile* Projectile, float DeltaSeconds)
{
	if (!Projectile) return;

	APawn* Player = CachedPlayer.Get();
	IWindAffectable* Affectable = Player ? Cast<IWindAffectable>(Player) : nullptr;
	if (!Affectable)
	{
		ReleasePlayerIfCaptured();
		return;
	}

	const float DistSq = FVector::DistSquared(Projectile->GetActorLocation(), Player->GetActorLocation());
	if (DistSq > FMath::Square(PullRadius))
	{
		ReleasePlayerIfCaptured();
		return;
	}

	FWindInfluence Wind;
	Wind.Center = Projectile->GetActorLocation();
	Wind.PullSpeed = PullSpeed;
	Wind.Source = Projectile;

	if (!bPlayerCaptured)
	{
		bPlayerCaptured = true;
		Affectable->OnWindEnter(Wind);
		BeginCaptureEffect(Player);
	}
	Affectable->OnWindTick(Wind, DeltaSeconds);
}

void UWindPullBehavior::OnDeactivate(AEnemyProjectile* Projectile)
{
	ReleasePlayerIfCaptured();
}

void UWindPullBehavior::ReleasePlayerIfCaptured()
{
	if (!bPlayerCaptured) return;
	bPlayerCaptured = false;

	EndCaptureEffect();

	if (APawn* Player = CachedPlayer.Get())
	{
		if (IWindAffectable* Affectable = Cast<IWindAffectable>(Player))
		{
			Affectable->OnWindExit();
		}
	}
}

void UWindPullBehavior::BeginCaptureEffect(APawn* Player)
{
	if (!CaptureEffect || !Player) return;

	USceneComponent* AttachTo = Player->GetRootComponent();
	if (!AttachTo) return;

	// SpawnSystemAttachedはNiagaraのスケーラビリティ/有意性カリングやプーリングの都合で
	// 不安定になることがある(アタッチ先が有効でもnullptrを返す等)ため、
	// HazardDotComponent/WindZoneと同じくコンポーネントを明示生成して確実にアタッチする。
	// SetAutoDestroy(true)により、Deactivate後は消失遷移(フェードアウト)を経て
	// Niagara自身が自然に破棄される
	UNiagaraComponent* Comp = NewObject<UNiagaraComponent>(Player);
	if (!Comp) return;

	Comp->SetupAttachment(AttachTo, CaptureEffectSocketName);
	Comp->SetRelativeLocation(FVector::ZeroVector);
	Comp->SetRelativeRotation(FRotator::ZeroRotator);
	Comp->SetAutoActivate(false);
	Comp->SetAsset(CaptureEffect);
	Comp->SetAutoDestroy(true);
	Comp->RegisterComponent();
	Comp->Activate(true);

	ActiveCaptureEffect = Comp;
}

void UWindPullBehavior::EndCaptureEffect()
{
	if (UNiagaraComponent* Effect = ActiveCaptureEffect.Get())
	{
		Effect->Deactivate();
	}
	ActiveCaptureEffect.Reset();
}
