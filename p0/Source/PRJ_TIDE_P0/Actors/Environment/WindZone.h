// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/EngineTypes.h"
#include "PRJ_TIDE_P0/Actors/Hazard/HazardArea.h"
#include "WindZone.generated.h"

class UBoxComponent;
class USphereComponent;
class UArrowComponent;
class UNiagaraComponent;
class UNiagaraSystem;
class AWindZoneExclusion;
class AWindSource;
class APawn;

/**
 * 風向きの決め方。
 * Fixed      : 従来通りアクター前方（+X）の固定方向
 * FromSource : WindSourceActor の位置から対象への方向（放射状）。未設定なら Fixed にフォールバック
 */
UENUM( BlueprintType )
enum class EWindDirectionMode : uint8
{
	Fixed		UMETA( DisplayName = "固定方向（アクター前方）" ),
	FromSource	UMETA( DisplayName = "発生源基準（放射状）" ),
};

/**
 * 向かい風ゾーン。エリア内の IWindAffectable へ「水平方向の風（押し出し加速度）」を毎フレーム配る。
 * ダメージは持たない環境ギミックで、竜巻（ASlidePassiveTornado）と同じ Enter/Tick/Exit 配布方式。
 *
 * 受け手側が状態別の抵抗倍率を掛けて解釈するため（IWindAffectable の思想）、通常は押されて前進不可・
 * チャージダッシュ中は軽減されて前進できる、を作れる。抵抗倍率はプレイヤー側
 * （TidePlayerParamDataAsset::WindPushScale*）、風の強さは本アクター側に持つ。
 */
UCLASS()
class PRJ_TIDE_P0_API AWindZone : public AActor
{
	GENERATED_BODY()

public:
	AWindZone();

protected:
	virtual void BeginPlay() override;
	virtual void Tick( float DeltaTime ) override;
	virtual void EndPlay( const EEndPlayReason::Type EndPlayReason ) override;
	virtual void OnConstruction( const FTransform& Transform ) override;

	void ApplyWind( float DeltaTime );		// 範囲の出入りで Enter/Exit を発火する
	void ReleaseAllWindTargets();			// 影響中の全対象へ Exit を通知して解放する（破棄時）

	// --- 風エフェクト（NS_OneWayWind）---
	void SpawnWindFlowVFX();				// BeginPlay で一括生成し、以降はループ再生し続ける
	void TrySpawnWindFlowVFX();				// なるべく既存と重ならない位置を抽選して 1 つ生成
	FVector RandomWindFlowLocalPos() const;	// Box: 各軸 ±Extent / Sphere: 半径内一様
	void UpdatePlayerFrontVFX( float DeltaTime );	// 被風中のみ再生・非被風で Deactivate

	// --- デバッグ描画（bDebugDraw 時のみ）---
	void DrawDebugFlowArrows() const;

	// 幅方向に並べた風向きレーンを風力倍率（緑=フル〜赤=弱）で色分けし、減衰帯の内側境界を白線で示す
	void DrawDebugLateralGradient() const;
	void GenerateDebugFlowSeeds();			// 流れる矢印の配置（横位置・位相）をランダム生成

	// --- 無効エリア ---
	void CollectExclusions();				// 自エリア内のアクターを収集（＋手動指定を追加）
	bool IsInExclusion( const FVector& WorldPoint ) const;

	// Fixed＝アクター前方固定／FromSource＝発生源からの放射状。押し出し方向と遮蔽トレース方向が
	// 独立したモードを持つため共通化している
	FVector ComputeDirectionForMode( EWindDirectionMode Mode, const FVector& WorldPoint, bool bFlattenToHorizontal ) const;

	// 風は常に水平（仕組み自体が「水平方向の風」前提のため、発生源と高さが違っても水平成分のみ使う）
	FVector ComputeWindDirectionAt( const FVector& WorldPoint ) const;

