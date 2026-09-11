// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "PRJ_TIDE_P0/Composites/Player/PlayerModules/GodActionSubModule.h"
#include "PRJ_TIDE_P0/Utilities/AutomaticTimer.h"
#include "PRJ_TIDE_P0/Data/Camera/ExCameraTypes.h"

#include "GodSlashSubModule.generated.h"

class ATidePlayerCharacter;
class ULockOnTargetComponent;

// 神技・一閃の進行状態
UENUM( BlueprintType )
enum class EGodSlashState : uint8
{
	Idle,			// 非発動
	LockingOn,		// マルチロックオン中（ST → LP ループ）。再入力で発動、B でキャンセル
	Slashing,		// 一閃発動中（ATK ST → LP → ED）
};

// 神技「一閃」。L2 長押しでマルチロックオン（部位単位・重複なし）、解除で発動、B でキャンセル。
// ロックオン数／距離は発動時のチャージギア段階で変わる。モーションだけでは前進しないため、
// 実行時は各ターゲットへ高速移動して切り抜ける。
// ダメージ判定はモンタージュ側（AnimNotify）、ゲージは傘 UGodActionPlayerModule が持つ
UCLASS()
class UGodSlashSubModule : public UGodActionSubModule
{
	GENERATED_BODY()

public:
	virtual bool IsEnabled() const override { return true; }
	virtual void OnModuleUpdate( float DeltaTime ) override;

	// --- 入力要求（傘 UGodActionPlayerModule から委譲される） ---

	void RequestLockOnStart();		// L2 押下。ゲージ不要（実行時に消費）
	void RequestLockOnRelease();	// L2 解除。ゲージ・対象があれば発動、無ければキャンセル
	void RequestCancel();			// B 入力

	// mover を神鳥へ差し替えて一閃を発動する（プレイヤーはその場に留まり演出はワイドカット強制）。
	// Mover が無効ならプレイヤーが mover になる。ゲージ・対象の判定は RequestLockOnRelease と同じ
	void ExecuteAsFrolic( AActor* Mover );

	// --- 状態問い合わせ ---

	bool IsActive() const { return CurrentState != EGodSlashState::Idle; }
	bool IsLockingOn() const { return CurrentState == EGodSlashState::LockingOn; }
	bool IsSlashing() const { return CurrentState == EGodSlashState::Slashing; }
	EGodSlashState GetCurrentState() const { return CurrentState; }

	// LP モンタージュに攻撃判定が誤って残っていた場合のヒットストップを、この区間だけ抑止するガードに使う
	// （ダメージ自体は ED の一斉適用まで保留する設計）
	bool IsWideCutLoopPassthrough() const { return bWideCutActive && CurrentState == EGodSlashState::Slashing && SlashPhase == ESlashPhase::Loop; }

	// --- HUD 表示用 ---
	int32 GetLockedTargetCount() const { return LockedTargets.Num(); }
	int32 GetMaxLockOnCount() const { return GetMaxLockOnCountForGear(); }	// 現在のギア段階での上限
	void GetLockedTargetLocations( TArray<FVector>& OutLocations ) const;	// レティクル描画用

	// 戯れが飛び回る対象として使う
	void GetLockedTargetActors( TArray<AActor*>& OutActors ) const;

	// 演出カメラ（UGodSlashExCameraMode）へフレーミング情報を渡す。発動中なら true
	bool GetCameraFraming( FVector& OutFocusLocation, FVector& OutTargetCenter, float& OutTargetRadius, int32& OutCutIndex, bool& bOutCutLanded, bool& bOutWideCut, bool& bOutWideCutFinishing ) const;

#if !UE_BUILD_SHIPPING
	virtual void DrawDebugImGui() override;
#endif

private:
	enum class ESlashPhase : uint8
	{
		Stance,	// ATK ST を通常速度で再生。この間はまだ斬りかからず、終わったら Loop へ直行する
		Loop,	// ATK LP（ターゲット毎に進行）
		Return,	// ワイドカット専用：全対象切り抜け後、開始位置へ高速で戻る
		End,	// ATK ED
	};

	bool CanActivate() const;	// 被弾・死亡中などは不可

	// --- 切り抜けを走らせる mover（既定＝プレイヤー本体。戯れ発動時のみ神鳥へ差し替える）---
	// 位置・向きの読み書きは全てこのアクセサ経由にして、鳥へ差し替えても同じ一閃ロジックを流用できるようにする
	// （ダメージ・Instigator・ゲージ・演出カメラ・ロックオン取得はプレイヤー基準のまま）
	AActor* GetMover() const;
	// mover がプレイヤー本体か。鳥 mover では false になり、
	// 飛行モード切替・速度リセット・地面クランプ・落下ループ等を skip する
	bool IsMoverCharacter() const;
	FVector GetMoverLocation() const;
	FRotator GetMoverRotation() const;
	void SetMoverLocation( const FVector& NewLocation );
	void SetMoverRotation( const FRotator& NewRotation );

