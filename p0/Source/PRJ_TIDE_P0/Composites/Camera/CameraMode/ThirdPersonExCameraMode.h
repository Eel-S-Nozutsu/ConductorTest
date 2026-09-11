// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

#include "PRJ_TIDE_P0/Composites/Camera/CameraMode/ExCameraMode.h"
#include "PRJ_TIDE_P0/Data/Camera/ExCameraTypes.h"

#include "ThirdPersonExCameraMode.generated.h"

class ACharacter;
class ATidePlayerCharacter;
class UTidePlayerParamDataAsset;

// 3人称のカメラモードクラス
UCLASS( Blueprintable )
class UThirdPersonExCameraMode : public UExCameraMode
{
	GENERATED_BODY()

public:
	virtual void UpdateCamera( const FCameraControlData& ControlData, FMinimalViewInfo& OutViewInfo ) override;
	virtual void OnActivated( const FMinimalViewInfo& LastViewInfo ) override;

	void InitializeFromRow( const struct FThirdPersonExCameraModeParamRow& Row );

	virtual FVector2D GetRotationSpeedRate() const override
	{
		return CommonParams.RotationSpeedRate;
	}

protected:
	// 共通パラメータ ------------------------------
	UPROPERTY( VisibleAnywhere, BlueprintReadOnly, Category = "Tide|Camera Settings" )
	FExCameraModeCommonParams CommonParams;

	// TPS特有のパラメータ -------------------------

	// キャラクターからカメラまでの距離
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Tide|Camera Settings|TPS" )
	float TargetArmLength = 500.0f;

	// キャラクターから見たローカル空間でのオフセット（X:前後, Y:左右, Z:上下）。
	// Z が「回転軸（カメラが周回する中心＝画面中心に留まる点）」の高さを兼ねる。腰基準（アクター原点）から
	// 頭あたりへ軸を上げたい（頭を軸に回したい）ときは Z を頭の高さへ上げる（Z は Yaw 回転の影響を受けない）。
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Tide|Camera Settings|TPS" )
	FVector TargetOffset = FVector( 0.0f, 0.0f, 60.0f );

	// 【注視点オフセット】カメラが実際に向く点（＝画面中心に来る点）を、回転軸（TargetOffset 適用後の点）から
	// 上下にずらす相対高さ（cm・ワールド垂直）。0 なら注視点＝回転軸（その点が画面中心＝その点を軸に周回）。
	// 正で軸より上・負で軸より下を画面中心に収める。カメラの向きは毎フレーム「回転軸 ＋ この値」の点を狙うよう
	// 再計算する。例：軸（TargetOffset.Z）を頭・この値を負にすると、頭を軸に周回しつつ胸あたりを画面中心に置ける。
	// 既定 0（＝注視点＝回転軸で従来と同じ挙動）。ロックオン追従中は無効（専用の向き計算を優先）へ補間で移行し、
	// 正面リセット中は即時無効（暗転中の即スナップ仕様に合わせる）。
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Tide|Camera Settings|TPS" )
	float FocusHeightOffset = 0.0f;

	// 注視点オフセットの有効⇔無効（ロックオン追従の出入り等）を補間する速度（大きいほど素早く切り替わる。0 で即時）。
	// ブールの即時トグルだと切り替わりの瞬間に傾きぶんのピッチが 1 フレームでスナップしてガクッとなるため補間する
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Tide|Camera Settings|TPS", meta = ( ClampMin = "0.0" ) )
	float FocusTiltInterpSpeed = 6.0f;

	// 壁へのめり込みを防ぐための判定球の半径
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Tide|Camera Settings|Collision" )
	float CollisionRadius = 15.0f;

	// 障害物判定に使用するコリジョンチャンネル
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Tide|Camera Settings|Collision" )
	TEnumAsByte<ECollisionChannel> CollisionChannel = ECC_Camera;

	// 【壁避けの寄り】地形に当たって理想位置より手前へ寄せる量（めり込み量）の追従速度。
	// 大きいほど素早く寄る（0 で即時＝従来のスナップ）。小さくするとゆっくり寄るが、
	// 寄り切るまでの間はカメラが地形に入るため下げすぎない
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Tide|Camera Settings|Collision", meta = ( ClampMin = "0.0" ) )
	float CollisionPullInInterpSpeed = 0.0f;

	// 【壁避けの戻り】地形から離れて理想位置へ戻る量の追従速度（0 で即時）。寄りより遅めにすると
	// 障害物を抜けた瞬間の飛び出しが穏やかになる
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Tide|Camera Settings|Collision", meta = ( ClampMin = "0.0" ) )
	float CollisionPullOutInterpSpeed = 0.0f;

	// XY軸（水平）のカメララグ
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Tide|Camera Settings|Lag" )
	bool bEnableCameraLagXY = false;

