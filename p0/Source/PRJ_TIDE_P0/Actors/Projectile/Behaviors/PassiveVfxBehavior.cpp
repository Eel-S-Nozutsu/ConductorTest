// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "PassiveVfxBehavior.h"
#include "PRJ_TIDE_P0/Actors/Projectile/EnemyProjectile.h"

#include "NiagaraComponent.h"

void UPassiveVfxBehavior::OnSpawn(AEnemyProjectile* Projectile)
{
	if (!Projectile || !System) return;

	if (UNiagaraComponent* Comp = Projectile->GetPassiveEffectComponent())
	{
		Comp->SetRelativeScale3D(Scale);
		Comp->SetAsset(System);
		Comp->Activate(true);
	}
}

void UPassiveVfxBehavior::OnLaunch(AEnemyProjectile* Projectile)
{
	// プール再利用に備え点滅の位相をリセットする
	BlinkElapsed = 0.0f;
}

void UPassiveVfxBehavior::OnTick(AEnemyProjectile* Projectile, float DeltaSeconds)
{
	if (!bBlinkByLifetime || !Projectile) return;

	UNiagaraComponent* Comp = Projectile->GetPassiveEffectComponent();
	if (!Comp) return;

	// 残り寿命の割合 (1=満タン、0=消滅間際)。MaxLifeTime未設定(<=0)なら常に満タン扱い
	float LifeFraction = 1.0f;
	if (Projectile->MaxLifeTime > KINDA_SMALL_NUMBER)
	{
		LifeFraction = FMath::Clamp(Projectile->GetLifeSpan() / Projectile->MaxLifeTime, 0.0f, 1.0f);
	}

	// 残り寿命が減るほど点滅を速くする
	const float Frequency = FMath::Lerp(MaxBlinkFrequency, MinBlinkFrequency, LifeFraction);

	BlinkElapsed += DeltaSeconds;
	// 1周期(0..1)のうちNormalColorDutyCycleぶんを白、残りを赤にする
	const float Phase = FMath::Frac(BlinkElapsed * Frequency);
	const bool bDanger = Phase >= FMath::Clamp(NormalColorDutyCycle, 0.0f, 1.0f);
	Comp->SetColorParameter(BlinkColorParamName, bDanger ? BlinkDangerColor : BlinkNormalColor);
}

void UPassiveVfxBehavior::OnDeactivate(AEnemyProjectile* Projectile)
{
	if (!Projectile) return;

	if (UNiagaraComponent* Comp = Projectile->GetPassiveEffectComponent())
	{
		Comp->Deactivate();
	}
}
