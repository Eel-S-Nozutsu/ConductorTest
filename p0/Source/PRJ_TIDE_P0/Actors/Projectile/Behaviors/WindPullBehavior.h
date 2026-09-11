// Copyright (c) 2026, I.Fukunaka EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ProjectileBehavior.h"
#include "WindPullBehavior.generated.h"

class UNiagaraSystem;
class UNiagaraComponent;

/**
 * 弾自身を風源として扱い、範囲内のプレイヤーを弾の位置へ引き寄せる
 *
 * IWindAffectable経由でPCへFWindInfluence(Center=弾の位置, PullSpeed)を渡す。
 * 複数の風源に同時に巻き込まれる可能性があるため、捕捉状態はEnter/Exitの参照カウント方式で管理する
 */
UCLASS(meta = (DisplayName = "プレイヤー引き寄せ"))
class PRJ_TIDE_P0_API UWindPullBehavior : public UProjectileBehavior
{
	GENERATED_BODY()

public:

	// プレイヤーを捕捉する範囲 (cm)
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"))
	float PullRadius = 500.0f;

	// 中心（弾の位置）へ寄せる引き込み速度 (cm/秒)
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"))
	float PullSpeed = 300.0f;

	// 捕捉中のプレイヤーにアタッチするエフェクト。未設定ならエフェクトを出さない
	UPROPERTY(EditAnywhere, Category = "Effect")
	TObjectPtr<UNiagaraSystem> CaptureEffect = nullptr;

	// エフェクトのアタッチ先ソケット名。未指定(None)ならルートコンポーネントへアタッチする
	UPROPERTY(EditAnywhere, Category = "Effect")
	FName CaptureEffectSocketName = NAME_None;

	virtual void OnSpawn(AEnemyProjectile* Projectile) override;
	virtual void OnLaunch(AEnemyProjectile* Projectile) override;
	virtual void OnTick(AEnemyProjectile* Projectile, float DeltaSeconds) override;
	virtual void OnDeactivate(AEnemyProjectile* Projectile) override;
	virtual bool WantsTick() const override { return true; }

private:

	// 捕捉中であれば OnWindExit を発火し、エフェクトを止めて解放する
	void ReleasePlayerIfCaptured();

	// 捕捉開始時にプレイヤーへエフェクトをアタッチする
	void BeginCaptureEffect(APawn* Player);
	// 捕捉解除時にエフェクトを止める
	void EndCaptureEffect();

	TWeakObjectPtr<APawn> CachedPlayer;
	bool bPlayerCaptured = false;

	TWeakObjectPtr<UNiagaraComponent> ActiveCaptureEffect;
};