	// 遮蔽トレースは高さ差も含む 3D 方向（発生源と高さが違ってもその角度どおりにトレースする）
	FVector ComputeOcclusionTraceDirectionAt( const FVector& WorldPoint ) const;

	// 左右幅に応じた風力倍率（0〜1）。中央はフル、両端は減衰。機能 OFF なら常に 1.0
	float ComputeLateralGradientScale( const FVector& WorldPoint ) const;

	// 露出割合（0=完全に遮蔽／1=完全露出）。TraceDir に直交する横方向に複数トレースし、
	// 遮った本数・遮蔽物までの距離で連続的に減衰させる
	float ComputeOcclusionExposure( const AActor* Target, const FVector& TraceDir ) const;

public:
	UPROPERTY( VisibleAnywhere, Category = "Tide|WindZone" )
	TObjectPtr<USceneComponent> SceneRoot;

	// 風向き（前方 +X）を示すエディタ用矢印
	UPROPERTY( VisibleAnywhere, Category = "Tide|WindZone" )
	TObjectPtr<UArrowComponent> WindArrow;

	// エディタで常時ワイヤー表示する可視化シェイプ（見た目専用・コリジョン無し）。Shape に応じて片方だけ表示
	UPROPERTY( VisibleAnywhere, Category = "Tide|WindZone" )
	TObjectPtr<UBoxComponent> BoxVis;
	UPROPERTY( VisibleAnywhere, Category = "Tide|WindZone" )
	TObjectPtr<USphereComponent> SphereVis;

	// --- エリア ---
	UPROPERTY( EditAnywhere, Category = "Tide|WindZone|Area" )
	EHazardAreaShape Shape = EHazardAreaShape::Box;
	UPROPERTY( EditAnywhere, Category = "Tide|WindZone|Area", meta = ( EditCondition = "Shape == EHazardAreaShape::Box", EditConditionHides ) )
	FVector BoxExtent = FVector( 500.0f, 500.0f, 300.0f );
	UPROPERTY( EditAnywhere, Category = "Tide|WindZone|Area", meta = ( EditCondition = "Shape == EHazardAreaShape::Sphere", EditConditionHides ) )
	float SphereRadius = 500.0f;

	// --- 風 ---
	// 風の押し出し速度（cm/秒。プレイヤー側の抵抗倍率が掛かる）。位置オフセットとして押されるので、通常歩行速度より大きいと押し戻される
	UPROPERTY( EditAnywhere, Category = "Tide|WindZone|Wind" )
	float WindPushSpeed = 1500.0f;

	// 風の押し出し方向の決め方（固定方向 / 発生源基準の放射状）。遮蔽トレースの向き（OcclusionDirectionMode）とは独立
	UPROPERTY( EditAnywhere, Category = "Tide|WindZone|Wind" )
	EWindDirectionMode WindDirectionMode = EWindDirectionMode::Fixed;

	// WindDirectionMode・OcclusionDirectionMode のどちらかが FromSource のときに使う発生源。
	// 未設定なら FromSource 指定分は Fixed 相当（アクター前方固定）にフォールバックする
	UPROPERTY( EditAnywhere, Category = "Tide|WindZone|Wind", meta = ( EditCondition = "WindDirectionMode == EWindDirectionMode::FromSource || OcclusionDirectionMode == EWindDirectionMode::FromSource", EditConditionHides ) )
	TObjectPtr<AWindSource> WindSourceActor;

	// --- 左右幅の風力グラデーション ---
	// ON で、エリアの左右幅（風向きに直交するローカル Y 軸）の両端で風力を弱める。中央はフル、端に近づくほど減衰
	UPROPERTY( EditAnywhere, Category = "Tide|WindZone|Wind|LateralGradient" )
	bool bEnableLateralGradient = false;

	// 左右の端からこの距離ぶんが減衰帯（cm）。端では LateralEdgeScale、そこから内側へ GradientWidth 進むとフルになる。
	// 0 以下でグラデーション無し（端まで一律フル）
	UPROPERTY( EditAnywhere, Category = "Tide|WindZone|Wind|LateralGradient", meta = ( EditCondition = "bEnableLateralGradient", EditConditionHides, ClampMin = "0.0" ) )
	float LateralGradientWidth = 300.0f;