	//  XY軸（水平）のカメララグ追従速度（値が大きいほどターゲットに素早く追従する）
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Tide|Camera Settings|Lag", meta = ( EditCondition = "bEnableCameraLagXY" ) )
	float CameraLagSpeedXY = 10.0f;

	// Z軸（垂直）のカメララグ
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Tide|Camera Settings|Lag" )
	bool bEnableCameraLagZ = false;

	// Z軸（垂直）のカメララグ追従速度（値が大きいほどターゲットに素早く追従する）
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Tide|Camera Settings|Lag", meta = ( EditCondition = "bEnableCameraLagZ" ) )
	float CameraLagSpeedZ = 20.0f;

	// カメラがターゲットから離れることができる最大距離（0.0以下で無制限）
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Tide|Camera Settings|Lag", meta = ( EditCondition = "bEnableCameraLag" ) )
	float CameraLagMaxDistance = 0.0f;

	// カメラの回転（視点移動）に遅延（ラグ）を持たせるかどうか
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Tide|Camera Settings|Lag" )
	bool bEnableCameraRotationLag = false;

	// カメラ回転の追従速度（値が大きいほど入力に対して素早く追従する）
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Tide|Camera Settings|Lag", meta = ( EditCondition = "bEnableCameraRotationLag" ) )
	float CameraRotationLagSpeed = 10.0f;

	// ジャンプ中の注視点の縦追従（縦デッドゾーン式）は機能トグルも含めて TidePlayerParamDataAsset へ集約済み。
	// UpdateCamera が PlayerParamData から直接読むため、ここにはプロパティを持たない

	// --- ロックオンカメラ（注視点・追従） ---

	// ターゲットの足元から注視点をどれくらい上に上げるかのオフセット
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Tide|Camera Settings|LockOn" )
	float TargetZOffset = 0.0f;

	// プレイヤーとターゲットの間の注視点の割合（0.0=プレイヤー, 1.0=ターゲット）
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Tide|Camera Settings|LockOn" )
	float FocusRatio = 0.3f;

	// 注視点が移動・復帰する際の滑らかさ
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Tide|Camera Settings|LockOn" )
	float FocusInterpSpeed = 5.0f;

	// 左右の斜め配置（フレーミング）が切り替わる際の滑らかさ
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Tide|Camera Settings|LockOn" )
	float OffsetInterpSpeed = 1.0f;

	// カメラを後ろに引く距離（ズーム）が変化する際の滑らかさ
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Tide|Camera Settings|LockOn" )
	float PullBackInterpSpeed = 5.0f;

	// ターゲット追従や対象切り替え時のカメラ回転の滑らかさ
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Tide|Camera Settings|LockOn" )
	float LockOnTrackingSpeed = 5.0f;

	// ターゲット追従（仮想ターゲット座標の補間）の滑らかさ
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Tide|Camera Settings|LockOn" )
	float TargetTrackingSpeed = 10.0f;

	// カニ歩き（回り込み方向）判定を行うためのスティック入力の閾値（遊び）
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Tide|Camera Settings|LockOn" )
	float InputThreshold = 0.2f;

	// 注視点がプレイヤーから離れられる最大距離
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Tide|Camera Settings|LockOn" )
	float MaxFocusOffsetDistance = 800.0f;

	// --- ロックオンカメラ（動的フレーミング：距離に応じた回り込み・見下ろし） ---

	// 動的フレーミング補間レンジの近距離側
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Tide|Camera Settings|LockOn|Framing" )
	float RangeMinDistance = 150.0f;

	// 動的フレーミング補間レンジの遠距離側
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Tide|Camera Settings|LockOn|Framing" )
	float RangeMaxDistance = 1000.0f;

	// 近距離でのカメラ左右回り込み角度（被り防止）
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Tide|Camera Settings|LockOn|Framing" )
	float CloseYawOffset = 40.0f;

	// 遠距離でのカメラ左右回り込み角度
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Tide|Camera Settings|LockOn|Framing" )
	float FarYawOffset = 15.0f;

	// 近距離でのカメラ見下ろし角度
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Tide|Camera Settings|LockOn|Framing" )
	float ClosePitchOffset = -10.0f;

	// 遠距離でのカメラ見下ろし角度
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Tide|Camera Settings|LockOn|Framing" )
	float FarPitchOffset = 0.0f;

	// ロックオン時のピッチ上限（マイナス値にすることで絶対に見上げさせない）
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Tide|Camera Settings|LockOn|Framing" )
	float MaxLookUpPitch = 5.0f;

