// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "PRJ_TIDE_P0/Composites/Player/PlayerModules/TidePlayerModule.h"

#include "SlidePassivePlayerModule.generated.h"

class ATidePlayerCharacter;
class UNiagaraComponent;
class USlidePassiveSubModule;

// 切り離してフェードアウト中の旧トレイルリボン。印完成で差し替えるとき、旧リボンは即破棄せずここへ移し、
// 新トレイルと並行して薄く消していく
USTRUCT()
struct FSlidePassiveFadingTrail
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UNiagaraComponent> VFX = nullptr;

	float Timer = 0.0f;
};

// スライドパッシブ。チャージ中に足元で図形を描くと効果が発動する。検出条件と効果は機能ごとに
// USlidePassiveSubModule 派生へ分離し、本モジュールは共有処理に徹する：
//   ・チャージ中（＋チャージダッシュ猶予）だけ足元軌跡をサンプリングし、符号付き旋回量を算出する
//   ・そのサンプルを各サブモジュールへ配信する（検出は各サブモジュールが自前で行う）
//   ・共有トレイルリボンの管理と、完成フラッシュ／フェードの提供
//
// サブモジュールは 円→竜巻 の Tornado と、S字→風まとい＋ギアMAX＋次チャージアクションのバフ の Gust
UCLASS()
class PRJ_TIDE_P0_API USlidePassivePlayerModule : public UTidePlayerModule
{
	GENERATED_BODY()

public:
	virtual void Initialize( ATidePlayerCharacter* InOwner ) override;
	virtual void OnModuleUpdate( float DeltaTime ) override;

	// --- サブモジュールから呼ばれる共有トレイル演出 API ---
	void SpawnTrailVFX();

	// 完成演出の共通入口。通常は即・赤フラッシュだが、解除時まとめモードならリボンを継続させ、
	// チャージ解除時にまとめて 1 回だけフラッシュする
	void NotifyFigureCompleted();

	void BeginTrailFlash();		// 赤フラッシュ → フェードアウト → 再スポーンを即開始する低レベル API
	void SetTrailColor( FLinearColor Color );

	// 完成インターバル中（フラッシュ〜フェードアウト完了まで）か。この間はサンプリングを止める
	bool IsTrailCompletionLocked() const;

	bool IsTracking() const { return bIsTracking; }
	void DebugSpawnTornadoInFront( bool bIsLarge, float ForwardDistance );	// 竜巻サブモジュールへ依頼する

#if !UE_BUILD_SHIPPING
public:
	void DrawDebugImGui();
#endif

private:
	// --- 軌跡のサンプリング ---
	void StartTracking();
	void StopTracking( bool bChargeReleased );
	void UpdateTracking( float DeltaTime );
	void ResetSampling();
	void DispatchSampleToSubModules( const FVector& Foot, float SignedTurnDeltaDeg );
	void NotifySubModulesTrajectoryReset();

	// --- トレイルリボン ---
	void BeginTrailFadeOut();	// スライド終了時：Alpha 1→0 後に破棄

	// 解除時まとめモードの完成演出。明るくしてから通常カラーと「今の明るさの半分」を行き来してチカチカし、
	// チャージ解除まで持続させる（リボン継続。フェードアウト・再スポーン・サンプリング停止はしない）
	void BeginTrailCompletionGlow();

	// 新色演出版。緑 → 赤へ遷移して解除まで持続させる（サンプリングも止めない＝続けて図形を描ける）
	void BeginTrailCompletionRed();
	void BeginTrailReleaseWhiteFade();	// 解除時：赤 → 白へ遷移してからフェードアウトして破棄

	// 新色演出を使うか。専用検証フラグ ON かつ解除時まとめモード ON のときだけ有効
	bool UseNewTrailColorScheme() const;
	void UpdateTrailVFX( float DeltaTime );

	void BeginTrailCompletionCooldown();	// フラッシュ後、トレイルを出さない期間へ入る
	void EndTrailCompletionCooldown();		// 追跡中なら軌跡をリセットして再スポーンする
	float GetTrailCompletionCooldownTime() const;	// DA 未設定時は 0＝即再スポーン

	// 旧リボンを発生停止＋切り離してフェードアウト側へ移す（即破棄だとパッと消えて見えるため。
	// 新トレイルはフェードを待たず即開始できる）
	void DetachTrailForFadeOut();
	void UpdateFadingTrailVFX( float DeltaTime );

	// スライド中は User.LifeTime を巨大固定にして全セグメントを表示させているので、終了時は LifeTime は
	// 触らず Color.A を 1→0 にしてリボン全体を一様にフェードアウトさせてから破棄する
	void StartTrailEndFade();

private:
	// CompletionBrighten/Flicker＝印完成の演出（光らせてからチカチカし、解除または次の完成まで持続）／
	// CompletionCooldown＝完成フラッシュ後のトレイル非表示期間／EndFade＝スライド終了の畳み消し／
	// NewToRed・NewHoldRed＝新色演出の「印完成で赤・解除まで持続」／NewToWhite・NewWhiteFade＝同「解除で白フェード」
	enum class ETrailColorState : uint8 { Idle, FlashToRed, FlashReturn, CompletionCooldown, CompletionBrighten, CompletionFlicker, EndFade,
		NewToRed, NewHoldRed, NewToWhite, NewWhiteFade };

	UPROPERTY()
	TObjectPtr<UNiagaraComponent> TrailVFX;

	// 新トレイルと並行してフェードアウト中の旧トレイル
	UPROPERTY()
	TArray<FSlidePassiveFadingTrail> FadingTrails;

	ETrailColorState TrailColorState = ETrailColorState::Idle;
	float TrailColorTimer = 0.0f;

	// 解除時に 1 回だけ完成演出を出すための予約
	bool bPendingReleaseFlash = false;

private:
	// 各機能のサブモジュール（検出＋効果を自己完結で持つ）
	UPROPERTY( Transient )
	TArray<TObjectPtr<USlidePassiveSubModule>> SubModules;

	bool bIsTracking = false;

	// チャージダッシュ派生後も判定を残す猶予（チャージ中は満タンに保ち、派生中に消化する）
	float ChargeDashTrackGraceTimer = 0.0f;
	bool bInChargeDashGrace = false;	// 猶予中の検出を許さないサブモジュールへの配信ゲート

	// --- サンプリング状態（足元基準）---
	FVector LastSampleLoc = FVector::ZeroVector;
	FVector LastHeading = FVector::ZeroVector;
	bool bHasLastHeading = false;
	bool bHasFirstSample = false;
};