	// --- ロックオン ---
	void OnStartLockOn();
	void UpdateLockingOn( float DeltaTime );
	void RefreshLockedTargets();	// 視界内を再取得し、ギア段階に応じた最大数までロックオンする
	void PruneInvalidTargets();		// 無効・範囲外になったものを除去する

	// 手前に別の敵（Pawn）がいるだけなら遮蔽とみなさず true を返す（地形のみを見る）
	bool IsTargetVisibleFromCamera( const FVector& TargetLocation ) const;
	// ビューポート中央の一定割合の矩形内（＝レティクル内）に収まっているか
	bool IsTargetOnScreen( const FVector& TargetLocation ) const;

	// --- 一閃の発動／進行／終了 ---
	void OnExecuteSlash();
	void UpdateSlashing( float DeltaTime );
	void BeginSlashOnTarget( int32 TargetIndex );
	// 対象が尽きたら ED（ワイドカットなら Return）フェーズへ
	void AdvanceToNextSlashTarget();
	void BeginReturning();					// ワイドカット専用：開始位置へ戻る移動
	void UpdateReturning( float DeltaTime );	// 到達したら対象群に背を向けて ED へ
	void OnEnterSlashEnd();					// ED（タメ→一斉吹っ飛び→余韻）
	// 各カット到達時の手応え演出（ヒットエフェクト・ヒットストップ）。ダメージ・吹っ飛びはまだ与えない
	void OnCutHitTarget( int32 TargetIndex );
	void ApplyFinishDamageToAllTargets();	// ED で全対象へダメージ＋ノックバックを一斉適用
	void OnEndSlash();

	// --- 演出（スロー＋専用カメラ）---
	// StartSlashStaging は専用カメラの Push のみ（構え中は通常速度で見せる）。
	// スロー（TimeDilation）は斬りかかりへ移る瞬間に BeginSlashSlowMotion で開始する
	void StartSlashStaging();
	void BeginSlashSlowMotion();
	void EndSlashStaging();
	void RestoreSlashSlowMotion();	// スローだけ通常へ戻す（カメラは維持。一斉吹っ飛びの瞬間に呼ぶ）
	float GetSlashPlayerDilation() const;

	// カットスロー（プレイヤー＋斬った敵を一緒にイージングでスローイン／アウト）
	void BeginCutSlow( AActor* Enemy );	// 貫通の瞬間に開始（前のが残っていれば貼り直し）
	void UpdateCutSlow();				// 実時間でイージング進行
	void EndCutSlow( bool bRestorePlayerDilation );	// 敵を元に戻して終了。プレイヤーは引数で制御

	// --- 移動 ---
	// 現在の斬りかかり対象（または戻り先）へ高速移動する。到達したら true。
	// MoveSpeedOverride > 0 でその速度を使う（省略時は GodSlashMoveSpeed）
	bool MoveTowardSlashTarget( float DeltaTime, float MoveSpeedOverride = -1.0f );
	// カプセル下端が地面（地形のみ・敵 Pawn は除外）より下に潜らないよう Z をクランプする。FromLocation で
	// 高速降下時にトレース開始点が地面下へ潜るのを防ぎ、MoveSpeedOverride でスイープ深度を実速度に合わせる
	FVector ClampLocationAboveGround( const FVector& FromLocation, const FVector& DesiredLocation, float MoveSpeedOverride = -1.0f ) const;
	// 一閃中は MOVE_Flying のため IsFalling で判定できない。足元の地形トレースで見る
	bool IsAirborneDuringSlash() const;

	void ClearAction( bool bStopMontage );	// ロックオン解除と内部状態の初期化

	// --- ギア段階（0:なし〜3:参）別の性能 ---
	int32 GetMaxLockOnCountForGear() const;
	float GetLockOnDistanceForGear() const;
	// ロックオン数が多いほど 1 周の距離が伸びるため対象数に応じて底上げし、
	// 合計時間が固定（GodSlashWideCutLoopDuration）でも移動が追いつかなくなるのを防ぐ
	float GetWideCutZigzagMoveSpeed() const;

	ULockOnTargetComponent* GetTargetAt( int32 Index ) const;
	// 対象部位が属する敵の「見た目の体」サイズ（スケルタルメッシュ水平半径）。巨大敵のカメラ引き／
	// 手応え前倒しに使う（GetActorBounds は索敵スフィア等を含むため使わない）
	float GetTargetMeshRadius( const ULockOnTargetComponent* Comp, FVector& OutCenter ) const;
	// 全対象を包むバウンズ（重心・最大到達半径）。ワイドカットの広角フレーミングと、
	// 戻り後に向く「対象群の反対方向」の算出に使う
	void GetLockedTargetsGroupBounds( FVector& OutCenter, float& OutRadius ) const;