	// 【近接フェード】対象との距離がこの値以下になると、左右回り込み角（サイド配置）を距離に比例して 0 へフェードさせる。
	// 突進攻撃で対象へ肉薄した瞬間に CloseYawOffset が最大で乗り、対象通過時の注視方向反転と重なってカメラが大きくぶれるのを防ぐ。
	// 0 でフェード無効（従来どおり）。突進攻撃（空中攻撃／チャージ攻撃）の当たり際のブレ対策
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Tide|Camera Settings|LockOn|Framing", meta = ( ClampMin = "0.0" ) )
	float CloseFramingFadeDistance = 250.0f;

	// 【注視方向ホールド】対象との水平距離がこの値以下のとき、注視方向（対象向き）を作り直さず現在のカメラ向きを保持する。
	// 対象の真上／直近では (TargetLoc-PlayerLoc) の水平成分が不安定・反転し、180 度スイングの原因になるためホールドする。
	// 0 でほぼ無効（真上のゼロ割回避のみ）。距離が開けば再び対象追従へ戻る
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Tide|Camera Settings|LockOn|Framing", meta = ( ClampMin = "0.0" ) )
	float LookAtYawHoldDistance = 120.0f;

	// --- ロックオンカメラ（巨大敵対応） ---

	// 敵との高さ差分を、どれくらい注視点のZ(高さ)上昇に変換するか
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Tide|Camera Settings|LockOn|GiantEnemy" )
	float GiantEnemyFocusZRatio = 0.0f;

	// 敵との高さ差分を、どれくらいカメラの引き距離に変換するか
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Tide|Camera Settings|LockOn|GiantEnemy" )
	float GiantEnemyPullBackRatio = 0.7f;

	// --- チャージアクション中のカメラ自動回り込み（背後センタリング） ---

	// チャージアクション中の自動回り込みの回転スピード
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Tide|Camera Settings|Charge" )
	float ChargeCameraCenteringSpeed = 3.0f;

	// 閾値を超えてから最大速度に達するまでの角度の範囲
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Tide|Camera Settings|Charge" )
	float ChargeCameraCenteringInterpolationWidth = 30.0f;

	// --- 面沿いモードのカメラ上方向ロール追従（A案・検証） ---

	// ロール追従の適用率（0=無適用⇔1=完全追従）を出入りで補間する速度。大きいほど素早く効き、小さいほど緩やか。
	// 面沿いライド中かつロール追従ゾーン（ASurfaceRideZone::bCameraRollFollowUp）内でのみ 1 へ寄せる。
	// フル1周でカメラが1回転するため酔いやすい。緩めにしたい／部分適用したい場合は下の MaxAlpha と合わせて調整
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Tide|Camera Settings|SurfaceRide", meta = ( ClampMin = "0.0" ) )
	float SurfaceRideRollInterpSpeed = 6.0f;

	// ロール追従の最大適用率（0〜1）。1 で「画面の上＝プレイヤーの上」に完全一致、0.5 等にすると半分だけ傾けて
	// 酔いを抑えつつ向きの手掛かりを残す（画面基準との折衷）。既定 1
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Tide|Camera Settings|SurfaceRide", meta = ( ClampMin = "0.0", ClampMax = "1.0" ) )
	float SurfaceRideRollMaxAlpha = 1.0f;

	// 進行方向追従カメラ（方式1）：カメラ→注視点の距離（アーム長）
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Tide|Camera Settings|SurfaceRide", meta = ( ClampMin = "0.0" ) )
	float SurfaceRideChaseArmLength = 500.0f;

	// 進行方向追従カメラ（方式1）：注視点をプレイヤー原点から上方向（プレイヤーの上）へずらす量（cm）
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Tide|Camera Settings|SurfaceRide" )
	float SurfaceRideChaseHeightOffset = 60.0f;

	// 進行方向追従カメラ（方式1）：進行方向背後への回り込みの追従速度（大きいほど素早く背後へ回る）
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Tide|Camera Settings|SurfaceRide", meta = ( ClampMin = "0.1" ) )
	float SurfaceRideChaseInterpSpeed = 6.0f;

private:
	// --- UpdateCamera の各段。呼び出し順に並べてあり、段を跨ぐローカル値は参照で受け渡す ---

	// 面沿いモードの進行方向追従ビュー。通常 TPS と両立しないため、成立したら true を返して呼び出し側を打ち切らせる
	bool TryUpdateSurfaceRideChaseCamera( const FCameraControlData& ControlData, ACharacter* PlayerChar, ATidePlayerCharacter* TidePlayer,
		float PlayerDeltaTime, float FinalFOV, FMinimalViewInfo& OutViewInfo );

	// ロックオンの注視点・自動回転・引き距離。戻り値は「対象追従で縦構図を作っている（＝ジャンプ注視点固定を無効化する）」か
	bool UpdateLockOnFraming( const FCameraControlData& ControlData, ACharacter* PlayerChar, ATidePlayerCharacter* TidePlayer,
		float PlayerDeltaTime, FRotator& InOutBaseRotation, FVector& InOutBaseFocusLocation );

