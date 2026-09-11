// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"

#include "PRJ_TIDE_P0/Interfaces/Environment/IWindAffectable.h"

#include "AntlionCore.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UNiagaraSystem;
class APawn;

// 破壊時に AAntlionPit が購読する通知（渦を止めるトリガー）
DECLARE_MULTICAST_DELEGATE( FOnAntlionCoreDestroyed );

/**
 * アリジゴクギミックの発生源。すり鉢（AAntlionPit）の中央に据え置きで配置する。
 *
 * PL が接近すると明滅が始まり一定時間後に爆発するが、自身は消滅せずクールダウンを挟んで
 * 何度でも起爆する（ABossMineHazard の接近起爆・点滅ロジックを参考にした非破壊ループ）。
 * 竜巻に囲まれた場合のみ破壊され、OnCoreDestroyed で AAntlionPit が渦の引き込みを止める。
 */
UCLASS()
class PRJ_TIDE_P0_API AAntlionCore : public AActor, public IWindAffectable
{
	GENERATED_BODY()

public:

	AAntlionCore();

	FOnAntlionCoreDestroyed OnCoreDestroyed;

	// 竜巻の判定範囲に入った時点で「囲まれた」とみなす（浮石・地雷ギミックと同じ簡易判定）
	virtual void OnWindEnter( const FWindInfluence& Wind ) override;

	bool IsDestroyed() const { return State == EAntlionCoreState::Destroyed; }

protected:

	virtual void BeginPlay() override;
	virtual void Tick( float DeltaTime ) override;
	virtual void OnConstruction( const FTransform& Transform ) override;

private:

	enum class EAntlionCoreState : uint8
	{
		Idle,
		Fusing,		// 接近起爆カウント中（明滅）
		Destroyed,	// 竜巻に囲まれて破壊済み（終端）
	};

	void UpdateProximityFuse( float DeltaTime );	// Destroyed 以外の状態で毎フレーム呼ぶ
	void ApplyBlinkMaterial( float DeltaTime );		// 起爆が近いほど速く点滅させる
	bool HasHostileTargetWithin( float Radius );	// ProximityOrigin 中心の 3D 距離（球）で判定
	void Explode();		// 範囲ダメージ＋VFX。自壊はせず Idle に戻す
	void DestroyCore();	// Pit へ破壊を通知し、自身は非表示にして Destroy する

public:

	// アクターのルート。Collision・Mesh・ProximityOrigin をそれぞれ独立して調整できるよう、
	// いずれか（特に Collision）をルートにせずプレーンな SceneComponent をルートにする
	UPROPERTY( VisibleAnywhere, Category = "Tide|Antlion" )
	TObjectPtr<USceneComponent> SceneRoot;

	// 竜巻（ASlidePassiveTornado）の AllDynamicObjects スイープに拾われるための当たり。
	// SceneRoot の子（ルートではない）なので、見た目・判定基準点と独立して位置調整できる
	UPROPERTY( VisibleAnywhere, Category = "Tide|Antlion" )
	TObjectPtr<USphereComponent> CollisionComp;

	// 美術支給のスタティックメッシュ
	UPROPERTY( VisibleAnywhere, Category = "Tide|Antlion" )
	TObjectPtr<UStaticMeshComponent> MeshComp;

	// 接近起爆・爆発ダメージの判定基準点（3D 距離の球で判定する中心）。エディタでドラッグして動かせるので、Core 全体を
	// すり鉢の底へ沈めてもこの点だけ地表付近へ戻せば「見た目上近づいたら反応する」を素直な球判定のまま実現できる
	UPROPERTY( VisibleAnywhere, Category = "Tide|Antlion", meta = ( MakeEditWidget = true ) )
	TObjectPtr<USceneComponent> ProximityOrigin;

	// ProximityFuseRadius の可視化（bDebugDraw が true の間、エディタ・実行時とも常時表示）。
	// ProximityOrigin を中心に表示する。見た目専用・コリジョン無し
	UPROPERTY( VisibleAnywhere, Category = "Tide|Antlion" )
	TObjectPtr<USphereComponent> ProximityVis;

	// 接近起爆：この半径内に敵対対象が入るとカウントを開始する（cm）
	UPROPERTY( EditAnywhere, Category = "Tide|Antlion|Proximity", meta = ( ClampMin = "0.0" ) )
	float ProximityFuseRadius = 400.0f;

	// 接近起爆までの秒数
	UPROPERTY( EditAnywhere, Category = "Tide|Antlion|Proximity", meta = ( ClampMin = "0.0" ) )
	float ProximityFuseDuration = 3.0f;

	// 接近起爆カウントが 0 に近づいたときの最速点滅周期（秒）。小さいほど速い
	UPROPERTY( EditAnywhere, Category = "Tide|Antlion|Proximity", meta = ( ClampMin = "0.01" ) )
	float ProximityFuseFastBlinkPeriod = 0.05f;

