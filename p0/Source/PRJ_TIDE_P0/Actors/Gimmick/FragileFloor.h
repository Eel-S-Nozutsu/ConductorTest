// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PRJ_TIDE_P0/Interfaces/Environment/IGimmickResettable.h"
#include "FragileFloor.generated.h"

class UProceduralMeshComponent;
class UMaterialInterface;

UENUM(BlueprintType)
enum class EFragileCellState : uint8
{
	Cracked,
	Triggered,
	Collapsed,
};

// Voronoiセル1つ。崩落・囲い込みは隣接グラフ上で処理するのでセル形状に依存しない
USTRUCT()
struct FFragileCell
{
	GENERATED_BODY()

	FVector2D Site = FVector2D::ZeroVector;
	TArray<FVector2D> Polygon;
	TArray<int32> Neighbors;

	// 床の縁に接するセル＝囲い込みフラッドフィルの種
	bool bBoundary = false;

	// 崩落時に作り直すタイル(実行時のみ有効)
	int32 TileIndex = 0;

	EFragileCellState State = EFragileCellState::Cracked;
	float CollapseTimer = 0.0f;

	// 警告色へのフェード進捗(0=通常 / 1=警告)。頂点カラーのRへ流す
	float WarnBlend = 0.0f;

};

/**
 * 刺激で割れて崩落する床。囲われた(外周から到達できない)立ちセルはまとめて崩落する。
 */
UCLASS()
class PRJ_TIDE_P0_API AFragileFloor : public AActor, public IGimmickResettable
{
	GENERATED_BODY()

public:

	AFragileFloor();

	// 各刺激源(移動・攻撃・敵弾)から崩落させたい座標を渡す
	UFUNCTION(BlueprintCallable, Category = "Tide|Gimmick")
	void NotifyStimulusAtLocation(const FVector& WorldLocation);

	// 崩落した床を全て立ち状態へ戻す (ギミックルームへの入りなおしでやり直せるようにする)
	UFUNCTION(BlueprintCallable, Category = "Tide|Gimmick")
	void ResetFloor();

	// IGimmickResettable
	virtual void ResetGimmick_Implementation() override { ResetFloor(); }

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaTime) override;

	// エディタ配置時に床をプレビュー表示する
	virtual void OnConstruction(const FTransform& Transform) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