	// 左右の端での風力倍率（0=端で無風／1=端でもフル）。端→内側へ GradientWidth かけて 1.0 まで線形に戻る
	UPROPERTY( EditAnywhere, Category = "Tide|WindZone|Wind|LateralGradient", meta = ( EditCondition = "bEnableLateralGradient", EditConditionHides, ClampMin = "0.0", ClampMax = "1.0" ) )
	float LateralEdgeScale = 0.0f;

	// --- 遮蔽 ---
	// ON で、対象の風上側に遮蔽物（壁等）があれば風を配らない
	UPROPERTY( EditAnywhere, Category = "Tide|WindZone|Occlusion" )
	bool bEnableOcclusion = true;
	// 遮蔽トレースの向きの決め方（固定方向 / 発生源基準の放射状）。WindDirectionMode（風の押し出し方向）とは独立に設定できる。
	// 例：風は固定方向で押すが、遮蔽判定だけ発生源（ファン等）からの向きで見たい、といった組み合わせが可能
	UPROPERTY( EditAnywhere, Category = "Tide|WindZone|Occlusion", meta = ( EditCondition = "bEnableOcclusion", EditConditionHides ) )
	EWindDirectionMode OcclusionDirectionMode = EWindDirectionMode::Fixed;
	// 遮蔽判定に使うトレースチャンネル
	UPROPERTY( EditAnywhere, Category = "Tide|WindZone|Occlusion", meta = ( EditCondition = "bEnableOcclusion", EditConditionHides ) )
	TEnumAsByte<ECollisionChannel> OcclusionTraceChannel = ECC_WorldStatic;
	// 対象の位置から風上へこの距離だけ遡ってトレースする（cm）。遮蔽物の手前まで届く距離を設定する
	UPROPERTY( EditAnywhere, Category = "Tide|WindZone|Occlusion", meta = ( EditCondition = "bEnableOcclusion", EditConditionHides, ClampMin = "0.0" ) )
	float OcclusionTraceDistance = 1000.0f;
	// 横方向（風向きに直交する軸）のサンプリング本数（体の何割が露出しているかの近似）。1 なら中心1本のみ
	UPROPERTY( EditAnywhere, Category = "Tide|WindZone|Occlusion", meta = ( EditCondition = "bEnableOcclusion", EditConditionHides, ClampMin = "1" ) )
	int32 OcclusionSampleCount = 3;
	// 遮蔽物までの距離が OcclusionTraceDistance ×この割合以内なら完全遮蔽（露出0）。
	// それを超えた分は OcclusionTraceDistance に向けて露出0→1へ線形に戻る（＝そこから先は遮蔽物の影響が徐々に無効化）
	UPROPERTY( EditAnywhere, Category = "Tide|WindZone|Occlusion", meta = ( EditCondition = "bEnableOcclusion", EditConditionHides, ClampMin = "0.0", ClampMax = "0.99" ) )
	float OcclusionFalloffStartRatio = 0.5f;
	// ON でサンプル別トレース（露出度に応じて緑〜赤のグラデーション）と合計露出率を毎フレーム描画する
	UPROPERTY( EditAnywhere, Category = "Tide|WindZone|Occlusion", meta = ( EditCondition = "bEnableOcclusion", EditConditionHides ) )
	bool bDebugDrawOcclusionTrace = false;

	// --- 風エフェクト（NS_OneWayWind）---
	// エリア内のランダムな箇所へ BeginPlay で一括配置し、以降ループ再生し続ける風の流れ。アクター前方（＝風向き）へ流す
	UPROPERTY( EditAnywhere, Category = "Tide|WindZone|VFX" )
	TObjectPtr<UNiagaraSystem> WindFlowVFX;

