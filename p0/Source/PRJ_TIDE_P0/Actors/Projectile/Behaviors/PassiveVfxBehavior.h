// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ProjectileBehavior.h"
#include "PassiveVfxBehavior.generated.h"

/**
 * 飛行中ずっと表示し続ける常駐効果
 */
UCLASS(DontCollapseCategories, meta = (DisplayName = "常駐VFX (本体/トレイル)"))
class PRJ_TIDE_P0_API UPassiveVfxBehavior : public UProjectileBehavior
{
	GENERATED_BODY()

public:

	// 飛行中ずっと表示するNiagara
	UPROPERTY(EditAnywhere)
	TObjectPtr<class UNiagaraSystem> System;

	// 常駐VFXのスケール
	UPROPERTY(EditAnywhere)
	FVector Scale = FVector(1.0f);

	// ONで、残り寿命に応じてNiagaraのカラーパラメータを点滅させる (時限自爆の警告演出など)
	UPROPERTY(EditAnywhere, Category = "Blink")
	bool bBlinkByLifetime = false;

	// 色を送るNiagaraパラメータ名
	UPROPERTY(EditAnywhere, Category = "Blink", meta = (EditCondition = "bBlinkByLifetime"))
	FName BlinkColorParamName = TEXT("Color_REF");

	// 点滅させる通常色
	UPROPERTY(EditAnywhere, Category = "Blink", meta = (EditCondition = "bBlinkByLifetime"))
	FLinearColor BlinkNormalColor = FLinearColor::White;

	// 点滅させる危険色 (残り寿命が減るとこちらへの切り替えが速くなる)
	UPROPERTY(EditAnywhere, Category = "Blink", meta = (EditCondition = "bBlinkByLifetime"))
	FLinearColor BlinkDangerColor = FLinearColor::Red;

	// 残り寿命が満タンのときの点滅周波数(Hz)
	UPROPERTY(EditAnywhere, Category = "Blink", meta = (EditCondition = "bBlinkByLifetime", ClampMin = "0.01"))
	float MinBlinkFrequency = 1.0f;

	// 残り寿命が0に近いときの点滅周波数(Hz)
	UPROPERTY(EditAnywhere, Category = "Blink", meta = (EditCondition = "bBlinkByLifetime", ClampMin = "0.01"))
	float MaxBlinkFrequency = 8.0f;

	// 1周期のうち通常色(白)を表示する割合。大きいほど白の時間が長くなる (0.5で白/赤が同じ長さ)
	UPROPERTY(EditAnywhere, Category = "Blink", meta = (EditCondition = "bBlinkByLifetime", ClampMin = "0.0", ClampMax = "1.0"))
	float NormalColorDutyCycle = 0.5f;

	// 常駐ビジュアルは生成時に起動 ※待機中から本体を表示するため
	virtual void OnSpawn(AEnemyProjectile* Projectile) override;
	// 点滅の位相をリセットする (プール再利用対策)
	virtual void OnLaunch(AEnemyProjectile* Projectile) override;
	// 点滅ON時、残り寿命に応じてカラーパラメータを更新する
	virtual void OnTick(AEnemyProjectile* Projectile, float DeltaSeconds) override;
	virtual bool WantsTick() const override { return bBlinkByLifetime; }
	// 起動したものを止めるだけなので演出ではなく後始末側
	virtual void OnDeactivate(AEnemyProjectile* Projectile) override;

private:

	float BlinkElapsed = 0.0f;

};