public:

	UPROPERTY(VisibleAnywhere, Category = "Tide|Gimmick")
	TObjectPtr<UProceduralMeshComponent> FloorMesh;

	// 床全体のサイズ
	UPROPERTY(EditAnywhere, Category = "Tide|Gimmick")
	FVector2D FloorExtent = FVector2D(2000.0f, 2000.0f);

	// セルの目安サイズ(塊の平均サイズ)
	UPROPERTY(EditAnywhere, Category = "Tide|Gimmick", meta = (ClampMin = "50.0"))
	float CellSize = 200.0f;

	// セルの不定形さ(0=格子 / 1=ランダム)
	UPROPERTY(EditAnywhere, Category = "Tide|Gimmick", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SiteJitter = 0.7f;

	// ひびの溝幅
	UPROPERTY(EditAnywhere, Category = "Tide|Gimmick", meta = (ClampMin = "0.0"))
	float CellGap = 12.0f;

	// ひびの溝の深さ
	UPROPERTY(EditAnywhere, Category = "Tide|Gimmick", meta = (ClampMin = "0.0"))
	float CrackDepth = 50.0f;

	// 床と塊の厚み
	UPROPERTY(EditAnywhere, Category = "Tide|Gimmick", meta = (ClampMin = "10.0"))
	float Thickness = 100.0f;

	// ひび模様のシード
	UPROPERTY(EditAnywhere, Category = "Tide|Gimmick")
	int32 RandomSeed = 12345;

	// セル数の上限。超えるとCellSizeを自動で粗くして負荷を抑える(コリジョンのクック破綻防止)
	UPROPERTY(EditAnywhere, Category = "Tide|Gimmick", meta = (ClampMin = "1"))
	int32 MaxCellCount = 20000;

	// 崩落時に作り直すメッシュの分割単位。小さいほど崩落時の再クックが軽いが、
	// コンポーネント数が増える
	UPROPERTY(EditAnywhere, Category = "Tide|Gimmick", meta = (ClampMin = "500.0"))
	float TileSize = 2000.0f;

	// 刺激から崩落までの時間
	UPROPERTY(EditAnywhere, Category = "Tide|Gimmick", meta = (ClampMin = "0.0"))
	float CollapseDelay = 1.0f;

	// 囲い込み崩落の1セルごとの時間差
	UPROPERTY(EditAnywhere, Category = "Tide|Gimmick", meta = (ClampMin = "0.0"))
	float EnclosureStagger = 0.04f;

	// 崩落待ちセルが警告色へ変わりきるまでの秒数(0で即時)
	// ※色そのものはマテリアル側。ここは頂点カラーRのフェード速度だけを決める
	UPROPERTY(EditAnywhere, Category = "Tide|Gimmick", meta = (ClampMin = "0.0"))
	float WarnFadeSeconds = 0.25f;

	// 落下片の寿命
	UPROPERTY(EditAnywhere, Category = "Tide|Gimmick", meta = (ClampMin = "0.1"))
	float ChunkLifeSpan = 4.0f;

	// 崩落時に塊へ与える下向き初速
	UPROPERTY(EditAnywhere, Category = "Tide|Gimmick")
	float ChunkDownImpulse = 300.0f;

	// 崩落セルの上でスリープ中の物理ボディを起こす高さ。静止した剛体は支えの床を消しても
	// 自動では起きないため、崩落セル直上をこの高さ分だけ探ってボディを叩き起こす
	UPROPERTY(EditAnywhere, Category = "Tide|Gimmick")
	float WakeBodiesHeight = 200.0f;

	UPROPERTY(EditAnywhere, Category = "Tide|Gimmick")
	TObjectPtr<UMaterialInterface> FloorMaterial;

	// 移動で刺激を加える(歩いた足元のセルを崩す)
	UPROPERTY(EditAnywhere, Category = "Tide|Gimmick|Stimulus")
	bool bMovementStimulus = true;

	// 攻撃で刺激を加える
	UPROPERTY(EditAnywhere, Category = "Tide|Gimmick|Stimulus")
	bool bAttackStimulus = true;

	// 移動とみなす速度
	UPROPERTY(EditAnywhere, Category = "Tide|Gimmick|Stimulus")
	float MovementSpeedThreshold = 50.0f;

	// 1回の刺激で崩す半径(0＝足元の1セルのみ)
	UPROPERTY(EditAnywhere, Category = "Tide|Gimmick|Stimulus")
	float StimulusRadius = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Tide|Gimmick|Debug")
	bool bDrawDebug = false;

	// エディタでひび割れをフル表示する(ひび模様の確認用)
	// 巨大なメッシュを抱えてD&D・保存が重くなるため、配置調整中はオフ推奨(外形の板だけ表示)
	UPROPERTY(EditAnywhere, Category = "Tide|Gimmick|Debug")
	bool bPreviewCracksInEditor = false;

private:

	void GenerateVoronoi();

	// 隣接エッジ頂点を一致させて穴・重複面ノイズを防ぐ
	void WeldCellVertices();

	void BuildSiteHash();
	int32 NearestSite(const FVector2D& LocalXY, int32 Exclude = INDEX_NONE) const;

	// エディタプレビュー(FloorMesh 1枚・当たりを焼かない)
	void BuildPreviewMesh();

	// 外形だけの箱プレビュー(既定。フルのひび表示はメッシュが巨大でD&D・保存が重い)
	void BuildPreviewBoxMesh();

	// 実行時のタイルPMC群を生成して床を構築する
	void InitRuntimeTiles();

	// 実行時タイルPMC群を解体(デリゲート解除＋破棄)。ストリーミングの
	// アウト・再入でOnComponentHitのスパースデリゲート束縛が残留するのを防ぐ
	void TeardownRuntimeTiles();

	// タイル内の立ちセルだけでメッシュ・コリジョンを作り直す
	void RebuildTile(int32 TileIdx);

	// 見た目セクションの頂点カラーだけ差し替える(コリジョンは触らない)
	void UpdateTileColors(int32 TileIdx);

	// セル状態を頂点カラーへ詰める。R=警告フェード / G=セルごとの位相オフセット
	FLinearColor MakeCellMask(const FFragileCell& Cell) const;

	void AppendCellVisual(const FFragileCell& Cell, TArray<FVector>& Verts, TArray<int32>& Tris, TArray<FVector>& Normals, TArray<FVector2D>& UVs, TArray<FLinearColor>& Colors) const;
	void AppendCellCollision(const FFragileCell& Cell, TArray<FVector>& Verts, TArray<int32>& Tris) const;

	int32 LocationToCell(const FVector& WorldLocation) const;
	void TriggerCell(int32 Index, float Delay);
	void TriggerCellsInRadius(const FVector& WorldLocation, float Radius);
	void CollapseCell(int32 Index);

	// 崩落セル直上のスリープ中の物理ボディを起こす(支えが消えても自動では落ちないため)
	void WakeBodiesOverCell(const FVector& WorldCenter, const FVector2D& HalfExtentXY);

	void RunEnclosureCheck();
	void DrawDebugCells() const;

	// 敵弾が床に着弾したらその位置のセルを崩す
	UFUNCTION()
	void OnFloorHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	UPROPERTY()
	TObjectPtr<UMaterialInterface> DefaultMaterial;

	TArray<FFragileCell> Cells;

	// エディタプレビュー再生成の判定用(生成パラメータの署名)。移動では作り直さない
	FString LastPreviewSignature;

	TArray<TArray<int32>> SiteBuckets;
	int32 HashDimX = 1;
	int32 HashDimY = 1;

	bool bEnclosureDirty = false;

	// 崩落のたびに床全体を再クックするとFPSが落ちるため、
	// タイル別PMCに分けて崩落タイルだけ作り直す
	UPROPERTY(Transient)
	TArray<TObjectPtr<UProceduralMeshComponent>> TileMeshes;

	TArray<TArray<int32>> TileCellIndices;

	// 崩落があったタイル → フレーム末にまとめて作り直す
	TSet<int32> DirtyTiles;

	// 色だけ変わったタイル → フレーム末に見た目セクションだけ更新する
	TSet<int32> ColorDirtyTiles;

	int32 TileDimX = 1;
	int32 TileDimY = 1;

#if WITH_EDITOR
	// プロパティをドラッグ編集中か(確定までフル再生成を抑止)
	bool bInteractiveEdit = false;
#endif

	TWeakObjectPtr<class ATidePlayerCharacter> CachedPlayer;

};
