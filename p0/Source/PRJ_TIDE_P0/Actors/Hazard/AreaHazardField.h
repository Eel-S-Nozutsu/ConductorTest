// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Templates/SubclassOf.h"
#include "PRJ_TIDE_P0/Actors/Hazard/HazardArea.h"
#include "AreaHazardField.generated.h"

class AAreaHazardStrike;
class AAreaHazardExclusion;
class APawn;
class USphereComponent;

/** Strike の抽選候補1件（クラス＋重み）。プレイヤー中心落としの複数指定に使う */
USTRUCT( BlueprintType )
struct FAreaHazardStrikeOption
{
	GENERATED_BODY()

	// 落とす Strike クラス
	UPROPERTY( EditAnywhere )
	TSubclassOf<AAreaHazardStrike> StrikeClass;

	// 抽選の重み（大きいほど選ばれやすい。0以下は選ばれない）
	UPROPERTY( EditAnywhere, meta = ( ClampMin = "0.0" ) )
	float Weight = 1.0f;
};

/**
 * エリアハザードの管理アクター。一定間隔でエリア内ランダム地点に Strike をスポーンする。
 * 流れは 抽選 → 除外チェック → 真下トレースで地面決定 → StrikeOptions から重み抽選してスポーン。
 *
 * 見た目は持たず、bDebugDraw の描画で成立を確認する。環境ハザードは NoTeam 運用（＝チーム無し）で
 * Strike/Zone のダメージが全員に効く。詳細は Docs/AreaHazard.md。
 */
UCLASS()
class PRJ_TIDE_P0_API AAreaHazardField : public AActor
{
	GENERATED_BODY()

public:
	AAreaHazardField();

	// 発生の開始/停止（BeginPlay で bAutoStart なら自動開始）
	UFUNCTION( BlueprintCallable, Category = "Tide|HazardField" )
	void StartHazard();
	UFUNCTION( BlueprintCallable, Category = "Tide|HazardField" )
	void StopHazard();

protected:
	virtual void BeginPlay() override;
	virtual void Tick( float DeltaTime ) override;
	virtual void OnConstruction( const FTransform& Transform ) override;

	// エディタ編集中（ビューポートのみの Tick）でもエリアをデバッグ描画するため true
	virtual bool ShouldTickIfViewportsOnly() const override { return true; }

	void CollectExclusions();	// 自エリア内の除外アクターを収集（＋手動指定を追加）

	// 以下の抽選系に共通：NewRadius は今回落とす Strike の判定半径（かぶり防止判定に使う）

	// XY 候補地点の地面決定＋除外＋かぶりチェック（Z は無視し TraceHeight から真下トレース）
	bool ResolveGroundPoint( const FVector& CandidateXY, float NewRadius, FVector& OutLocation, FVector& OutNormal ) const;
	bool SampleStrikeLocation( float NewRadius, FVector& OutLocation, FVector& OutNormal ) const;
	bool SamplePlayerCenteredLocation( const FVector& PlayerLocation, float NewRadius, FVector& OutLocation, FVector& OutNormal ) const;
	bool OverlapsActiveStrike( const FVector& Location, float NewRadius ) const;

	static float GetStrikeRadiusFromClass( TSubclassOf<AAreaHazardStrike> InStrikeClass );	// CDO 参照。無効なら 0
	bool IsPlayerInArea( const APawn* PlayerPawn ) const;

	// 中央 Frac 割の矩形に映っているか。フィルタ無効・PC 無しは常に true
	bool IsPointOnPlayerScreen( const FVector& WorldPoint ) const;

	// 重み付き乱数で選ぶ（候補が空／全滅なら nullptr）
	TSubclassOf<AAreaHazardStrike> PickStrikeClass( const TArray<FAreaHazardStrikeOption>& Options ) const;

	void SpawnStrike();					// 上限・抽選失敗時は何もしない
	void SpawnPlayerCenteredStrike();	// プレイヤーがエリア内にいるときだけ
	void SpawnStrikeAt( const FVector& Location, const FVector& Normal, TSubclassOf<AAreaHazardStrike> InStrikeClass );
	int32 PruneAndCountActiveStrikes();	// 無効参照を掃除して数を返す

public:
	UPROPERTY( VisibleAnywhere, Category = "Tide|HazardField" )
	TObjectPtr<USceneComponent> SceneRoot;

