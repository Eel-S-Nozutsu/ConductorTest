// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "ContactVfxBehavior.h"
#include "PRJ_TIDE_P0/Actors/Projectile/EnemyProjectile.h"

#include "GameFramework/Pawn.h"
#include "NiagaraFunctionLibrary.h"

void UContactVfxBehavior::OnHit(AEnemyProjectile* Projectile, AActor* HitActor, const FHitResult& Hit, EDamageResult Result)
{
	if (!Projectile) return;

	// 当たった相手で出し分ける ※Pawn=PC接触/それ以外(壁・地形・null)=背景接触
	UNiagaraSystem* Vfx = Cast<APawn>(HitActor) ? HitVfx : BackgroundVfx;
	if (!Vfx) return;

	// 背景はブロッキングヒットでImpactPoint/Normalが信頼できる
	// PCはオーバーラップ経由でHitが不正なことがあるので弾の位置/向きにフォールバックする
	const FVector Location = Hit.bBlockingHit ? static_cast<FVector>(Hit.ImpactPoint) : Projectile->GetActorLocation();
	const FRotator Rotation = Hit.bBlockingHit ? Hit.ImpactNormal.Rotation() : Projectile->GetActorRotation();

	UNiagaraFunctionLibrary::SpawnSystemAtLocation(Projectile, Vfx, Location, Rotation, Scale);
}

void UContactVfxBehavior::OnExpire(AEnemyProjectile* Projectile)
{
	// ヒットして消えた弾には消滅効果を出さない
	if (!Projectile || Projectile->WasHit() || !DeathVfx) return;

	UNiagaraFunctionLibrary::SpawnSystemAtLocation(Projectile, DeathVfx,
		Projectile->GetActorLocation(), Projectile->GetActorRotation(), Scale);
}