	// 配置する本数。BeginPlay でこの数だけランダム位置へ置き、ループで流し続ける（0＝生成しない）
	UPROPERTY( EditAnywhere, Category = "Tide|WindZone|VFX", meta = ( ClampMin = "0" ) )
	int32 WindFlowVFXMaxConcurrent = 8;

	// 生成位置どうしの最小間隔（cm）。なるべくこの距離を空けて抽選する（重なり回避）
	UPROPERTY( EditAnywhere, Category = "Tide|WindZone|VFX", meta = ( ClampMin = "0.0" ) )
	float WindFlowVFXMinSeparation = 300.0f;

	// 最小間隔を満たす位置を探す抽選リトライ上限（全て満たさなければ最も離れた候補を採用する）
	UPROPERTY( EditAnywhere, Category = "Tide|WindZone|VFX", meta = ( ClampMin = "1" ) )
	int32 WindFlowVFXMaxSampleAttempts = 8;

	// --- プレイヤー目の前エフェクト（被風中のにぎやかし） ---
	// ON で、プレイヤーが風の影響を受けている間だけプレイヤーの目の前に風エフェクトを出す（にぎやかしとは別枠）
	UPROPERTY( EditAnywhere, Category = "Tide|WindZone|VFX|PlayerFront" )
	bool bEnablePlayerFrontVFX = true;

	// プレイヤー目の前エフェクトのアセット。内容は同じなので未設定なら WindFlowVFX を流用する
	UPROPERTY( EditAnywhere, Category = "Tide|WindZone|VFX|PlayerFront", meta = ( EditCondition = "bEnablePlayerFrontVFX" ) )
	TObjectPtr<UNiagaraSystem> PlayerFrontVFX;

	// プレイヤー位置から風向きへ前方に出すオフセット（cm）。マイナスで風上側（向かってくる側）に出せる
	UPROPERTY( EditAnywhere, Category = "Tide|WindZone|VFX|PlayerFront", meta = ( EditCondition = "bEnablePlayerFrontVFX" ) )
	float PlayerFrontVFXForwardOffset = -600.0f;

	// プレイヤー位置からの高さオフセット（cm）
	UPROPERTY( EditAnywhere, Category = "Tide|WindZone|VFX|PlayerFront", meta = ( EditCondition = "bEnablePlayerFrontVFX" ) )
	float PlayerFrontVFXHeightOffset = 50.0f;

	// --- 無効エリア ---
	// 手動指定の無効エリア（自動収集に追加。エリア外に中心があり縁で重なる無効エリアなどに使う）
	UPROPERTY( EditAnywhere, Category = "Tide|WindZone|Exclusion" )
	TArray<TObjectPtr<AWindZoneExclusion>> ManualExclusions;

	// --- パフォーマンス（遠距離カリング）---
	// プレイヤーがゾーン境界（Box は対角長／Sphere は半径）からこの距離ぶん以上離れている間は、範囲内アクター走査・
	// 遮蔽トレース・前方 VFX 更新をスキップする（cm。0以下で無効）。ゲート外へ出た瞬間、影響中の対象を一度だけ解放する
	UPROPERTY( EditAnywhere, Category = "Tide|WindZone|Performance", meta = ( ClampMin = "0.0" ) )
	float ActivationCullMargin = 3000.0f;

	// ON で作動範囲（境界＋マージンの球）を可視化する。
	// エディタ（非PIE）では ActivationCullVis のワイヤー球を常時表示、実行時は作動中=緑・スキップ中=赤の DrawDebug
	UPROPERTY( EditAnywhere, Category = "Tide|WindZone|Performance" )
	bool bDrawActivationCullRange = false;

	// 作動範囲のエディタ可視化用シェイプ（見た目専用・コリジョン無し）。OnConstruction で半径・可視性を反映。
	// ゲーム中は実行時の色分け DrawDebug に任せるため隠す
	UPROPERTY( VisibleAnywhere, Category = "Tide|WindZone|Performance" )
	TObjectPtr<USphereComponent> ActivationCullVis;