	// --- エリア ---
	UPROPERTY( EditAnywhere, Category = "Tide|HazardField|Area" )
	EHazardAreaShape Shape = EHazardAreaShape::Box;
	UPROPERTY( EditAnywhere, Category = "Tide|HazardField|Area", meta = ( EditCondition = "Shape == EHazardAreaShape::Box", EditConditionHides ) )
	FVector BoxExtent = FVector( 1000.0f, 1000.0f, 300.0f );
	UPROPERTY( EditAnywhere, Category = "Tide|HazardField|Area", meta = ( EditCondition = "Shape == EHazardAreaShape::Sphere", EditConditionHides ) )
	float SphereRadius = 1000.0f;

	// --- 発生 ---
	// 発生間隔（秒）。実際は ±IntervalJitter で揺らす
	UPROPERTY( EditAnywhere, Category = "Tide|HazardField|Spawn", meta = ( ClampMin = "0.01" ) )
	float SpawnInterval = 2.0f;
	UPROPERTY( EditAnywhere, Category = "Tide|HazardField|Spawn", meta = ( ClampMin = "0.0" ) )
	float IntervalJitter = 0.5f;
	// ランダム抽選で落とす Strike 候補（雷→溶岩の差し替え口。複数指定可・Weight の重み付き乱数で毎回選ぶ）。
	// 空（または全候補が無効）ならスポーンしない
	UPROPERTY( EditAnywhere, Category = "Tide|HazardField|Spawn" )
	TArray<FAreaHazardStrikeOption> StrikeOptions;
	// 同時進行の Strike 上限（0＝無制限）
	UPROPERTY( EditAnywhere, Category = "Tide|HazardField|Spawn", meta = ( ClampMin = "0" ) )
	int32 MaxConcurrentStrikes = 0;
	// 抽選リトライ上限（除外だらけでも無限ループしない）
	UPROPERTY( EditAnywhere, Category = "Tide|HazardField|Spawn", meta = ( ClampMin = "1" ) )
	int32 MaxSampleAttempts = 8;
	// ON で、新しい Strike の落下範囲が進行中 Strike と重ならない地点だけを採用する（水平距離が両者の半径合計＋
	// StrikeOverlapMargin 未満なら棄却して再抽選。MaxSampleAttempts 回で全滅ならその回は見送り）
	UPROPERTY( EditAnywhere, Category = "Tide|HazardField|Spawn" )
	bool bPreventStrikeOverlap = false;
	// かぶり防止時に、両範囲の間へさらに空ける余白（cm）。0 でちょうど接する位置まで許容
	UPROPERTY( EditAnywhere, Category = "Tide|HazardField|Spawn", meta = ( EditCondition = "bPreventStrikeOverlap", ClampMin = "0.0" ) )
	float StrikeOverlapMargin = 0.0f;
	// BeginPlay で自動開始するか
	UPROPERTY( EditAnywhere, Category = "Tide|HazardField|Spawn" )
	bool bAutoStart = true;

	// --- 地面トレース ---
	// 抽選 XY からこの高さ上空を起点に真下へトレースする（cm）
	UPROPERTY( EditAnywhere, Category = "Tide|HazardField|Ground", meta = ( ClampMin = "0.0" ) )
	float TraceHeight = 2000.0f;
	// 真下トレースの最大距離（cm）
	UPROPERTY( EditAnywhere, Category = "Tide|HazardField|Ground", meta = ( ClampMin = "0.0" ) )
	float TraceDistance = 5000.0f;

