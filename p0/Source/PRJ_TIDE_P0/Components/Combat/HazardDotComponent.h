// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PRJ_TIDE_P0/Data/Combat/HazardDotSpec.h"
#include "HazardDotComponent.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;

/**
 * 継続ダメージ（しびれ床など）の受け側コンポーネント。ATideCharacter 基底に付ける。
 *
 * 継続ダメージ源（AAreaHazardZone 等）から RefreshDot され続ける間、型タグごとに自前の一定間隔
 * タイマーでダメージを適用する。複数ソースが重なってもカデンツは型タグごと1本（倍速で食らわない）。
 * リフレッシュが RefreshGrace を超えて途切れたら（＝範囲から出た）その型の DoT を止める（退出＝即停止）。
 *
 * 汎用フレーム [Docs/AreaHazard.md] の一部。毒・炎上など他ソースの受け皿にも流用できる。
 */
UCLASS( ClassGroup = ( Custom ), meta = ( BlueprintSpawnableComponent ) )
class UHazardDotComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHazardDotComponent();

	// 継続ダメージ源が毎フレーム呼ぶ。TypeTag ごとに entry を追加/更新する
	void RefreshDot( const FHazardDotSpec& Spec );

	// 被弾中に対象へアタッチするループ演出 VFX をリフレッシュする。HoldDuration を過ぎて途切れたら Deactivate する。
	// Zone は床にいる間毎フレーム、Strike は一撃時に一定時間ぶん呼ぶ
	void RefreshDamageAura( UNiagaraSystem* AuraVFX, float HoldDuration );

	virtual void TickComponent( float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction ) override;

public:
	// リフレッシュが途切れたと判定するまでの猶予（秒）。フレーム順・判定揺らぎ対策の内部 debounce。
	// これを過ぎてリフレッシュが無ければ「範囲外へ出た」とみなし DoT を止める（退出＝即停止）
	UPROPERTY( EditAnywhere, Category = "Tide|HazardDot" )
	float RefreshGrace = 0.1f;

	// 入った最初のリフレッシュで即1刻み入れるか（ON＝入った瞬間に痛い）
	UPROPERTY( EditAnywhere, Category = "Tide|HazardDot" )
	bool bTickImmediatelyOnEnter = true;

private:
	// 型タグごとに保持する進行中 DoT
	struct FActiveHazardDot
	{
		FHazardDotSpec Spec;
		double LastTickTime = 0.0;	// 最後にダメージを適用したワールド時刻
		double ExpiryTime = 0.0;	// この時刻までにリフレッシュされなければ止める
	};
	TArray<FActiveHazardDot> ActiveDots;

	// 被弾中のループ演出（対象へアタッチ）。リフレッシュ切れで Deactivate（bAutoDestroy=false で使い回す）
	UPROPERTY( Transient )
	TObjectPtr<UNiagaraComponent> DamageAuraComponent;
	// 現在アタッチ中のオーラアセット（アセットが変わったら張り替える）
	TWeakObjectPtr<UNiagaraSystem> DamageAuraAsset;
	// この時刻までにリフレッシュされなければオーラを Deactivate する
	double DamageAuraExpiryTime = 0.0;

	// オーナー（IDamageable）へ1刻みぶんのダメージを適用する
	void ApplyTick( const FActiveHazardDot& Dot );
};
