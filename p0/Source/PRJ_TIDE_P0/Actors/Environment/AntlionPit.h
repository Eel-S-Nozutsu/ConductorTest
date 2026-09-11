// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "AntlionPit.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class AAntlionCore;
class APawn;

/**
 * アリジゴクのすり鉢地形が発する渦の吸い込みゾーン。地形メッシュ自体は美術配置で、
 * 本アクターは判定と引き込みのみ担当する。詳細は Docs/AntlionGimmick.md 参照。
 *
 * 範囲内の IGroundPullAffectable（PL・敵）へ毎フレーム「中心への引き込み＋渦巻き」を配布する
 * （ASlidePassiveTornado・AWindZone と同じ Enter/Tick/Exit 方式。地面の吸い込みという別現象のため
 * IWindAffectable ではなく専用インターフェースを使う）。
 *
 * 中心に置かれた AAntlionCore の破壊デリゲートを購読し、破壊されたら引き込みを止める。
 */
UCLASS()
class PRJ_TIDE_P0_API AAntlionPit : public AActor
{
	GENERATED_BODY()

public:

	AAntlionPit();

protected:

	virtual void BeginPlay() override;
	virtual void Tick( float DeltaTime ) override;
	virtual void EndPlay( const EEndPlayReason::Type EndPlayReason ) override;
	virtual void OnConstruction( const FTransform& Transform ) override;

	void ApplyPull( float DeltaTime );		// 範囲の出入りで Enter/Exit も発火する
	void ReleaseAllTargets();				// 影響中の全対象へ Exit を通知して解放する
	void HandleCoreDestroyed();				// AAntlionCore 破壊時に呼ばれ、渦を止める
	void UpdateVortexFade( float DeltaTime );	// VortexMesh の該当パラメータを 1→0 へ

public:

	UPROPERTY( VisibleAnywhere, Category = "Tide|AntlionPit" )
	TObjectPtr<USceneComponent> SceneRoot;

	// エディタで常時表示する可視化用（見た目専用・コリジョン無し）
	UPROPERTY( VisibleAnywhere, Category = "Tide|AntlionPit" )
	TObjectPtr<USphereComponent> SphereVis;

	// --- エリア ---
	// 引き込みが働く半径（cm）。すり鉢地形の縁に合わせる
	UPROPERTY( EditAnywhere, Category = "Tide|AntlionPit|Area", meta = ( ClampMin = "0.0" ) )
	float PitRadius = 800.0f;

	// --- 引き込み ---
	// 中心付近での引き込み速度（cm/秒）。縁より強くする既定
	UPROPERTY( EditAnywhere, Category = "Tide|AntlionPit|Pull" )
	float PullSpeedNear = 800.0f;

	// 縁での引き込み速度（cm/秒）
	UPROPERTY( EditAnywhere, Category = "Tide|AntlionPit|Pull" )
	float PullSpeedFar = 150.0f;

	// 中心付近での渦巻き（接線）速度（cm/秒）
	UPROPERTY( EditAnywhere, Category = "Tide|AntlionPit|Pull" )
	float SwirlSpeedNear = 100.0f;

	// 縁での渦巻き（接線）速度（cm/秒）
	UPROPERTY( EditAnywhere, Category = "Tide|AntlionPit|Pull" )
	float SwirlSpeedFar = 400.0f;

	// 敵も引き込み対象にするか（既定 true）
	UPROPERTY( EditAnywhere, Category = "Tide|AntlionPit|Pull" )
	bool bAffectEnemies = true;

	// ON で、すり鉢メッシュ（VortexMesh）を踏んでいる対象だけ吸い込む。円内に上階など別の床がある場合の誤吸い込みを防ぐ
	// （すり鉢以外のコンポーネントにコリジョンを持たせている場合は OFF で従来の接地のみ判定に戻す）
	UPROPERTY( EditAnywhere, Category = "Tide|AntlionPit|Pull" )
	bool bRequireStandingOnPitMesh = true;

	// --- 渦の見た目（Core 破壊時のフェード）---
	// すり鉢の渦メッシュ。Core 破壊時に ArijigokuParameterName のスカラーを 1→0 へフェードする。
	// 未設定なら BeginPlay で最初の UStaticMeshComponent を自動解決する（BP 側で追加しても名前を知らずに拾える）
	UPROPERTY( EditAnywhere, Category = "Tide|AntlionPit|Visual" )
	TObjectPtr<UStaticMeshComponent> VortexMesh;

	// 渦が止まったときにフェードする Global Scalar Parameter 名
	UPROPERTY( EditAnywhere, Category = "Tide|AntlionPit|Visual" )
	FName ArijigokuParameterName = TEXT( "Arijigoku" );

	// 渦が止まってから ArijigokuParameterName を 1→0 へフェードし終えるまでの秒数
	UPROPERTY( EditAnywhere, Category = "Tide|AntlionPit|Visual", meta = ( ClampMin = "0.0" ) )
	float VortexFadeDuration = 1.5f;

	// --- 連携 ---
	// 中央の発生源。レベルに別アクターとして配置してここへドラッグ参照する（破壊デリゲートを購読し、壊れたら渦を止める）
	UPROPERTY( EditAnywhere, Category = "Tide|AntlionPit" )
	TObjectPtr<AAntlionCore> AntlionCore;

	// --- パフォーマンス（遠距離カリング）---
	// プレイヤーが引き込み範囲からこの距離ぶん以上離れている間は球オーバーラップ走査をスキップする（cm。0以下で無効）。
	// ゲート外の間は引き込み中の対象を解放する
	UPROPERTY( EditAnywhere, Category = "Tide|AntlionPit|Performance", meta = ( ClampMin = "0.0" ) )
	float ActivationCullMargin = 3000.0f;

	// ON で作動範囲（PitRadius＋マージンの球）を可視化する。
	// エディタ（非PIE）では ActivationCullVis のワイヤー球を常時表示、実行時は作動中=緑・スキップ中=赤の DrawDebug
	UPROPERTY( EditAnywhere, Category = "Tide|AntlionPit|Performance" )
	bool bDrawActivationCullRange = false;

	// 作動範囲のエディタ可視化用シェイプ（見た目専用・コリジョン無し）。OnConstruction で半径・可視性を反映。
	// ゲーム中は実行時の色分け DrawDebug に任せるため隠す
	UPROPERTY( VisibleAnywhere, Category = "Tide|AntlionPit|Performance" )
	TObjectPtr<USphereComponent> ActivationCullVis;

	// --- デバッグ ---
	UPROPERTY( EditAnywhere, Category = "Tide|AntlionPit|Debug" )
	bool bDebugDraw = true;

private:

	TWeakObjectPtr<APawn> CachedPlayerPawn;	// 距離ゲート用。無効時のみ取り直す

	bool bVortexActive = true;	// 発生源が破壊されたら false。以後引き込みを配らない

	// 現在引き込み中の対象。Enter/Exit を出入りで発火するために保持する
	TSet<TWeakObjectPtr<AActor>> PulledActors;

	float VortexFadeElapsed = 0.0f;
};
