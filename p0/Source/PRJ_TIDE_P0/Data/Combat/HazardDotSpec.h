// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "HazardDotSpec.generated.h"

class UNiagaraSystem;

/**
 * しびれ床など「継続ダメージ源」が対象へ渡す DoT 仕様。
 * 対象側 UHazardDotComponent が TypeTag ごとに保持し、自前タイマーで刻む。
 * 同じ TypeTag は1本に集約（複数ソースが重なってもカデンツ1本）、異なる TypeTag は独立して刻む。
 */
USTRUCT( BlueprintType )
struct FHazardDotSpec
{
	GENERATED_BODY()

	// DoT の型（カデンツのキー）。例：HazardDot.Lightning
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "HazardDot" )
	FGameplayTag TypeTag;

	// 1刻みのダメージ量
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "HazardDot" )
	float DamagePerTick = 5.0f;

	// 刻み間隔（秒）
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "HazardDot" )
	float TickInterval = 0.5f;

	// 刻みダメージのリアクション種別（毎刻みのけぞらせたくない場合は未設定）
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "HazardDot" )
	FGameplayTag HitReactionTag;

	// 1刻みごとに対象位置へ再生する単発ヒットエフェクト（未設定なら出さない）
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "HazardDot" )
	TObjectPtr<UNiagaraSystem> HitEffect;

	// 被弾中、対象へアタッチしてループ再生する演出VFX（NS_LightningDamage 等。単発 HitEffect とは別枠）。
	// 床にいる間ずっと再生し、離れて猶予（RefreshGrace）を過ぎたら Deactivate。未設定なら出さない
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "HazardDot" )
	TObjectPtr<UNiagaraSystem> DamageAuraVFX;

	// 1刻みごとにヒットストップをかけるか（プレイヤーのみ適用）
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "HazardDot" )
	bool bUseHitStop = false;

	// ヒットストップ持続時間（秒）
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "HazardDot" )
	float HitStopDuration = 0.1f;

	// ヒットストップ時の TimeDilation（0 で停止）
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "HazardDot", meta = ( ClampMin = "0.0", ClampMax = "1.0" ) )
	float HitStopDilation = 0.1f;

	// 加害元（敵味方判定・キルログ等に使う）
	UPROPERTY()
	TWeakObjectPtr<AActor> Instigator;
};