	// 通常（待機）時の点滅周期（秒）。大きいほどゆっくり点滅する
	UPROPERTY( EditAnywhere, Category = "Tide|Antlion|Visual" )
	float BlinkPeriod = 2.0f;

	UPROPERTY( EditAnywhere, Category = "Tide|Antlion|Visual" )
	FName BlinkParameterName = TEXT( "Blink" );

	UPROPERTY( EditAnywhere, Category = "Tide|Antlion|Damage", meta = ( ClampMin = "0.0" ) )
	float ExplosionRadius = 300.0f;

	UPROPERTY( EditAnywhere, Category = "Tide|Antlion|Damage" )
	float ExplosionDamage = 20.0f;

	UPROPERTY( EditAnywhere, Category = "Tide|Antlion|Damage" )
	FGameplayTag HitReactionTag;

	// ON で爆発範囲内の壊れ物も破壊する。Pawn ではないので通常の被爆対象走査に入らず、専用に集めて同じ球判定にかける
	// （壊れ物側は既定でプレイヤーの攻撃しか受けないため bCanBreakProps を立てて許可する）。空中チャージ攻撃限定の壊れ物は対象外
	UPROPERTY( EditAnywhere, Category = "Tide|Antlion|Damage" )
	bool bDamageBreakableProps = true;

	// 爆発後、再び明滅を開始できるまでの猶予（連続起爆の防止）
	UPROPERTY( EditAnywhere, Category = "Tide|Antlion|Damage", meta = ( ClampMin = "0.0" ) )
	float PostExplosionReArmDelay = 1.5f;

	UPROPERTY( EditAnywhere, Category = "Tide|Antlion|Visual" )
	TObjectPtr<UNiagaraSystem> ExplosionEffect;

	// ExplosionEffect の大きさ。アセット側がアクター Scale を無視する作りのため、
	// Transform Scale ではなく Niagara の User Parameter（Float）にのみ書き込む
	UPROPERTY( EditAnywhere, Category = "Tide|Antlion|Visual", meta = ( ClampMin = "0.0" ) )
	float ExplosionEffectScale = 1.0f;

	// ExplosionEffectScale を書き込む Niagara User Parameter 名（未設定/該当パラメータが無ければ無視される）
	UPROPERTY( EditAnywhere, Category = "Tide|Antlion|Visual" )
	FName ExplosionEffectScaleParameterName = TEXT( "EXP_Size" );

	// ExplosionEffect（2D ビルボード想定）のスポーン位置を、カメラ方向へこの距離だけずらす（cm）。
	// 0 でオフセット無し。地面やメッシュに埋まって見えるのを防ぐための見た目調整用
	UPROPERTY( EditAnywhere, Category = "Tide|Antlion|Visual" )
	float ExplosionEffectCameraOffset = 0.0f;

	// ExplosionEffect のスポーン位置の Z オフセット（cm）。正の値で上へずらす
	UPROPERTY( EditAnywhere, Category = "Tide|Antlion|Visual" )
	float ExplosionEffectZOffset = 0.0f;

	// プレイヤーが接近起爆範囲からこの距離ぶん以上離れている間は、カウント開始前の毎フレーム全 Pawn 走査をスキップする
	// （cm。0以下で無効）。起爆カウント開始後は離れても継続する既存仕様は変えない
	UPROPERTY( EditAnywhere, Category = "Tide|Antlion|Performance", meta = ( ClampMin = "0.0" ) )
	float ActivationCullMargin = 3000.0f;

	// ON で作動範囲（ProximityFuseRadius＋マージンの球）を可視化する。
	// エディタ（非PIE）では ActivationCullVis のワイヤー球を常時表示、実行時は作動中=緑・スキップ中=赤の DrawDebug
	UPROPERTY( EditAnywhere, Category = "Tide|Antlion|Performance" )
	bool bDrawActivationCullRange = false;

	// 作動範囲のエディタ可視化用シェイプ（ProximityOrigin 基準・見た目専用）。OnConstruction で半径・可視性を反映。
	// ゲーム中は実行時の色分け DrawDebug に任せるため隠す
	UPROPERTY( VisibleAnywhere, Category = "Tide|Antlion|Performance" )
	TObjectPtr<USphereComponent> ActivationCullVis;

	UPROPERTY( EditAnywhere, Category = "Tide|Antlion|Debug" )
	bool bDebugDraw = false;

private:

	TWeakObjectPtr<APawn> CachedPlayerPawn;	// 距離ゲート用。無効時のみ取り直す

	EAntlionCoreState State = EAntlionCoreState::Idle;

	bool bProximityFuseActive = false;
	float ProximityFuseRemaining = 0.0f;
	float ReArmRemaining = 0.0f;
	float BlinkPhase = 0.0f;	// 周期が変わっても不連続にならないよう位相で保持する

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> BlinkMID = nullptr;
};