	// --- プレイヤー中心の範囲落とし ---
	// ON で、ランダム抽選と並行してプレイヤーがエリア内にいる間プレイヤー中心にも Strike を降らせる
	UPROPERTY( EditAnywhere, Category = "Tide|HazardField|PlayerSpawn" )
	bool bEnablePlayerCenteredSpawn = false;
	// プレイヤー中心落としの発生間隔（秒）。実際は ±PlayerIntervalJitter で揺らす
	UPROPERTY( EditAnywhere, Category = "Tide|HazardField|PlayerSpawn", meta = ( EditCondition = "bEnablePlayerCenteredSpawn", ClampMin = "0.01" ) )
	float PlayerSpawnInterval = 2.0f;
	UPROPERTY( EditAnywhere, Category = "Tide|HazardField|PlayerSpawn", meta = ( EditCondition = "bEnablePlayerCenteredSpawn", ClampMin = "0.0" ) )
	float PlayerIntervalJitter = 0.5f;
	// プレイヤー中心からのブレ半径（cm）
	UPROPERTY( EditAnywhere, Category = "Tide|HazardField|PlayerSpawn", meta = ( EditCondition = "bEnablePlayerCenteredSpawn", ClampMin = "0.0" ) )
	float PlayerAreaRadius = 300.0f;
	// プレイヤー中心落とし専用の Strike 候補（複数指定可・Weight の重み付き乱数で毎回選ぶ）。
	// 空（または全候補が無効）ならランダム抽選と同じ StrikeOptions にフォールバックする
	UPROPERTY( EditAnywhere, Category = "Tide|HazardField|PlayerSpawn", meta = ( EditCondition = "bEnablePlayerCenteredSpawn" ) )
	TArray<FAreaHazardStrikeOption> PlayerStrikeOptions;
	// ON で、抽選した落下点がカメラに映っている（画面内に収まる）ことを要求する。
	// 画面外の候補は棄却して再抽選する（MaxSampleAttempts 回試して全滅ならその回はスキップ）
	UPROPERTY( EditAnywhere, Category = "Tide|HazardField|PlayerSpawn", meta = ( EditCondition = "bEnablePlayerCenteredSpawn" ) )
	bool bPlayerSpawnRequireOnScreen = true;
	// 画面内判定に使う矩形（ビューポート中央基準・画面サイズに対する割合）。
	// 1.0 で画面ぴったり、小さくするほど中央寄りを要求。どちらか 0 以下でフィルタ無効（常に画面内扱い）
	UPROPERTY( EditAnywhere, Category = "Tide|HazardField|PlayerSpawn", meta = ( EditCondition = "bEnablePlayerCenteredSpawn && bPlayerSpawnRequireOnScreen", ClampMax = "1.0" ) )
	float PlayerSpawnScreenFractionX = 0.9f;
	UPROPERTY( EditAnywhere, Category = "Tide|HazardField|PlayerSpawn", meta = ( EditCondition = "bEnablePlayerCenteredSpawn && bPlayerSpawnRequireOnScreen", ClampMax = "1.0" ) )
	float PlayerSpawnScreenFractionY = 0.9f;

	// --- 除外 ---
	// 手動指定の除外（自動収集に追加。エリア外に中心があり縁で重なる除外などに使う）
	UPROPERTY( EditAnywhere, Category = "Tide|HazardField|Exclusion" )
	TArray<TObjectPtr<AAreaHazardExclusion>> ManualExclusions;

	// --- パフォーマンス（遠距離カリング）---
	// 作動半径（cm）。プレイヤーがこの球の外にいる間は Strike のスポーン処理を止める（0 以下で無効）。
	// エリアサイズには依存しないので、エリアより小さくするとエリア内の遠い端でもスポーンが止まる点に注意
	UPROPERTY( EditAnywhere, Category = "Tide|HazardField|Performance", meta = ( ClampMin = "0.0" ) )
	float ActivationCullRadius = 30000.0f;

	// ON で作動範囲（ActivationCullRadius の球）を可視化する。
	// エディタ（非PIE）では ActivationCullVis のワイヤー球を常時表示、実行時は作動中=緑・スキップ中=赤の DrawDebug
	UPROPERTY( EditAnywhere, Category = "Tide|HazardField|Performance" )
	bool bDrawActivationCullRange = false;

	// 作動範囲のエディタ可視化用シェイプ（見た目専用・コリジョン無し）。OnConstruction で半径・可視性を反映。
	// ゲーム中は実行時の色分け DrawDebug に任せるため隠す
	UPROPERTY( VisibleAnywhere, Category = "Tide|HazardField|Performance" )
	TObjectPtr<USphereComponent> ActivationCullVis;

	// --- デバッグ ---
	UPROPERTY( EditAnywhere, Category = "Tide|HazardField|Debug" )
	bool bDebugDraw = true;

private:
	bool bRunning = false;
	float SpawnTimer = 0.0f;
	float PlayerSpawnTimer = 0.0f;	// ランダム抽選とは独立に進む

	TArray<TWeakObjectPtr<AAreaHazardExclusion>> Exclusions;	// BeginPlay で収集
	TArray<TWeakObjectPtr<AAreaHazardStrike>> ActiveStrikes;	// 上限管理・掃除用
	TWeakObjectPtr<APawn> CachedPlayerPawn;					// 距離ゲート用。無効時のみ取り直す
};
