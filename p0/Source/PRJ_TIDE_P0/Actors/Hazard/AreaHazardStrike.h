// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Templates/SubclassOf.h"
#include "AreaHazardStrike.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;
class AAreaHazardZone;

/**
 * エリアハザードの落雷。汎用フレーム [Docs/AreaHazard.md] の一部で、雷/溶岩は BP・数値の差し替えで表現する。
 *
 * Activate(落雷地点) で開始し、Warning（予兆）→ Strike（範囲一撃ダメージ）→ ZoneClass 展開 → 自壊 と進む。
 * 落雷は一撃（再アームなし）で、床の継続ダメージは展開した AAreaHazardZone が担う。
 * Owner はスポーンしたソース（環境ハザードは NoTeam 運用＝全員に効く）。
 */
UCLASS()
class PRJ_TIDE_P0_API AAreaHazardStrike : public AActor
{
	GENERATED_BODY()

public:
	AAreaHazardStrike();

	// スポーン直後に呼ぶ。InFloorNormal は見た目を傾けるのに使い（展開する Zone にも引き継ぐ）、
	// 当たり判定は常に world Z 方向の縦筒スイープのままで傾きの影響を受けない
	void Activate( const FVector& InStrikeLocation, const FVector& InFloorNormal = FVector::UpVector );

protected:
	virtual void Tick( float DeltaTime ) override;

	void EnterStrike();
	void ApplyStrikeDamage();	// 範囲内の敵対 IDamageable へ一撃（再アームなし）
	void SpawnZone();

	// StrikeRadius を基準半径で割った、Niagara の "Scale" へ渡す値
	float GetWarningVFXScale() const;
	float GetStrikeVFXScale() const;

public:
	UPROPERTY( VisibleAnywhere, Category = "Tide|HazardStrike" )
	TObjectPtr<USceneComponent> SceneRoot;

	// 予兆の見た目（ループ。BP で NS_LightningWarning 等を割り当てる。Strike へ移行時に Deactivate する）
	UPROPERTY( VisibleAnywhere, Category = "Tide|HazardStrike" )
	TObjectPtr<UNiagaraComponent> WarningVFX;

	// 落雷の見た目（単発。BP で NS_LightningStrike 等を割り当てる）
	UPROPERTY( VisibleAnywhere, Category = "Tide|HazardStrike" )
	TObjectPtr<UNiagaraComponent> StrikeVFX;

	// 予兆VFX の Scale パラメータ算出用の基準半径（cm）。Scale=1.0 のとき WarningVFX が表現する半径。
	// 判定 StrikeRadius をこの値で割った比率を Niagara の float "Scale" へ渡し、判定範囲に見た目を合わせる
	UPROPERTY( EditAnywhere, Category = "Tide|HazardStrike|VFX", meta = ( ClampMin = "1.0" ) )
	float WarningVFXBaseRadius = 200.0f;

	// 落雷VFX の Scale パラメータ算出用の基準半径（cm）。Scale=1.0 のとき StrikeVFX が表現する半径
	UPROPERTY( EditAnywhere, Category = "Tide|HazardStrike|VFX", meta = ( ClampMin = "1.0" ) )
	float StrikeVFXBaseRadius = 200.0f;

	// 予兆の長さ（秒）
	UPROPERTY( EditAnywhere, Category = "Tide|HazardStrike" )
	float WarningDuration = 2.0f;

	// 落雷後、余韻を見せて自壊するまでの猶予（秒）
	UPROPERTY( EditAnywhere, Category = "Tide|HazardStrike" )
	float StrikeLingerBeforeDestroy = 0.3f;

	// 落雷の一撃範囲（半径 cm）
	UPROPERTY( EditAnywhere, Category = "Tide|HazardStrike|Damage" )
	float StrikeRadius = 200.0f;

	// 落雷の判定高さ（地面から上方向、cm）。段差上・空中の対象も拾えるようにする円柱状判定の高さ
	UPROPERTY( EditAnywhere, Category = "Tide|HazardStrike|Damage", meta = ( ClampMin = "0.0" ) )
	float StrikeHeight = 400.0f;

	// 落雷ダメージ量
	UPROPERTY( EditAnywhere, Category = "Tide|HazardStrike|Damage" )
	float StrikeDamage = 20.0f;

	// 落雷のリアクション種別
	UPROPERTY( EditAnywhere, Category = "Tide|HazardStrike|Damage" )
	FGameplayTag HitReactionTag;

	// 被弾者位置へ再生する単発ヒットエフェクト（未設定なら出さない）
	UPROPERTY( EditAnywhere, Category = "Tide|HazardStrike|Damage" )
	TObjectPtr<UNiagaraSystem> HitEffect;

	// 被弾中、対象へアタッチしてループ再生する演出VFX（NS_LightningDamage 等。単発 HitEffect とは別枠）。
	// 落雷は一撃なので DamageAuraDuration 秒だけ出して Deactivate する。未設定なら出さない
	UPROPERTY( EditAnywhere, Category = "Tide|HazardStrike|Damage" )
	TObjectPtr<UNiagaraSystem> DamageAuraVFX;

	// 落雷一撃で上記オーラを出す時間（秒）
	UPROPERTY( EditAnywhere, Category = "Tide|HazardStrike|Damage", meta = ( ClampMin = "0.0" ) )
	float DamageAuraDuration = 0.5f;

	// 命中時にヒットストップをかけるか（プレイヤーのみ適用）
	UPROPERTY( EditAnywhere, Category = "Tide|HazardStrike|Damage" )
	bool bUseHitStop = true;

	// ヒットストップ持続時間（秒）
	UPROPERTY( EditAnywhere, Category = "Tide|HazardStrike|Damage", meta = ( EditCondition = "bUseHitStop" ) )
	float HitStopDuration = 0.12f;

	// ヒットストップ時の TimeDilation（0 で停止）
	UPROPERTY( EditAnywhere, Category = "Tide|HazardStrike|Damage", meta = ( EditCondition = "bUseHitStop", ClampMin = "0.0", ClampMax = "1.0" ) )
	float HitStopDilation = 0.05f;

	// 展開する残留床（未設定なら床なし）
	UPROPERTY( EditAnywhere, Category = "Tide|HazardStrike|Zone" )
	TSubclassOf<AAreaHazardZone> ZoneClass;

	// ON で予兆輪・落雷フラッシュを描画する
	UPROPERTY( EditAnywhere, Category = "Tide|HazardStrike|Debug" )
	bool bDebugDraw = true;

private:
	enum class EPhase : uint8 { Warning, Strike, Done };
	EPhase Phase = EPhase::Warning;
	float ElapsedTime = 0.0f;
	FVector StrikeLocation = FVector::ZeroVector;

	// 予兆/落雷 VFX の Rotation を傾け、展開する Zone の見た目へも引き継ぐ
	FVector FloorNormal = FVector::UpVector;
};
