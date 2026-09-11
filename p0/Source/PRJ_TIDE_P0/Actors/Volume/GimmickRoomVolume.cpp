// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/Actors/Volume/GimmickRoomVolume.h"
#include "PRJ_TIDE_P0/Actors/Character/Player/TidePlayerCharacter.h"
#include "PRJ_TIDE_P0/Actors/Projectile/EnemyProjectile.h"
#include "PRJ_TIDE_P0/Components/Player/RestartComponent.h"
#include "PRJ_TIDE_P0/Interfaces/Environment/IGimmickResettable.h"

#include "EngineUtils.h"

void AGimmickRoomVolume::BeginPlay()
{
	Super::BeginPlay();

	// 祠が別レベルにある場合、そのレベルが読み込まれている間しか解決できない。
	// 拾えるうちに拾っておく(入室時にストリームアウトしていても復帰地点を失わない)
	TryCacheRestartPoint();
}

void AGimmickRoomVolume::NotifyActorBeginOverlap(AActor* OtherActor)
{
	Super::NotifyActorBeginOverlap(OtherActor);

	ATidePlayerCharacter* Player = Cast<ATidePlayerCharacter>(OtherActor);
	if (!Player) return;

	if (TryCacheRestartPoint())
	{
		if (URestartComponent* Restart = Player->FindComponentByClass<URestartComponent>())
		{
			Restart->PushRestartOverride(CachedRestartLocation, CachedRestartRotation, this);
		}
	}
	else if (!RestartTargetActor.IsNull())
	{
		// 指定はあるのに解決できない = 祠のレベルが未ロード。黙って本線の
		// チェックポイントへ落ちると原因が見えないので知らせる
		UE_LOG(LogTemp, Warning, TEXT("GimmickRoomVolume(%s): RestartTargetActor(%s)を解決できず在室中の復帰地点を設定できない。祠のレベルが未ロードの可能性"),
			*GetName(), *RestartTargetActor.ToString());
	}

	if (bResetGimmicksOnEnter)
	{
		ResetGimmicks();
	}

	OnPlayerEnterRoom.Broadcast();
}

void AGimmickRoomVolume::NotifyActorEndOverlap(AActor* OtherActor)
{
	Super::NotifyActorEndOverlap(OtherActor);

	ATidePlayerCharacter* Player = Cast<ATidePlayerCharacter>(OtherActor);
	if (!Player) return;

	// クリア・死亡どちらの退室でも本線のチェックポイントへ戻す
	if (URestartComponent* Restart = Player->FindComponentByClass<URestartComponent>())
	{
		Restart->ClearRestartOverride(this);
	}

	if (bResetGimmicksOnExit)
	{
		ResetGimmicks();
	}

	OnPlayerExitRoom.Broadcast();
}

bool AGimmickRoomVolume::TryCacheRestartPoint()
{
	if (bRestartPointCached) return true;

	// 未設定・未ロードのときは上書きしない = 既存のチェックポイント挙動のまま
	const AActor* Target = RestartTargetActor.Get();
	if (!Target) return false;

	CachedRestartLocation = Target->GetActorLocation();
	CachedRestartRotation = Target->GetActorRotation();
	bRestartPointCached   = true;
	return true;
}

void AGimmickRoomVolume::ResetGimmicks()
{
	// ルーム内を飛んでいる敵弾を先に消す。残すと復活直後の床を撃ち抜き、着弾から
	// CollapseDelay(既定1秒)後に穴が開く。弾は既に消えているので「復活漏れ」に見える
	for (TActorIterator<AEnemyProjectile> It(GetWorld()); It; ++It)
	{
		AEnemyProjectile* Projectile = *It;
		if (Projectile && EncompassesPoint(Projectile->GetActorLocation()))
		{
			Projectile->Despawn();	// 待機中(プール返却済み)の弾は内部で弾かれる
		}
	}

	for (const TSoftObjectPtr<AActor>& Target : ResetTargets)
	{
		AActor* Gimmick = Target.Get();
		if (!Gimmick) continue;

		// Cast<>ではBP実装を拾えないためExecute_経由で呼ぶ
		if (Gimmick->Implements<UGimmickResettable>())
		{
			IGimmickResettable::Execute_ResetGimmick(Gimmick);
		}
	}
}
