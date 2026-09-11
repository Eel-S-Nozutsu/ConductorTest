// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "PRJ_TIDE_P0/Composites/Player/PlayerModules/SlidePassiveSubModule.h"

#include "SlidePassiveTornadoSubModule.generated.h"

class UNiagaraComponent;

// スライドパッシブ：竜巻。チャージ中に足元で円を描くと中心に印が出てから竜巻が発生する。
// 円の判定は累積旋回角度で緩く行い（厳密な交差判定はしない）、半径で小／大に分かれてアセット・LifeTime・
// ジャンプ倍率が変わる。
//
// 通常モードは円完成ごとに印→遅延後に竜巻、解除時まとめモードは完成時に印だけ出してリボンを継続し、
// チャージ解除時に溜めた円から順番に時間差で竜巻を発生させる
UCLASS()
class USlidePassiveTornadoSubModule : public USlidePassiveSubModule
{
	GENERATED_BODY()

public:
	virtual bool IsEnabled() const override;
	virtual void OnTrackingStarted() override;
	virtual void OnSampleAdded( const FVector& Foot, float SignedTurnDeltaDeg ) override;
	virtual void OnTrajectoryReset() override;
	virtual void OnTrackingStopped( bool bChargeReleased ) override;
	virtual void OnUpdate( float DeltaTime ) override;

	// まだ生きている印等の追跡 VFX を明示破棄する保険
	virtual void BeginDestroy() override;

	void DebugSpawnTornadoInFront( bool bIsLarge, float ForwardDistance );	// 足元前方へ 1 つ発生させる

#if !UE_BUILD_SHIPPING
	virtual void DrawDebugImGui() override;
#endif

private:
	// 軌跡区間を円とみなしたときの幾何量（判定はせず算出だけ。デバッグ表示にも使う）
	struct FCircleFit
	{
		FVector Center = FVector::ZeroVector;
		float Radius = 0.0f;			// 中心からの平均距離
		float ClosureRatio = 0.0f;		// 始点〜終点の距離 ÷ 半径（小さいほど閉じている）
		float RoundnessRatio = 0.0f;	// 半径の平均絶対偏差 ÷ 半径（小さいほど真円に近い）
	};
	bool EvaluateCircleFit( int32 StartIndex, FCircleFit& OutFit ) const;	// 点数不足・半径ゼロなら false
	bool DetectCircleClosure( int32 StartIndex, FVector& OutCenter, float& OutRadius ) const;

	// 直近で一周ぶん旋回した区間の開始インデックス（始点に依存せず途中の線でも検出）。なければ INDEX_NONE
	int32 FindLoopStartIndex() const;

	// 円として成立する最小半径。小をマスクしているときは大の閾値を下限として扱う
	float GetEffectiveMinCircleRadius() const;
	float ComputeCircleScale( float Radius, bool bIsLarge ) const;	// 半径連動 OFF なら大小の固定値

	void OnCircleCompleted( const FVector& Center, float Radius );

	// 以下 2 つの Override 引数は、負なら通常算出（大小の固定値）を使う
	UNiagaraComponent* SpawnMarkEffect( const FVector& Center, bool bIsLarge, float LifeTimeOverride = -1.0f, float ScaleOverride = -1.0f );
	void SpawnTornado( const FVector& Center, bool bIsLarge, float ScaleOverride = -1.0f );

	void MaybeTriggerBirdEscort( const FVector& Center );	// セッション最初の竜巻に限り起動する
	void UpdatePendingTornadoes( float DeltaTime );
	void ReleaseDeferredTornadoes();	// 溜めた全ての円を順番に時間差で竜巻化する予約に積む

	// 蓄積印を竜巻が出るまで OUT させず HOLD 帯に保持し続ける。正規化年齢方式の印は LifeTime を巨大にすると
	// IN で止まるため、OUT 手前まで来たら再延長して HOLD へ戻す（Gust の足元印と同じ方式）
	void UpdateDeferredMarkPersistence();

	// 印やフォールバック竜巻の Niagara は消滅をアセット任せにしているため、kill を配線し損ねると残り続ける。
	// Lifetime を渡して登録し、UpdateVFXCleanups が期限切れで DestroyComponent する保険
	void RegisterVFXCleanup( UNiagaraComponent* Comp, float Lifetime );
	void UpdateVFXCleanups( float DeltaTime );
	void DestroyAllTrackedVFX();	// 保留中・追跡中の全 VFX を即時破棄する

	void ResetTrajectory();

#if !UE_BUILD_SHIPPING
	void DrawDebugTrajectory() const;
#endif

private:
	TArray<FVector> Trajectory;			// 足元基準の軌跡

	// 各軌跡点までの符号付き累積旋回角度（Trajectory と同じ要素数。区間の旋回量を差分で取れる）
	TArray<float> CumulativeTurnDeg;
	float AccumulatedTurnDeg = 0.0f;	// 現在までの累積

	// 「1 個目の竜巻」だけ演出するためのフラグ。OnTrackingStarted でリセットする
	bool bTornadoEscortTriggered = false;

	// --- 完成フィードバック（デバッグ円の点滅表示用）---
	float CompletionFlashTimer = 0.0f;
	FVector LastCompletedCenter = FVector::ZeroVector;
	float LastCompletedRadius = 0.0f;
	float LastCompletedScale = 0.0f;
	bool bLastCompletedIsLarge = false;

	// 印発生後、遅延して竜巻を出すための予約。複数の円を連続で描いた場合にも対応するため配列で持つ
	struct FPendingTornado
	{
		FVector Center = FVector::ZeroVector;
		bool bIsLarge = false;
		float Scale = -1.0f;	// 負なら大小の固定値
		float TimeRemaining = 0.0f;

		// 解除時まとめモードでこの竜巻が出るまで残す蓄積印（通常モードでは未使用）。
		// 竜巻発生時に破棄せず、残り寿命を竜巻に合わせて延長する
		TWeakObjectPtr<UNiagaraComponent> MarkComp;
		float MarkSpawnTime = 0.0f;
	};
	TArray<FPendingTornado> PendingTornadoes;

	// 解除時まとめモードで円完成ごとに溜める円。ReleaseDeferredTornadoes が順番に竜巻化する
	struct FDeferredCircle
	{
		FVector Center = FVector::ZeroVector;
		bool bIsLarge = false;
		float Scale = -1.0f;						// 負なら大小の固定値
		TWeakObjectPtr<UNiagaraComponent> MarkComp;	// 解除まで残す印
		float MarkSpawnTime = 0.0f;
		float MarkLifeTime = 0.0f;					// HOLD 保持の再延長で更新される
	};
	TArray<FDeferredCircle> DeferredCircles;

	// アセットが User.LifeTime で kill しなくても、締切（実時間）で確実に DestroyComponent する保険
	struct FTrackedVFX
	{
		TWeakObjectPtr<UNiagaraComponent> Comp;
		float TimeRemaining = 0.0f;	// 実時間の締切。0 以下で破棄する
	};
	TArray<FTrackedVFX> TrackedVFX;

#if !UE_BUILD_SHIPPING
	bool bDebugDrawTrajectory = false;
#endif
};
