// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Camera/CameraShakeBase.h"
#include "TidePlayerParamDataAsset.generated.h"

// ターン（切り返し）の発動判定。通常移動／ダッシュ／チャージダッシュで個別に調整する
USTRUCT( BlueprintType )
struct FPlayerTurnParams
{
	GENERATED_BODY()

	// OFF のとき共通値（MinSpeedForTurn / TurnThresholdDot / TurnBrakeRate）を使う
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Turn" )
	bool bOverrideCommon = false;

	// ターンを許可する移動速度
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Turn", meta = ( EditCondition = "bOverrideCommon" ) )
	float MinSpeedForTurn = 300.0f;
	// ターン時の内積閾値（-1.0f 〜 1.0f）。この値を下回ったときにターンが許可される。例: -0.5f なら約120度以上逆に入力されたらターン
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Turn", meta = ( EditCondition = "bOverrideCommon" ) )
	float TurnThresholdDot = -0.2f;
	// ターン時の慣性削り割合
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Turn", meta = ( EditCondition = "bOverrideCommon" ) )
	float TurnBrakeRate = 0.3f;
};

// プレイヤーのパラメータ
UCLASS()
class UTidePlayerParamDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	// ギア別配列から値を引く。GearIndex は 1 起点（要素0＝ギア壱）で範囲外は端へクランプ。空配列は設定ミスなので中立値 Fallback を返す
	static float GetValueForGear( const TArray<float>& ForGear, int32 GearIndex, float Fallback = 1.0f )
	{
		if ( ForGear.IsEmpty() ) return Fallback;
		return ForGear[FMath::Clamp( GearIndex - 1, 0, ForGear.Num() - 1 )];
	}

	// ==========================================
	// 基本移動
	// ==========================================
	// キャラクターの最大歩行速度
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Default|Movement" )
	float MaxWalkSpeed = 500.0f;
	// キャラクターのアナログ入力時の最低歩行速度（0で完全停止）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Default|Movement" )
	float MinAnalogWalkSpeed = 200.0f;

	// ==========================================
	// 向かい風／追い風（WindZone）
	// ==========================================
	// AWindZoneの水平風へ掛ける倍率。進行方向と逆＝向かい風/同＝追い風でグループが変わり、状態別（通常/チャージ/突風/ブースト）に持つ
	// 向かい風・通常（歩き等）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Default|Wind" )
	float WindPushScaleNormal = 1.0f;
	// 向かい風・チャージダッシュ中
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Default|Wind" )
	float WindPushScaleChargeDash = 0.5f;
	// 向かい風・突風バフ付きチャージアクション中
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Default|Wind" )
	float WindPushScaleGustDash = 0.4f;
	// 向かい風・加速ギミックのブーストダッシュ中
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Default|Wind" )
	float WindPushScaleBoostDash = 0.5f;
	// 追い風・通常（歩き等）。既定 1.0＝抵抗なし・風の効果をそのまま受ける
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Default|Wind" )
	float WindPushScaleTailwindNormal = 1.0f;
	// 追い風・チャージダッシュ中
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Default|Wind" )
	float WindPushScaleTailwindChargeDash = 1.0f;
	// 追い風・突風バフ付きチャージアクション中
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Default|Wind" )
	float WindPushScaleTailwindGustDash = 1.0f;
	// 追い風・加速ギミックのブーストダッシュ中
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Default|Wind" )
	float WindPushScaleTailwindBoostDash = 1.0f;

	// 水平風の押し出しを目標へ寄せる補間速度（VInterpTo）。風に入った瞬間に一気に押されるのを防ぐ。0 で即時
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Default|Wind", meta = ( ClampMin = "0.0" ) )
	float WindPushInterpSpeed = 3.0f;

	// ==========================================
	// アリジゴクの引き込み（GroundPull）
	// ==========================================
	// AAntlionPit の引き込み倍率。風向きの概念はなく状態別（通常/チャージ/突風/ブースト）のみで、ダッシュ中は軽減して逃れやすくする
	// 通常（歩き等）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Default|GroundPull" )
	float GroundPullScaleNormal = 1.0f;
	// チャージダッシュ中
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Default|GroundPull" )
	float GroundPullScaleChargeDash = 0.0f;
	// 突風バフ付きチャージアクション中
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Default|GroundPull" )
	float GroundPullScaleGustDash = 0.0f;
	// 加速ギミックのブーストダッシュ中
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Default|GroundPull" )
	float GroundPullScaleBoostDash = 0.5f;

	// キャラクターのジャンプ力
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Default|Movement" )
	float JumpZVelocity = 1000.0f;
	// 重力スケール（1.0fが標準。大きくすると落下が速くなり、ジャンプの滞空時間が短くなる）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Default|Movement" )
	float GravityScale = 2.0f;
	// 空中での移動制御の強さ（0.0f = 全く制御できない、1.0f = 地上と同じ制御）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Default|Movement" )
	float AirControl = 0.35f;
	// ジャンプの頂点付近など、横方向のスピードが落ちたときに空中での移動制御を一時的に強化するための乗数
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Default|Movement" )
	float AirControlBoostMultiplier = 2.0f;
	// 空中での移動制御を強化するための速度閾値。キャラクターの横方向の速度がこの値以下のときにAirControlBoostMultiplierが適用される
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Default|Movement" )
	float AirControlBoostVelocityThreshold = 50.0f;

	// 死亡してからリスタート（暗転＋テレポート）を要求するまでの遅延秒。0 で即時
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Default|Death" )
	float PlayerRestartDelay = 1.0f;

	// キャラクターの旋回速度（PitchやRollは通常不要なためYawのみ）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Default|Movement" )
	float RotationRateYaw = 1000.0f;
	// キャラクターの旋回速度（空中 - 抑えめに設定）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Default|Movement" )
	float FallingRotationRateYaw = 100.0f;

	// 歩ける斜面の最大角度（度数法）。これを超える斜面は壁扱いになり登れず滑る。CMCのWalkableFloorAngleへ適用
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Default|Movement", meta = ( ClampMin = "0.0", ClampMax = "90.0" ) )
	float WalkableFloorAngle = 55.0f;

	// 地面の摩擦係数（高いほど滑りにくい）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Default|Movement|Friction" )
	float GroundFriction = 8.0f;
	// 歩行中のブレーキ減速度（高いほど急停止する）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Default|Movement|Friction" )
	float BrakingDecelerationWalking = 1000.0f;
	// 空中でのブレーキ減速度（高いほど急停止する）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Default|Movement|Friction" )
	float BrakingDecelerationFalling = 1500.0f;

	// ==========================================
	// 回避
	// ==========================================

	// 連続で回避できる最大回数
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Dodge" )
	int32 MaxConsecutiveDodges = 2;
	// 回避終了後に発生するクールタイム（秒）。この間は次の回避ができない。
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Dodge" )
	float DodgeCooldownTime = 0.5f;

	// ステップ回避力（DashWalkSpeedにかける速度）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Dodge" )
	float DodgeSpeedMultiplier = 1.5f;
	// 回避時に最高速度を維持する時間の割合（0.0〜1.0。例: 0.5でモーション半分まで等速）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Dodge" )
	float DodgeSpeedMaintainRatio = 0.3f;
	// ステップ中の旋回速度(MovementComponentのRotationYawとは別の計算式)
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Dodge" )
	float DodgeTurnSpeed = 5.0f;
	 // ステップ回避開始から重力を通常に戻すまでの時間（秒）（非ルートモーション用）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Dodge" )
	float DodgeGravityLockTime = 0.3f;

	// ステップ中の摩擦係数
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Dodge|Movement|Friction" )
	float DodgeStepGroundFriction = 1.0f;
	// ステップ中のブレーキ減速度
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Dodge|Movement|Friction" )
	float DodgeStepBrakingDeceleration = 1000.0f;
	// 空中でのステップブレーキ減速度
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Dodge|Movement|Friction" )
	float DodgeStepBrakingDecelerationFalling = 1500.0f;

	// 回避開始時のスローモーションの時間倍率（0.1fなら10分の1の速度になる）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Dodge|Success" )
	float DodgeSuccessGlobalTimeDilation = 1.0f;
	// 回避開始時のプレイヤーの時間倍率（0.1fなら10分の1の速度になる。GlobalTimeDilationと組み合わせて使用）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Dodge|Success" )
	float DodgeSuccessPlayerTimeScale = 1.0f;
	// スローモーションの時間倍率を元に戻すのにかける時間（秒）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Dodge|Success" )
	float DodgeSuccessSlowMotionDuration = 0.0f;

	// 回避成功時に左右へ生成する分身ポーズのマテリアル（未設定なら分身演出そのものを出さない）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Dodge|Success" )
	TObjectPtr<class UMaterialInterface> DodgeSuccessPoseMaterial;
	// 分身ポーズが横に広がる最大距離（cm）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Dodge|Success" )
	float DodgeSuccessPoseLateralOffset = 200.0f;
	// 分身ポーズが広がって戻るまでの時間（秒）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Dodge|Success" )
	float DodgeSuccessPoseDuration = 0.5f;

	// 2波目（遅れて出るもう1組）が出るまでの遅延（秒）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Dodge|Success" )
	float DodgeSuccessSecondPoseDelay = 0.12f;
	// 2波目の分身が横に広がる最大距離（cm）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Dodge|Success" )
	float DodgeSuccessSecondPoseLateralOffset = 200.0f;
	// 2波目の分身が広がって戻るまでの時間（秒）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Dodge|Success" )
	float DodgeSuccessSecondPoseDuration = 0.5f;

	// ==========================================
	// とどめ（フィニッシュ）演出
	// ==========================================
	// とどめヒット時のワールド全体スローの時間倍率（0.1fなら10分の1の速度になる）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Finisher" )
	float FinisherGlobalTimeDilation = 0.15f;
	// とどめヒット時のプレイヤーの時間倍率（GlobalTimeDilation と組み合わせて使用）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Finisher" )
	float FinisherPlayerTimeScale = 0.3f;
	// スローの時間倍率を元に戻すのにかける時間（秒・実時間）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Finisher" )
	float FinisherSlowMotionDuration = 0.2f;

	// --- 切り抜け（とどめ・壊れ物破壊で前へ抜ける） ---
	// ON で慣性を残して前進、OFF で自己リコイルを打ち消してその場停止
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Finisher" )
	bool bEnableFinisherBreakthrough = true;
	// 前進速度へ引き継ぐヒット直前の水平速度の割合（1.0 で等倍・0 で慣性を使わず下限速度のみ）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Finisher", meta = ( EditCondition = "bEnableFinisherBreakthrough" ) )
	float FinisherBreakthroughInheritRate = 1.0f;
	// 前進速度の下限（cm/s。止まっていてもこの速度で前へ抜ける。0 で下限なし＝慣性ぶんだけ進む）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Finisher", meta = ( EditCondition = "bEnableFinisherBreakthrough" ) )
	float FinisherBreakthroughMinSpeed = 1500.0f;
	// 前進速度の上限（cm/s・0 で無制限）。対象詰めのイーズインで実測速度が跳ねて飛び出すのを抑える
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Finisher", meta = ( EditCondition = "bEnableFinisherBreakthrough" ) )
	float FinisherBreakthroughMaxSpeed = 1500.0f;

	// ==========================================
	// ダッシュアクション
	// ==========================================

	// 以下 3 つは種別で上書きしていないときの共通値
	// ターンを許可する移動速度
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Turn|Common" )
	float MinSpeedForTurn = 500.0f;
	// ターン時の内積閾値（-1.0f 〜 1.0f）。この値以上のときにターンが許可される。例: -0.5f なら約120度以上逆に入力されたらターンが許可される。
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Turn|Common" )
	float TurnThresholdDot = -0.5f;
	// ターン時の慣性削り割合
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Turn|Common" )
	float TurnBrakeRate = 0.3f;

	// 種別ごとの上書き。bOverrideCommon が OFF なら上の共通値を使う
	// 通常移動（RUN_TURN）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Turn|Run" )
	FPlayerTurnParams RunTurnParams;
	// ダッシュ（DASH_TURN）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Turn|Dash" )
	FPlayerTurnParams DashTurnParams;
	// チャージダッシュ（CHARGE_DASH_TURN）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Turn|ChargeDash" )
	FPlayerTurnParams ChargeDashTurnParams;

	// 種別パラメータを解決する。未上書きなら共通値を返す
	FPlayerTurnParams ResolveTurnParams( const FPlayerTurnParams& InParams ) const
	{
		if ( InParams.bOverrideCommon ) return InParams;

		FPlayerTurnParams Resolved;
		Resolved.MinSpeedForTurn  = MinSpeedForTurn;
		Resolved.TurnThresholdDot = TurnThresholdDot;
		Resolved.TurnBrakeRate    = TurnBrakeRate;
		return Resolved;
	}

	// ダッシュ中の入力途切れを許容する猶予時間（秒）。反対方向に切り返す際の途切れ防止。
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Dash" )
	float DashGracePeriod = 0.2f;

	// スティック入力の大きさがこの値を下回った場合、ダッシュ（ニュートラル）をキャンセルして停止する
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Action|Dash" )
	float DashCancelInputThreshold = 0.45f;

	// ダッシュ開始時のブースト旋回力
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Action|Dash" )
	float DashInitialRotationYaw = 1000.0f;
	// 回避派生ダッシュ開始時の初期旋回ブースト時間
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Action|Dash" )
	float DodgeToDashInitialTurnBoostTime = 0.2f;
	// 回避からダッシュへ移行する際のアニメーションブレンドアウト時間
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Action|Dash" )
	float DodgeToDashBlendOutTime = 0.25f;

	// ダッシュ中の傾き反映速度補間(大きい値ほど早く反映される)
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Dash|Animation" )
	float DashDirectionInterpSpeed = 10.0f;

	// ダッシュの最大歩行速度
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Dash|Movement" )
	float DashWalkSpeed = 800.0f;
	// ダッシュ中の旋回速度
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Dash|Movement" )
	float DashRotationYaw = 300.0f;

	// チャージダッシュ等の超高速状態からダッシュへ移行した際、通常のダッシュ速度へ落ち着くまでの時間
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Dash|Movement" )
	float DashInertiaDecelerationTime = 1.0f;
	// 慣性減衰中の開始ブレーキ減速度（低いほど初速の勢いが維持されて滑る。例: 500.0f）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Dash|Movement" )
	float DashInertiaBrakingDeceleration = 500.0f;
	// 慣性減衰のイージング指数（滑らかさの調整）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Dash|Movement" )
	float DashInertiaEaseExpo = 2.0f;

	// ダッシュ中の摩擦係数
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Dash|Movement|Friction" )
	float DashGroundFriction = 2.0f;
	// ダッシュ中のブレーキ減速度
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Dash|Movement|Friction" )
	float DashBrakingDeceleration = 1000.0f;
	// 空中でのダッシュブレーキ減速度
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Dash|Movement|Friction" )
	float DashBrakingDecelerationFalling = 1500.0f;

	// ==========================================
	// ブーストダッシュ（加速ギミック）
	// ==========================================

	// ギミックが速度未指定（0 以下）で BeginBoostDash を呼んだ場合のフォールバック最高速度
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|BoostDash" )
	float BoostDashDefaultMaxSpeed = 3150.0f;
	// ギミックが時間未指定（0 以下）で BeginBoostDash を呼んだ場合のフォールバック持続時間（秒）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|BoostDash" )
	float BoostDashDefaultDuration = 0.5f;
	// 地上ブースト中の旋回速度（方向転換の効き。大きいほど鋭く曲がれる）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|BoostDash" )
	float BoostDashRotationRateYawGround = 300.0f;
	// 空中ブースト中の旋回速度（大きいほど鋭く曲がれる）。慣性が強い空中を地上と別に調整する用
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|BoostDash" )
	float BoostDashRotationRateYawAir = 150.0f;
	// ブースト中の摩擦係数（低いほど高速を維持しやすい）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|BoostDash|Friction" )
	float BoostDashGroundFriction = 2.0f;
	// ブースト中のブレーキ減速度（低いほど惰性で滑る。時間切れ後の減速もこれで効く）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|BoostDash|Friction" )
	float BoostDashBrakingDecelerationWalking = 1000.0f;
	// 空中でのブーストブレーキ減速度
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|BoostDash|Friction" )
	float BoostDashBrakingDecelerationFalling = 1500.0f;
	// 空中で加速ギミックに触れたときの水平維持（重力カット）時間（秒）。0で無効。この間だけ落下を止め水平にブースト
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|BoostDash" )
	float BoostDashAirGravityLockTime = 0.5f;
	// 空中ブースト後半で水平速度を減衰し始める進行割合（0〜1）。以降EaseOutでBoostDashAirDecayEndSpeedへ減速。1に近いほど直前まで全速。地上は無影響
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|BoostDash" )
	float BoostDashAirDecayStartRatio = 0.8f;
	// 空中ブースト終了時に残す水平速度（cm/秒）。0にはせず終了後の慣性を残す（大きいほど滑り出しが強い）。開始速度がこれ以下なら減速しない
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|BoostDash" )
	float BoostDashAirDecayEndSpeed = 1200.0f;
	// 地上ブースト終了直後、進行方向を入力方向へ滑らかに旋回させる時間（秒）。0で即スナップ（カクッと曲がる）。この間はTurn検知を抑止し急な方向転換を防ぐ
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|BoostDash" )
	float BoostDashGroundExitTurnTime = 0.25f;
	// 中断していたチャージダッシュ再開直後、進行方向を中断前のブースト速度の向きから機体前方向へ寄せるイージング時間（秒）。0で直角スナップ。長いほど曲がりが緩やか
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|BoostDash" )
	float BoostResumeChargeDashTurnEaseTime = 0.4f;
	// 上記イージング中の回頭速度（deg/秒）。この遅めから始めChargeDashRotationRateYawへ戻す。小さいほど再開直後がゆっくり曲がる
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|BoostDash" )
	float BoostResumeChargeDashTurnRateYaw = 40.0f;

	// ブーストダッシュ中に適用する三人称カメラモードのキー（テーブル行名）。発動時Push/終了時Pop。NAME_Noneで切替なし（通常カメラのまま）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|BoostDash|Camera" )
	FName BoostDashCameraModeKey = "BoostDash";

	// 【旧仕様切替】チャージ溜め中に加速ギミックへ触れた挙動。false（新）＝溜めを中断しブースト専用モーションで統一（ブースト中は新規チャージ不可）、true（旧）＝溜めを維持し速度加算のみ。※ダッシュ/ジャンプ中の中断・再開はフラグ非依存
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|BoostDash" )
	bool bBoostKeepChargeMotionWhileCharging = false;

	// ==========================================
	// ジャンプアクション
	// ==========================================

	// ジャンプ可能回数
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Jump" )
	int32 MaxJumpCount = 1;

	// ==========================================
	// 竜巻ジャンプ（竜巻内でのジャンプ強化）
	//   竜巻内にいる間だけジャンプの高さ(Z)を下記の専用初速へ差し替える（水平初速XYは据え置き）。
	//   最終的な高さ＝基準初速 × 竜巻側の倍率。通常の JumpZVelocity と同値にすれば従来挙動と一致する
	// ==========================================

	// 【竜巻ジャンプ】通常ジャンプの基準ジャンプ力（Z方向初速＝高さ）。竜巻倍率が上乗せされる
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Jump|Wind", meta = ( ClampMin = "0.0" ) )
	float WindJumpZVelocity = 1000.0f;

	// 【竜巻ジャンプ】チャージジャンプの基準ジャンプ力（Z方向初速＝高さ）。竜巻倍率が上乗せされる
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Jump|Wind", meta = ( ClampMin = "0.0" ) )
	float WindJumpChargeJumpZVelocity = 1000.0f;

	// 【竜巻ジャンプ】チャージ幅跳び（ホップ）の基準ジャンプ力（Z方向初速＝高さ）。竜巻倍率が上乗せされる
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Jump|Wind", meta = ( ClampMin = "0.0" ) )
	float WindJumpChargeHopZVelocity = 1000.0f;

	// 【竜巻ジャンプ】小竜巻の強化倍率（基準初速へ掛かる）。見た目スケールとは独立。1.0で強化なし＝竜巻ジャンプモーションにもならない
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Jump|Wind", meta = ( ClampMin = "0.0" ) )
	float WindJumpBoostMultiplierSmall = 2.0f;

	// 【竜巻ジャンプ】大竜巻の強化倍率（基準初速へ掛かる）。見た目スケールとは独立
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Jump|Wind", meta = ( ClampMin = "0.0" ) )
	float WindJumpBoostMultiplierLarge = 3.0f;

	// 【機能検証】竜巻に触れた瞬間、ジャンプ入力なしで強制的に竜巻ジャンプさせる。OFF で押したときだけ強化
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Jump|Wind" )
	bool bForceWindJumpOnTornadoEnter = true;

	// ==========================================
	// 竜巻ジャンプ／打ち上げ（ジャンプパッド）の風切りトレイル
	//   ChargeDashWind と ChargeDashWindEffectForwardOffset を流用。チャージダッシュ中は二重表示を避けて出さない
	// ==========================================

	// 風切りトレイルの再生秒数。0以下なら着地まで流し続ける
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Jump|WindTrail", meta = ( ClampMin = "0.0" ) )
	float JumpWindTrailDuration = 0.0f;

	// 風切りトレイルへ渡すギア段階（Niagara の int パラメータ "Gear"）。竜巻ジャンプ／打ち上げはギアを持たないので固定値で出す
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Jump|WindTrail", meta = ( ClampMin = "1", ClampMax = "3" ) )
	int32 JumpWindTrailGear = 3;

	// ==========================================
	// 打ち上げ（ジャンプパッド等）中のアクション封印
	// ==========================================
	// 打ち上げ〜頂点でアクション封印する保険の最大封印秒数（通常は頂点で自動解除、横打ち上げ等で解除されない事故を防ぐ）。0以下で保険無効（頂点・着地でのみ解除）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|LaunchLock" )
	float LaunchActionLockMaxDuration = 3.0f;

	// 上昇区間のうちアクションを封じる先頭割合（0〜1。打ち上げ直後=0/頂点=1）。1.0で頂点まで封印、0.5で前半のみ、0で封印なし。封印中も移動は可能
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|LaunchLock", meta = ( ClampMin = "0.0", ClampMax = "1.0" ) )
	float LaunchActionLockAscentRate = 0.8f;

	// 溜め中に打ち上げへ乗ったときの扱い。false＝溜めを即キャンセル、true＝溜めを保持し封印中のR2リリースを封印解除時まで後回しにして発動
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|LaunchLock" )
	bool bLaunchLockDeferChargeRelease = false;

	// ==========================================
	// チャージアクション V2 (段階ストック式) 用パラメータ
	// ==========================================
	// チャージギア最大数
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|ChargeV2", meta = ( ClampMin = "1", ClampMax = "4" ) )
	int32 MaxChargeGearCount = 3;

	// ボタンホールド中、次の段階へシフトするまでの時間
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|ChargeV2" )
	float ChargeV2ShiftHoldTime = 2.0f;
	// チャージアクション後の短縮チャージ時間（秒）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|ChargeV2" )
	float PostActionMaxChargeTime = 0.4f;
	// アクション終了後、短縮チャージを受け付ける猶予時間（秒）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|ChargeV2" )
	float PostActionChargeWindowTime = 0.2f;
	// チャージ攻撃ロック直後、タグ（CanAttack 等）の残留による誤発火を無視する短い窓（秒）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|ChargeV2" )
	float ChargeV2TagResidueIgnoreTime = 0.25f;

	// 【検証用】溜めの再開に R2 の押し直しを要求する。OFF なら握っていれば自動で溜め直す
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Action|ChargeV2" )
	bool bRequireChargeRepressToRestart = false;

	// 【検証用】上記が OFF のときだけ効く部分適用版。コンボ最終段後・被弾後の2ケースに限って押し直しを要求する
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Action|ChargeV2" )
	bool bRequireChargeRepressAfterComboFinishAndDamage = true;

	// ============================================================
	// ギアの手動操作（検証用）。溜め中のみ受け付ける（IA_ChargeGearDown = L1 / IA_ChargeGearUp = R1）
	// ============================================================

	// L1（IA_ChargeGearDown）でギアを1段階下げられるようにする
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Action|ChargeV2|Gear" )
	bool bEnableManualGearDown = true;

	// ギア上昇を R1 だけに限定し、自動上昇（時間経過シフト・AnimNotify_ShiftUpGear・突風の最大ギア化）を全マスクする
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Action|ChargeV2|Gear" )
	bool bManualGearUpOnly = false;

	// ヒットバックをキャンセルしてチャージを開始した際の、開始モーション(ST)の再生速度倍率
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|ChargeV2|HitBack" )
	float ChargeStartAnimPlayRateFromHitCancel = 1.5f;

	// チャージ段階 1〜4ごとのダッシュパワー
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|ChargeV2|Dash" )
	TArray<float> ChargeV2DashPowersForGear = { 2000.0f, 2500.0f, 3000.0f, 3500.0f };

	// コンボ段階 1〜4ごとのダッシュパワー倍率
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|ChargeV2|Dash" )
	TArray<float> ChargeV2DashPowersRateForCombo = { 1.0f, 1.0f, 1.0f, 1.0f };

	// チャージ段階 1〜4ごとのダッシュ移動速度
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|ChargeV2|Dash" )
	TArray<float> ChargeV2DashSpeedsForGear = { 2000.0f, 2500.0f, 3000.0f, 3500.0f };

	// コンボ段階 1〜4ごとのダッシュ移動速度倍率
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|ChargeV2|Dash" )
	TArray<float> ChargeV2DashSpeedsRateForCombo = { 1.0f, 1.0f, 1.0f, 1.0f };

	// コンボ段階 1〜4ごとのダッシュ持続時間
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|ChargeV2|Dash" )
	TArray<float> ChargeV2DashLockTimes = { 0.82f, 1.0f, 1.0f, 1.0f };

	// ダッシュ初速バーストの持続時間（秒）。初速Power→持続Speedへこの秒数で減衰（最初だけ速い演出）。0＝即Speed（バーストなし）。地上・空中・空中チャージ攻撃共通
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|ChargeV2|Dash" )
	float ChargeDashSpeedBurstDuration = 0.0f;

	// 【1回目】空中ダッシュの寿命/ST持続（秒・ギア別）。地上共用のChargeV2DashLockTimesとは別に独立指定。0以下=ST長へ自動追従、正値で固定
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|ChargeV2|AirDash" )
	TArray<float> AirChargeDashLockTimesForGear = { 0.43f, 0.43f, 0.43f, 0.0f };

	// 【1回目】空中ダッシュの移動速度（ギア別・cm/s）。0以下=地上のChargeV2DashSpeedsForGearへフォールバック、正値で上書き
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|ChargeV2|AirDash" )
	TArray<float> AirChargeDashSpeedsForGear = { 1200.0f, 1500.0f, 1500.0f, 0.0f };

	// 【1回目】空中ダッシュの水平維持（滞空）時間（秒・ギア別、重力を切る長さ）。空中チャージ攻撃もこれを流用
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|ChargeV2|AirDash" )
	TArray<float> AirChargeDashGravityLockTimesForGear = { 0.4f, 0.4f, 0.4f, 0.4f };

	// 【1回目】空中ダッシュの滞空中の上昇速度（ギア別・cm/s）。0で水平維持、正値でその速度ぶん上昇（ZUp）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|ChargeV2|AirDash" )
	TArray<float> AirChargeDashZUpSpeedsForGear = { 150.0f, 150.0f, 150.0f, 0.0f };

	// 空中で出せるチャージダッシュ回数（着地でリセット）。2以上で2回目以降に AirChargeDash2*ForGear を使う（未設定なら1回目へフォールバック）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|ChargeV2|AirDash", meta = ( ClampMin = "1" ) )
	int32 MaxAirChargeDashCount = 1;

	// 【1回目】空中チャージダッシュ ST（ダッシュモーション）のギア別再生速度（1.0＝等速、大きいほど速く＝短くなる）。ロック時間内にSTを収めるための尺合わせ
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|ChargeV2|AirDash" )
	TArray<float> AirChargeDashStAnimPlayRatesForGear = { 0.8f, 0.8f, 0.8f, 1.0f };

	// 空中チャージダッシュ中の旋回速度（deg/秒。大きいほど鋭く曲がれる）。0以下＝地上共通の ChargeDashRotationRateYaw を使う
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|ChargeV2|AirDash", meta = ( ClampMin = "0.0" ) )
	float AirChargeDashRotationRateYaw = 300.0f;

	// --- 【1回目】空中チャージダッシュ「終了時（→通常落下）」の水平慣性コントロール ---
	// 水平速度を EaseOut で減衰させてふわっと滑り続けるのを抑える。向き変え（エアコントロール）は残し、
	// 別チャージアクション開始・着地・滑空開始で即キャンセル
	// 減衰させる秒数（0 で無効＝水平速度を残したまま通常落下物理に任せる）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|ChargeV2|AirDash", meta = ( ClampMin = "0.0" ) )
	float AirChargeDashEndInertiaTime = 0.5f;

	// 終了瞬間の初速倍率（ダッシュ終了時の水平速度に対する割合）。1.0＝そのまま始める、小さいほど終了直後にガクッと落ちる
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|ChargeV2|AirDash", meta = ( ClampMin = "0.0", ClampMax = "1.0" ) )
	float AirChargeDashEndInertiaStartRate = 1.0f;

	// 減衰の到達先（初速に対する割合）。0＝ほぼ停止まで削る（空中前回避と同様）、正値で慣性を残す
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|ChargeV2|AirDash", meta = ( ClampMin = "0.0", ClampMax = "1.0" ) )
	float AirChargeDashEndInertiaTargetRate = 1.0f;

	// --- 【2回目以降】空中チャージダッシュのギア別パラメータ（MaxAirChargeDashCount>=2 のとき）。1回目と同じ意味で、空／範囲外なら1回目へフォールバック ---
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|ChargeV2|AirDash|2nd" )
	TArray<float> AirChargeDash2LockTimesForGear = { 0.43f, 0.43f, 0.43f, 0.0f };

	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|ChargeV2|AirDash|2nd" )
	TArray<float> AirChargeDash2SpeedsForGear = { 1200.0f, 1500.0f, 1500.0f, 0.0f };

	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|ChargeV2|AirDash|2nd" )
	TArray<float> AirChargeDash2GravityLockTimesForGear = { 0.4f, 0.4f, 0.4f, 0.4f };

	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|ChargeV2|AirDash|2nd" )
	TArray<float> AirChargeDash2ZUpSpeedsForGear = { 150.0f, 150.0f, 150.0f, 0.0f };

	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|ChargeV2|AirDash|2nd" )
	TArray<float> AirChargeDash2StAnimPlayRatesForGear = { 1.0f, 1.0f, 1.0f, 1.0f };

	// --- 滑空（空中でジャンプボタン長押し → AirGlideStart→AirGlideLoop。離すと落下、押し直しで再滑空）---
	// 滑空を有効にするか。OFF ならジャンプ長押しでも滑空しない
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Glide" )
	bool bEnableAirGlide = true;

	// 空中チャージダッシュが空中のまま終わったら自動で滑空へ移るか（ボタン押下は不要）。
	// 自動展開中にジャンプボタンを押すと以降は通常操作（押している間だけ滑空／離すと落下）へ戻る
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Glide" )
	bool bAutoGlideAfterAirChargeDash = false;

	// 空中チャージダッシュを使い切った空中で、R2（チャージ入力）の長押しでも滑空へ移れるようにするか。
	// 判定時間・操作（離す＝落下／押し直しで再滑空）はジャンプ長押しと共通
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Glide" )
	bool bGlideByChargeHoldAfterAirChargeDash = true;

	// 滑空へ移るまでのジャンプ押下時間（秒）。ジャンプが残っていればそれが先に出て、上昇が終わり次第（Velocity.Z<=0）滑空へ
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Glide", meta = ( ClampMin = "0.0" ) )
	float GlideJumpHoldTime = 0.2f;

	// 滑空中の最大前進速度（cm/s）。倒し量に比例して 0〜この値（入力ゼロなら水平停止＝その場で緩降下）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Glide" )
	float GlideForwardSpeed = 800.0f;

	// 滑空の水平速度を目標へ寄せる加減速レート（VInterpTo）。0＝即時、大きいほど”ふわっ”と滑る
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Glide", meta = ( ClampMin = "0.0" ) )
	float GlideMoveAccelRate = 5.0f;

	// 滑空中の下降速度（cm/s、正値＝1秒あたりの降下量。緩降下）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Glide" )
	float GlideDescendSpeed = 200.0f;

	// 滑空中の旋回速度（deg/s、スティック左右で進行方向を変える速さ）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Glide" )
	float GlideTurnRateYaw = 90.0f;

	// 滑空中の空中制御の強さ（0〜1）。1=入力方向(カメラ相対)へ直接進める＝制御しやすい、0=機体前方基準で回頭のみ
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Glide", meta = ( ClampMin = "0.0", ClampMax = "1.0" ) )
	float GlideAirControlStrength = 1.0f;

	// 滑空の持続時間（秒）。空中1回ぶんの予算で滑空⇄落下を往復しても引き継ぎ、着地でリセット。0以下＝無制限
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Glide" )
	float GlideMaxDuration = 3.0f;

	// 滑空フレネルの点滅を開始する残り時間（秒）。周波数はチャージダッシュと共通（ChargeDashFresnel*BlinkHz）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Glide" )
	float GlideFresnelBlinkTriggerTime = 2.0f;

	// 滑空解除時、落下速度の上限を GlideDescendSpeed から徐々に開放する時間（秒）。解除直後の急降下を防ぐ。0＝即通常落下
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Glide", meta = ( ClampMin = "0.0" ) )
	float GlideReleaseFallRampTime = 0.6f;

	// 上記ランプの終点となる落下速度（cm/s）。小さいほど解除後もふわっと落ちる
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Glide", meta = ( ClampMin = "0.0" ) )
	float GlideReleaseTargetFallSpeed = 1500.0f;

	// チャージダッシュのフレネルフェードアウト速度
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|ChargeV2|Dash|Fresnel" )
	float ChargeDashFresnelFadeOutSpeed = 5.0f;
	// チカチカ演出開始する残り時間秒数
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|ChargeV2|Dash|Fresnel" )
	float ChargeDashFresnelBlinkTriggerTime = 2.0f;
	// チカチカ演出の基本周波数（チカチカ演出開始時）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|ChargeV2|Dash|Fresnel" )
	float ChargeDashFresnelBaseBlinkHz = 1.0f;
	// チカチカ演出の最大周波数（チカチカ演出終了時）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|ChargeV2|Dash|Fresnel" )
	float ChargeDashFresnelMaxBlinkHz = 5.0f;

	// チャージダッシュの速度を弾き力に変換する倍率
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|ChargeV2|Dash|Rebound" )
	float ChargeReboundMultiplier = 0.7f;
	// チャージダッシュの最低保証の弾き力
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|ChargeV2|Dash|Rebound" )
	float ChargeBaseReboundPower = 300.0f;
	// チャージダッシュのはじかれた後の復帰時間
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|ChargeV2|Dash|Rebound" )
	float ChargeReboundRecoveryTime = 0.5f;
	// チャージダッシュのはじかれた後のブレーキ減速度（大きいほど速く止まる）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|ChargeV2|Dash|Rebound" )
	float ChargeReboundBrakingDeceleration = 2000.0f;
	// 弾かれ中に「どれくらいグイグイ曲がれるか」(値を大きくすると、弾かれ中でも鋭くカーブできるようになります。)
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|ChargeV2|Dash|Rebound" )
	float ChargeReboundSteerPower = 1500.0f;
	// はじかれ後のチャージダッシュ加速率
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|ChargeV2|Dash|Rebound" )
	float ChargeReboundAccelationRate = 4000.0f;
	// チャージダッシュのはじかれ可能かどうかの閾値速度（この速度以上でダッシュがはじかれる。）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|ChargeV2|Dash|Rebound" )
	float MinReboundSpeed = 500.0f;
	// チャージダッシュのはじかれ可能かどうかの閾値角度（-1.0 が完全な正面衝突、0.0 が平行（かすり）)
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|ChargeV2|Dash|Rebound" )
	float MinReboundAngleCos = -0.2f;

	// チャージ攻撃最大コンボ数
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|ChargeV2|Attack" )
	int32 MaxChargeComboCount = 4;

	// コンボ段階 1〜4ごとの攻撃のダッシュ基礎パワー（通常Power）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|ChargeV2|Attack" )
	TArray<float> ChargeV2AttackDashPowersForCombo = { 4000.0f, 4400.0f, 4800.0f, 5000.0f };

	// チャージ段階 1〜4ごとのダッシュ突進パワー倍率（基礎パワーに掛ける）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|ChargeV2|Attack" )
	TArray<float> ChargeV2AttackDashPowersRateForGear = { 1.0f, 1.0f, 1.0f, 1.0f };

	// チャージ攻撃の水平維持（滞空）時間（秒）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|ChargeV2|Attack" )
	TArray<float> ChargeAttackGravityLockTimes = { 0.0f, 0.0f, 0.0f, 0.0f };

	// チャージ攻撃（空中）の水平維持（滞空）時間（秒）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|ChargeV2|Attack" )
	float AirChargeAttackGravityLockTime = 0.4f;

	// コンボ段階 チャージ攻撃がヒットした際、自分が後ろに押し戻される基礎力（通常Power）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|ChargeV2|Attack|HitBack" )
	TArray<float> ChargeAttackSelfPushbackPowersForCombo = { 2500.0f, 2500.0f, 2500.0f, 2500.0f };

	// チャージ段階 チャージ攻撃がヒットした際、自分が後ろに押し戻される力倍率（基礎力に掛ける）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|ChargeV2|Attack|HitBack" )
	TArray<float> ChargeAttackSelfPushbackPowersRateForGear = { 1.0f, 1.0f, 1.0f, 1.0f };

	// ノックバック中のスキッドエフェクト生成間隔（秒）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|ChargeV2|Attack|HitBack" )
	float HitbackSkidEffectInterval = 0.06f;

	// ヒットバックをキャンセルして溜め直し、後退で流されている間（HitCancelChargingSteeringDuration と同区間）足元へ小さい火花を出す
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|ChargeV2|Attack|HitBack" )
	bool bEnableHitbackChargeSpark = true;

	// その小さい火花のサイズ。ChargeBoostMinVelocityRate〜MaxVelocityRate の補間位置（0=最小）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|ChargeV2|Attack|HitBack", meta = ( EditCondition = "bEnableHitbackChargeSpark", ClampMin = "0.0", ClampMax = "1.0" ) )
	float HitbackChargeSparkSizeRate = 0.0f;

	// その区間を継続するとギアを1段上げる（上がった瞬間に大きい火花）。1回の溜めにつき1回
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|ChargeV2|Attack|HitBack" )
	bool bEnableHitbackChargeGearUp = true;

	// ギアが上がるまでの継続秒数。HitCancelChargingSteeringDuration を超えると窓が先に閉じて発火しない
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|ChargeV2|Attack|HitBack", meta = ( EditCondition = "bEnableHitbackChargeGearUp", ClampMin = "0.0" ) )
	float HitbackChargeGearUpTime = 0.4f;

	// ヒットキャンセルでチャージを開始した直後のドリフトステアリング速度倍率（1.0未満で旋回が重くなる）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|ChargeV2|Attack|HitBack" )
	float HitCancelChargingDriftSteeringRate = 0.0f;

	// ヒットキャンセルチャージ直後の移動入力（推進力）倍率（1.0未満で前進・方向転換の加速が鈍くなる。0で実質ロック）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|ChargeV2|Attack|HitBack" )
	float HitCancelChargingPropulsionRate = 0.5f;

	// ヒットキャンセルチャージ中のステアリング・推進制限を維持する時間（秒）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|ChargeV2|Attack|HitBack" )
	float HitCancelChargingSteeringDuration = 0.4f;

	// ヒットキャンセルでチャージを開始した後、チャージアクション（ダッシュ／攻撃／ジャンプ）の発動を禁止する時間（秒）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|ChargeV2|Attack|HitBack" )
	float HitCancelChargeActionLockDuration = 0.0f;

	// ヒットバック後、R2 を押さず攻撃連打で連続チャージ攻撃する際のギア別インターバル（秒）。0で即時連続。R2長押し中は非適用
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|ChargeV2|Attack|HitBack" )
	TArray<float> ChargeAttackHitbackComboIntervalsForGear = { 0.0f, 0.0f, 0.0f };

	// チャージ段階 1〜4ごとのチャージ攻撃アニメーション再生速度倍率
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|ChargeV2|Attack" )
	TArray<float> ChargeAttackAnimPlayRatesForGear = { 1.0f, 1.0f, 1.0f, 1.0f };

	// 空中チャージ攻撃の落下速度
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Action|ChargeV2|AirAttack" )
	float AirChargeAttackPlungeSpeed = -2000.0f;

	// 空中チャージ攻撃の滞空時間（秒）。ST 開始からこの秒数で LP（斜め突進）へ移る。0 で ST の再生終了を待つ
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Action|ChargeV2|AirAttack", meta = ( ClampMin = "0.0" ) )
	float AirChargeAttackHoverTime = 0.2f;

	// 空中チャージ攻撃LP フェーズの斜め突進速度（cm/s）。ST（滞空）後、地面へ向かって斜めに突っ込む速さ
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Action|ChargeV2|AirAttack" )
	float AirChargeAttackDiveSpeed = 5000.0f;

	// 空中チャージ攻撃LP フェーズの突進角度（水平からの下向き角度・度）。0=水平、90=真下。斜め突進なので既定45°
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Action|ChargeV2|AirAttack", meta = ( ClampMin = "0.0", ClampMax = "89.0" ) )
	float AirChargeAttackDiveAngleDeg = 20.0f;

	// 非ロックの角度ダイブ中、プレイヤーの向きを突進方向（ダイブ角度）へ傾ける（ピッチ込み）。OFF なら従来どおり水平（Yaw）のみ＝傾けない
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Action|ChargeV2|AirAttack" )
	bool bAirChargeAttackDiveOrientToDirection = true;

	// 非ロックの角度ダイブで突進方向へ傾ける際の追加ピッチ（度）。LP モーション固有の斜め角度を打ち消すなどの微調整用（＋で上向き。ロックオン突進の AirChargeAttackLockOnRushPitchOffset と同趣旨）
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Action|ChargeV2|AirAttack", meta = ( ClampMin = "-89.0", ClampMax = "89.0", EditCondition = "bAirChargeAttackDiveOrientToDirection" ) )
	float AirChargeAttackDivePitchOffset = 40.0f;

	// 非ロックの角度ダイブの持続時間（秒）。着地・壁ヒットの前でもこの秒数で突進を終え通常落下へ返す。0 で無制限（従来どおり着地・壁ヒットまで）
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Action|ChargeV2|AirAttack", meta = ( ClampMin = "0.0" ) )
	float AirChargeAttackDiveMaxTime = 0.8f;

	// 角度ダイブが AirChargeAttackDiveMaxTime で終わったときに残す水平速度の割合。0=前進を止めて素直に落下／1=突進速度をそのまま慣性として残す。
	// 落下中の減速（ChargeAttackBrakingDecelerationFalling）はスティックを倒している間は効かないため、1 のままだと突進が止まって見えない
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Action|ChargeV2|AirAttack", meta = ( ClampMin = "0.0", ClampMax = "1.0" ) )
	float AirChargeAttackDiveEndSpeedRate = 0.8f;

	// ロックオン中の空中チャージ攻撃突進。ON なら角度に依らずロック部位へ 3D 突進して毎フレーム追う。OFF／非ロックは角度ダイブ
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Action|ChargeV2|AirAttack" )
	bool bEnableAirChargeAttackLockOnRush = true;

	// ロックオン突進の持続時間（秒）。当たらなくてもこの秒数で終了。0 で無制限。速度は AirChargeAttackDiveSpeed を流用
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Action|ChargeV2|AirAttack", meta = ( ClampMin = "0.0", EditCondition = "bEnableAirChargeAttackLockOnRush" ) )
	float AirChargeAttackLockOnRushMaxTime = 1.0f;

	// ロックオン突進中、プレイヤーの向きを対象方向（3D）へ傾ける（ピッチ込み）。OFF なら水平（Yaw）のみ＝従来
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Action|ChargeV2|AirAttack", meta = ( EditCondition = "bEnableAirChargeAttackLockOnRush" ) )
	bool bAirChargeAttackLockOnRushOrientToTarget = true;

	// ロックオン突進で対象方向へ傾ける際の追加ピッチ（度）。LP モーションに元から付いている斜め角度を打ち消すなどの微調整用（＋で上向き）
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Action|ChargeV2|AirAttack", meta = ( ClampMin = "-89.0", ClampMax = "89.0", EditCondition = "bEnableAirChargeAttackLockOnRush && bAirChargeAttackLockOnRushOrientToTarget" ) )
	float AirChargeAttackLockOnRushPitchOffset = 60.0f;

	// 空中チャージ攻撃の ST（溜め）モンタージュ再生速度。1.0=等速
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Action|ChargeV2|AirAttack", meta = ( ClampMin = "0.01" ) )
	float AirChargeAttackStartPlayRate = 1.0f;

	// 空中チャージ攻撃の LP（突進）モンタージュ再生速度。1.0=等速
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Action|ChargeV2|AirAttack", meta = ( ClampMin = "0.01" ) )
	float AirChargeAttackLoopPlayRate = 1.5f;

	// 空中チャージ攻撃の終了後、この秒数は振り下ろし（空中通常攻撃）を出せない。0 で無効
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Action|ChargeV2|AirAttack", meta = ( ClampMin = "0.0" ) )
	float AirChargeAttackToDiveAttackCooldown = 0.5f;

	// 着地ED開始からこの秒数は、CanMove 残留＋握りっぱなし入力による「開始即キャンセル」を無視する（ED montage の再生位置で計測）
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Action|ChargeV2|AirAttack", meta = ( ClampMin = "0.0" ) )
	float AirChargeAttackEndResidueIgnoreTime = 0.25f;

	// チャージジャンプSTモーションの再生速度（ギア段階ごと）
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Action|ChargeV2|Jump" )
	TArray<float> ChargeJumpAnimPlayRatesForGear = { 1.6f, 1.4f, 1.2f, 1.0f };

	// チャージ段階 1〜4ごとの残像マテリアル配列
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|ChargeV2|GhostTrail" )
	TArray<TObjectPtr<class UMaterialInterface>> ChargeV2GhostTrailMaterials;

	// ブーストチャージボーナスを有効にするかどうか。これを有効にすると、スピードやドリフトの状態に応じてチャージの溜まる速度が変化します。
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Action|ChargeV2|Boost" )
	bool bEnableSpeedAndDriftChargeBonus = true;

	// スピードブースト（移動速度に応じたチャージ溜め加速）を有効にするか。OFF でもドリフトボーナスは従来どおり働く
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Action|ChargeV2|Boost", meta = ( EditCondition = "bEnableSpeedAndDriftChargeBonus" ) )
	bool bEnableChargeSpeedBonus = false;

	// スピードボーナスが乗り始める最低速度（ドリフト判定の最低速度も兼ねるため bEnableChargeSpeedBonus とは独立）
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Action|ChargeV2|Boost", meta = ( EditCondition = "bEnableSpeedAndDriftChargeBonus" ) )
	float MinSpeedForChargeBonus = 900.0f;

	// 最高速度（これ以上はボーナスが頭打ち）
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Action|ChargeV2|Boost", meta = ( EditCondition = "bEnableSpeedAndDriftChargeBonus && bEnableChargeSpeedBonus" ) )
	float MaxSpeedForChargeBonus = 3500.0f;

	// 最高速度に達した時のゲージ上昇倍率（1.0を加算。1.0なら2倍の速度で溜まる）
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Action|ChargeV2|Boost", meta = ( EditCondition = "bEnableSpeedAndDriftChargeBonus && bEnableChargeSpeedBonus" ) )
	float MaxSpeedChargeBonusRate = 1.0f;

	// 回転ブースト発動と判定するドリフト角度（度数法）。進行方向と入力方向のなす角がこれ以上で「ドリフト中」（例:60で60°以上のターン）
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Action|ChargeV2|Boost", meta = ( EditCondition = "bEnableSpeedAndDriftChargeBonus", ClampMin = "0.0", ClampMax = "180.0" ) )
	float DriftBoostAngleThreshold = 60.0f;

	// 上記の角度ドリフトを継続すべき秒数（満たして初めて回転ブースト＆演出が発動、途切れるとタイマーは0へリセット）
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Action|ChargeV2|Boost", meta = ( EditCondition = "bEnableSpeedAndDriftChargeBonus", ClampMin = "0.0" ) )
	float DriftBoostSustainTime = 0.3f;

	// 最大のドリフト（真後ろへのターン＝内積-1.0）時のゲージ上昇倍率（加算）
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Action|ChargeV2|Boost", meta = ( EditCondition = "bEnableSpeedAndDriftChargeBonus" ) )
	float DriftChargeBonusRate = 1.0f;

	// 【機能検証用】ドリフトボーナスの加算モード。OFF＝DriftChargeBonusRate × 角度由来の強度、ON＝角度に依らず素の値を加算
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Action|ChargeV2|Boost", meta = ( EditCondition = "bEnableSpeedAndDriftChargeBonus" ) )
	bool bUseFixedDriftChargeBonus = false;

	// ============================================================
	// ドリフトボーナスのギア直上昇モード（溜め速度への加算と入れ替わる検証仕様）
	// ============================================================

	// 【仕様検証用】ドリフトボーナスを「溜め速度の加算」から「ギアの直接上昇（クールダウン制）」へ入れ替える。
	// 切り返しでクールダウンがリセットされるため、右・左と繋げばギアを連続で上げられる
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Action|ChargeV2|Boost|GearUp", meta = ( EditCondition = "bEnableSpeedAndDriftChargeBonus" ) )
	bool bUseDriftGearUpBonus = true;

	// 同じ方向のドリフトを継続してギアが1段上がるまでの秒数（ドリフト成立＝DriftBoostSustainTime のゲート後から計測）
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Action|ChargeV2|Boost|GearUp", meta = ( EditCondition = "bEnableSpeedAndDriftChargeBonus && bUseDriftGearUpBonus", ClampMin = "0.0" ) )
	float DriftGearUpSustainTime = 0.1f;

	// ギアアップ後、同じ方向のドリフトでは再上昇できない秒数（切り返しでリセット。ドリフトが途切れている間も消化する）
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Action|ChargeV2|Boost|GearUp", meta = ( EditCondition = "bEnableSpeedAndDriftChargeBonus && bUseDriftGearUpBonus", ClampMin = "0.0" ) )
	float DriftGearUpCooldownTime = 2.5f;

	// 切り返し時、継続秒数を待たず即1段上げるか。OFF なら切り返し後も DriftGearUpSustainTime ぶん曲がり続ける必要がある
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Action|ChargeV2|Boost|GearUp", meta = ( EditCondition = "bEnableSpeedAndDriftChargeBonus && bUseDriftGearUpBonus" ) )
	bool bDriftGearUpImmediateOnReverse = false;

	// ============================================================
	// ドリフトブースト中の攻撃判定（火花エフェクトと同じタイミングで発生する Sphere）
	// ============================================================

	// ドリフトブースト中にプレイヤー周囲へ攻撃判定（Sphere）を出すか。ダメージは AttackParameterTable の ChargeDrift＋現在ギア行（無ければ無ダメージ）
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Action|ChargeV2|Boost|Attack" )
	bool bEnableDriftAttack = true;

	// ドリフト攻撃判定 Sphere の半径（cm）
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Action|ChargeV2|Boost|Attack", meta = ( EditCondition = "bEnableDriftAttack", ClampMin = "0.0" ) )
	float DriftAttackRadius = 200.0f;

	// ドリフト攻撃判定 Sphere のローカルオフセット（X=前, Y=右, Z=上）。Z はカプセル半径ぶん自動で下がるため 0 で足元
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Action|ChargeV2|Boost|Attack", meta = ( EditCondition = "bEnableDriftAttack" ) )
	FVector DriftAttackOffset = FVector::ZeroVector;

	// 同一の相手へ連続ヒットさせない再アーム間隔（秒）。この間隔を空けて再度当たる
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Action|ChargeV2|Boost|Attack", meta = ( EditCondition = "bEnableDriftAttack", ClampMin = "0.0" ) )
	float DriftAttackReArmInterval = 0.3f;

	// エフェクトを発生させるための最低速度
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|ChargeV2|Boost", meta = ( EditCondition = "bEnableSpeedAndDriftChargeBonus" ) )
	float MinSpeedForBoostEffect = 2000.0f;

	// ドリフト判定時、壁ずりなどの微小な移動（ノイズ）を無視し、キャラの正面を進行方向として扱うための最低速度
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|ChargeV2|Boost", meta = ( EditCondition = "bEnableSpeedAndDriftChargeBonus" ) )
	float MinSpeedForVelocityDirection = 10.0f;

	// ドリフト（急旋回）中、エフェクトを旋回方向へ横に倒す最大角度（度数法）。Roll軸に適用される。
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|ChargeV2|Boost", meta = ( EditCondition = "bEnableSpeedAndDriftChargeBonus" ) )
	float ChargeBoostMaxLeanAngle = 60.0f;

	// ドリフトしていない時の傾き（度数法）。強制バーストは強度 0 なので 0 だと火花が真上に立つ。傾きは Base〜Max を強度で補間
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|ChargeV2|Boost", meta = ( EditCondition = "bEnableSpeedAndDriftChargeBonus" ) )
	float ChargeBoostBaseLeanAngle = 60.0f;

	// 火花を前後へ寝かせる静的オフセット（度数法・正で前傾・0 で Roll のみ）。旋回方向によらず常に一定量かかる
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|ChargeV2|Boost", meta = ( EditCondition = "bEnableSpeedAndDriftChargeBonus" ) )
	float ChargeBoostLeanPitchOffset = 0.0f;

	// ドリフトエフェクトの再生速度（VelocityRate）の最小値
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|ChargeV2|Boost|Effect" )
	float ChargeBoostMinVelocityRate = 0.5f;

	// ドリフトエフェクトの再生速度（VelocityRate）の最大値
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|ChargeV2|Boost|Effect" )
	float ChargeBoostMaxVelocityRate = 1.0f;

	// ギアアップ時の火花サイズバースト。通常は小さく保ち、上がった瞬間だけ満額サイズへ跳ねる（Min/Max 補間結果へ倍率として掛ける）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|ChargeV2|Boost|Effect" )
	bool bEnableDriftSparkGearUpBurst = true;

	// 通常時の火花サイズ倍率（1.0 で従来どおりの最大サイズ）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|ChargeV2|Boost|Effect", meta = ( EditCondition = "bEnableDriftSparkGearUpBurst", ClampMin = "0.0" ) )
	float DriftSparkNormalScale = 0.25f;

	// ギアアップ直後の火花サイズ倍率
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|ChargeV2|Boost|Effect", meta = ( EditCondition = "bEnableDriftSparkGearUpBurst", ClampMin = "0.0" ) )
	float DriftSparkGearUpScale = 2.0f;

	// ギアアップ後、満額サイズを保つ秒数
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|ChargeV2|Boost|Effect", meta = ( EditCondition = "bEnableDriftSparkGearUpBurst", ClampMin = "0.0" ) )
	float DriftSparkGearUpHoldTime = 0.2f;

	// 満額サイズから通常サイズへ戻すまでの秒数（0 で即座に戻す）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|ChargeV2|Boost|Effect", meta = ( EditCondition = "bEnableDriftSparkGearUpBurst", ClampMin = "0.0" ) )
	float DriftSparkGearUpBlendOutTime = 0.1f;

	// チャージ解放エフェクトの発生位置（キャラクターの正面方向へのオフセット距離）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|ChargeV2|Effect" )
	float ChargeReleaseEffectForwardOffset = 100.0f;

	// チャージダッシュ風切りエフェクトの発生位置（キャラクターの正面方向へのオフセット距離）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|ChargeV2|Effect" )
	float ChargeDashWindEffectForwardOffset = 200.0f;

	// ==========================================
	// チャージ中の基本移動・物理パラメータ
	// ==========================================

	// チャージ開始時の踏み込みパワー
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging" )
	float ChargeBeginMovePower = 1000.0f;

	// 進行方向を目標へ曲げる速さ（度/秒）。旋回継続ランプ有効時は「旋回し始めの旋回量」になる
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging" )
	float ChargingDriftSteeringSpeed = 90.0f;

	// 【機能検証用】スライドチャージ旋回の「継続ランプ」。ON で同方向へ旋回し続けるほど小さく曲がれる。OFF で固定値（ブレーキ処理には影響しない）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging|TurnRamp" )
	bool bEnableChargingTurnRamp = true;
	// 旋回継続中と判定される最小時間（秒）。この時間が経過するまでは ChargingDriftSteeringSpeed のまま
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging|TurnRamp" )
	float ChargingTurnRampMinDuration = 0.15f;
	// 旋回継続中の旋回量の最大値（度/秒）。ChargingDriftSteeringSpeed からこの値まで増加する
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging|TurnRamp" )
	float ChargingTurnRampMaxSteeringSpeed = 360.0f;
	// 旋回継続中の旋回量 UP レート（度/秒毎秒）。最小時間経過後、1秒あたりこの値だけ旋回量が増加する
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging|TurnRamp" )
	float ChargingTurnRampUpRate = 240.0f;

	// 旋回継続中にキャラ本体の回転力（RotationRate.Yaw）もランプさせるか。ON で振り向きも速くなる
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging|TurnRamp" )
	bool bEnableChargingTurnRampRotation = true;
	// 旋回継続中のキャラ回転力の最大値（度/秒）。基準の RotationRate.Yaw からこの値まで増加する
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging|TurnRamp", meta = ( EditCondition = "bEnableChargingTurnRampRotation" ) )
	float ChargingTurnRampMaxRotationRateYaw = 700.0f;
	// 旋回継続中のキャラ回転力 UP レート（度/秒毎秒）。最小時間経過後、1秒あたりこの値だけ回転力が増加する
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging|TurnRamp", meta = ( EditCondition = "bEnableChargingTurnRampRotation" ) )
	float ChargingTurnRampRotationUpRate = 480.0f;

	// チャージの移動イージング指数（大きいほど急激に変化）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging" )
	float ChargingEaseExpo = 5.0f;

	// 地面に吸い付くか
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging|GroundSnap" )
	bool bEnableChargingGroundSnapping = true;
	// レイ判定長さ
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging|GroundSnap" )
	float ChargingGroundSnapTraceDistance = 150.0f;
	// 地面に吸い付くためのトレース距離
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging|GroundSnap" )
	float ChargingGroundSnapDistance = 50.0f;

	// チャージ中の傾き反映速度補間(大きい値ほど早く反映される)
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging|Animation" )
	float ChargingDirectionInterpSpeed = 5.0f;

	// チャージダッシュLoop中の傾き反映速度補間(大きい値ほど早く反映される)
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging|Animation" )
	float ChargeDashDirectionInterpSpeed = 10.0f;

	// 物理サーフェスと NiagaraTag の紐づけ
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging|Effects" )
	TMap<TEnumAsByte<EPhysicalSurface>, FName> ChargeSkidEffectTagsMap;
	// スポーンさせる間隔（0.05秒〜0.1秒などにするとズザーッと繋がって見えます）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging|Effects" )
	float ChargeSkidEffectInterval = 0.08f;

	// 最大チャージ到達時の歩行速度倍率（0.0fで完全に停止）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging|Movement" )
	float MaxChargingWalkSpeedRate = 0.0f;
	// 最大チャージ到達時の旋回速度（0.0fで一切振り向けなくなる）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging|Movement" )
	float MaxChargingRotationRateYaw = 500.0f;

	// チャージ中の歩行速度倍率
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging|Movement" )
	float ChargingWalkSpeedRate = 1.0f;
	// チャージ中の重力スケール
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging|Movement" )
	float ChargingGravityScale = 1.0f;
	// 空中での移動制御の強さ（0.0f = 全く制御できない、1.0f = 地上と同じ制御）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging|Movement" )
	float ChargingAirControl = 0.35f;

	// キャラクターの旋回速度（PitchやRollは通常不要なためYawのみ）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging|Movement" )
	float ChargingRotationRateYaw = 500.0f;
	// キャラクターの旋回速度（空中 - 抑えめに設定）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging|Movement" )
	float ChargingFallingRotationRateYaw = 100.0f;

	// ※ ChargingWalkableFloorAngle は廃止（境界で歩行⇔落下のフリップが起きて勝手に加速するため）。面沿いモード中のみ SurfaceRideWalkableFloorAngle へ上げる

	// ============================================================
	// 【ギア別調整の規約】Slope 系は `***ForGear` 配列（要素0＝ギア壱）だけを設定元にする。
	//   参照は GetCurrentChargeGearIndex() ＋ GetValueForGear() を通す（範囲外は端へクランプ）。
	//   **空配列は全パラメータ 1.0 扱いで移動が破綻するので、要素は必ず MaxChargeGearCount ぶん残す。**
	// ============================================================

	// スライド中（および坂道計算時）の絶対的な最高速度上限（下り・上り共通）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging|Slope", meta = ( ClampMin = "0.0" ) )
	TArray<float> MaxSlideSpeedForGear = { 2400.0f, 3000.0f, 3900.0f };

	// 坂道処理が参照する床法線の補間速度（度/秒）。生の法線はポリゴンの継ぎ目で飛び、倍率・摩擦・速度上限が毎フレーム跳ねる。0 以下で補間なし
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging|Slope", meta = ( ClampMin = "0.0" ) )
	float FloorNormalSmoothSpeedDeg = 600.0f;

	// 足元の複数点をトレースして床法線を平均するか。時間方向の補間だけではポリゴンを跨ぐ段差が残るため、空間的にも均す
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging|Slope" )
	bool bEnableFloorNormalMultiSample = true;

	// 平均を取るサンプル点までの距離（cm）。大きいほど滑らかだが、細かい起伏では実際の足元の傾きとズレる
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging|Slope", meta = ( ClampMin = "1.0", EditCondition = "bEnableFloorNormalMultiSample" ) )
	float FloorNormalSampleRadius = 60.0f;

	// サンプル点のトレース長（cm）。足元から上下へこの長さぶん探す
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging|Slope", meta = ( ClampMin = "1.0", EditCondition = "bEnableFloorNormalMultiSample" ) )
	float FloorNormalSampleTraceLength = 200.0f;

	// 補間中と生の法線の差がこの角度（度）を超えたら即スナップする保険。小さいと正当な急変（トンネルの床→側壁）でも「ガクッ」となる
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging|Slope", meta = ( ClampMin = "0.0", ClampMax = "180.0" ) )
	float FloorNormalSnapAngle = 120.0f;

	// 床面へ投影した進行方向の「水平成分の縮み」をどれだけ打ち消すか（0〜1）。急斜面・側壁で水平速度が急減するのを防ぐ。1.0 で維持
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging|Slope", meta = ( ClampMin = "0.0", ClampMax = "1.0" ) )
	float ChargeDashSlopeSpeedPreserve = 1.0f;

	// 坂道倍率（歩行上限へ ×／摩擦・ブレーキへ ÷）の追従速度（FInterpTo）。0 で即時、6〜10 が目安。
	// 倍率の目標は面沿いモードの ON/OFF と上り／下り判定の反転で階段状に飛び、両方が同時に起きると
	// 歩行上限が上がりつつ摩擦が 1/倍率になって CMC が一瞬で加速する（＝「急に移動してガクッ」）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging|Slope", meta = ( ClampMin = "0.0" ) )
	float ChargingSlopeMultiplierInterpSpeed = 8.0f;

	// --- 下り坂（加速）---
	// 急斜面と判定する角度（度数法）。この角度以上の下り坂で一気にトップスピードへ加速する
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging|Slope|Downhill", meta = ( ClampMin = "0.0", ClampMax = "90.0" ) )
	TArray<float> SteepDescentAngleForGear = { 20.0f, 20.0f, 20.0f };

	// 下り坂補正の基本ボーナス（急斜面トップスピード時の最大倍率の基準）。滑り落ち力の基準も兼ねる（片方だけなら SlideGravityRate 側）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging|Slope|Downhill" )
	TArray<float> ChargingDownhillBaseBonusForGear = { 7.5f, 7.5f, 7.5f };

	// なだらかな下り坂（急斜面未満）での最大ボーナス抑制率（例: 0.3 でトップスピードの30%までしか出ない）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging|Slope|Downhill", meta = ( ClampMin = "0.0", ClampMax = "1.0" ) )
	TArray<float> GentleDownhillMaxBonusRateForGear = { 1.0f, 1.0f, 1.0f };

	// なだらかな下り坂における傾斜の影響度（カーブ）。数値を上げるほど、角度による加速の変化が急激になる
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging|Slope|Downhill", meta = ( ClampMin = "0.1" ) )
	TArray<float> DownhillSensitivityForGear = { 1.0f, 1.0f, 1.0f };

	// 下り坂補正倍率の下限。**下限は倍率を強制する**ので、低ギアを遅くしたいならボーナスだけでなくここも下げる
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging|Slope|Downhill" )
	TArray<float> MinChargingDownhillMultiplierForGear = { 5.0f, 5.0f, 5.0f };

	// 下り坂補正倍率の上限（例: 4.0f で最大4倍まで加速をクランプする）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging|Slope|Downhill" )
	TArray<float> MaxChargingDownhillMultiplierForGear = { 20.0f, 20.0f, 20.0f };

	// 下り坂で毎フレーム加算する滑り落ち力の基準倍率（力＝MaxWalkSpeed × DownhillBaseBonus × 本値 × SlopeAlpha × 下り成分）。倍率系とは別系統の直接加算
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging|Slope|Downhill", meta = ( ClampMin = "0.0" ) )
	TArray<float> ChargingDownhillSlideGravityRateForGear = { 0.5f, 0.5f, 0.5f };

	// --- 上り坂（減速）---
	// 急な上り坂と判定する角度（度数法）。この角度以上の上り坂で減速が最大になる
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging|Slope|Uphill", meta = ( ClampMin = "0.0", ClampMax = "90.0" ) )
	TArray<float> SteepAscentAngleForGear = { 20.0f, 20.0f, 20.0f };

	// 上り坂減速の基準強度（1.0 に近いほど強く減速）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging|Slope|Uphill", meta = ( ClampMin = "0.0" ) )
	TArray<float> ChargingUphillBasePenaltyForGear = { 0.45f, 0.45f, 0.45f };

	// なだらかな上り坂（急斜面未満）での最大減速抑制率（例: 1.0 で角度比そのまま、小さいほど緩い上りの減速を抑える）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging|Slope|Uphill", meta = ( ClampMin = "0.0", ClampMax = "1.0" ) )
	TArray<float> GentleUphillMaxRateForGear = { 0.35f, 0.35f, 0.35f };

	// なだらかな上り坂における傾斜の影響度（カーブ）。下りとは独立して調整できる
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging|Slope|Uphill", meta = ( ClampMin = "0.1" ) )
	TArray<float> UphillSensitivityForGear = { 1.0f, 1.0f, 1.0f };

	// 上り坂補正倍率の下限（1.0 未満で減速を許可。例: 0.2 で最大 1/5 まで減速）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging|Slope|Uphill", meta = ( ClampMin = "0.0" ) )
	TArray<float> MinChargingUphillMultiplierForGear = { 0.5f, 0.5f, 0.5f };

	// 上り坂補正倍率の上限（通常は 1.0＝等倍まで）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging|Slope|Uphill" )
	TArray<float> MaxChargingUphillMultiplierForGear = { 1.0f, 1.0f, 1.0f };

	// --- 面沿い移動モード（スケボー感。CMC の重力方向を床法線へ向ける）---
	// 急斜面を勢いで駆け上がる仕組みはこれ 1 本（登坂角度だけ緩める旧スロープライド方式は、上限角度を
	// 超えた瞬間に床から外れて射出される限界があり撤去済み）。重力を床法線の逆向きへ向けると「その面が床」
	// になり上限角度の概念が消える。戻る力は本来の重力の面沿い成分で作る

	// 面沿い移動モードを有効化する
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging|Slope|SurfaceRide" )
	bool bEnableSurfaceRide = true;

	// 面沿いモードを ASurfaceRideZone のエリア内でのみ開始できるようにする（OFF ではなんでもない壁を登れてしまう）。
	// **エリアを1つも置いていないと発動しなくなる点に注意**
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging|Slope|SurfaceRide", meta = ( EditCondition = "bEnableSurfaceRide" ) )
	bool bSurfaceRideRequireZone = true;

	// エリア内でチャージ中に適用する登坂角度（度数法）。面沿いは「急斜面に接地」が条件なので、先に接地できる状態を作るために上げる
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging|Slope|SurfaceRide", meta = ( ClampMin = "0.0", ClampMax = "90.0", EditCondition = "bEnableSurfaceRide" ) )
	TArray<float> SurfaceRideWalkableFloorAngleForGear = { 85.0f, 85.0f, 85.0f };

	// 面沿いモードへ入る床の傾斜（度数法）。これを超える斜面に乗ると重力を面法線へ向け始める
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging|Slope|SurfaceRide", meta = ( ClampMin = "0.0", ClampMax = "90.0", EditCondition = "bEnableSurfaceRide" ) )
	TArray<float> SurfaceRideEnterAngleForGear = { 25.0f, 25.0f, 25.0f };

	// 面沿いモードを抜ける床の傾斜（度数法）。ギアごとに入る角度より小さく保たないとヒステリシスが消えて境界で点滅する
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging|Slope|SurfaceRide", meta = ( ClampMin = "0.0", ClampMax = "90.0", EditCondition = "bEnableSurfaceRide" ) )
	TArray<float> SurfaceRideExitAngleForGear = { 15.0f, 15.0f, 15.0f };

	// 面沿いモードへ入るのに必要な速度（cm/s）。低速でその辺の斜面に貼り付かないようにする
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging|Slope|SurfaceRide", meta = ( ClampMin = "0.0", EditCondition = "bEnableSurfaceRide" ) )
	TArray<float> SurfaceRideMinSpeedForGear = { 400.0f, 400.0f, 400.0f };

	// チャージ幅跳びでも面沿いモードへ入れる（壁へ飛び乗る入り方）。幅跳びは着地後ダッシュへ戻るため乗ったまま走り続けられる
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging|Slope|SurfaceRide", meta = ( EditCondition = "bEnableSurfaceRide" ) )
	bool bSurfaceRideAllowChargeHopEntry = true;

	// 面沿い中の幅跳びを通常チャージジャンプへ差し替える（壁からの脱出手段）。幅跳びのままだとチャージが途切れず面沿いが解除されない
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging|Slope|SurfaceRide", meta = ( EditCondition = "bEnableSurfaceRide" ) )
	bool bSurfaceRideChargeHopBecomesChargeJump = true;

	// 重力方向を目標（床法線の逆向き／真下）へ向ける回転速度（度/秒）。速すぎると出入りでガクつき、遅いと面への追従が遅れて浮く
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging|Slope|SurfaceRide", meta = ( ClampMin = "1.0", EditCondition = "bEnableSurfaceRide" ) )
	float SurfaceRideGravityInterpSpeedDeg = 900.0f;

	// キャラクターの姿勢を重力方向へ合わせる回転速度（度/秒）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging|Slope|SurfaceRide", meta = ( ClampMin = "1.0", EditCondition = "bEnableSurfaceRide" ) )
	float SurfaceRideRotationInterpSpeedDeg = 540.0f;

	// 追従（法線平滑化・重力方向・姿勢）の遅れ比例ゲイン（1/度）。角度差 1 度につき補間速度を (1 + 差×本値) 倍し、高速周回で溜まる遅れを詰める。0 で定速
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging|Slope|SurfaceRide", meta = ( ClampMin = "0.0", EditCondition = "bEnableSurfaceRide" ) )
	float SurfaceRideTrackingCatchupGain = 0.4f;

	// 面沿いモード中の「戻る力」倍率。重力を面法線へ向けると落下が消えるため |g|×sin(傾斜) を明示的に加えて滑り降りさせる
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging|Slope|SurfaceRide", meta = ( ClampMin = "0.0", EditCondition = "bEnableSurfaceRide" ) )
	TArray<float> SurfaceRideSlideBackScaleForGear = { 1.0f, 1.0f, 1.0f };

	// --- 上り坂アシスト（ASurfaceRideZone の bUphillAssist が ON のエリアだけで効く「嘘」）---
	// 物理準拠だと上りは登るほど減速するため、「上りでも減速せず少し伸びる」手触りを作る差し替え値
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging|Slope|SurfaceRideAssist", meta = ( EditCondition = "bEnableSurfaceRide" ) )
	bool bEnableSurfaceRideUphillAssist = true;

	// アシストエリアで SurfaceRideSlideBackScale の代わりに使う戻る力倍率。0 で壁に張り付いたまま止まれてしまう
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging|Slope|SurfaceRideAssist", meta = ( ClampMin = "0.0", EditCondition = "bEnableSurfaceRide && bEnableSurfaceRideUphillAssist" ) )
	TArray<float> SurfaceRideAssistSlideBackScaleForGear = { 0.15f, 0.15f, 0.15f };

	// アシストエリアで進行方向へ足す加速（cm/s^2）。登り成分に比例するので平坦・下りでは 0（二重掛けにならない）。チャージスライドに効く
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging|Slope|SurfaceRideAssist", meta = ( ClampMin = "0.0", EditCondition = "bEnableSurfaceRide && bEnableSurfaceRideUphillAssist" ) )
	TArray<float> SurfaceRideAssistUphillAccelForGear = { 1500.0f, 1500.0f, 1500.0f };

	// アシストエリアでのチャージダッシュのロック速度倍率（登り成分に比例して 1.0 → 本値）。ダッシュは接線速度をハードセットするため上の加速加算が効かない
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging|Slope|SurfaceRideAssist", meta = ( ClampMin = "1.0", EditCondition = "bEnableSurfaceRide && bEnableSurfaceRideUphillAssist" ) )
	TArray<float> SurfaceRideAssistDashSpeedScaleForGear = { 1.1f, 1.1f, 1.1f };

	// アシストが速度を伸ばせる上限（cm/s）。0 で MaxSlideSpeed を使う。下りで出た速度を更に伸ばさないための蓋
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging|Slope|SurfaceRideAssist", meta = ( ClampMin = "0.0", EditCondition = "bEnableSurfaceRide && bEnableSurfaceRideUphillAssist" ) )
	float SurfaceRideAssistMaxSpeed = 0.0f;

	// 面沿い中の入力基準（接平面での前方）を不安定とみなすしきい値＝カメラ前方と床法線のなす角の sin（既定 0.35 ≒ 法線から 20°）。
	// 割ったフレームは前フレームの基準を持ち回し、入力方向が一瞬あらぬ方向へ振れて急加速するのを防ぐ。0 で無効
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging|Slope|SurfaceRide", meta = ( ClampMin = "0.0", ClampMax = "0.9", EditCondition = "bEnableSurfaceRide" ) )
	float SurfaceRideInputDegenerateSin = 0.35f;

	// 入力基準が接平面内で回れる最大角速度（度/秒）。0 で即時。縮退域の出入りで基準が飛ぶケースだけ均す上記しきい値の保険
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging|Slope|SurfaceRide", meta = ( ClampMin = "0.0", EditCondition = "bEnableSurfaceRide" ) )
	float SurfaceRideInputForwardTurnSpeedDeg = 540.0f;

	// 【重要】重力方向（＝CMC の移動が乗る平面）の目標に「平滑化前の生の接地法線」を使う。
	// 平滑化済み法線は実際の接地面から傾き（実測：床 38°の曲面で 16〜25°）、そのズレぶん接線速度が
	// 毎フレーム地形へ押し込む → めり込み解消（ResolvePenetration）で突然の打ち上げになる。OFF で従来
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging|Slope|SurfaceRide", meta = ( EditCondition = "bEnableSurfaceRide" ) )
	bool bSurfaceRideGravityUseRawNormal = true;

	// めり込み時に CMC が 1 回の移動で押し出せる最大距離（cm）。0 でエンジン既定（500cm＝1フレームで 5m 飛ぶ）。絞ると打ち上げの被害が小さくなる
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = ( ClampMin = "0.0" ) )
	float MaxDepenetrationWithGeometry = 100.0f;

	// 空中へ出たあとモードを維持する猶予（秒）。段差で一瞬浮くたびに解除しないため。エリア外へ出た時点で打ち切り
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging|Slope|SurfaceRide", meta = ( ClampMin = "0.0", EditCondition = "bEnableSurfaceRide" ) )
	TArray<float> SurfaceRideAirGraceForGear = { 0.2f, 0.2f, 0.2f };

	// --- エリア（ASurfaceRideZone）から出たときの解除 ---
	// 壁の上で出ると沿面速度がそのまま射出になるため速度を削る。緩い床で出たときは一切効かない

	// OFF で旧挙動（速度を完全にゼロクリア）。ON で下の残存率・上限とジャンプモーションが効く
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging|Slope|SurfaceRideZoneExit", meta = ( EditCondition = "bEnableSurfaceRide" ) )
	bool bSurfaceRideZoneExitSoftRelease = true;

	// 解除時の水平成分の倍率（1.0 で素通し・**1.0 超で元の慣性より強く飛ばす**）。垂直に近い壁では下の Vertical が支配的
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging|Slope|SurfaceRideZoneExit", meta = ( ClampMin = "0.0", UIMax = "3.0", EditCondition = "bEnableSurfaceRide && bSurfaceRideZoneExitSoftRelease" ) )
	float SurfaceRideZoneExitHorizontalScale = 1.0f;

	// 解除時の上向き成分の倍率。射出の本体はここ（0 で打ち上げが消えて急停止に近くなる）。下向き成分は対象外
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging|Slope|SurfaceRideZoneExit", meta = ( ClampMin = "0.0", UIMax = "3.0", EditCondition = "bEnableSurfaceRide && bSurfaceRideZoneExitSoftRelease" ) )
	float SurfaceRideZoneExitVerticalScale = 1.0f;

	// 上の残存率を掛けたあとの速度上限（cm/s・0 で無制限）。割合だけでは元が速いほど残りも増えるため飛距離の最悪値を抑える
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging|Slope|SurfaceRideZoneExit", meta = ( ClampMin = "0.0", EditCondition = "bEnableSurfaceRide && bSurfaceRideZoneExitSoftRelease" ) )
	float SurfaceRideZoneExitMaxSpeed = 0.0f;

	// 解除時にチャージジャンプのモーションを ST から流す（遷移は UJumpActionPlayerModule::EnterChargeJumpStart）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging|Slope|SurfaceRideZoneExit", meta = ( EditCondition = "bEnableSurfaceRide && bSurfaceRideZoneExitSoftRelease" ) )
	bool bSurfaceRideZoneExitUseChargeJumpMotion = true;

	// エリア内でも壁・天井の上で面沿いが解除されたら同じモーションを流す。エリア退出と違い**速度もチャージも触らない**。他のモーション再生中は流さない
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging|Slope|SurfaceRide", meta = ( EditCondition = "bEnableSurfaceRide" ) )
	bool bSurfaceRideFallUseChargeJumpMotion = true;

	// 上記の落下からダッシュへ戻るとき、着地モーション（CHARGE_JUMP_ED）を見せる時間（秒）。ダッシュは続くので最後まで流さない。0 で即ダッシュ
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging|Slope|SurfaceRide", meta = ( ClampMin = "0.0", EditCondition = "bEnableSurfaceRide && bSurfaceRideFallUseChargeJumpMotion" ) )
	float SurfaceRideFallLandingEdTime = 0.15f;

	// ※空中チャージダッシュ専用カメラの調整数値は UAirChargeDashExCameraMode 側（BP）に集約している

	// 旋回を完全にロックし、強いブレーキをかける閾値
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging|Brake" )
	float ChargingFullBrakeDotThreshold = -0.8f;
	// 旋回を許可しつつ、若干のブレーキをかけ始める閾値
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging|Brake" )
	float ChargingHalfBrakeDotThreshold = -0.5f;
	// 完全ブレーキ時のVelocity減衰スピード（急停止）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging|Brake" )
	float ChargingFullBrakeDecelerationSpeed = 2.0f;
	// 半ブレーキ（旋回中）時のVelocity減衰スピード（緩やかな減速）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging|Brake" )
	float ChargingHalfBrakeDecelerationSpeed = 0.05f;
	// 半ブレーキ時のキャラクター旋回速度倍率（通常チャージ時より高めに設定。例: 1.5f〜2.0f）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging|Brake" )
	float ChargingHalfBrakeRotationSpeedRate = 1.5f;
	// 半ブレーキ時の進行ベクトル引き込み速度（通常のドリフトより鋭く曲がる。例: 360.0f）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging|Brake" )
	float ChargingHalfBrakeSteeringSpeed = 360.0f;

	// ============================================================
	// ドリフト中の速度維持／加速（連続ドリフト）
	//   CMC の方向転換は角度差のぶん速度が縮む（減衰率 ≒ GroundFriction ×（1-cosθ）/秒）ため、放置すると
	//   ドリフト成立の下限（MinSpeedForChargeBonus）を割って連続ドリフトできない。その摩擦ロスを打ち消す設定
	// ============================================================

	// 【機能検証用】ドリフト中の速度維持を有効にするか。OFF で従来どおり（摩擦に食われるまま）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging|Drift" )
	bool bEnableDriftSpeedMaintain = true;

	// ドリフト開始時の速度に掛ける維持倍率（1.0=等倍維持／1.0超で開始時に上乗せ）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging|Drift", meta = ( EditCondition = "bEnableDriftSpeedMaintain", ClampMin = "0.0" ) )
	float DriftSpeedMaintainRate = 1.0f;

	// ドリフト継続中の加速（cm/秒^2）。0 で等倍維持のみ、正値でドリフトし続けるほど速くなる
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging|Drift", meta = ( EditCondition = "bEnableDriftSpeedMaintain", ClampMin = "0.0" ) )
	float DriftSpeedAccelPerSec = 0.0f;

	// 維持／加速の速度上限（cm/秒）。0 で MaxSlideSpeed を使う
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging|Drift", meta = ( EditCondition = "bEnableDriftSpeedMaintain", ClampMin = "0.0" ) )
	float DriftSpeedMaxSpeed = 0.0f;

	// ドリフト中の地面摩擦倍率（ChargingGroundFriction へ掛ける）。下げるほど方向転換ロスが減る（0 で向きの制御を自前のステアリングへ任せる）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging|Drift", meta = ( EditCondition = "bEnableDriftSpeedMaintain", ClampMin = "0.0", ClampMax = "1.0" ) )
	float DriftGroundFrictionRate = 0.25f;

	// 地面の摩擦係数（高いほど滑りにくい）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging|Movement|Friction" )
	float ChargingGroundFriction = 8.0f;
	// 歩行中のブレーキ減速度（高いほど急停止する）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging|Movement|Friction" )
	float ChargingBrakingDecelerationWalking = 2000.0f;
	// 空中でのブレーキ減速度（高いほど急停止する）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charging|Movement|Friction" )
	float ChargingBrakingDecelerationFalling = 1500.0f;

	// ==========================================
	// チャージ後アクション
	// ==========================================

	// チャージに必要な最大時間（この時間でチャージ完了とみなす）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charged" )
	float MaxChargeTime = 1.25f;
	// チャージ開始からこの時間以上経過していればダッシュが発動可能になる
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charged" )
	float MinChargeTimeForDash = 1.25f;

	// 正面衝突判定の閾値（真正面が -1.0。-0.1f より小さければ正面気味にぶつかったと判定）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charged" )
	float ChargeMoveStopThreshold = -0.1f;

	// チャージアクション終了後、通常の摩擦・ブレーキへ滑らかに戻すのにかける時間（秒）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charged" )
	float ChargeActionFrictionRecoveryTime = 1.0f;
	// チャージアクション終了後、通常の摩擦・ブレーキへ滑らかに戻す際のイージング指数（大きいほど最初は滑りやすく、最後はキュッと止まる）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charged" )
	float ChargeActionFrictionRecoveryEaseExpo = 3.0f;

	// チャージ攻撃の出し切り後だけ別管理する摩擦回復時間（秒）。0 以下なら ChargeActionFrictionRecoveryTime へフォールバック
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charged" )
	float ChargeAttackFrictionRecoveryTime = 5.0f;

	// チャージダッシュの最小初速
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charged|Dash" )
	float MinChargeDashMoveSpeed = 1000.0f;
	// チャージダッシュの最大初速
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charged|Dash" )
	float MaxChargeDashMoveSpeed = 2000.0f;
	// 最小チャージダッシュ実行秒数
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charged|Dash" )
	float MinChargeDashTime = 0.2f;
	// 最大チャージダッシュ実行秒数
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charged|Dash" )
	float MaxChargeDashTime = 0.4f;

	// チャージダッシュ中の旋回速度
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charged|Dash|Movement" )
	float ChargeDashRotationRateYaw = 100.0f;
	// チャージダッシュターン再生中の回頭補助速度（deg/秒）。ルートモーションの旋回に加え、ターン明けの出口方向を入力方向へ寄せる。0 以下で補助なし
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charged|Dash|Movement" )
	float ChargeDashTurnSteerRateYaw = 300.0f;
	// 地面の摩擦係数（高いほど滑りにくい）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charged|Dash|Movement|Friction" )
	float ChargeDashGroundFriction = 2.0f;
	// 歩行中のブレーキ減速度（高いほど急停止する）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charged|Dash|Movement|Friction" )
	float ChargeDashBrakingDecelerationWalking = 1000.0f;
	// 空中でのブレーキ減速度（高いほど急停止する）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charged|Dash|Movement|Friction" )
	float ChargeDashBrakingDecelerationFalling = 1500.0f;

	// チャージ攻撃の最小初速
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charged|Attack" )
	float MinChargeAttackMovePower = 1000.0f;
	// チャージ攻撃の最大初速
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charged|Attack" )
	float MaxChargeAttackMovePower = 3000.0f;
	// 最小チャージ攻撃発動後、この時間が経過するまでは自前の移動入力を無効化する
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charged|Attack" )
	float MinChargeAttackMovementLockTime = 0.6f;
	// 最大チャージ攻撃発動後、この時間が経過するまでは自前の移動入力を無効化する
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charged|Attack" )
	float MaxChargeAttackMovementLockTime = 0.6f;
	// 地上チャージ攻撃の出し切り後、ChargeAttackGearカメラを通常へ戻す(Pop)までの追加ホールド時間。0でアクション終了と同時にPop。カメラの戻りだけ後ろ倒しする
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charged|Attack", meta = ( ClampMin = "0.0" ) )
	float ChargeAttackCameraHoldTime = 1.0f;

	// --- チャージ攻撃の対象詰め（攻撃判定の発生タイミングに合わせて対象手前まで移動する） ---
	// 有効化フラグ。OFF または対象なし／判定時刻取得失敗時は、従来どおり開始時の弾道射出になる
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charged|Attack|Approach" )
	bool bEnableChargeAttackTimedApproach = true;
	// 到達点を対象位置からどれだけ手前にするか（cm）。対象カプセル半径＋武器リーチ相当を見込む
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charged|Attack|Approach" )
	float ChargeAttackApproachStandoff = 150.0f;
	// イーズ詰めの指数（1=等速、2以上で終盤に一気に詰める加速感が強まる）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charged|Attack|Approach", meta = ( ClampMin = "1.0" ) )
	float ChargeAttackApproachEaseExp = 3.0f;
	// 詰め中の床追従トレースの上下範囲（cm）。到達 XY 直下の歩行可能床へ高さを合わせ、上り坂の敵にも詰められる。0 で高さ据え置き（従来）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charged|Attack|Approach", meta = ( ClampMin = "0.0" ) )
	float ChargeAttackApproachFloorProbe = 300.0f;

	// ギア段階ごとの吸着距離（Index 0 = ギア1, Index 1 = ギア2...）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charged|Homing" )
	TArray<float> ChargeAttackHomingDistanceForGear = { 1500.0f, 2000.0f, 2500.0f, 3000.0f };

	// ギア段階ごとの吸着角度（Index 0 = ギア1, Index 1 = ギア2...）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charged|Homing" )
	TArray<float> ChargeAttackHomingAngleForGear = { 10.0f, 15.0f, 30.0f, 45.0f };

	// 【チャージダッシュ専用】ギア段階ごとの吸着距離（Index 0=ギア1...）。チャージ攻撃とは独立に調整
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charged|Homing" )
	TArray<float> ChargeDashHomingDistanceForGear = { 1500.0f, 1500.0f, 1500.0f, 1500.0f };

	// 【チャージダッシュ専用】ギア段階ごとの吸着角度（Index 0 = ギア1, Index 1 = ギア2...）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charged|Homing" )
	TArray<float> ChargeDashHomingAngleForGear = { 20.0f, 20.0f, 20.0f, 20.0f };

	// 【チャージ攻撃】吸着扇の頂点をプレイヤーから後方へ下げる量（cm）。大きいほど横の敵を拾いやすい
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charged|Homing", meta = ( ClampMin = "0.0" ) )
	float ChargeAttackHomingApexBackOffset = 0.0f;

	// 【チャージダッシュ】吸着扇の頂点をプレイヤーから後方へ下げる量（cm）。大きいほど横の敵を拾いやすい
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charged|Homing", meta = ( ClampMin = "0.0" ) )
	float ChargeDashHomingApexBackOffset = 300.0f;

	// 【チャージ攻撃】吸着を許す高さ（Z）差の上限（cm）。距離・角度は水平判定なのでこれが唯一の上下の足切り。0 で無制限
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charged|Homing", meta = ( ClampMin = "0.0" ) )
	float ChargeAttackHomingMaxHeightDiff = 600.0f;

	// 【チャージダッシュ】吸着を許す高さ（Z）差の上限（cm）。0 で無制限（従来どおり）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charged|Homing", meta = ( ClampMin = "0.0" ) )
	float ChargeDashHomingMaxHeightDiff = 600.0f;

	// 【チャージダッシュ】吸着対象への回頭ロックを解除する到達距離（cm）。水平距離がこれ以下でロック解除し操舵を返す
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charged|Homing", meta = ( ClampMin = "0.0" ) )
	float ChargeDashHomingReachDistance = 150.0f;

	// 地面の摩擦係数（高いほど滑りにくい）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charged|Attack|Movement|Friction" )
	float ChargeAttackGroundFriction = 1.0f;
	// 歩行中のブレーキ減速度（高いほど急停止する）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charged|Attack|Movement|Friction" )
	float ChargeAttackBrakingDecelerationWalking = 0.0f;
	// 空中でのブレーキ減速度（高いほど急停止する）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charged|Attack|Movement|Friction" )
	float ChargeAttackBrakingDecelerationFalling = 1500.0f;

	// ============================================================
	// 通常チャージジャンプ（現行 V2・ギア別パラメータに一本化）
	//   ジャンプ力(Z)・前方初速(XY)・移動ロック時間はギア別配列（要素0＝ギア壱）で指定する。
	//   配列が空のときだけ末尾「旧V1用（Deprecated）」の Min/Max をギア線形補間してフォールバック
	// ============================================================

	// ジャンプ後の空中でチャージジャンプを封印するか。ON でジャンプ入力は通常ジャンプへ流れ、チャージジャンプは接地専用になる
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charged|Jump" )
	bool bBlockChargeJumpAfterNormalJump = true;

	// 【ギア別】通常チャージジャンプのジャンプ力（Z方向初速＝高さ）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charged|Jump" )
	TArray<float> ChargeJumpPowerForGear = { 1200.0f, 1600.0f, 2000.0f };

	// 【ギア別】通常チャージジャンプの前方初速（XY方向＝飛距離）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charged|Jump" )
	TArray<float> ChargeJumpMovePowerForGear = { 0.0f, 500.0f, 1000.0f };

	// 【ギア別】チャージジャンプ発動後、この時間ぶん自前の移動入力を無効化する（移動ロック時間・秒）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charged|Jump" )
	TArray<float> ChargeJumpMovementLockTimeForGear = { 0.2f, 0.2f, 0.2f };

	// チャージジャンプの移動量割合
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charged|Jump|Movement" )
	float ChargeJumpWalkSpeedRate = 1.0f;

	// 【通常チャージジャンプ用】チャージ直前の水平移動速度を発射初速へ引き継ぐ倍率（0で引き継ぎ無効、1で100%そのまま加算）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charged|Jump|Movement", meta = ( ClampMin = "0.0" ) )
	float ChargeJumpInheritedSpeedRate = 1.0f;
	// 【通常チャージジャンプ用】引き継ぐ水平移動速度の上限（0以下で上限なし）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charged|Jump|Movement", meta = ( ClampMin = "0.0" ) )
	float ChargeJumpMaxInheritedSpeed = 0.0f;

	// ============================================================
	// チャージホップ（チャージダッシュ中のジャンプから派生する、短く細かく刻めるジャンプ）
	//   ジャンプ力・前方初速は発動元ダッシュのギア別配列。モーション・移動ロック時間はギア壱固定
	// ============================================================

	// チャージホップを有効にするか。ON＝ダッシュ中ジャンプをギア壱固定モーションのホップ扱いにし着地後もダッシュ継続、OFF＝通常チャージジャンプ扱い
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charged|Hop" )
	bool bEnableChargeHop = true;

	// 【ギア別】チャージホップのジャンプ力（Z方向初速＝高さ）。発動元チャージダッシュのギア段階で引く
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charged|Hop" )
	TArray<float> ChargeHopPowerForGear = { 1200.0f, 1300.0f, 1400.0f };

	// ※ 幅跳びの前方初速・引き継ぎ倍率／上限（旧 ChargeHopMovePower / ChargeHopInherited*）は廃止。水平は下の
	//    MoveSpeed 方式に一本化（発射直前の水平速度を全量引き継ぎ ChargeHopMaxMoveSpeedForGear でクランプ）。
	//    通常チャージジャンプ側の ChargeJumpInheritedSpeedRate / ChargeJumpMaxInheritedSpeed は従来どおり

	// ------------------------------------------------------------
	// 幅跳び（ホップ）の水平速度＝MoveSpeed 方式
	//   初速を1回入れるだけだと滞空中・着地ED中に CMC のブレーキへ食われ、連続幅跳びで慣性が落ち続ける。
	//   水平速度の「大きさ」を毎フレーム維持目標へ当て直して複利を消す（UpdateChargeHopSpeedMaintain）。
	//   向きはエアコントロールでの操舵結果を優先する
	// ------------------------------------------------------------

	// 【ギア別】幅跳び中の水平維持速度（cm/s・発射初速も同値）。0 以下なら発射直前の水平速度をそのまま引き継いで維持
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charged|Hop|Movement", meta = ( ClampMin = "0.0" ) )
	TArray<float> ChargeHopMoveSpeedForGear = { 1500.0f, 2100.0f, 3000.0f };

	// 【ギア別】幅跳びの水平速度上限（cm/s・0以下で無制限）。発射初速と維持目標の両方に効く
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charged|Hop|Movement", meta = ( ClampMin = "0.0" ) )
	TArray<float> ChargeHopMaxMoveSpeedForGear = { 1500.0f, 2100.0f, 3000.0f };

	// 維持目標の減衰（cm/s^2・0 で完全維持）。滞空が長いほど速度を落としたいとき用
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charged|Hop|Movement", meta = ( ClampMin = "0.0" ) )
	float ChargeHopSpeedDecayPerSec = 0.0f;

	// 着地〜幅跳びED の接地区間でも維持を続けるか。ED 中が一番削られる区間なので ON で連続幅跳びの初速が安定する
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charged|Hop|Movement" )
	bool bChargeHopMaintainDuringLanding = true;

	// ------------------------------------------------------------
	// 幅跳び（ホップ）の坂対応
	//   ワールド真上へ射出すると上り坂で滞空が T ≒ 2 × ( Z初速 − 水平速度 × tanθ ) / 重力 まで縮み、
	//   ほぼ 0 になると持続時間も消費されず「上り坂でジャンプ連打」ができてしまう。対処は2系統：
	//   【A】坂へ沿わせて滞空を取り戻す（SlopeAlign）／【B】1ホップに固定コストと回数上限を持たせる（Chain）
	// ------------------------------------------------------------

	// 【A】幅跳びの射出を床面へ沿わせるか。ON で平滑化済み床法線を上方向として射出（向きだけ変わり大きさは同じ）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charged|Hop|Slope" )
	bool bEnableChargeHopSlopeAlign = true;

	// 沿わせる強さ（0＝従来どおりワールド真上／1＝床法線そのまま）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charged|Hop|Slope", meta = ( ClampMin = "0.0", ClampMax = "1.0" ) )
	float ChargeHopSlopeAlignRate = 1.0f;

	// 傾ける角度の上限（度・0以下で床角度そのまま）。沿わせるほど坂を階段状に登れてしまうのでここで抑える
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charged|Hop|Slope", meta = ( ClampMin = "0.0" ) )
	float ChargeHopSlopeAlignMaxAngleDeg = 20.0f;

	// 上り坂だけ沿わせるか。OFF なら下りも沿わせる（下りは射出が下向きへ傾いて滞空が縮むため既定は上りのみ）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charged|Hop|Slope" )
	bool bChargeHopSlopeAlignUphillOnly = true;

	// 【B】連続幅跳び1回ぶんの持続時間コスト（秒・0で無効）。再ホップごとに発動元ダッシュの残り時間から引き、払えなくなったら通常ジャンプへ落とす
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charged|Hop|Chain", meta = ( ClampMin = "0.0" ) )
	float ChargeHopTimeCostPerHop = 0.25f;

	// 【B】連続幅跳びの最大回数（0以下で無制限・1回目を含む）。以降は通常ジャンプになり、ダッシュへ戻る／アクション終了で数え直す
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charged|Hop|Chain", meta = ( ClampMin = "0" ) )
	int32 ChargeHopMaxChainCount = 0;

	// 幅跳び終了からチャージダッシュカメラを Pop するまでの遅延（秒・0で終了と同時）。窓の間に次のアクションが同じカメラを要求すれば維持され、Pop→Push のガタつきを防ぐ
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charged|Hop|Camera", meta = ( ClampMin = "0.0" ) )
	float ChargeHopCameraPopDelayTime = 1.0f;

	// ============================================================
	// 旧V1用（Deprecated）チャージジャンプ Min/Max パラメータ
	//   旧 UChargeActionPlayerModule（削除予定）用。V2 はギア別配列が空のときだけ線形補間で参照する
	// ============================================================

	// [V1] チャージジャンプの前方初速（最小／最大）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charged|Jump|Legacy(V1)" )
	float MinChargeJumpMovePower = 1500.0f;
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charged|Jump|Legacy(V1)" )
	float MaxChargeJumpMovePower = 3000.0f;
	// [V1] チャージジャンプ発動後の移動ロック時間（最小／最大・秒）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charged|Jump|Legacy(V1)" )
	float MinChargeJumpMovementLockTime = 0.2f;
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charged|Jump|Legacy(V1)" )
	float MaxChargeJumpMovementLockTime = 0.6f;
	// [V1] チャージジャンプのジャンプ力Z（最小／最大）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charged|Jump|Legacy(V1)" )
	float MinChargeJumpPower = 500.0f;
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charged|Jump|Legacy(V1)" )
	float MaxChargeJumpPower = 1100.0f;

	// チャージジャンプ中の空中制御力
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charged|Jump|Movement" )
	float ChargeJumpAirControl = 0.8f;
	// チャージジャンプ中の空中制御ブースト倍率
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charged|Jump|Movement" )
	float ChargeJumpAirControlBoostMultiplier = 3.0f;
	// チャージジャンプ中の空中制御ブースト閾値
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charged|Jump|Movement" )
	float ChargeJumpAirControlBoostVelocityThreshold = 50.0f;

	// 地面の摩擦係数（高いほど滑りにくい）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charged|Jump|Movement|Friction" )
	float ChargeJumpGroundFriction = 2.0f;
	// 空中でのブレーキ減速度（高いほど急停止する）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charged|Jump|Movement|Friction" )
	float ChargeJumpBrakingDecelerationFalling = 1500.0f;

	// ガードブレーキ開始直後（もっとも滑っている時）の最大旋回速度
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charged|GuardBrake" )
	float ChargeGuardBrakeMaxTurnSpeed = 2.0f;
	// ガードブレーキ停止時（滑り終わった時）の最小旋回速度（基本は0.0fで固定化）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charged|GuardBrake" )
	float ChargeGuardBrakeMinTurnSpeed = 0.0f;
	// ガードブレーキで滑り続ける時間（＝徐々に旋回できなくなるまでの猶予時間）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charged|GuardBrake" )
	float ChargeGuardBrakeSlideTime = 1.0f;
	// ガードブレーキ中の減速力（値が小さいほど遠くまで滑る。）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charged|GuardBrake" )
	float ChargeGuardBrakeDeceleration = 3000.0f;
	// ガードブレーキ中の移動方向の曲がりやすさ（高いほど向いた方向へ鋭くカーブする）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charged|GuardBrake" )
	float ChargeGuardBrakeDirectionShiftSpeed = 3.0f;
	// ガードブレーキを短押しして離した際、移動入力があればダッシュへ移行できる猶予時間
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Charged|GuardBrake" )
	float ChargeGuardBrakeDashCancelWindow = 0.25f;

	// ==========================================
	// 弱攻撃（Light Attack）の吸着設定
	// ==========================================

	// 地上の弱攻撃コンボ（LightAttack01〜04）を出せるか。OFF で全マスク（チャージ最終段の締め弱攻撃も出ない）。空中の振り下ろしは対象外
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|LightAttack" )
	bool bEnableGroundNormalAttack = true;

	// 弱攻撃コンボの最大段数（1〜4）。段 N は "LightAttack0N" のモンタージュとテーブル行を引く（最終段のモンタージュには CanCombo を付けない）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|LightAttack", meta = ( ClampMin = "1", ClampMax = "4" ) )
	int32 MaxLightComboCount = 4;

	// 前段モンタージュの残留タグを無視する時間（秒）。NotifyEnd はブレンドアウト完了まで遅れるため、窓が無いと段飛びする。AttackBufferTime より短くすること
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|LightAttack", meta = ( ClampMin = "0.0" ) )
	float LightAttackTagResidueIgnoreTime = 0.1f;

	// 弱攻撃の吸着を探す最大距離
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|LightAttack|Homing" )
	float LightAttackHomingDistance = 1000.0f;

	// 弱攻撃の吸着を判定する前方角度（左右合計）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|LightAttack|Homing" )
	float LightAttackHomingAngle = 90.0f;

	// 弱攻撃の吸着扇の頂点をプレイヤーから後方へ下げる量（cm）。大きいほど横の敵を拾いやすい
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|LightAttack|Homing", meta = ( ClampMin = "0.0" ) )
	float LightAttackHomingApexBackOffset = 0.0f;

	// 弱攻撃の吸着を許す高さ（Z）差の上限（cm）。距離・角度は水平判定なのでこれが唯一の上下の足切り。0 で無制限
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|LightAttack|Homing", meta = ( ClampMin = "0.0" ) )
	float LightAttackHomingMaxHeightDiff = 600.0f;

	// --- 弱攻撃の対象詰め（ルートモーションの前進量を伸ばして吸着対象へ届かせる） ---
	// チャージ攻撃の詰めと違い位置を直接動かさず、ルートモーション倍率だけを変える（ブロックは CMC の通常移動が担う）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|LightAttack|Approach" )
	bool bEnableLightAttackApproach = true;

	// 到達点を対象位置からどれだけ手前にするか（cm）。対象カプセル半径＋武器リーチ相当を見込む
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|LightAttack|Approach", meta = ( EditCondition = "bEnableLightAttackApproach", ClampMin = "0.0" ) )
	float LightAttackApproachStandoff = 150.0f;

	// 前進量を伸ばせる上限倍率。これを超える距離の対象には届かせない（不自然なダッシュパンチを防ぐ）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|LightAttack|Approach", meta = ( EditCondition = "bEnableLightAttackApproach", ClampMin = "1.0" ) )
	float LightAttackApproachMaxScale = 10.0f;

	// 素の前進量がこれ未満の段は詰めない（cm）。その場攻撃で倍率が発散するのを防ぐ
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|LightAttack|Approach", meta = ( EditCondition = "bEnableLightAttackApproach", ClampMin = "0.0" ) )
	float LightAttackApproachMinRootMotion = 20.0f;

	// 弱攻撃がヒットした際、自分が後ろに押し戻される力
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|LightAttack|HitBack" )
	float LightAttackSelfPushbackPower = 1200.0f;

	// 弱攻撃ヒットバック中の減速度（高いほど早く滑りが止まる）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|LightAttack|HitBack" )
	float LightAttackHitBackDeceleration = 6000.0f;

	// 弱攻撃ヒットバックの持続時間（秒）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|LightAttack|HitBack" )
	float LightAttackHitBackDuration = 0.2f;

	// ==========================================
	// 神技（ゲージ技）共通ゲージ
	// ==========================================

	// ゲージの最大値（この値に達すると神技が発動可能）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Gauge", meta = ( ClampMin = "1.0" ) )
	float GodActionGaugeMax = 100.0f;

	// 【デバッグ】ゲージの部分消費（節約）を使うか。false＝一閃1回で全消費、true＝ロックオン対象数/最大数の割合だけ消費して温存
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Gauge" )
	bool bDebugGodActionPartialGaugeConsume = false;

	// 攻撃ヒット時のゲージ加算量はAttackParameterTable（FPlayerAttackParameterRow::GodActionGaugeGain）へ移行済み。行はAttackTypeTag+GearLevelで個別設定

	// 被弾（実際にダメージを受けたとき）に加算するゲージ量。0 で無効。DoT（継続ダメージ）は毎刻み加算を避けるため対象外
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Gauge", meta = ( ClampMin = "0.0" ) )
	float GodActionGaugeGainOnDamaged = 10.0f;

	// 加速ギミック（BoostGimmick）取得時に加算するゲージ量。0 で無効
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Gauge", meta = ( ClampMin = "0.0" ) )
	float GodActionGaugeGainOnBoostGimmick = 5.0f;

	// 加速ギミック取得時のエネルギー玉を目立たせる倍率（半径）。他経路（攻撃/被弾）は等倍のまま
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Gauge", meta = ( ClampMin = "0.1" ) )
	float GodGaugeOrbBoostRadiusScale = 2.2f;

	// 加速ギミック取得時のエネルギー玉を目立たせる倍率（不透明度）。既定の GodGaugeOrbAlpha に掛ける（1.0で不透明化）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Gauge", meta = ( ClampMin = "0.1" ) )
	float GodGaugeOrbBoostAlphaScale = 2.0f;

	// --- 神技ゲージ蓄積演出（エネルギー玉）---
	// 【検証用】被弾箇所からエネルギー玉がカーブを描いてゲージへ収納され、その瞬間にゲージが増える演出。ON＝玉で遅延加算、OFF＝ヒット即時加算
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Gauge|Orb" )
	bool bEnableGodGaugeOrbEffect = true;
	// 玉が被弾箇所からゲージへ到達するまでの飛行時間（秒）。この時間ぶん加算が遅れる
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Gauge|Orb", meta = ( ClampMin = "0.05" ) )
	float GodGaugeOrbFlightTime = 0.55f;
	// 飛行カーブの膨らみ（始点→終点の距離に対する張り出し比。序盤を大きく横へ膨らませる）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Gauge|Orb", meta = ( ClampMin = "0.0" ) )
	float GodGaugeOrbCurveStrength = 0.4f;
	// カーブの膨らみに掛ける乱数幅（Min〜Max を CurveStrength に乗算。玉ごとに張り出し量がばらける）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Gauge|Orb", meta = ( ClampMin = "0.0" ) )
	float GodGaugeOrbCurveJitterMin = 0.1f;
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Gauge|Orb", meta = ( ClampMin = "0.0" ) )
	float GodGaugeOrbCurveJitterMax = 1.0f;
	// 玉の基本半径（1080p 基準の px。解像度でスケールする。控えめにしたいので小さめ既定）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Gauge|Orb", meta = ( ClampMin = "1.0" ) )
	float GodGaugeOrbRadius = 9.0f;
	// 玉の不透明度（0〜1。小さいほど半透明）。グロー・尾・コア全体に掛かる
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Gauge|Orb", meta = ( ClampMin = "0.0", ClampMax = "1.0" ) )
	float GodGaugeOrbAlpha = 0.5f;
	// 1 ヒットで出す玉の個数（Min〜Max でランダム）。ゲージ加算量は個数で等分し、到達ごとに少しずつ増える
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Gauge|Orb", meta = ( ClampMin = "1" ) )
	int32 GodGaugeOrbCountMin = 3;
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Gauge|Orb", meta = ( ClampMin = "1" ) )
	int32 GodGaugeOrbCountMax = 5;
	// 各玉の発生位置を攻撃位置からどれだけ散らすか（cm 半径のランダム球。0 で攻撃位置ぴったり）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Gauge|Orb", meta = ( ClampMin = "0.0" ) )
	float GodGaugeOrbSpawnRadius = 40.0f;
	// ゲージが増えた瞬間にゲージ周りへ出すフラッシュの長さ（秒。大きいほどゆっくり消える）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Gauge|Orb", meta = ( ClampMin = "0.05" ) )
	float GodGaugeFlashTime = 0.45f;

	// ==========================================
	// 神技・一閃（Slash）
	// ==========================================

	// ギア段階（Index 0:なし / 1:壱 / 2:弍 / 3:参）ごとのロックオン目安数
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Slash|LockOn" )
	TArray<int32> GodSlashLockOnCountForGear = { 2, 3, 5, 8 };

	// ギア段階（Index 0:なし / 1:壱 / 2:弍 / 3:参）ごとのロックオン可能距離
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Slash|LockOn" )
	TArray<float> GodSlashLockOnDistanceForGear = { 3000.0f, 3500.0f, 4500.0f, 6000.0f };

	// ロックオン中、ターゲットを再取得する間隔（秒）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Slash|LockOn" )
	float GodSlashLockOnScanInterval = 0.1f;

	// 一閃実行時、対象を斬り抜けて通り過ぎる距離（cm）。次の対象へ向かう前にこの分だけ敵を貫く
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Slash" )
	float GodSlashPassThroughDistance = 300.0f;

	// 一閃実行時、対象へ斬りかかる移動速度（cm/秒）。高速で複数対象を切り抜けるイメージ
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Slash" )
	float GodSlashMoveSpeed = 8000.0f;

	// 1 対象あたりの移動の最大許容時間（秒）。対象に届かない場合の保険として次へ進む
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Slash" )
	float GodSlashMaxTimePerTarget = 0.4f;

	// ロックオン時、カメラ→対象が地形で遮られていたらロックオンしない（手前に別の敵がいるだけのPawn遮蔽は可）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Slash|LockOn" )
	bool bGodSlashRequireLineOfSight = true;

	// ロックオン時、画面中央のこの割合の矩形内に収まる対象のみ対象化（横。既定は大型レティクルの横8割）。0以下で画面内フィルタ無効
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Slash|LockOn", meta = ( ClampMin = "0.0", ClampMax = "1.0" ) )
	float GodSlashLockOnScreenFractionX = 0.8f;

	// 同上（縦。既定は大型レティクルの縦7割）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Slash|LockOn", meta = ( ClampMin = "0.0", ClampMax = "1.0" ) )
	float GodSlashLockOnScreenFractionY = 0.7f;

	// 一閃終了時、地面下に潜っていた場合に床上へ押し上げる足元クリアランス（cm。高速移動中はZクランプせず最後にここで接地）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Slash", meta = ( ClampMin = "0.0" ) )
	float GodSlashGroundClearance = 2.0f;

	// ==========================================
	// 神技・一閃（Slash）演出
	// ==========================================

	// 発動中のワールド全体の時間倍率（0.3 ＝ 周囲が 30% 速度のスロー）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Slash|Stage", meta = ( ClampMin = "0.01", ClampMax = "1.0" ) )
	float GodSlashGlobalTimeDilation = 0.1f;

	// 発動中のプレイヤー自身の時間倍率（1.0 ＝ プレイヤーだけ等速。GlobalTimeDilation と組み合わせて補正する）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Slash|Stage", meta = ( ClampMin = "0.01" ) )
	float GodSlashPlayerTimeScale = 1.0f;

	// --- 神技・構え中（3択メニュー選択中）のワールドスロー ---
	// ON で神技の構え中（神鳥検証モードの 3 択選択中）にプレイヤー以外の世界をスローにする（バレットタイム照準）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Stance" )
	bool bGodArtStanceWorldSlow = true;

	// ON で構え中もプレイヤーが自由に動ける（構えモーションは流さない）。OFF で移動を止め GodSlashStart→Loop を流す
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Stance" )
	bool bGodArtStanceAllowMovement = false;

	// ON で構え中の神技選択を X／Y／B の直接選択にする（X=戯／Y=導／B=依）。OFF でカーソル移動（十字キー・L スティック・L1/R1）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Stance" )
	bool bGodArtSelectByFaceButtons = true;

	// 構え中のワールド全体の時間倍率（0.3 ＝ 周囲が 30% 速度のスロー）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Stance", meta = ( ClampMin = "0.01", ClampMax = "1.0" ) )
	float GodArtStanceGlobalTimeDilation = 0.1f;

	// 構え中のプレイヤー自身の時間倍率（1.0 ＝ プレイヤーだけ等速。GlobalTimeDilation と組み合わせて補正する）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Stance", meta = ( ClampMin = "0.01" ) )
	float GodArtStancePlayerTimeScale = 0.5f;

	// ON で構えモーション（GodSlashStart/Loop）だけ実時間で等速再生する（本体は GodArtStancePlayerTimeScale ぶん遅いまま）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Stance" )
	bool bGodArtStanceMontageRealtime = true;

	// 構え中のプレイヤーの重力スケール（1.0＝通常、小さいほど滞空して狙える）。MOVE_Fallingのときに効く
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Stance", meta = ( ClampMin = "0.0" ) )
	float GodArtStanceGravityScale = 0.15f;

	// 空中で構えを始めた場合の降下速度（cm/秒）。MOVE_Flyingにしてこの速度でゆっくり降下（0でその場滞空）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Stance", meta = ( ClampMin = "0.0" ) )
	float GodArtStanceFallSpeed = 50.0f;

	// 構え突入からブレーキの効きを 0→100% へ立ち上げる時間（秒）。「構えた瞬間に足が生える」のを防ぐ。
	// ※ブレーキを止める猶予ではなく減速量に掛ける係数（EaseIn）なので、同じ秒数でも猶予よりずっと短く滑る。0 で最初からフル
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Stance", meta = ( ClampMin = "0.0" ) )
	float GodArtStanceBrakeRampTime = 0.25f;

	// 構え中に水平速度を減衰させる時定数（秒）。大きいほど慣性が残る。0 で指数減衰なし（下の一定減速のみ）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Stance", meta = ( ClampMin = "0.0" ) )
	float GodArtStanceBrakeTime = 0.6f;

	// 上の指数減衰に上乗せする一定減速度（cm/秒^2）。指数減衰だけでは滑走距離が初速比例で高ギアほど流れるため、速度に依らず削ってギア差を詰める。0 で無効
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Stance", meta = ( ClampMin = "0.0" ) )
	float GodArtStanceBrakeDeceleration = 1800.0f;

	// 減衰の結果この速度（cm/秒）を下回ったら完全停止させる（いつまでも微速で流れるのを防ぐ）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Stance", meta = ( ClampMin = "0.0" ) )
	float GodArtStanceBrakeStopSpeed = 10.0f;

	// 構え中に CMC へ残す地面摩擦（既定 0＝制動を殺し、減速カーブは上のブレーキが単独で持つ）。
	// CMC の制動は GroundFriction × BrakingFrictionFactor × 速度 で効くため、上げると GodArtStanceBrakeTime を無視して急停止する
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Stance", meta = ( ClampMin = "0.0" ) )
	float GodArtStanceBrakeFriction = 0.0f;

	// 構え解除でチャージダッシュへ戻るとき、落ちた速度から本来のダッシュ速度へ戻す時間（秒）。0 で即満速＝復帰の瞬間にガクッと出る
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Stance", meta = ( ClampMin = "0.0" ) )
	float GodArtStanceDashResumeEaseTime = 0.15f;

	// ヒットストップ・ダメージ・リアクションは AttackParameterTable（TAG_AttackType_Player_GodActionSlash 行）で管理

	// --- ペーシング（カットを魅せる間の取り方） ---
	// ※各カットの静止（dwell）は廃止。連続移動し、貫通時のスロー（bGodSlashCutSlow / GodSlashCutSlow*）で"溜め"を見せる方針

	// ED 突入から一斉吹っ飛びまでのタメ時間（秒）。正面カメラに回り込む間
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Slash|Pacing", meta = ( ClampMin = "0.0" ) )
	float GodSlashFinishWindupTime = 1.0f;

	// 一斉吹っ飛び後、カメラを見せ続けてから神技を終える余韻時間（秒）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Slash|Pacing", meta = ( ClampMin = "0.0" ) )
	float GodSlashFinishHoldTime = 1.0f;

	// 一斉吹っ飛びの瞬間にスロー（TimeDilation）を解除して迫力を出すか
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Slash|Pacing" )
	bool bGodSlashRestoreTimeOnFinish = true;

	// 一斉吹っ飛びの瞬間に再生するカメラ揺れ（未設定なら揺れなし）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Slash|Pacing" )
	TSubclassOf<UCameraShakeBase> GodSlashFinishCameraShake;

	// 上記カメラ揺れのスケール（大きいほど強く揺れる）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Slash|Pacing", meta = ( ClampMin = "0.0" ) )
	float GodSlashFinishCameraShakeScale = 1.0f;

	// 各カットの手応えを完全停止でなく「プレイヤーと斬った敵を一緒にスロー」で出すか（false で従来の敵のみ停止）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Slash|Pacing" )
	bool bGodSlashCutSlow = true;

	// カットスロー時にプレイヤーと斬った敵へ与えるCustomTimeDilation（0〜1。0寄りで停止、0.3〜0.5でゆっくり動くスロー）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Slash|Pacing", meta = ( ClampMin = "0.0", ClampMax = "1.0" ) )
	float GodSlashCutSlowDilation = 0.01f;

	// カットスローの最も遅い状態を保持する時間（秒・実時間）。In→Hold→Out順で進み、入り/抜けのイージングは別（Hold=0でも山なりに効く）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Slash|Pacing", meta = ( ClampMin = "0.0" ) )
	float GodSlashCutSlowDuration = 0.5f;

	// カットスローに入るイージング時間（秒・実時間）。等速→スローへ滑らかに落とし、パッと変わる印象を消す
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Slash|Pacing", meta = ( ClampMin = "0.0" ) )
	float GodSlashCutSlowEaseInTime = 0.0f;

	// カットスローから抜けるイージング時間（秒・実時間）。スロー→等速へ滑らかに戻す
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Slash|Pacing", meta = ( ClampMin = "0.0" ) )
	float GodSlashCutSlowEaseOutTime = 0.2f;

	// カットの手応え（スロー/FX）を貫通の少し手前（敵に触れた瞬間）へ前倒しする量。発火閾値をPassThrough+（敵メッシュ水平半径×この値）に。0で対象中心、1で敵の手前ふち
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Slash|Pacing", meta = ( ClampMin = "0.0" ) )
	float GodSlashCutHitLeadRadiusScale = 0.2f;

	// --- 演出カメラ（専用 ExCameraMode）のフレーミング ---

	// カット時（プレイヤーと敵の2ショット）のカメラ最小距離（cm）。実距離はFOVと両者間隔から自動算出し、これを下限とする
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Slash|StageCamera" )
	float GodSlashCamTwoShotDistance = 500.0f;

	// 2 ショット時のカメラ高さ（cm。見下ろし量）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Slash|StageCamera" )
	float GodSlashCamTwoShotHeight = 100.0f;

	// 2 ショットのプレイヤー側注視基準の高さ（足元からの cm。90 = 胸〜頭あたり）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Slash|StageCamera" )
	float GodSlashCamTwoShotPlayerLookHeight = 0.0f;

	// 2 ショットの注視点全体の上下オフセット（cm。正=上を見る、負=下。構図を上下に振りたいとき）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Slash|StageCamera" )
	float GodSlashCamTwoShotLookAtZOffset = 0.0f;

	// 2 ショット専用 FOV（度）。0 以下のときは演出カメラ共通 FOV を使用
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Slash|StageCamera", meta = ( ClampMin = "0.0", ClampMax = "120.0" ) )
	float GodSlashCamTwoShotFOV = 0.0f;

	// 2 ショットで、プレイヤーと対象が画面のどれだけの割合に収まるか（0〜1。小さいほど余白が増え引き気味）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Slash|StageCamera", meta = ( ClampMin = "0.1", ClampMax = "1.0" ) )
	float GodSlashCamTwoShotFitFraction = 0.75f;

	// 対象（敵）のサイズを引き距離にどれだけ反映するか（0〜1）。巨大敵で近すぎ/はみ出しを防ぐ（敵バウンズ水平半径×この値ぶん引く）。0で点フレーミング、1でフル反映
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Slash|StageCamera", meta = ( ClampMin = "0.0", ClampMax = "1.0" ) )
	float GodSlashCamTargetSizeInfluence = 0.5f;

	// カメラを敵バウンズ球の外へ押し出す半径倍率（巨大敵の体内から撮る絵を防ぐ）。敵バウンズ水平半径×この値を最低距離に（1.0で球面ぴったり）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Slash|StageCamera", meta = ( ClampMin = "0.0" ) )
	float GodSlashCamTargetClearanceScale = 1.0f;

	// 2 ショットは左右交互に撮る（毎カットで撮る側を反転）。同じ画が続くのを防ぐ
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Slash|StageCamera" )
	bool bGodSlashCamAlternateSide = true;

	// カット毎に加える「ゆらぎ」の最大量（左右交互で単調さを消す）・ヨー角（度）：Midを中心に左右へ回り込む量
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Slash|StageCamera", meta = ( ClampMin = "0.0", ClampMax = "60.0" ) )
	float GodSlashCamYawVariation = 22.0f;

	// ・高さ（cm）：見下ろし/見上げ方向のゆらぎ
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Slash|StageCamera", meta = ( ClampMin = "0.0" ) )
	float GodSlashCamHeightVariation = 20.0f;

	// ・距離（cm）：引き方向にのみ加算するゆらぎ（収まりを崩さないため外側のみ）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Slash|StageCamera", meta = ( ClampMin = "0.0" ) )
	float GodSlashCamDistanceVariation = 20.0f;

	// 2ショット構図の前後ブレンド（0=純側面, 0.5=45度斜め, 1=純前後）。0.5前後推奨（手前に置いた斜めショット）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Slash|StageCamera", meta = ( ClampMin = "0.0", ClampMax = "1.0" ) )
	float GodSlashCamForwardBlend = 0.6f;

	// 注視点の中心ずれ最大量（0=常に中間, 0.3=主体寄りに最大30%ずれる）。主体が少し端寄りに映り奥行き感が出る
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Slash|StageCamera", meta = ( ClampMin = "0.0", ClampMax = "0.5" ) )
	float GodSlashCamLookAtBias = 0.0f;

	// カメラ位置・回転の補間速度（大きいほど素早くカットが切り替わる）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Slash|StageCamera", meta = ( ClampMin = "0.0" ) )
	float GodSlashCamInterpSpeed = 1000.0f;

	// カット切替のタイミング。true＝次の対象へ飛ぶ瞬間（対象切替時）に切替＝カット中は同じ構図で固定、false＝対象を貫通した瞬間に切替（貫通前は直前カット保持）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Slash|StageCamera" )
	bool bGodSlashCamSwitchOnLaunch = false;

	// 2 ショットの障害物チェックに使うスフィアの半径（cm。大きいほど壁際で早めに別アングルへ切り替わる）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Slash|StageCamera", meta = ( ClampMin = "0.0" ) )
	float GodSlashCamCollisionProbeRadius = 15.0f;

	// 地形めり込み回避でカメラを手前へ引き寄せる際の、ピボットからの最低距離（cm）。これより近くへ寄せず真上化（俯角過多）を防ぐ。小さいほど寄りを許し、大きいほど真上化を抑える
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Slash|StageCamera", meta = ( ClampMin = "0.0" ) )
	float GodSlashCamMinCollisionDistance = 500.0f;

	// 神技終了時の演出カメラ（GodSlash ExCamera）からのブレンドアウト設定行名（BlendSettingsTableに同名行を追加、BlendTime>0・bUseFixedBlendStart=true推奨）。空でDefault行、無ければ即切替
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Slash|StageCamera" )
	FName GodSlashCamPopBlendRowName = TEXT( "PopGodSlash" );

	// ==========================================
	// 一閃 演出モード切替（クラシック／ワイドカット）
	// ==========================================
	// trueでワイドカット演出：切り抜け中(Loop)は全対象が収まる広角カメラにし手応え演出なしの連続移動。切り抜け後は開始位置へ高速帰還しクラシック演出（寄り2ショット・タメ→一斉吹っ飛び）に合流。falseでクラシックのまま
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Slash|WideCut" )
	bool bGodSlashWideCutPresentation = false;

	// ワイドカット中、全対象を切り抜けた後に開始位置へ戻る速度（cm/s）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Slash|WideCut" )
	float GodSlashWideCutReturnMoveSpeed = 10000.0f;

	// ワイドカット中、切り抜け（Loop）の基準移動速度（cm/s、ロックオン1体あたり）。クラシックのGodSlashMoveSpeedとは独立
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Slash|WideCut" )
	float GodSlashWideCutZigzagMoveSpeed = 20000.0f;

	// ワイドカット中、ロックオン対象1体増えるごとに切り抜け移動速度へ加算する量（cm/s）。対象が多いほど1周が伸び合計時間に移動が追いつかないのを防ぐ。0で対象数によらず一定速度
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Slash|WideCut", meta = ( ClampMin = "0.0" ) )
	float GodSlashWideCutZigzagSpeedPerTarget = 3000.0f;

	// ワイドカット中、ロックオン1体のときに切り抜け速度へかける倍率（単独は対象中心の小周回になるため複数時の基準速度から調整）。1.0＝同速、未満で減速、超で増速
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Slash|WideCut", meta = ( ClampMin = "0.0" ) )
	float GodSlashWideCutSingleTargetSpeedScale = 0.8f;

	// ワイドカット中、ロックオン2体のときに切り抜け速度へかける倍率（単独用GodSlashWideCutSingleTargetSpeedScaleとは体感傾向が異なるため独立調整）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Slash|WideCut", meta = ( ClampMin = "0.0" ) )
	float GodSlashWideCutDualTargetSpeedScale = 1.0f;

	// ワイドカット中、ロックオン1〜2体のとき対象を中心に巡回させる周回半径（cm）。1体だと星型の水平移動が0・2体だと単純往復に見える問題を、対象中心の仮想五芒星を合成して舞うように見せる
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Slash|WideCut", meta = ( ClampMin = "0.0" ) )
	float GodSlashWideCutSingleTargetOrbitRadius = 350.0f;

	// ワイドカット中、切り抜け（Loop）を続ける合計時間（秒）。全対象を星型巡回で何周も往復し、この時間に達したら次の到達タイミングでReturnへ進む
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Slash|WideCut" )
	float GodSlashWideCutLoopDuration = 0.6f;

	// ワイドカット中の移動先（星型頂点）の水平距離を、重心から見た対象の実距離への倍率で決める。1.0で対象位置に一致（空振りに見えない）、大で外側・小で手前を通る星型
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Slash|WideCut", meta = ( ClampMin = "0.0" ) )
	float GodSlashWideCutStarRadiusScale = 1.0f;

	// ワイドカット中、星型頂点の高さを対象高さから上下交互（Stepの偶奇）にずらす量（cm）。0で水平面だけ、大で3D的に大きく振れる。下方向は地面下へ潜らない範囲に自動クランプ（上は無制限）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Slash|WideCut", meta = ( ClampMin = "0.0" ) )
	float GodSlashWideCutStarHeightVariation = 200.0f;

	// ワイドカット中、次の頂点への向き直り（Yaw）角速度の倍率。区間の移動所要時間ちょうどで向き終わる角速度を自動計算しこの倍率をかける。1.0で移動と同時、大で早めに向き終わる。折り返しで回頭が追いつかず滑って見えるのを防ぐ
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Slash|WideCut", meta = ( ClampMin = "0.0" ) )
	float GodSlashWideCutFacingSpeedMultiplier = 1.3f;

	// ==========================================
	// ワイドカット カメラ（固定2段ショット）
	// ==========================================
	// 発動時点のTPSカメラ姿勢を基準に、切り抜け中(Start/Loop)は引いて上げた位置で静止、戻り〜仕上げ(Return/End)は基準へ近づけて再静止。基準からの引き距離/高さのみで決まり追従・再フレーミングはしない

	// 切り抜け中（Start/Loop）：基準カメラ位置から後方へ引く距離（cm）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Slash|WideCut" )
	float GodSlashWideCutPullBackDistance = 250.0f;

	// 切り抜け中（Start/Loop）：基準カメラ位置から上へ上げる高さ（cm）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Slash|WideCut" )
	float GodSlashWideCutPullUpHeight = 120.0f;

	// 戻り〜仕上げ（Return/End）：基準カメラ位置から後方へ引く距離（cm）。切り抜け中より小さくして「近づける」
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Slash|WideCut" )
	float GodSlashWideCutFinishDistance = 80.0f;

	// 戻り〜仕上げ（Return/End）：基準カメラ位置から上へ上げる高さ（cm）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodAction|Slash|WideCut" )
	float GodSlashWideCutFinishHeight = 40.0f;

	// ==========================================
	// スライドパッシブ（チャージスライド中に円を描いて発動）
	// ==========================================

	// 機能の有効/無効
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|SlidePassive" )
	bool bEnableSlidePassive = true;

	// 軌跡をサンプリングする最小移動距離（cm）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|SlidePassive" )
	float SlidePassiveSampleDistance = 30.0f;

	// 一周（円が閉じた）とみなす累積旋回角度（度）。厳密な交差判定はせず旋回量で判定
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|SlidePassive" )
	float SlidePassiveLoopAngleThreshold = 330.0f;

	// 有効な円とみなす最小半径（cm）。これ未満の小さな旋回は無視する
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|SlidePassive" )
	float SlidePassiveMinRadius = 100.0f;

	// 円の閉じ判定：判定区間の始点〜終点の距離が「平均半径 × この値」以下なら閉じているとみなす（0で判定しない）。小さいほど厳しい。助走の弧を巻き込んだ未閉曲線の誤検出を防ぐ
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|SlidePassive", meta = ( ClampMin = "0.0" ) )
	float SlidePassiveCircleClosureRatio = 1.0f;

	// 円らしさ判定：中心からの距離の平均ズレが「平均半径 × この値」以下なら円とみなす（0で判定しない）。小さいほど真円に近い形しか通さない
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|SlidePassive", meta = ( ClampMin = "0.0" ) )
	float SlidePassiveCircleRoundnessRatio = 1.0f;

	// この半径（cm）以上なら「印・大」、未満なら「印・小」
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|SlidePassive" )
	float SlidePassiveLargeRadiusThreshold = 100.0f;

	// 小の竜巻を有効にするか。falseで「小」をマスクし、SlidePassiveLargeRadiusThreshold 未満の円は成立しない（＝大の竜巻のみ）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|SlidePassive" )
	bool bEnableSlidePassiveSmallTornado = false;

	// 描いた円の半径に応じて竜巻・印のScaleを連続的に決めるか（成立最小半径でSmallScale → 下のScaleMaxRadiusでLargeScale）。falseで従来の大小2値固定
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|SlidePassive" )
	bool bEnableSlidePassiveRadiusScale = true;

	// 半径連動ScaleがLargeScaleに達する半径（cm。これ以上の円は頭打ち）。0でSlidePassiveLargeRadiusThresholdを上限にする。小マスク時は成立最小半径＝大閾値になるので、それより大きい値にしないと常にLargeScaleになる
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|SlidePassive", meta = ( ClampMin = "0.0" ) )
	float SlidePassiveScaleMaxRadius = 900.0f;

	// 印エフェクト発生後、竜巻エフェクトを発生させるまでの遅延（秒）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|SlidePassive" )
	float SlidePassiveTornadoDelay = 0.5f;

	// 【検証用】一斉軌道：円完成時は印だけ出しリボンを継続、竜巻はチャージ解除時に溜めた全円から一斉発生。falseで従来（円完成ごとに遅延後の竜巻＋トレイルフラッシュ）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|SlidePassive" )
	bool bSlidePassiveDeferOnRelease = false;

	// 【検証用】解除時まとめモード時の共有トレイルリボン色演出。true＝緑→完成で赤（解除まで持続）→解除で白フェードアウト、false＝従来（完成で緑点滅→解除で赤フラッシュ→通常色→フェード）。まとめOFF時は本フラグ無関係
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|SlidePassive" )
	bool bSlidePassiveNewTrailColor = true;

	// 【検証用】解除時まとめ竜巻モードで、溜めた円を順番に発生させる間隔（秒。i番目をi×この値だけ遅延）。0で一斉発生
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|SlidePassive", meta = ( ClampMin = "0.0" ) )
	float SlidePassiveDeferReleaseInterval = 0.25f;

	// 【検証用】解除時まとめ竜巻モードで蓄積印をスポーンする初期LifeTime（秒。主にINの速さを決める。大小基本寿命との大きい方を採用）。解除まで再延長で永続表示。大きすぎるとINが遅く見づらい点に注意
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|SlidePassive", meta = ( ClampMin = "0.0" ) )
	float SlidePassiveDeferMarkLifeTime = 12.0f;

	// 竜巻アクターのクラス（スリップダメージ・判定・見た目を持つ）。未設定なら竜巻 Niagara のみ発生
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|SlidePassive" )
	TSubclassOf<class ASlidePassiveTornado> SlidePassiveTornadoClass;

	// チャージダッシュ派生後もスライドパッシブの軌跡・円判定を継続させる猶予時間（秒。チャージダッシュ中のみ消化）。0でチャージ終了と同時に即停止
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|SlidePassive", meta = ( ClampMin = "0.0" ) )
	float SlidePassiveChargeDashGraceTime = 0.5f;

	// 【まとめモードOFF時】印完成のフラッシュ後、トレイルを出さない時間（秒）。この間はサンプリングも止まる。0で従来どおり即再開
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|SlidePassive", meta = ( ClampMin = "0.0" ) )
	float SlidePassiveTrailCompletionCooldown = 0.5f;

	// 小（印・竜巻）のエフェクトパラメータ（LifeTime / Scale。印・竜巻で共通）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|SlidePassive|Small" )
	float SlidePassiveSmallLifeTime = 5.0f;
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|SlidePassive|Small" )
	float SlidePassiveSmallScale = 1.44f;

	// 大（印・竜巻）のエフェクトパラメータ（LifeTime / Scale。印・竜巻で共通）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|SlidePassive|Large" )
	float SlidePassiveLargeLifeTime = 2.5f;
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|SlidePassive|Large" )
	float SlidePassiveLargeScale = 2.7f;

	// ==========================================
	// スライドパッシブ：突風（チャージスライド中に S 字を描いて発動）
	// ==========================================

	// 機能の有効/無効（機能検証フラグ）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|SlidePassive|Gust" )
	bool bEnableSlidePassiveGust = true;

	// S 字の 1 弧を成立とみなす旋回量（度）。片側へこの角度以上旋回 → 反対側へこの角度以上旋回で S 成立
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|SlidePassive|Gust" )
	float GustArcAngleThreshold = 60.0f;

	// 1 弧として有効とみなす最小移動距離（cm）。これ未満の小さな振りはジッタとして無視する
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|SlidePassive|Gust" )
	float GustMinArcDistance = 120.0f;

	// 直前の弧が成立してから次の弧が成立するまでに許容する最大サンプル数。超えたら検出をリセットする
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|SlidePassive|Gust", meta = ( ClampMin = "1" ) )
	int32 GustMaxArcSamples = 20;

	// 成立に必要な交互弧の本数（各弧は左右交互に切り返す）。2＝S字、3＝左右左、4＝左右左右…
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|SlidePassive|Gust", meta = ( ClampMin = "2" ) )
	int32 GustRequiredArcCount = 3;

	// 【検証フラグ】発動条件を軌跡（交互弧）から「左スティック↓→↑」の入力に差し替える。ONの間は軌跡検出を行わない
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|SlidePassive|Gust" )
	bool bGustUseStickFlickTrigger = true;

	// ↓／↑と判定する前後入力の大きさ（0〜1）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|SlidePassive|Gust", meta = ( ClampMin = "0.05", ClampMax = "1.0" ) )
	float GustStickFlickThreshold = 0.7f;

	// ↓を入れてから↑が来るまでの許容時間（秒）。超えたら待ちを解除する
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|SlidePassive|Gust", meta = ( ClampMin = "0.0" ) )
	float GustStickFlickWindowTime = 0.6f;

	// ↓と↑の間に通過を要求するニュートラルの大きさ（入力ベクトル長がこれ以下）。倒したまま回す1回転を弾くための条件
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|SlidePassive|Gust", meta = ( ClampMin = "0.0", ClampMax = "1.0" ) )
	float GustStickFlickNeutralThreshold = 0.35f;

	// ↓検知後に許容する横入力の最大値。これを超えたら「回転」と見なして待ちを破棄する（0 で無効）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|SlidePassive|Gust", meta = ( ClampMin = "0.0", ClampMax = "1.0" ) )
	float GustStickFlickMaxLateral = 0.7f;

	// 突風成立の瞬間にギアアップ時と同じ足元の火花を出す（強制バーストなのでサイズ・尺は Boost|Effect の DriftSparkGearUp* に従う）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|SlidePassive|Gust" )
	bool bEnableGustSparkBurst = true;

	// 突風バフのarm中に足元へ表示し続ける印（PASSIVE_MARK）のスケール（竜巻印より小さめで「バフ中」を示す）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|SlidePassive|Gust" )
	float GustBuffFootMarkScale = 0.5f;

	// 足元印（PASSIVE_MARK）の生成時LifeTime（秒）。正規化年齢でIN→ホールド→OUTを再生するのでこの値がINの速さを決める。ホールド後はバフ消費まで再延長される
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|SlidePassive|Gust" )
	float GustBuffFootMarkLifeTime = 4.0f;

	// バフ消費で足元印をOUTへ移行させた後、強制破棄するまでの追加マージン秒（通常はOUT完了＋bAutoDestroyで消えるための取りこぼし保険）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|SlidePassive|Gust" )
	float GustBuffFootMarkFadeOutTime = 0.2f;

	// 風まといVFXとバーストは寿命固定せずバフ/バフ付きアクションが続く間だけ持続（終了時にコード側で破棄）。アセットはループ前提で作る

	// 範囲攻撃バーストのアクター（判定・見た目を持つ）。未設定なら突風 VFX のみ発生
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|SlidePassive|Gust" )
	TSubclassOf<class ASlidePassiveGustBurst> GustBurstClass;

	// 範囲攻撃バーストの半径（cm）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|SlidePassive|Gust" )
	float GustBurstRadius = 400.0f;

	// 範囲攻撃バーストのダメージ（AttackParameterTable の SlidePassiveGust 行が無い場合のフォールバック）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|SlidePassive|Gust" )
	float GustBurstDamage = 30.0f;

	// 範囲攻撃バーストの見た目スケール
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|SlidePassive|Gust" )
	float GustBurstScale = 1.0f;

	// 範囲攻撃バーストのスポーン相対位置（プレイヤーにアタッチした際のオフセット）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|SlidePassive|Gust" )
	FVector GustBurstRelativeLocation = FVector( 200.0f, 0.0f, 0.0f );

	// 範囲攻撃バーストのスポーン相対回転（プレイヤーにアタッチした際のオフセット）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|SlidePassive|Gust" )
	FRotator GustBurstRelativeRotation = FRotator( 0.0f, 180.0f, 0.0f );

	// 範囲攻撃バーストを消す際の Alpha フェードアウト時間（秒）。0 以下なら即破棄
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|SlidePassive|Gust" )
	float GustBurstFadeOutTime = 0.3f;

	// =======================================================
	// 神鳥（お供精霊）関連
	// =======================================================

	// チャージ中にプレイヤーの横へ出す神鳥アクターのクラス。未設定なら神鳥は出ない
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodBird" )
	TSubclassOf<class ATideGodBird> GodBirdActorClass;

	// 神鳥の相対位置（プレイヤーにアタッチした際のローカルオフセット。X=前後 / Y=左右 / Z=上下、cm）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodBird" )
	FVector GodBirdSideOffset = FVector( 0.0f, 100.0f, 60.0f );

	// 神鳥の相対回転（プレイヤー基準の向き調整。メッシュの正面が合わない場合などに使う）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodBird" )
	FRotator GodBirdRelativeRotation = FRotator( 0.0f, -90.0f, 0.0f );

	// 滑空中に神鳥をアタッチ（追従）させるソケット名。空なら滑空中も通常どおりプレイヤー横（GodBirdSideOffset）に出す
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodBird" )
	FName GodBirdGlideHandSocket = TEXT( "hand_l" );

	// 滑空中の神鳥オフセット（上記ソケット位置からのプレイヤー基準ローカル。X=前後 / Y=左右 / Z=上下、cm）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodBird" )
	FVector GodBirdGlideHandOffset = FVector( 0.0f, 0.0f, 20.0f );

	// --- SlidePassive 連動演出（竜巻エスコート／突風の正面追従） ---

	// SlidePassive の神鳥演出を有効にするか。ON で「竜巻へ飛んで周回して戻る」「突風バフ中は正面へ回り込む」を行う
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodBird|SlidePassive" )
	bool bGodBirdSlidePassiveEscort = true;

	// 突風バフ中に神鳥が回り込むプレイヤー正面位置（プレイヤー基準ローカル。X=前後 / Y=左右 / Z=上下、cm）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodBird|SlidePassive" )
	FVector GodBirdGustFrontOffset = FVector( 420.0f, 0.0f, 0.0f );

	// 竜巻エスコートの周回半径（cm）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodBird|SlidePassive", meta = ( ClampMin = "0.0" ) )
	float GodBirdTornadoOrbitRadius = 400.0f;

	// 竜巻エスコートの周回高さ＝周回し終わりの高さ（竜巻中心＝地面からの高さ、cm）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodBird|SlidePassive", meta = ( ClampMin = "0.0" ) )
	float GodBirdTornadoOrbitHeight = 400.0f;

	// 竜巻エスコートの周回し始めの高さ（cm）。周回の進行に合わせてここからGodBirdTornadoOrbitHeightまで上がる（螺旋）。同じ値にすれば従来の水平周回
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodBird|SlidePassive", meta = ( ClampMin = "0.0" ) )
	float GodBirdTornadoOrbitStartHeight = 50.0f;

	// 竜巻エスコートの飛行速度（竜巻へ向かう／戻る際の移動速度、cm/秒）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodBird|SlidePassive", meta = ( ClampMin = "0.0" ) )
	float GodBirdTornadoEscortSpeed = 2500.0f;

	// 竜巻エスコートの周回角速度（度/秒。くるくる回る速さ）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodBird|SlidePassive", meta = ( ClampMin = "0.0" ) )
	float GodBirdTornadoOrbitAngularSpeed = 540.0f;

	// 竜巻エスコートの周回数（何周してから戻るか）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodBird|SlidePassive", meta = ( ClampMin = "0.0" ) )
	float GodBirdTornadoOrbitTurns = 2.0f;

	// =======================================================
	// 神技「導き」（前方へ巨大な鳥が突進）関連
	// =======================================================

	// 導きで突進させる鳥クラス（都度スポーン）。未設定なら常駐お供のGodBirdActorClassを流用（それも未設定なら発動しない）。スケールはGuidanceBirdScaleで拡大するため通常は未設定で流用可
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodGuidance" )
	TSubclassOf<class ATideGodBird> GuidanceBirdClass;

	// 発射方向を3D（カメラのピッチ＋ヨー）にするか。ONで着地補正せずカメラ方向へ直進、OFFで水平前方（Yawのみ）＋到達点の真下トレースで着地補正
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodGuidance" )
	bool bGuidanceAim3D = true;

	// 到達点までの前方距離（cm）。照準方向へこの距離進んだ地点を到達点にする（2D 時は真下トレースで着地補正）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodGuidance", meta = ( ClampMin = "0.0" ) )
	float GuidanceDistance = 6000.0f;

	// 突進速度（cm/秒）。到達点へ向かってこの速度で飛ぶ
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodGuidance", meta = ( ClampMin = "1.0" ) )
	float GuidanceChargeSpeed = 3000.0f;

	// 突進の最大持続時間（秒）。到達点へ着く前でもこの時間で打ち切って自壊する（保険）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodGuidance", meta = ( ClampMin = "0.05" ) )
	float GuidanceDuration = 5.0f;

	// 巨大な鳥のスケール倍率（＝突進時の最終スケール）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodGuidance", meta = ( ClampMin = "0.1" ) )
	float GuidanceBirdScale = 12.0f;

	// 開始スケールの割合（GuidanceBirdScaleに対する比率）。ST中はこの割合から始めST進捗でGuidanceBirdScaleまで拡大。1.0で拡大なし。ST未登録時も拡大なし（即・最終スケール）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodGuidance", meta = ( ClampMin = "0.0", ClampMax = "1.0" ) )
	float GuidanceBirdStartScaleRatio = 0.15f;

	// 鳥のスポーン位置オフセット（プレイヤー基準ローカル。X=前/Y=右/Z=上）。発動時この分ずらした位置へ強制スポーンしてSTを再生
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodGuidance" )
	FVector GuidanceBirdSpawnOffset = FVector( 200.0f, 0.0f, 100.0f );

	// 突進中の範囲ダメージ量（BaseDamage としてそのまま与える）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodGuidance", meta = ( ClampMin = "0.0" ) )
	float GuidanceDamage = 40.0f;

	// 突進中の範囲ダメージ半径（cm）。鳥の現在地を中心にこの半径で毎フレーム判定する
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodGuidance", meta = ( ClampMin = "1.0" ) )
	float GuidanceDamageRadius = 800.0f;

	// 同一対象への再ダメージ間隔（秒）。竜巻／突風と同じ再アーム方式で多段ヒットを防ぐ
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodGuidance", meta = ( ClampMin = "0.0" ) )
	float GuidanceDamageReArmInterval = 0.5f;

	// ON で突進中の範囲ダメージ半径（GuidanceDamageRadius）を鳥の現在地中心にデバッグ描画する（開発ビルドのみ）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodGuidance" )
	bool bGuidanceDrawDamageRange = false;

	// ON で発動時に到達点（Destination）を水色の Box でデバッグ描画する（開発ビルドのみ）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodGuidance" )
	bool bGuidanceDrawDestination = false;

	// 突進の軌跡に沿って等間隔に配置する加速ギミック（BoostGimmick）のクラス。未設定なら配置しない
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodGuidance" )
	TSubclassOf<class ABoostGimmick> GuidanceBoostGimmickClass;

	// 加速ギミックを配置する間隔（cm）。鳥が通った軌跡をこの距離ごとに埋める
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodGuidance", meta = ( ClampMin = "1.0" ) )
	float GuidanceBoostGimmickInterval = 1200.0f;

	// 加速ギミックの初回生成をずらす距離（cm）。鳥がこの距離離れてから配置開始し、足元へ置いて自分で当たるのを防ぐ（0でスポーン直後から配置）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodGuidance", meta = ( ClampMin = "0.0" ) )
	float GuidanceBoostGimmickStartOffset = 800.0f;

	// 配置した加速ギミックの寿命（秒）。0以下で消えない（BP の復活設定に従う）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodGuidance", meta = ( ClampMin = "0.0" ) )
	float GuidanceBoostGimmickLifeTime = 10.0f;

	// 加速ギミックの地面からの最低スポーン高さ（cm）。真下トレースで地面を探し埋まらないようこの高さまで持ち上げる。0以下なら軌跡位置のまま
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodGuidance", meta = ( ClampMin = "0.0" ) )
	float GuidanceBoostGimmickMinGroundHeight = 50.0f;

	// 加速ギミックを中心・左右3列で配置する際の左右列間隔（cm。各距離ごとに中心＋左右±この距離の3個）。0以下なら中心1列のみ
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodGuidance", meta = ( ClampMin = "0.0" ) )
	float GuidanceBoostGimmickColumnSpacing = 250.0f;

	// =======================================================
	// 神技「戯れ」（鳥が自律で飛び回って範囲攻撃）関連
	// =======================================================

	// 戯れで飛ばす鳥アクターのクラス。未設定なら常駐お供と同じ GodBirdActorClass を流用する（それも未設定なら発動しない）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodFrolic" )
	TSubclassOf<class ATideGodBird> GodFrolicBirdClass;

	// 自律攻撃の持続時間（秒）。この時間だけ鳥が飛び回って攻撃し、経過で自壊する
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodFrolic", meta = ( ClampMin = "0.05" ) )
	float GodFrolicDuration = 10.0f;

	// 鳥のスケール倍率（＝飛び回り時の最終スケール）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodFrolic", meta = ( ClampMin = "0.1" ) )
	float GodFrolicBirdScale = 2.0f;

	// 開始スケールの割合（GodFrolicBirdScaleに対する比率）。ST中はこの割合から始めST進捗でGodFrolicBirdScaleまで拡大。1.0で拡大なし。ST未登録時も拡大なし（即・最終スケール）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodFrolic", meta = ( ClampMin = "0.0", ClampMax = "1.0" ) )
	float GodFrolicBirdStartScaleRatio = 0.15f;

	// 鳥の飛行速度（cm/秒）。狙った敵へこの速度で飛ぶ
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodFrolic", meta = ( ClampMin = "1.0" ) )
	float GodFrolicMoveSpeed = 2000.0f;

	// 曲がれる最大角速度（度/秒）。狙う方向へはこの速さでしか向き直らないため、小さいほど大きく弧を描いて曲がる（曲がりきれなければ通り過ぎて旋回し直す）。追跡・帰還で共用
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodFrolic", meta = ( ClampMin = "1.0" ) )
	float GodFrolicTurnRate = 400.0f;

	// 狙う敵の探索半径（cm）。鳥の現在地からこの範囲内の敵中で最も近い相手を狙う
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodFrolic", meta = ( ClampMin = "1.0" ) )
	float GodFrolicTargetRange = 2500.0f;

	// 対象の見限り時間（秒）。鳥は対象へ到達（ダメージ半径内＝ヒット）するまで追い、届かない対象（高所・逃げ続ける敵）はこの時間で諦め次へ切り替える安全弁。短すぎると折り返して見えるので余裕を持たせる
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodFrolic", meta = ( ClampMin = "0.1" ) )
	float GodFrolicChaseGiveUpTime = 2.0f;

	// 当てた直後の離脱距離（cm）。対象へ当てたら通り抜けてこの距離まで離れ、そこから次の対象へ突入し直す（ダイブの反復に見せる）。0 で離脱なし＝当てたら即次の対象を狙う従来挙動（対象が 1 体だと敵の周りを旋回し続ける）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodFrolic", meta = ( ClampMin = "0.0" ) )
	float GodFrolicPulloutDistance = 300.0f;

	// 離脱方向へ加える上方成分の比率。0 で水平に抜け、大きいほど上へ舞い上がってから突っ込み直す
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodFrolic", meta = ( ClampMin = "0.0", ClampMax = "4.0" ) )
	float GodFrolicPulloutRiseRatio = 0.3f;

	// 離脱の打ち切り時間（秒）。壁際などで離脱距離まで離れられないときの安全弁
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodFrolic", meta = ( ClampMin = "0.05" ) )
	float GodFrolicPulloutMaxTime = 1.0f;

	// 対象が居ないときその場で待機する秒数。これを超えたら持続時間の残りは使わずプレイヤーの元へ帰還する（待機中に敵が湧けば追跡へ戻る）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodFrolic", meta = ( ClampMin = "0.0" ) )
	float GodFrolicNoTargetWaitTime = 0.5f;

	// プレイヤーの元へ帰るときの飛行速度（cm/秒）。帰りながらスケールが元サイズ（常駐お供と同じ）へ戻り、着いたら消失エフェクト無しで収まる
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodFrolic", meta = ( ClampMin = "1.0" ) )
	float GodFrolicReturnSpeed = 2000.0f;

	// 攻撃の範囲ダメージ量（BaseDamage としてそのまま与える）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodFrolic", meta = ( ClampMin = "0.0" ) )
	float GodFrolicDamage = 30.0f;

	// 攻撃の範囲ダメージ半径（cm）。鳥の現在地を中心にこの半径で毎フレーム判定する
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodFrolic", meta = ( ClampMin = "1.0" ) )
	float GodFrolicDamageRadius = 300.0f;

	// 同一対象への再ダメージ間隔（秒）。多段ヒットを防ぐ再アーム間隔
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodFrolic", meta = ( ClampMin = "0.0" ) )
	float GodFrolicDamageReArmInterval = 0.4f;

	// ON で攻撃の範囲ダメージ半径を鳥の現在地中心にデバッグ描画する（開発ビルドのみ）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodFrolic" )
	bool bGodFrolicDrawDamageRange = false;

	// 戯れトレイル（GODBIRD_FROLIC_TRAIL）の鳥メッシュ基準の回転オフセット（縦向きで出る場合ここで倒す。既定Roll90°で横倒し）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|GodFrolic" )
	FRotator GodFrolicTrailRotation = FRotator( 0.0f, 0.0f, 90.0f );

	// =======================================================
	// 落下アクション・ダメージ関連
	// =======================================================

	// 落下ダメージシステムを有効にするか（既定OFF）。OFF の間は落下ダメージ・落下死・着地よろけを一切行わない
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Fall" )
	bool bEnableFallDamageSystem = false;

	// 【検証用】ダメージ・落下死を与えず着地よろけモーション（＋ヒットストップ）だけ行うモード。DamageFallDistance超過で距離に依らず一律よろけ再生。bEnableFallDamageSystemがONのときのみ機能
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Fall", meta = ( EditCondition = "bEnableFallDamageSystem" ) )
	bool bFallDamageMotionOnly = true;

	// 落下ダメージおよび着地よろけ硬直が発生し始める落下距離の閾値
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Fall" )
	float DamageFallDistance = 3000.0f;

	// 強制死亡（即死）判定となる落下距離の閾値
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Fall" )
	float DeathFallDistance = 6000.0f;

	// 通常のよろけ落下（DamageFallDistance超過時）に受けるダメージ量
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Fall" )
	float NormalFallDamage = 20.0f;

	// -------------------------------------------------------
	// 落下中の重力ランプ（下降が続くほど重力を強め、落下を加速させる）
	// -------------------------------------------------------

	// 落下（下降）中に重力を徐々に強めるか（機能フラグ）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Fall|GravityRamp" )
	bool bEnableFallGravityRamp = true;

	// 下降開始からランプ（重力増加）を始めるまでの猶予時間（秒）。0 で即座に強め始める
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Fall|GravityRamp", meta = ( EditCondition = "bEnableFallGravityRamp", ClampMin = "0.0" ) )
	float FallGravityRampStartDelay = 0.0f;

	// 猶予後、重力が最大倍率に達するまでにかかる時間（秒）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Fall|GravityRamp", meta = ( EditCondition = "bEnableFallGravityRamp", ClampMin = "0.01" ) )
	float FallGravityRampDuration = 1.0f;

	// ランプ最大時に基準 GravityScale へ掛ける倍率（1.0 で変化なし・2.0 で通常の2倍まで加速）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Fall|GravityRamp", meta = ( EditCondition = "bEnableFallGravityRamp", ClampMin = "1.0" ) )
	float FallGravityRampMaxScale = 2.0f;

	// 通常のよろけ着地時に発生するヒットストップの持続時間（秒）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Fall|HitStop" )
	float FallDamageHitStopDuration = 0.15f;
	// 通常のよろけ着地時の時間スケール倍率
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Fall|HitStop" )
	float FallDamageHitStopDilation = 0.2f;

	// 即死級の着地時に発生するヒットストップの持続時間（秒）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Fall|HitStop" )
	float FallDeathHitStopDuration = 0.5f;
	// 即死級の着地時の時間スケール倍率。
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Fall|HitStop" )
	float FallDeathHitStopDilation = 0.2f;

	// モーションのみモード（bFallDamageMotionOnly）の着地ヒットストップ持続時間（秒。0でなし＝モーションだけ）。通常ダメージ着地とは独立調整
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Fall|HitStop", meta = ( EditCondition = "bFallDamageMotionOnly", ClampMin = "0.0" ) )
	float FallMotionOnlyHitStopDuration = 0.0f;
	// モーションのみモードの着地ヒットストップ時間スケール倍率（Duration が 0 のときは無効）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Action|Fall|HitStop", meta = ( EditCondition = "bFallDamageMotionOnly" ) )
	float FallMotionOnlyHitStopDilation = 0.2f;

	// ==========================================
	// ロックオン設定
	// ==========================================

	// ロックオン可能な最大距離
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "LockOn" )
	float CanLockOnDistance = 4000.0f;

	// これ以上離れたら即座に解除する距離
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "LockOn" )
	float MaxLockOnDistance = 6000.0f;

	// 障害物に隠れてから解除されるまでの猶予時間
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "LockOn" )
	float TimeToLoseLockOn = 0.5f;

	// ロックオン中の最大歩行速度（カニ歩きは通常より遅く設定するのが一般的）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "LockOn|Movement" )
	float LockOnMaxWalkSpeed = 500.0f;

	// ロックオン中の旋回速度（敵の移動に対して素早く向きを合わせるため高めに設定）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "LockOn|Movement" )
	float LockOnRotationRateYaw = 300.0f;

	// ==========================================
	// 被弾・ダメージ関連パラメータ
	// ==========================================

	// 被弾時の無敵時間（秒）
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "HitReaction", meta = ( ClampMin = "0.0" ) )
	float HitInvincibilityTime = 1.0f;

	// --- 被弾時のカメラ揺れ（ダメージ量で小・中・大の3段階）---
	// 共通のカメラ揺れアセット。段階ごとに下の Scale を変えて強弱を付ける（未設定なら揺れない）
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "HitReaction|CameraShake" )
	TSubclassOf<UCameraShakeBase> HitCameraShake;

	// 「中」とみなす下限ダメージ（これ未満は「小」）
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "HitReaction|CameraShake", meta = ( ClampMin = "0.0" ) )
	float HitCameraShakeMediumThreshold = 10.0f;

	// 「大」とみなす下限ダメージ（これ以上は「大」。Medium 以上 Large 未満が「中」）
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "HitReaction|CameraShake", meta = ( ClampMin = "0.0" ) )
	float HitCameraShakeLargeThreshold = 30.0f;

	// 小ダメージ時のカメラ揺れ Scale
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "HitReaction|CameraShake", meta = ( ClampMin = "0.0" ) )
	float HitCameraShakeScaleSmall = 0.5f;

	// 中ダメージ時のカメラ揺れ Scale
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "HitReaction|CameraShake", meta = ( ClampMin = "0.0" ) )
	float HitCameraShakeScaleMedium = 1.0f;

	// 大ダメージ時のカメラ揺れ Scale
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "HitReaction|CameraShake", meta = ( ClampMin = "0.0" ) )
	float HitCameraShakeScaleLarge = 1.5f;

	// 吹っ飛び時に後ろ方向へ押し出す力
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "HitReaction|Blowback", meta = ( ClampMin = "0.0" ) )
	float BlowbackHorizontalPower = 1500.0f;

	// 吹っ飛び時に上方向へ打ち上げる力
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "HitReaction|Blowback", meta = ( ClampMin = "0.0" ) )
	float BlowbackVerticalPower = 1000.0f;

	// 吹っ飛び開始からこの秒数が経過すると、回避／ジャンプで吹っ飛びやられを即キャンセルできる
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "HitReaction|Blowback", meta = ( ClampMin = "0.0" ) )
	float BlowbackCancelEnableTime = 0.4f;

	// 吹っ飛び復帰ジャンプのジャンプ力倍率（通常ジャンプ力に対する割合。0.5 で半分）
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "HitReaction|Blowback", meta = ( ClampMin = "0.0" ) )
	float RecoveryJumpPowerRate = 0.7f;

	// 吹っ飛び復帰ジャンプ ST モーションの再生速度倍率
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "HitReaction|Blowback", meta = ( ClampMin = "0.01" ) )
	float RecoveryJumpStPlayRate = 1.6f;

	// 起き上がり終了後、この秒数はDashのTurnを抑制し地上移動を入力方向ベースにする（後ろを向いたまま復帰しても正面入力でターン暴発しないため）
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "HitReaction|Blowback", meta = ( ClampMin = "0.0" ) )
	float RecoveryTurnSuppressTime = 0.3f;

	// ==========================================
	// 残像設定
	// ==========================================
	// 回避成功時の残像マテリアル（単色発光などを指定）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "GhostTrail" )
	TObjectPtr<class UMaterialInterface> DodgeGhostTrailMaterial;
	// チャージアクション成功時の残像マテリアル（単色発光などを指定）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "GhostTrail" )
	TObjectPtr<class UMaterialInterface> ChargeActionGhostTrailMaterial;
	// 加速ギミックのブーストダッシュ中の残像マテリアル（未設定時は ChargeActionGhostTrailMaterial にフォールバック）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "GhostTrail" )
	TObjectPtr<class UMaterialInterface> BoostDashGhostTrailMaterial;
	// 残像を連続スポーンする間隔（秒）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "GhostTrail" )
	float GhostTrailSpawnInterval = 0.05f;
	// 各残像がフェードアウトして完全に消えるまでの時間（秒）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "GhostTrail" )
	float GhostTrailLifespan = 0.5f;

	// ==========================================
	// 入力バッファ・猶予時間
	// ==========================================

	// 回避ボタンが押された後、方向キーの入力を待つ猶予時間（秒）。同時押しのズレを吸収する。
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "InputBuffer" )
	float DodgeDirectionWaitTime = 0.05f;

	// 回避の先行入力受付時間（アクション中に回避が押された場合、この時間内なら次のアクションとして予約される）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "InputBuffer" )
	float DodgeBufferTime = 0.3f;

	// チャージ開始アクションの先行入力受付時間
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "InputBuffer" )
	float ChargeBufferTime = 0.2f;

	// 攻撃の先行入力受付時間
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "InputBuffer" )
	float AttackBufferTime = 0.2f;

	// ジャンプの先行入力受付時間
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "InputBuffer" )
	float JumpBufferTime = 0.2f;

	// ==========================================
	// カメラ設定
	// ==========================================
	// カメラの横方向（Yaw）の操作感度
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Camera" )
	float CameraYawSensitivity = 1.0f;

	// カメラの縦方向（Pitch）の操作感度
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Camera" )
	float CameraPitchSensitivity = 1.0f;

	// ゲームパッド視点のFPS非依存補正の基準FPS。高FPSほど速く回るのをDeltaTime×この値でスケールして揃える（既定60＝60FPSで従来と同感度）。マウスは補正対象外
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Camera", meta = ( ClampMin = "1.0" ) )
	float CameraGamepadLookReferenceFps = 60.0f;

	// ジャンプ中の注視点の縦"置いていき"（縦デッドゾーン）機能の全体On/Off（全カメラ共通）。OFFで注視点が常にプレイヤーへ完全追従
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Camera|JumpFocus" )
	bool bEnableJumpFocusHold = false;

	// 【上方向】置いていく距離（離陸時の注視点からこの距離まで上昇しても注視点を動かさない。超えたら追従開始）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Camera|JumpFocus", meta = ( EditCondition = "bEnableJumpFocusHold", ClampMin = "0.0" ) )
	float JumpFocusRiseDeadZone = 200.0f;

	// 【上方向】デッドゾーン超過後の追従の速さ（小さいほどゆっくり置いていきながら追う。大きいほどキビキビ縁に保つ）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Camera|JumpFocus", meta = ( EditCondition = "bEnableJumpFocusHold" ) )
	float JumpFocusAirborneInterpSpeed = 10.0f;

	// 【下方向】置いていく距離（離陸時の注視点からこの距離まで落下しても注視点を動かさない。崖落ち等で別調整したい時用）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Camera|JumpFocus", meta = ( EditCondition = "bEnableJumpFocusHold", ClampMin = "0.0" ) )
	float JumpFocusFallDeadZone = 50.0f;

	// 【下方向】デッドゾーン超過後の追従の速さ（崖落ちで着地点を見せたい場合は大きめにして早めに追わせる）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Camera|JumpFocus", meta = ( EditCondition = "bEnableJumpFocusHold" ) )
	float JumpFocusFallInterpSpeed = 50.0f;

	// 着地後、注視点Zをプレイヤー高さへ戻す速さ
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Camera|JumpFocus", meta = ( EditCondition = "bEnableJumpFocusHold" ) )
	float JumpFocusReconvergeSpeed = 12.0f;

	// 離陸時に注視点Zを基準高さへ寄せる速さ（0以下で即座に合わせる）。戻し切る前に跳んだときの残差が1フレームで消えて「ガクッ」となるのを防ぐ（連続幅跳びで効く）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Camera|JumpFocus", meta = ( EditCondition = "bEnableJumpFocusHold" ) )
	float JumpFocusHoldInterpSpeed = 12.0f;

	// プレイヤーが画面上方へ抜けられる最大量（注視点とプレイヤーの許容高低差。0.0以下で無制限の安全クランプ）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Camera|JumpFocus", meta = ( EditCondition = "bEnableJumpFocusHold" ) )
	float JumpFocusMaxVerticalOffset = 0.0f;

	// 連続幅跳びの間は接地しても基準高さ（離陸高さ）を取り直さないか。ON で連鎖を 1 回の滞空として扱い、跳ぶごとに注視点が上下するのを防ぐ（上り坂で顕著）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Camera|JumpFocus", meta = ( EditCondition = "bEnableJumpFocusHold" ) )
	bool bJumpFocusHoldThroughChargeHop = true;

	// カメラ操作を停止してから自動回り込みが開始するまでの猶予時間（秒）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Camera|Charging" )
	float ChargeCameraCenteringDelay = 0.5f;

	// チャージダッシュ開始後のカメラ解除タイミング秒数
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Camera|Charged|Dash" )
	float ChargeCameraPopDelay = 0.2f;

	// カメラの自動背面回り込みを開始する角度閾値（度。例: 20で正面から20度以上横を向くまでカメラ固定）
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Camera|Charging" )
	float ChargeCameraCenteringThreshold = 15.0f;

	// ==========================================
	// 検証用設定
	// ==========================================
	// チャージ中にカメラ操作がない場合、自動でプレイヤーの背後に回り込むか
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Debug|Camera" )
	bool bEnableChargeCameraCentering = true;

	// 「ガクッ」の原因切り分け用の計測しきい値（cm/s・1フレームあたり）。0 で計測しない。
	// 速度変化を CMC／モジュール／面沿いの 3 区間に分けて測り、超過フレームをラッチして ImGui の「面沿い移動情報 →ガクッ検出ログ」に残す
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Debug|SurfaceRide", meta = ( ClampMin = "0.0" ) )
	float MovementSpikeLogThresholdSpeed = 250.0f;

	// 上記ラッチの「向き」側のしきい値（度・1フレームあたり）。大きさが変わらない急な方向転換は差分に出ないため、こちらで拾う
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Debug|SurfaceRide", meta = ( ClampMin = "1.0" ) )
	float MovementSpikeLogThresholdTurnDeg = 12.0f;

	// チャージ解除で攻撃にする
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Debug|ChargeAction" )
	bool bEnableChargeReleaseAttack = false;

	// V2モジュールを使用するかどうか
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Debug|ChargeAction" )
	bool bUseChargeV2 = true;

	// チャージ中のブレーキ処理を使用するか
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Debug|ChargeAction" )
	bool bUseChargingBrake = false;

	// チャージアクションから再チャージした際のギア段階維持を有効にするか
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Debug|ChargeAction" )
	bool bEnableChargeGearKeep = true;

	// チャージダッシュで壁や重い敵に跳ね返る
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Debug|ChargeAction|Dash" )
	bool bEnableChargeDashRebound = false;

	// チャージダッシュで敵を貫通する
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Debug|ChargeAction|Dash" )
	bool bEnableChargeDashThrough = false;

	// 吸着（自動ターゲット補完）を有効にするか
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Debug|ChargeAction|Attack" )
	bool bEnableChargeAttackHoming = true;

	// チャージダッシュの吸着（自動ターゲット補完）を有効にするか
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Debug|ChargeAction|Dash" )
	bool bEnableChargeDashHoming = true;

	// 攻撃ボタン連打でのチャージコンボ進行を有効にするか
	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = "Debug|ChargeAction|Attack" )
	bool bEnableAttackButtonChargeCombo = true;
};