	// --- ワイドカット演出：星型ジグザグ巡回 ---
	// 重心を基準に角度でソートし、N と互いに素なステップで飛ばして辿ることで
	// 隣同士ではなく対角側へ跳ぶ軌跡にする（重心もここで確定する）
	void BuildWideCutStarOrder();
	// Step 番目の LockedTargets インデックス（周回すれば周期的に繰り返す）
	int32 GetWideCutTargetIndexForStep( int32 Step ) const;
	// 対象の実位置ではなく重心基準の「星型の頂点」を移動先にする。水平は実位置に揃えて明らかな空振りに
	// 見せないまま、高さだけ Step の偶奇で上下交互にずらして 3D 的なジグザグ軌跡にする
	FVector GetWideCutStarWaypoint( const FVector& RawTargetLoc, int32 Step ) const;

private:
	EGodSlashState CurrentState = EGodSlashState::Idle;

	// 未設定＝プレイヤー本体。戯れ発動時のみ神鳥アクターを差し込む。
	// 発動終了（ClearAction）で解除し、鳥なら外部駆動フラグも戻す
	TWeakObjectPtr<AActor> MoverActor;
	bool bForceWideCut = false;	// 戯れは演出をワイドカット強制にする

	// 戯れ発動時のカメラ向き（Yaw）。戻り〜ED では鳥をこの向きの正反対へ向け、
	// 最初のカメラ方向へ背を向けさせる（鳥 mover のときのみ使用）
	bool bHasFrolicCameraFacing = false;
	float FrolicCameraYaw = 0.0f;

	TArray<TWeakObjectPtr<ULockOnTargetComponent>> LockedTargets;	// 敵の部位単位

	FAutomaticTimer LockOnMontageTimer;	// 現在再生中モーションの残り
	FAutomaticTimer LockOnScanTimer;	// ターゲット再取得の間隔

	// --- 一閃中の進行管理 ---
	ESlashPhase SlashPhase = ESlashPhase::Stance;
	int32 CurrentSlashIndex = 0;
	FAutomaticTimer SlashTimer;
	FVector SlashMoveTarget = FVector::ZeroVector;
	// 以下 2 つは発動時のスナップショット（発動中の途中変更に影響されない）
	int32 ActivatedGearIndex = 0;
	bool bWideCutActive = false;
	FVector SlashStartLocation = FVector::ZeroVector;	// Return フェーズで戻る先

	// --- ワイドカット演出：星型ジグザグ巡回（Loop フェーズ） ---
	TArray<int32> WideCutStarOrder;	// 発動時に一度だけ構築する巡回順
	int32 WideCutStepCounter = 0;	// 尽きたら先頭から繰り返す＝何周でも往復できる
	// Loop に入ってからの経過（ゲーム内 DeltaTime 積算）。GodSlashWideCutLoopDuration で Return へ進む
	float WideCutLoopElapsed = 0.0f;
	FVector WideCutCentroid = FVector::ZeroVector;	// 星型の頂点計算の基準点
	// 毎フレーム補間でこの向きへ近づけ、クラシックのような瞬間スナップにしない
	// （頂点ごとに向きが急変しても "止まる" ように見えない）
	FRotator WideCutDesiredFacing = FRotator::ZeroRotator;
	// 上への回頭角速度（度/秒）。区間の移動所要時間と向きの変化量から逆算し、折り返しでも小さな調整でも
	// 到達までにちょうど向き終わるようにする（固定値だと折り返しで回頭が追いつかず滑って見える）
	float WideCutFacingAngularSpeed = 0.0f;
	// 毎回 LP を再生し直すとブレンドインが終わらず巻き戻り続けて止まって見えるため、
	// 前回の再生が終わってからのみ再生し直す
	FAutomaticTimer WideCutLpMontageTimer;

	// --- 演出（スロー＋専用カメラ） ---
	FCameraModeHandle SlashCameraHandle;
	bool bStagingActive = false;			// 演出（カメラ Push 含む）が有効か
	bool bSlowMoActive = false;				// スロー（TimeDilation）が掛かっているか
	FVector CameraFocusLocation = FVector::ZeroVector;	// 現在カットで注視している対象座標

	bool bCutHitFired = false;		// 現セグメントで OnCutHitTarget を出したか
	// 神鳥検証モードの空中終了時、ED モーション完了を待って滞空している累計時間（上限で強制終了）
	float EndAirWaitElapsed = 0.0f;

	// --- カットスロー（イージング） ---
	bool bCutSlowActive = false;
	float CutSlowElapsed = 0.0f;			// 開始からの実時間（秒）
	TWeakObjectPtr<AActor> CutSlowEnemy;
	float CutSlowEnemyBaseDilation = 1.0f;	// その敵の元の CustomTimeDilation（復帰先）
	bool bFinishBlownOff = false;	// End 中の進行管理（タメ→吹っ飛び→余韻）
};
