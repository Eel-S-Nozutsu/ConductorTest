// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "AreaHazardZone.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;
class APawn;

/**
 * エリアハザードの残留ダメージ床（しびれ床）。汎用フレーム [Docs/AreaHazard.md] の一部で、
 * 雷/溶岩は BP・数値・型タグの差し替えで表現する。
 *
 * 一定時間、球範囲内かつ接地している敵対対象が持つ UHazardDotComponent へ毎フレーム RefreshDot するだけ。
 * ダメージの刻み自体は対象側コンポーネントが自前タイマーで行う（複数 Zone が重なってもカデンツは型ごと1本）。
 * Zone 自身はダメージを与えず再アームも持たない。
 * Owner はスポーンしたソース（環境ハザードは NoTeam 運用＝全員に効く）。
 *
 * 竜巻が範囲に触れると（bErasableByWind）床を吹き飛ばしてフェードアウト消滅する。竜巻の DamageVolume は
 * WorldDynamic なので、DoT リフレッシュ用の既存の AllDynamicObjects オーバーラップにそのまま乗る。
 */
UCLASS()
class PRJ_TIDE_P0_API AAreaHazardZone : public AActor
{
	GENERATED_BODY()

public:
	AAreaHazardZone();

	// スポーン直後に呼ぶ。負値の項目はエディタ設定の値をそのまま使う。
	// InFloorNormal は見た目にのみ使い、当たり判定は常に world Z 方向のスイープ
	void Activate( float InRadius = -1.0f, float InZoneDuration = -1.0f, const FVector& InFloorNormal = FVector::UpVector );

protected:
	virtual void Tick( float DeltaTime ) override;

	// 併せて竜巻が範囲内にいれば風消滅を開始する（true を返したら今フレームは風消滅へ移行）
	bool RefreshDotInArea();

	void BeginWindDissipate();	// DoT を止め、ZoneVFX を Deactivate してフェードへ
	float GetZoneVFXScale() const;	// Radius を基準半径で割った、Niagara の "Scale" へ渡す値

public:
	UPROPERTY( VisibleAnywhere, Category = "Tide|HazardZone" )
	TObjectPtr<USceneComponent> SceneRoot;

	// 床の見た目（ループ。BP で NS_LightningFloor 等を割り当てる。破棄時に Deactivate する）
	UPROPERTY( VisibleAnywhere, Category = "Tide|HazardZone" )
	TObjectPtr<UNiagaraComponent> ZoneVFX;

	// 床VFX の Scale パラメータ算出用の基準半径（cm）。Scale=1.0 のとき ZoneVFX が表現する半径。
	// 判定 Radius をこの値で割った比率を Niagara の float "Scale" へ渡し、判定範囲に見た目を合わせる
	UPROPERTY( EditAnywhere, Category = "Tide|HazardZone|VFX", meta = ( ClampMin = "1.0" ) )
	float ZoneVFXBaseRadius = 200.0f;

	// 床の範囲（球の半径 cm）。判定は接地している対象のみを拾う球オーバーラップ
	UPROPERTY( EditAnywhere, Category = "Tide|HazardZone" )
	float Radius = 300.0f;

	// 床の持続時間（秒）。0 以下で無限（手動破棄まで）
	UPROPERTY( EditAnywhere, Category = "Tide|HazardZone" )
	float ZoneDuration = 5.0f;

	// DoT の型（カデンツのキー）。同じ型の床が重なってもカデンツは1本
	UPROPERTY( EditAnywhere, Category = "Tide|HazardZone|Damage", meta = ( Categories = "HazardDot" ) )
	FGameplayTag HazardDotTypeTag;

	// 1刻みのダメージ量
	UPROPERTY( EditAnywhere, Category = "Tide|HazardZone|Damage" )
	float Damage = 5.0f;

	// 刻み間隔（秒）
	UPROPERTY( EditAnywhere, Category = "Tide|HazardZone|Damage" )
	float DamageInterval = 1.0f;

	// 刻みのリアクション種別（毎刻みのけぞらせたくない場合は空）
	UPROPERTY( EditAnywhere, Category = "Tide|HazardZone|Damage" )
	FGameplayTag HitReactionTag;

	// 1刻みごとに対象位置へ再生する単発ヒットエフェクト（未設定なら出さない）
	UPROPERTY( EditAnywhere, Category = "Tide|HazardZone|Damage" )
	TObjectPtr<UNiagaraSystem> HitEffect;

	// 被弾中、対象へアタッチしてループ再生する演出VFX（NS_LightningDamage 等。単発 HitEffect とは別枠）。
	// 床にいる間ずっと再生し、離れたら Deactivate。未設定なら出さない
	UPROPERTY( EditAnywhere, Category = "Tide|HazardZone|Damage" )
	TObjectPtr<UNiagaraSystem> DamageAuraVFX;

	// 1刻みごとにヒットストップをかけるか（プレイヤーのみ適用）。毎刻みかかるので短めに
	UPROPERTY( EditAnywhere, Category = "Tide|HazardZone|Damage" )
	bool bUseHitStop = true;

	// ヒットストップ持続時間（秒）
	UPROPERTY( EditAnywhere, Category = "Tide|HazardZone|Damage", meta = ( EditCondition = "bUseHitStop" ) )
	float HitStopDuration = 0.05f;

	// ヒットストップ時の TimeDilation（0 で停止）
	UPROPERTY( EditAnywhere, Category = "Tide|HazardZone|Damage", meta = ( EditCondition = "bUseHitStop", ClampMin = "0.0", ClampMax = "1.0" ) )
	float HitStopDilation = 0.1f;

	// プレイヤーが床の範囲からこの距離ぶん以上離れている間は毎フレームの球オーバーラップ走査をスキップする
	// （cm。0以下で無効）。止まるのは DoT のリフレッシュだけで、刻みは対象側タイマーで自然に途切れる
	UPROPERTY( EditAnywhere, Category = "Tide|HazardZone|Performance", meta = ( ClampMin = "0.0" ) )
	float ActivationCullMargin = 3000.0f;

	// ON で作動範囲（Radius＋マージンの球）を描画する。作動中は緑・スキップ中は赤
	UPROPERTY( EditAnywhere, Category = "Tide|HazardZone|Performance" )
	bool bDrawActivationCullRange = false;

	// ON で、スライドパッシブの竜巻（ASlidePassiveTornado）が範囲に触れると床を吹き飛ばして消滅させる。
	// 溶岩など「風で消えてほしくない」床は OFF にする
	UPROPERTY( EditAnywhere, Category = "Tide|HazardZone|Wind" )
	bool bErasableByWind = true;

	// 風消滅（フェードアウト）にかける時間（秒）。検出時に DoT を止め ZoneVFX を Deactivate し、
	// この時間ぶん残粒子をフェードさせてから Destroy する
	UPROPERTY( EditAnywhere, Category = "Tide|HazardZone|Wind", meta = ( EditCondition = "bErasableByWind", ClampMin = "0.0" ) )
	float WindDissipateTime = 0.4f;

	// ON で範囲円を毎フレーム描画する
	UPROPERTY( EditAnywhere, Category = "Tide|HazardZone|Debug" )
	bool bDebugDraw = true;

private:
	float ElapsedTime = 0.0f;

	bool bDissipating = false;		// 風消滅中。true の間は DoT を止めフェード計時する
	float DissipateElapsed = 0.0f;	// WindDissipateTime に達したら Destroy

	TWeakObjectPtr<APawn> CachedPlayerPawn;	// 距離ゲート用。無効時のみ取り直す
};