	// チャージ／ダッシュ中にカメラを背後へ寄せる（ロックオン対象がいないときだけ働く）
	void UpdateChargeCameraCentering( const FCameraControlData& ControlData, ATidePlayerCharacter* TidePlayer,
		float PlayerDeltaTime, FRotator& InOutBaseRotation );

	// ジャンプ中の注視点の縦追従（縦デッドゾーン式）。モードを跨いで継続させるため共有ストアへも書き戻す
	void UpdateJumpFocusVertical( const FCameraControlData& ControlData, ACharacter* PlayerChar, ATidePlayerCharacter* TidePlayer,
		float PlayerDeltaTime, bool bIsLockOnFollowActive, FVector& InOutBaseFocusLocation );

	// 角度クランプとカメララグ（回転・XY・Z を独立に補間）
	void ApplyCameraLagAndClamp( float PlayerDeltaTime, const FRotator& BaseRotation, const FVector& BaseFocusLocation,
		bool bResetToFront, FRotator& OutSmoothedRotation, FVector& OutSmoothedFocusLocation );

	// 理想位置までスフィアスイープして壁へのめり込みを避けた実位置を返す。
	// 「手前へ寄せる量」を補間で持つため const ではない
	FVector ResolveCameraCollision( const FVector& SweepStart, const FVector& IdealLocation, ACharacter* PlayerChar,
		float PlayerDeltaTime, bool bSnapCollision );

	// 面沿いライド中、視線は保ったまま画面の上をプレイヤーの上方向へロールさせる
	void ApplySurfaceRideRoll( ATidePlayerCharacter* TidePlayer, float PlayerDeltaTime, FRotator& InOutViewRotation );

#if !UE_BUILD_SHIPPING
	// 縦デッドゾーンの基準線・上限線・現在の注視点線をプレイヤーの横に描く
	void DrawJumpFocusDebug( const FCameraControlData& ControlData, ACharacter* PlayerChar, const UTidePlayerParamDataAsset* Params ) const;

	// 回転軸・注視点・カメラ位置の関係を描く（分離が効いているかの確認用）
	void DrawPivotFocusDebug( const FCameraControlData& ControlData, const FVector& PivotLocation, const FVector& CameraLocation, float LookAtDeltaZ ) const;
#endif

	// --- ラグ計算用の前フレーム状態 ---
	FVector PreviousFocusLocation = FVector::ZeroVector;
	FRotator PreviousControlRotation = FRotator::ZeroRotator;

	// OnActivated 直後の初回 UpdateCamera でラグの始点を自モードの理想値へ合わせる。直前ビューからの逆算
	// シードはアーム長差・ブレンド途中の合成ビュー・注視点傾きでずれ、ラグが追いかけてガクつくため使わない
	bool bPendingLagSeedFromIdeal = false;

	// --- ロックオン追従 ---
	TWeakObjectPtr<class ULockOnTargetComponent> LastLockedTargetComp;
	float CurrentLockOnSide = 1.0f;						// 1.0 = ターゲットが画面右、-1.0 = 画面左
	float CurrentPullBackDistance = 0.0f;
	FVector CurrentTrackingTargetLoc = FVector::ZeroVector;	// 仮想ターゲット座標（ワープ防止）
	FVector CurrentFocusOffset = FVector::ZeroVector;

	// FocusHeightOffset の適用率（0〜1、1＝通常時フル適用）。ロックオン追従の出入りで即時トグルせず補間する
	float CurrentFocusTiltAlpha = 1.0f;

	// 面沿いロール追従の適用率（0〜1）。出入りを補間して急なロールを避ける
	float CurrentSurfaceRideRollAlpha = 0.0f;

	// 壁避けで理想位置から手前へ寄せている量（cm）。位置ではなく「寄せ量」を補間するため、アーム長や
	// プレイヤー移動による理想位置の変化はそのまま通り、地形による寄り／戻りだけがゆっくり動く
	float CurrentCollisionPullIn = 0.0f;
	bool bCollisionPullInInitialized = false;

	// 進行方向追従カメラの現在向き。急な回り込みを避けるため QInterp で寄せる
	FQuat CurrentChaseRotation = FQuat::Identity;
	bool bChaseRotationInitialized = false;

	// --- ジャンプ中の注視点の縦追従 ---
	float CurrentFocusZ = 0.0f;
	float GroundedFocusZ = 0.0f;	// 離陸時の基準高さ。ここからの上下移動量でデッドゾーンを判定する
	bool bPrevAirborne = false;
	bool bExceededDeadZone = false;	// 今回の滞空で一度でも超えたか（戻り時だけ補間するため）
	bool bFocusZInitialized = false;
};
