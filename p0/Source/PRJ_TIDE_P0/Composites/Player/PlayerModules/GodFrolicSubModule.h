// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "PRJ_TIDE_P0/Composites/Player/PlayerModules/GodActionSubModule.h"

#include "GodFrolicSubModule.generated.h"

class ATideGodBird;

// 神技「戯れ」。発動で鳥を都度スポーンし、一定時間だけ自律で飛び回って近くの敵へ範囲ダメージを与える。
// プレイヤーは短いキャストモーションを流すだけで即・自由行動へ戻り、スローは掛けない（導きと同じ撃ちっぱなし型）。
// 鳥の移動・寿命・攻撃は本サブモジュールが毎フレーム駆動する
UCLASS()
class UGodFrolicSubModule : public UGodActionSubModule
{
	GENERATED_BODY()

public:
	virtual bool IsEnabled() const override { return true; }
	virtual void OnModuleUpdate( float DeltaTime ) override;

	// ゲージ満タン確認は傘側で済ませてから呼ぶ。InLockedTargets は構え中にロックオンしていた敵で、
	// 優先的に巡回する（空なら最寄りを狙う）
	void Execute( const TArray<AActor*>& InLockedTargets );

	bool IsActive() const { return bActive; }	// 鳥の自律攻撃が進行中か

	// 発動モーション（ST→ED）の再生中か。鳥の寿命とは独立し、ED が終わると false＝プレイヤー解放
	bool IsPlayerCasting() const { return bActive && !bCastEnded; }

#if !UE_BUILD_SHIPPING
	virtual void DrawDebugImGui() override;
#endif

private:
	enum class EBirdPhase : uint8
	{
		Dive,		// 対象へ突入中
		Pullout,	// 当てた直後の離脱中（次の突入へ入り直すための助走）
		Wait,		// 対象が居らずその場で待機中（探索は続け、見つかれば Dive へ戻る）
		Return,		// プレイヤーの元へ帰還中（以降は敵を探さず、着いたら終了）
	};

	// 当てたら離脱へ、対象が居なければ待機し、待機が尽きたら帰還へ移す
	void UpdateDive( float DeltaTime );
	void BeginPullout();	// 抜ける向きと基準対象を決めて Pullout へ

	// 基準対象から GodFrolicPulloutDistance ぶん離れたら、次の対象を選んで突入へ戻す
	void UpdatePullout( float DeltaTime );

	// 進行方向を DesiredDir へ旋回上限ぶんだけ寄せ、その方向へ Step 進めて向きも合わせる
	void MoveAlongFlyDirection( ATideGodBird* Bird, const FVector& DesiredDir, float Step, float DeltaTime );

	void BeginReturn();		// 縮小補間の基準として開始時の距離・スケールを覚える
	void UpdateReturn( float DeltaTime );	// 飛んで戻りつつ元サイズへ縮み、着いたら片付ける

	FVector GetHomeLocation() const;	// お供位置。スポーン位置と帰り先で共用する

	AActor* FindNearestEnemy( const FVector& From, float Range ) const;	// 敵対＋被ダメージ可能のみ
	AActor* PickNextTarget();	// ロックオン対象があれば順番に巡回、無ければ最寄りの敵
	void ApplyAttackDamage();	// 鳥の現在地中心の範囲ダメージ（同一対象は再アーム間隔ごと）

	// bPlayDisappearVFX=false なら消失エフェクトを出さない（帰還完了＝そのまま戻ってきた締め用）
	void EndFrolic( bool bPlayDisappearVFX = true );

	// 鳥は破棄されるためアタッチではなくワールド位置へ出す（破棄後も残って再生される）
	void SpawnFrolicBirdVFX( const FName& NiagaraTag, const FVector& WorldLoc );

private:
	bool bActive = false;
	float Elapsed = 0.0f;

	TWeakObjectPtr<ATideGodBird> FrolicBird;			// 都度スポーン。寿命で自壊
	TArray<TWeakObjectPtr<AActor>> LockedTargets;		// 優先的に巡回する対象
	int32 LockedCursor = 0;								// ロックオン巡回のカーソル
	TWeakObjectPtr<AActor> CurrentTarget;
	float RetargetTimer = 0.0f;							// 狙い直しまでの残り秒

	// 狙う方向へは GodFrolicTurnRate（度/秒）までしか寄せないので、急な折り返しにならず弧を描いて曲がる
	FVector FlyDirection = FVector::ForwardVector;

	EBirdPhase BirdPhase = EBirdPhase::Dive;
	TWeakObjectPtr<AActor> PulloutTarget;	// 離脱の基準にする「直前に当てた対象」
	FVector PulloutDirection = FVector::UpVector;
	float PulloutElapsed = 0.0f;
	float NoTargetElapsed = 0.0f;	// GodFrolicNoTargetWaitTime を超えたら帰還へ
	float BirdBaseScale = 1.0f;		// スポーン時＝常駐お供と同じ元サイズ。帰還時の縮小先

	// 残り距離の比で元サイズへ補間するための基準
	float ReturnStartDistance = 0.0f;
	float ReturnStartScale = 1.0f;

	// 旋回で通り過ぎて距離が伸びても縮みが巻き戻らないよう、最大値を保持する
	float ReturnScaleAlpha = 0.0f;

	TMap<TWeakObjectPtr<AActor>, double> LastDamageTimes;	// 同一対象への再アーム管理

	// 発動モーション（ST→ED、ループ無し）。ED まで再生し終えたら bCastEnded＝プレイヤー解放
	float MontageTimer = 0.0f;
	bool bCastInEnd = false;
	bool bCastEnded = false;
};