	// --- デバッグ ---
	UPROPERTY( EditAnywhere, Category = "Tide|WindZone|Debug" )
	bool bDebugDraw = true;

	// 風向きへ疎らに流す矢印の本数（見た目のみ・ゲームプレイに影響なし）
	UPROPERTY( EditAnywhere, Category = "Tide|WindZone|Debug", meta = ( EditCondition = "bDebugDraw", EditConditionHides, ClampMin = "0" ) )
	int32 DebugFlowArrowCount = 12;

	// 流れる矢印の長さ（cm）
	UPROPERTY( EditAnywhere, Category = "Tide|WindZone|Debug", meta = ( EditCondition = "bDebugDraw", EditConditionHides, ClampMin = "1.0" ) )
	float DebugFlowArrowLength = 100.0f;

	// 流れる矢印の線の太さ
	UPROPERTY( EditAnywhere, Category = "Tide|WindZone|Debug", meta = ( EditCondition = "bDebugDraw", EditConditionHides, ClampMin = "0.0" ) )
	float DebugFlowArrowThickness = 2.0f;

	// 流れる矢印が風向きへ進む速度（cm/秒）。見た目専用で WindPushSpeed とは独立
	UPROPERTY( EditAnywhere, Category = "Tide|WindZone|Debug", meta = ( EditCondition = "bDebugDraw", EditConditionHides, ClampMin = "0.0" ) )
	float DebugFlowArrowSpeed = 1500.0f;

	// 横断面内での広がり（0=中心線上のみ／1=エリア横幅いっぱいまで）
	UPROPERTY( EditAnywhere, Category = "Tide|WindZone|Debug", meta = ( EditCondition = "bDebugDraw", EditConditionHides, ClampMin = "0.0", ClampMax = "1.0" ) )
	float DebugFlowArrowLateralSpread = 0.85f;

	// 矢印頭のサイズ（矢印の長さに対する比率）
	UPROPERTY( EditAnywhere, Category = "Tide|WindZone|Debug", meta = ( EditCondition = "bDebugDraw", EditConditionHides, ClampMin = "0.0", ClampMax = "1.0" ) )
	float DebugFlowArrowHeadSizeRatio = 0.35f;

	// 流れる矢印の色
	UPROPERTY( EditAnywhere, Category = "Tide|WindZone|Debug", meta = ( EditCondition = "bDebugDraw", EditConditionHides ) )
	FColor DebugFlowArrowColor = FColor( 0, 220, 255 );

private:
	TWeakObjectPtr<APawn> CachedPlayerPawn;	// 距離ゲート用。無効時のみ取り直す

	// 風の影響中の対象。遮蔽（露出0）で押し出しを止めている間はここから外れる
	TSet<TWeakObjectPtr<AActor>> WindAffectedActors;

	// エリア内（無効エリアを除く・遮蔽は無視）の対象。壊れ物などで遮蔽され
	// WindAffectedActors から外れていても、エリア内であればこちらには残る
	TSet<TWeakObjectPtr<AActor>> WindAreaActors;

	TArray<TWeakObjectPtr<AWindZoneExclusion>> Exclusions;	// BeginPlay で収集

	// EndPlay での Deactivate・重なり回避の位置参照に使う
	struct FWindFlowVFXInstance
	{
		TWeakObjectPtr<UNiagaraComponent> Component;
		FVector LocalPos = FVector::ZeroVector;	// 重なり回避の距離比較に使う
	};
	TArray<FWindFlowVFXInstance> WindFlowVFXInstances;

	UPROPERTY( Transient )
	TObjectPtr<UNiagaraComponent> PlayerFrontVFXComponent;

	// 流れる矢印 1 本ごとの配置シード（ローカル空間の横位置単位円座標・時間位相）
	struct FWindDebugFlowSeed
	{
		FVector2D LateralUnit = FVector2D::ZeroVector;
		float PhaseOffset = 0.0f;
	};
	TArray<FWindDebugFlowSeed> DebugFlowSeeds;
};
