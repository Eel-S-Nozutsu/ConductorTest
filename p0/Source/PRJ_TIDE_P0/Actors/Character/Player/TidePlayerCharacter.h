// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GenericTeamAgentInterface.h"
#include "Logging/LogMacros.h"

#include "PRJ_TIDE_P0/Actors/Character/TideCharacter.h"
#include "PRJ_TIDE_P0/Data/Player/PlayerAttackType.h"
#include "PRJ_TIDE_P0/Data/Camera/ExCameraTypes.h"
#include "PRJ_TIDE_P0/Interfaces/Environment/IWindAffectable.h"
#include "PRJ_TIDE_P0/Interfaces/Environment/IGroundPullAffectable.h"

#include "TidePlayerCharacter.generated.h"

class UInputBufferComponent;
class ULockOnComponent;
class UFallRecoveryComponent;
class URestartComponent;

class UDodgeActionPlayerModule;
class UDashActionPlayerModule;
class UAttackActionPlayerModule;
class ULockOnPlayerModule;
class UHitReactionPlayerModule;
class UJumpActionPlayerModule;
class UFallActionPlayerModule;
class UGodActionPlayerModule;
class USlidePassivePlayerModule;
class UFinisherPlayerModule;
class ULaunchActionLockPlayerModule;
class UBoostDashPlayerModule;

// ASurfaceRideZone が要求するトンネル挙動の検証フラグ束（ゾーン単位・すべて既定 OFF）。
// プレイヤーは挙動ごとにゾーンの重なり数を数え、1つでも重なっていればその挙動を効かせる
struct FSurfaceRideZoneFlags
{
	bool bInvertLateral = false;	// 90°超で左右反転（原案）
	bool bInvertForward = false;	// 90°超で上下（前後）反転
	bool bScreenRelative = false;	// 左右入力を画面基準に（B案）
	bool bCameraRoll = false;		// カメラ上方向をプレイヤーの上へロール追従（A案）
	bool bChaseCamera = false;		// カメラを進行方向の背後へ追従（方式1・フリールック解除）
	bool bTubeRelative = false;		// 移動入力をチューブ基準に（前=進行方向／左右=円周・方式2）
	bool bUphillAssist = false;		// 上り坂でも減速させず少し伸ばす（DA の SurfaceRideAssist* を参照）
};

UCLASS( abstract )
class ATidePlayerCharacter
	: public ATideCharacter
	, public IGenericTeamAgentInterface
	, public IWindAffectable
	, public IGroundPullAffectable
{
	GENERATED_BODY()

public:
	ATidePlayerCharacter();

	// --- 入力要求 ---

	void RequestMove( float Right, float Forward );
	void RequestMoveEnd();
	void RequestLook( float Yaw, float Pitch );
	void RequestJumpStart();
	void RequestJumpEnd();
	void RequestChargeActionEnd();
	void RequestDash( bool bIsFromDodge = false );
	void RequestLockOn();
	void RequestLockOnTargetSwitch( float SwitchInput );	// 符号で左右判定（負=左／正=右）
	void RequestChargeCancelStart();
	void RequestChargeCancelEnd();
	void RequestCameraReset();

	// CanJump タグによる攻撃キャンセルジャンプ。実行中の攻撃を破棄し、R2 保持中はチャージジャンプを出す
	bool TryJumpCancelDuringAction();

	// R2／ジャンプボタンの物理押下状態。バッファ／Disable を介さないため、打ち上げ封印中の押下も拾える
	void SetChargeInputHeld( bool bHeld );
	void SetJumpInputHeld( bool bHeld );		// 長押しでの滑空移行判定に使う

	// --- 神技（GodAction） ---

	void RequestGodActionStart();
	void RequestGodActionRelease();
	void RequestGodActionCancel();
	void RequestGodArtSelect( int32 Dir );			// 十字キー左右（-1 左 / +1 右）
	void RequestGodArtSelectAt( int32 ArtIndex );	// X／Y／B で直接選択（0 戯 / 1 導 / 2 依）
	void RequestGodArtStickSelect( float AxisX );	// L スティック水平（フリック 1 回＝1 段。移動禁止モード時）
	bool IsGodArtFaceButtonSelectEnabled() const;	// DA: bGodArtSelectByFaceButtons
	bool IsGodArtStanceMovementAllowed() const;		// DA: bGodArtStanceAllowMovement

	// 構え中なら R2 で選択神技を発動する。横取りしたら true（呼び出し側は通常チャージを積まない）
	bool TryGodArtExecute();

	// 構え中かつ直接選択モード。この間は X（攻撃）・B（キャンセル）の通常アクションをマスクする
	bool IsGodArtFaceButtonSelectActive() const;

	// 外部アクターからのゲージ加算。OrbStartWorldLoc からエネルギー玉を飛ばす（演出 OFF なら即時加算）。
	// bAnchorOrbToOwner=true で玉の発生源をプレイヤー追従にする（高速移動で発生源が暴れないように）
	void AddGodActionGauge( float Amount, const FVector& OrbStartWorldLoc, float OrbRadiusScale = 1.0f, float OrbAlphaScale = 1.0f, bool bAnchorOrbToOwner = false );

	// --- ギミック連携（継承なしの BP Actor から呼べる） ---

	// 移動・カメラ視点は封じず、上昇区間の設定割合（LaunchActionLockAscentRate）で自動解除する
	UFUNCTION( BlueprintCallable, Category = "Tide|LaunchLock" )
	void BeginLaunchActionLock();

	// 打ち上げ封印の解除処理（頂点手前のレート地点／着地）から ULaunchActionLockPlayerModule が呼ぶ
	void FlushPendingLaunchChargeRelease();
	void ResumeHeldChargeAfterLaunchLock();

	// 取得した瞬間の向きへ MaxSpeed で加速し Duration の間ブーストダッシュする
	// （方向転換は可能。0 以下なら DA のフォールバック値）。
	// VelocityCap＝加算後の水平速度上限／AirZUpSpeed＝空中取得時のみ加算する上方向速度（0 以下で無効）
	UFUNCTION( BlueprintCallable, Category = "Tide|BoostDash" )
	void BeginBoostDash( float MaxSpeed, float Duration, float VelocityCap = 0.0f, float AirZUpSpeed = 0.0f, float StartMontagePlayRate = 1.0f );

	// 【デバッグ】PlayerParamData のフラグが立っていれば竜巻（大）を目の前に発生させる
	void RequestDebugSpawnTornadoInFront();

	// --- 移動パラメータ ---

	void RefreshMovementParams();
	void UpdateChargingMovementParams( float ChargeRate, float DriftSteeringScale = 1.0f );
	void UpdateFrictionRecovery( float RecoveryRate );
	float ApplyChargingSlopeMultiplierInterp( float TargetMultiplier, float DeltaTime );

	// 【重要】CMC の登坂角度はこの関数だけで設定する（直接 SetWalkableFloorAngle すると壁の上で落ちる）
	void ApplyWalkableFloorAngle( float BaseAngle );
	bool WantsSurfaceRideWalkableRaise() const;			// ライド中／チャージ中かつエリア内
	bool IsSurfaceRideChargeHopEntryAllowed() const;	// 幅跳びでの面沿い入場可否（着地後の ED 待機も含む）

	// DA の ***ForGear 配列を現在のチャージギアで引く。
	// ギア数を超える／未満は端の要素へクランプ。配列が空なのは設定ミスなので中立値 1.0 を返す
	float GetSlopeParamForGear( const TArray<float>& ForGear ) const;

	// --- 床法線・入力基準 ---

	void UpdateSmoothedFloorNormal( float DeltaTime );	// Tick の先頭（モジュール更新より前）で毎フレーム
	void UpdateSurfaceRideInputBasis( float DeltaTime );	// 上方向が決まった後・入力処理より前に 1 回だけ

	// 足元の複数点をトレースしてポリゴンの継ぎ目を均す（サンプルが取れなければ引数をそのまま返す）
	FVector SampleAveragedFloorNormal( const FVector& InFallbackNormal ) const;

	// 坂道の倍率・摩擦・進行方向の投影はすべてこれを使う（生の法線はカクついた床でフレーム毎に飛ぶ）
	FVector GetSmoothedFloorNormal() const { return SmoothedFloorNormal; }

	// 面沿いモードの発動判定はこちら。平滑化値は曲面で傾斜が浅く出て閾値に届かない
	float GetRawFloorAngleDeg() const;

	// 移動入力を解釈する基準（前・右）。通常はワールド水平、面沿いモード中は面の接平面で作る
	void GetInputBasis( FVector& OutForward, FVector& OutRight ) const;

	// 入力基準の「潰れ具合」（ImGui 表示用）。小さいほど不安定域＝カメラが面を正面から見ている
	float GetSurfaceRideInputDegenerateSin() const { return SurfaceRideInputDegenerateSin; }

	// --- 「ガクッ」計測（デバッグ）---
	// Tick の頭 → 面沿い処理の直後 → Tick の末尾の 3 点で呼ぶ。
	// 位置を直接動かす処理は Velocity に出ないため、区間を分けないと出どころが特定できない
	void BeginMovementSpikeFrame( float DeltaTime );
	void MarkMovementSpikeMidTick();
	void EndMovementSpikeFrame( float DeltaTime );

	// 1 フレームの速度変化を CMC／モジュール／面沿いの 3 区間に分けて測り、跳ねたフレームをラッチして残す
	struct FMovementSpikeSample
	{
		float Time = 0.0f;
		float DeltaTimeMs = 0.0f;		// 跳ねていればフレーム落ち由来の見た目の揺れ
		// 区間ごとの速度変化。Cmc は Tick 外の書き込み（ジャンプ・ブースト・被弾）も含む
		float DeltaCmc = 0.0f, DeltaModules = 0.0f, DeltaRide = 0.0f, DeltaPushWind = 0.0f;
		// 同じ区間での速度の「向き」の変化（度）。大きさが変わらない急な方向転換はこちらにしか出ない
		float TurnCmc = 0.0f, TurnModules = 0.0f, TurnRide = 0.0f, TurnPushWind = 0.0f;
		float SpeedIn = 0.0f;
		float ActualSpeed = 0.0f;		// 位置差分 / 前フレームの DeltaTime
		// 法線／接線の内訳。接線側が膨らめば斜面での移動量の伸び、法線側なら縦方向の位置補正
		float MoveUpSpeed = 0.0f, MoveTangentSpeed = 0.0f, VelUpSpeed = 0.0f, VelTangentSpeed = 0.0f;
		// 実移動の出どころ（速度換算）。Modules＝モジュール更新＋面沿い／PushWind＝押し出し＋水平風＋吸い込み／
		// OutsideTick＝CMC の移動・ルートモーション・アニメ通知・他アクターの Tick
		float MoveModulesSpeed = 0.0f, MovePushWindSpeed = 0.0f, MoveOutsideTickSpeed = 0.0f;
		float MaxDepenetration = 0.0f;	// 打ち上げ量がこの範囲内ならめり込み解消が原因の候補
		float GravityScale = 0.0f;		// 0 なら重力ロック中（滞空維持）
		float FloorAngleDeg = 0.0f;		// 生の床傾斜
		float GravityLagDeg = 0.0f;		// 面沿いの追従遅れ
		float FloorDist = 0.0f, MaxStepHeight = 0.0f, WalkableAngle = 0.0f, MaxWalkSpeed = 0.0f;
		float WindSpeed = 0.0f, PenetrationDepth = 0.0f;
		FString PenetrationName;
		FString MontageName;			// ルートモーション源の特定用
		int32 ZoneCount = 0;
		bool bSeparated = false;			// このフレームで床から離れた
		bool bOverlappingGeometry = false;	// スタック → めり込み解消による打ち上げの検出用
		bool bAnimRootMotion = false, bRiding = false, bOnGround = false;
	};
	const TArray<FMovementSpikeSample>& GetMovementSpikeLog() const { return MovementSpikeLog; }
	void ClearMovementSpikeLog() { MovementSpikeLog.Reset(); }

	// --- 面沿い移動モード（重力方向を床法線へ向け、壁面にも立って走れる）。詳細は PlayerArchitecture.md ---

	void UpdateSurfaceRide( float DeltaTime );
	bool IsSurfaceRiding() const { return bIsSurfaceRiding; }
	bool IsSurfaceRideUphillAssistActive() const;
	bool IsSurfaceRidePostureApplied() const { return bSurfaceRidePostureApplied; }	// 解除後の戻し中も true

	// 急斜面（＝壁）の上でエリアを出たときの解除。接地・空中の両経路から呼ぶ。
	// 推進を止めないと落下後もロック移動が水平速度をセットし直して再射出になるためチャージごと終了する
	void HandleSurfaceRideZoneExitRelease();

	// 登坂角度より急な面の上で解除されたときのチャージジャンプモーション（速度・チャージには触らない）。
	// 解除フレームでは流さず「実際に落下し始めたら」流す
	void RequestSurfaceRideFallChargeJumpMotion( float RideSurfaceAngleDeg );
	void UpdateSurfaceRideFallChargeJumpMotion( float Now );

	// 挙動は変えず、エリアを出たのに上の解除へ行かなかったフレームを理由付きで残す
	void RecordSurfaceRideZoneExitSkip( const FString& Reason );

	// 直近のエリア退出の記録。「左から出たら解除されるのに右だと何も起きない」の切り分け用
	struct FSurfaceRideZoneExitRecord
	{
		float Time = -100.0f;			// 発生時刻（未発生は負）
		float SurfaceAngleDeg = 0.0f;	// 乗っていた面の傾斜
		float SpeedBefore = 0.0f, SpeedAfter = 0.0f;
		float UpBefore = 0.0f, UpAfter = 0.0f;	// 上向き成分＝射出の本体
		bool bWasAirborne = false;
		bool bReleased = true;			// false なら SkipReason に見送り理由
		FString SkipReason;
	};
	const FSurfaceRideZoneExitRecord& GetSurfaceRideZoneExitRecord() const { return SurfaceRideZoneExitRecord; }

	// 面沿いモードを許可するエリア（ASurfaceRideZone）の出入り。
	// 重複して覆われても正しく動くよう、挙動ごとに重なり数を別々に数える
	void EnterSurfaceRideZone( const FSurfaceRideZoneFlags& Flags );
	void ExitSurfaceRideZone( const FSurfaceRideZoneFlags& Flags );

	bool IsInSurfaceRideZone() const { return SurfaceRideZoneCount > 0; }
	bool IsInSurfaceRideInvertZone() const { return SurfaceRideInvertZoneCount > 0; }
	bool IsInSurfaceRideInvertForwardZone() const { return SurfaceRideInvertForwardZoneCount > 0; }
	bool IsInSurfaceRideScreenRelativeZone() const { return SurfaceRideScreenRelativeZoneCount > 0; }
	bool IsInSurfaceRideCameraRollZone() const { return SurfaceRideCameraRollZoneCount > 0; }
	bool IsInSurfaceRideChaseZone() const { return SurfaceRideChaseZoneCount > 0; }
	bool IsInSurfaceRideTubeRelativeZone() const { return SurfaceRideTubeRelativeZoneCount > 0; }
	bool IsInSurfaceRideUphillAssistZone() const { return SurfaceRideUphillAssistZoneCount > 0; }
	int32 GetSurfaceRideZoneCount() const { return SurfaceRideZoneCount; }

	// 進行方向（接平面）がどれだけ「登り」を向いているか。1＝真っ直ぐ登り／0＝横切るか下り。
	// アシスト量はこれに比例させるので、平坦・下りではアシストが自然に消える
	float GetSurfaceRideUphillRate() const;

	// ロック移動が参照する。ダッシュは毎フレーム接線速度をハードセットするため加速の加算では伸びない
	float GetSurfaceRideUphillAssistDashSpeedScale() const;

	// フリールック抑止の判定に使う（進行方向追従カメラ＝方式1）
	bool IsSurfaceRideChaseCameraActive() const { return bIsSurfaceRiding && IsInSurfaceRideChaseZone(); }

	// 面沿いモード中は床法線、通常は真上。移動・回転を接平面で組む処理が参照する
	FVector GetSurfaceRideUp() const { return CurrentGravityUp; }

	// --- チャージ滑走 ---

	void ResetChargingTurnRamp();
	void ResetDriftSpeedMaintain();		// 溜め開始時。前回のドリフトで確保した維持目標を持ち越さない
	bool IsDriftSpeedMaintaining() const { return bIsDriftSpeedMaintaining; }
	float GetDriftMaintainSpeed() const { return DriftMaintainSpeed; }
	float GetChargingSlopeMultiplier() const { return ChargingSlopeMultiplier; }	// 補間後。歩行上限へ ×／摩擦へ ÷

	// ImGui 計測用。「前フレーム出口 → 今フレーム入口」の差が CMC（摩擦・ブレーキ）に食われた分、
	// 「入口 → 出口」が自前の補正分
	float GetChargingSpeedIn() const { return ChargingSpeedIn; }
	float GetChargingSpeedOut() const { return ChargingSpeedOut; }
	float GetChargingSpeedPrevOut() const { return ChargingSpeedPrevOut; }
	float GetChargingAppliedGroundFriction() const { return ChargingAppliedGroundFriction; }

	// --- 攻撃 ---

	void RequestAttack( EPlayerAttackType AttackType, bool bForce = false );

	// 起動／停止は AnimNotifyState_WeaponTrail から呼ばれ、
	// NiagaraTag・LifeTime・ソケット・オフセットはすべて Notify 側が保持して渡す
	void StartWeaponTrail( FName NiagaraTag, float LifeTime, FName SocketName, const FVector& LocationOffset, const FRotator& RotationOffset );
	void StopWeaponTrail();

	// とどめ・壊れ物の破壊など「押し返されないヒット」の受け手側から呼ばれる。DA の bEnableFinisherBreakthrough が
	// ON なら慣性を残して前進（切り抜け）、OFF ならヒットバックを打ち消してその場で止める
	void ResolveAttackHitBackOnLethalHit();

	void CancelAttackHitBack();

	// 敵側から呼ばれる。ヒットバックを打ち消し、とどめスローを発動する
	void OnDeliveredFinishingBlow( AActor* Enemy, const FDamageInfo& DamageInfo );

	bool RequestChargeJump();
	void RequestShiftUpChargeGear();

	// --- ギアの手動操作（検証用・専用 IA から呼ばれる） ---
	void RequestChargeGearDown();	// L1。bEnableManualGearDown が ON のとき
	void RequestChargeGearUp();		// R1。bManualGearUpOnly が ON のとき（OFF なら何もしない）

	// --- スライドパッシブ「突風」連携（SlidePassive モジュールからの委譲口） ---
	bool ForceMaxChargeGear();	// 実際に段が上がったら true
	void NotifyChargeDriftSparkBurst();	// ギアアップ時と同じ火花（ドリフト中でなくても出る）
	void ArmGustChargeBuff();	// 次の1チャージアクションに範囲攻撃バフを arm（単発）
	bool IsGustChargeBuffArmed() const;			// 重ね掛け防止判定に使う
	void ClearGustChargeBuff();					// 未消費ぶんを破棄（神技など別アクション移行時）
	bool IsGustBuffedChargeActionActive() const;

	// --- ジャンプ・ダッシュ ---

	void ReserveAutoDashOnLanding();
	void ConsumeAirJumps();
	void EnterJumpFallingLoop();	// 他アクション（一閃の空中終了など）から JumpModule の落下ループへ引き継ぐ
	void EnterJumpLandingEnd();	// 他アクション（滑空など）の着地から JumpModule の JUMP_ED へ引き継ぐ
	void ClearJumpAndLandingDash();	// 神技発動時。着地オートダッシュの暴発を止める
	void ForceStartDash( bool bIsFromDodge = false );
	void StopDash();
	void StartDashInertiaDecayFromSpeed( float StartSpeed );	// 速度を外部から上書きされた状態からの復帰

	// 打ち上げから竜巻ジャンプと同じ風切りトレイルを出す（チャージダッシュ中は出さない）。
	// 破棄は JumpModule 側が行うので停止呼び出しは不要
	void StartJumpWindTrailEffect();

	// 三人称カメラの行キー（FirstJump / ChargeJumpGear1〜3 等）を指定して Push。
	// FirstJumpCameraHandle を共用するため、ポップは RequestPopFirstJumpCamera で行う
	void RequestPushFirstJumpCamera();
	void RequestPushJumpCamera( FName CameraRowName );
	void RequestPopFirstJumpCamera();

	// --- キャンセル・中断 ---

	void CancelCharge( bool bRestoreDash = true );
	void CancelAttack();
	void CancelDodge();
	void CancelAllActions();
	// 被弾でアクションを強制終了したときに呼ぶ。溜め直しに R2、神技の再突入に L2 の押し直しを要求する
	void NotifyChargeInterruptedByDamage();
	void NotifyGodArtStanceInterruptedByDamage();

	// 加速ギミック（ブースト）連携。いずれも中断／再開できたら true
	bool PauseChargeDashForBoost();		// 残り時間を退避する
	bool CancelChargeJumpForBoost();	// 退避・再開なし
	bool ResumeChargeDashAfterBoost();
	bool PauseChargeForBoost();			// ギア／コンボ段を退避する
	bool ResumeChargeAfterBoost();		// 長押し継続時に退避ギアのまま再開する

	// やられ（吹き飛び）キャンセル用：発動条件を無視して強制的にジャンプ／回避させる
	void ForceJump( bool bOverrideXY = false );
	void ForceDodge();
	bool IsHitCancelAirMove() const;	// 緊急ジャンプ中か（空中横移動の特別許可に使う）
	bool HasJumpedThisAirtime() const;	// この滞空でジャンプ済みか。チャージジャンプの封印判定に使う

	// 起き上がり終了と、その直後の抑制ウィンドウ（RecoveryTurnSuppressTime 以内）。
	// Turn 暴発の抑制・入力方向移動に使う
	void NotifyHitReactionRecovered();
	bool IsRecoveryMoveAssistActive() const;
	bool ConsumeCameraResetRequest();

	// --- 状態問い合わせ ---

	bool IsLockOnActive() const;
	bool IsFalling() const;
	bool IsDodging() const;
	bool IsInvincible() const;
	bool IsHitReacting() const;
	bool IsAttacking() const;
	bool IsDashing() const;
	bool IsCharging() const;
	bool IsPlayingChargeAction() const;
	bool IsPlayingChargeDash() const;
	bool IsPlayingChargeJump() const;
	bool IsPlayingChargeAttack() const;
	bool IsPlayingChargeHopJump() const;	// ダッシュ由来ジャンプ中か（着地後の CHARGE_HOP_ED 待機を含む）
	bool IsAirChargeDashing() const;		// 空中で開始したチャージダッシュ中か（着地後の ED 待機を含む）
	bool IsDead() const;

	// CHARGE_DASH_ED の硬直中で、まだ MoveCancelable が付いていない区間か（移動・ダッシュでのキャンセルを弾く）
	bool IsChargeDashEndMovementLocked() const;
	// 空中ダッシュ中は AirChargeDashRotationRateYaw を優先し、未設定なら地上共通値を返す
	float GetChargeDashRotationRateYaw() const;
	// 空中チャージダッシュを使い切った／空中チャージ攻撃を出した後の空中か。true の間は攻撃・ジャンプ
	// （＋長押しの滑空）・神技のみ許可し、チャージ・回避・ダッシュは受け付けない。着地で解除される
	bool IsAirActionLimitedAfterAirCharge() const;
	// 地上の弱攻撃コンボがマスクされているか（DA: bEnableGroundNormalAttack が OFF ＋ 地上）
	bool IsGroundNormalAttackMasked() const;
	// チャージジャンプ／幅跳び中に R2 を離しているか（＝攻撃入力を振り下ろしへ回す窓）
	bool IsChargeJumpNormalAttackWindow() const;

	bool IsGodActionActive() const;		// ロックオン・斬撃のいずれかが進行中
	bool IsGodActionLockingOn() const;	// L2長押しの対象選択フェーズ
	bool IsGodArtSelecting() const;		// 3択メニューの構え中（選択・入力マスク判定に使う）
	bool IsGodActionExecuting() const;	// 発動専有中（憑依の一閃 Slashing のみ。構え選択中は通常アクション自由）
	bool IsGodFrolicActive() const;		// 戯れの自律鳥が飛行中（常駐追従鳥の非表示制御に使う）
	bool IsGodGuidanceActive() const;	// 導きの突進鳥が進行中（常駐追従鳥の非表示制御に使う）
	// 移動禁止モードの構え中か。慣性ブレーキ（UpdateGodArtStanceBraking）が速度を握る区間なので、
	// 滑走を維持する側の処理（坂道の滑り落ち・低摩擦化・チャージダッシュの推進）はこれを見て一斉に止める
	bool IsGodArtStanceMovementLocked() const;
	// 一閃・ワイドカットの切り抜け中（Loop）か。LP モンタージュに攻撃判定が残っていた場合に
	// OnModifyDamageInfo で通常ヒットストップの誤発火を抑止する保険に使う
	bool IsGodSlashWideCutLoopPassthrough() const;

	bool IsLaunchActionLocked() const;
	// 打ち上げの上昇中か。封印（LaunchActionLockAscentRate）が途中で解けても頂点まで true。
	// 上昇中にチャージ（溜め）へ入らせないためのゲートに使う
	bool IsLaunchAscending() const;
	bool IsBoostDashing() const;
	bool IsBoostDashExiting() const;	// 地上ブースト終了直後の滑らか旋回（Turn 検知抑止）中か

	// --- 空中攻撃・滑空 ---

	// 非チャージの空中攻撃ボタンで振り下ろし（縦ダイブ）を開始する。
	// AttackActionPlayerModule から地上ライトコンボの代わりに呼ぶ
	bool TryStartAirNormalDiveAttack();
	// チャージ演出（神鳥等）が「チャージ攻撃中」と誤認するのを避ける除外判定に使う
	bool IsAirNormalDiveAttack() const;
	// 空中チャージ攻撃（横ダイブ）中で他アクションを封印する区間か。着地 ED のキャンセル窓で解除される
	bool IsAirChargeAttackActionLocked() const;

	// 滑空中か（空中でジャンプ長押し〜離す/着地/攻撃/持続時間切れ）。攻撃で振り下ろしへキャンセルさせる判定、
	// 移動・ジャンプモーション制御の抑止、神鳥の手元固定などに使う
	bool IsInGlideSession() const;
	float GetGlideRemainingTime() const;		// 滑空中のみ有効。フレネルの点滅判定に使う
	bool RequestGlideAfterAirChargeDash();		// 空中チャージダッシュ終わりの引き継ぎ

	// --- カメラ ---

	// 神技演出カメラ（UGodSlashExCameraMode）へフレーミング情報を渡す。演出中なら true
	bool GetGodSlashCameraFraming( FVector& OutFocusLocation, FVector& OutTargetCenter, float& OutTargetRadius, int32& OutCutIndex, bool& bOutCutLanded, bool& bOutWideCut, bool& bOutWideCutFinishing ) const;

	// 空中チャージダッシュ専用カメラへ渡すダイブ方向。モジュール側が発動時に設定し、寄せ切ったらクリアする
	bool GetAirChargeDashCameraFraming( FRotator& OutTargetRotation ) const;
	void SetAirChargeDashCameraTarget( const FRotator& InTargetRotation );
	void ClearAirChargeDashCameraFraming();
	// 寄せ演出中＝カメラ入力ロック中か。フレーミング有効（専用カメラ表示中）とは分離しており、
	// 寄せ完了後はカメラ保持のままロックだけ解除される
	bool IsAirChargeDashCameraSwinging() const { return bAirChargeDashCamInputLocked; }
	void SetAirChargeDashCameraInputLocked( bool bLocked ) { bAirChargeDashCamInputLocked = bLocked; }

	bool IsCameraInputActive() const;
	// ロックオン開始時に呼ぶ。直前のスティック操作が残ってカメラが手動扱い（対象に追従しない）になるのを防ぐ
	void ResetCameraInputActivity();

	// --- IWindAffectable（PL は巻き上げではなくジャンプ強化を受ける） ---

	virtual void OnWindEnter( const FWindInfluence& Wind ) override;
	virtual void OnWindTick( const FWindInfluence& Wind, float DeltaTime ) override;
	virtual void OnWindExit() override;
	// 向かい風ゾーンのエリア（無効エリア除く・遮蔽無視）の出入り
	virtual void OnWindZoneAreaEnter() override { ++WindZoneAreaCount; }
	virtual void OnWindZoneAreaExit() override { if ( WindZoneAreaCount > 0 ) --WindZoneAreaCount; }

	bool IsInWind() const { return WindSourceCount > 0; }
	float GetWindJumpBoostMultiplier() const { return IsInWind() ? CurrentWindJumpBoost : 1.0f; }
	// 抵抗倍率適用前の生値（cm/秒）
	const FVector& GetCurrentHorizontalWindVelocity() const { return CurrentHorizontalWindVelocity; }
	// 竜巻は HorizontalWindVelocity が常にゼロなので、IsInWind()（巻き上げ判定）とは独立に判定できる
	bool IsReceivingHorizontalWind() const { return !CurrentHorizontalWindVelocity.IsNearlyZero(); }
	bool IsFacingHeadwind() const { return IsReceivingHorizontalWind() && !IsWindTailwind( CurrentHorizontalWindVelocity ); }
	bool IsFacingTailwind() const { return IsReceivingHorizontalWind() && IsWindTailwind( CurrentHorizontalWindVelocity ); }
	// 進行方向に依らない静的なエリア判定なので、遮蔽で押し出しが止まっていても true のまま
	// （向き依存の判定は IsFacingHeadwind()）
	bool IsInHeadwindZone() const { return WindZoneAreaCount > 0; }

	// WindZone 専用（アリジゴクの引き込みは GetGroundPullScale()）。状態は
	// ブースト＞突風＞チャージダッシュ＞通常の優先度。向かい風／追い風スケールはなす角のコサインで連続ブレンド
	float GetWindPushScale( const FVector& WindDirection = FVector::ZeroVector ) const;

	// --- IGroundPullAffectable（アリジゴクの渦などの地面の吸い込み。Wind とは別現象）---

	// Enter/Exit は素通りでよいので Tick のみ実装する
	virtual void OnGroundPullTick( const FGroundPullInfluence& Pull, float DeltaTime ) override;
	// 状態優先度は GetWindPushScale() と共通だが、風向きの概念はなく専用の GroundPullScale* を返す
	float GetGroundPullScale() const;

	// --- その他 ---

	FName GetChargeAttackAnimTag() const;
	int32 GetCurrentChargeComboIndex() const;
	int32 GetCurrentChargeGearIndex() const;
	float GetCurrentRotationRateYaw() const;
	void SetCustomTimeDilation( float TimeDilation );
	void StopVelocity();
	void CaptureFrictionRecoveryStartParams();
	float CalculateDamage( float DamageMultiplier );

	bool HasDodgeInputBuffered() const;
	bool HasRecoveryInput() const;
	void SetDodgeInputHeld( bool bHeld ) { bIsDodgeInputHeld = bHeld; }
	bool IsDodgeInputHeld() const { return bIsDodgeInputHeld; }
	FVector2D GetRawMovementInput() const { return CachedMovementInput; }
	FVector2D GetAllowedMovementInput() const;
	// 生のスティック入力をカメラ向き基準のワールド方向へ変換する（正規化は呼び出し側）
	FVector GetControlRelativeInputDirection( const FVector2D& InRawInput ) const;

	// チャージダッシュ／ブーストダッシュ共通の Loop BlendSpace 駆動。GearIndex は AnimBP でのギア別 BS 選択用
	void UpdateChargeDashLoopBlendSpace( int32 GearIndex, float DeltaTime );
	void StopChargeDashLoopBlendSpace();

	bool IsPushingPawn() const { return bIsPushingPawn; }
	FVector GetPushTargetSafetyLocation() const { return PushTargetSafetyLocation; }

	UAnimMontage* GetAnimMontage( const FName& MontageName ) const;
	float PlayAnimMontage( const FName& MontageName, float InPlayRate = 1.0f, FName StartSectionName = NAME_None );
	virtual void StopAnimMontage( class UAnimMontage* AnimMontage = nullptr ) override;
	void StopAnimMontage( float BlendOutTime, class UAnimMontage* AnimMontage = nullptr );

	virtual FGenericTeamId GetGenericTeamId() const override { return FGenericTeamId( 0 ); }

	FORCEINLINE UInputBufferComponent* GetInputBufferComponent() const { return InputBufferComponent; }
	FORCEINLINE ULockOnComponent* GetLockOnComponent() const { return LockOnComponent; }
	FORCEINLINE UFallRecoveryComponent* GetFallRecoveryComponent() const { return FallRecoveryComponent; }
	FORCEINLINE UFallActionPlayerModule* GetFallActionModule() const { return CachedFallModule; }

	// モジュール同士を直接参照させないため、本体で受けて GodBird モジュールへ委譲する
	bool StartGodBirdTornadoEscort( const FVector& TornadoCenter );
	bool IsGodBirdShownByPlayerAction() const;

	// --- デバッグ ---

	void StopAllMovementAndInputs();
	void DebugDamage( float DamageAmount );
	// 各モジュールの DrawDebugImGui が Shipping では宣言ごと消えるため、仲介側もまとめて落とす
#if !UE_BUILD_SHIPPING
	void DrawAttackDebugImGui();
	void DrawDodgeDebugImGui();
	void DrawLockOnDebugImGui();
	void DrawFallDebugImGui();
	void DrawChargeActionDebugImGui();
	void DrawGlideDebugImGui();
	void DrawGodActionDebugImGui();
	void DrawSlidePassiveDebugImGui();
#endif
	// InMaxHeightDiff > 0 なら吸着を許す高さ差の上限も上下の扇として描く（0 で無制限＝描画しない）
	void DrawDebugHomingArea( const FVector& InStartLoc, const FVector& InDefaultDir, const FVector& InResultDir, class ULockOnTargetComponent* InTargetComp, float InDistance, float InAngle, float InDuration, FColor InAreaColor = FColor::Cyan, float InMaxHeightDiff = 0.0f ) const;

private:
	virtual void BeginPlay() override;
	virtual void EndPlay( const EEndPlayReason::Type EndPlayReason ) override;
	virtual void Tick( float DeltaTime ) override;
	virtual void UnPossessed() override;

	virtual void FellOutOfWorld( const UDamageType& DmgType ) override;
	virtual void OnMovementModeChanged( EMovementMode PrevMovementMode, uint8 PreviousCustomMode ) override;
	virtual void NotifyHit( class UPrimitiveComponent* MyComp, AActor* Other, class UPrimitiveComponent* OtherComp, bool bSelfMoved, FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit ) override;
	virtual void Landed( const FHitResult& Hit ) override;
	virtual void OnModifyDamageInfo( FDamageInfo& OutDamageInfo, FGameplayTag AttackTypeTag, AActor* Target = nullptr ) override;
	virtual void OnAttackHitConfirmed( const FDamageInfo& DamageInfo ) override;

	void UpdatePushPawn( float DeltaTime );

	// 風源が渡す目標値へ補間してから位置オフセットを加え、風の急な出入りを緩和する
	void UpdateHorizontalWindPush( float DeltaTime );

	// 竜巻へ触れた瞬間、ジャンプ入力なしで竜巻ジャンプを発動させる
	// （PlayerParamData の bForceWindJumpOnTornadoEnter が ON のときのみ）
	void TryForceWindJumpOnEnter();

	virtual float PlayAnimMontage( class UAnimMontage* AnimMontage, float InPlayRate = 1.f, FName StartSectionName = NAME_None ) override;

	UFUNCTION() void OnMontageEnded( UAnimMontage* Montage, bool bInterrupted );
	UFUNCTION() void OnMontageBlendingOut( class UAnimMontage* Montage, bool bInterrupted );
	UFUNCTION() void OnStatusDeath();
	UFUNCTION() void OnStatusRevive();

	void ExecutePlayerDeath();
	void ExecutePlayerRevive();

	EDamageResult ReceiveDamage( const FDamageInfo& DamageInfo ) override;
	bool CanBeDamaged() const override;

	// ダメージ量を小・中・大に分類し、それぞれの Scale で PlayerParamData の共通アセットを再生する
	void PlayHitCameraShake( float DamageAmount );

	void ApplyNormalMovementParams();
	void ApplyChargingMovementParams();
	void ApplyChargeDashMovementParams();
	void ApplyChargeAttackMovementParams();
	void ApplyChargeJumpMovementParams();
	void ApplyDodgeActionMovementParams();
	void ApplyDashActionMovementParams();
	void ApplyLockOnMovementParams();
	void ApplyBoostDashMovementParams();

	void RemoveBasicPermissionTags();
	void SetupModules();
	void UpdateUI();
	bool IsUsingChargeV2() const;
	bool IsWindTailwind( const FVector& WindDirection ) const { return FVector::DotProduct( WindDirection, GetActorForwardVector() ) > 0.0f; }
#if !UE_BUILD_SHIPPING
	void DrawDebugCoordinate();
#endif

public:
	UPROPERTY( EditAnywhere ) TObjectPtr<class UTidePlayerParamDataAsset> PlayerParamData;
	UPROPERTY( EditAnywhere ) TObjectPtr<class UAnimMontageListDataAsset> AnimMontageDataAsset;
	UPROPERTY( EditAnywhere ) TObjectPtr<class UNiagaraSystemListDataAsset> NiagaraSystemDataAsset;
	UPROPERTY( EditAnywhere ) TObjectPtr<UMaterialParameterCollection> PlayerMPC;

	// ModeClass に UGodSlashExCameraMode を割り当てた ExCameraModeParam
	UPROPERTY( EditAnywhere, Category = "Tide|GodAction" )
	TObjectPtr<class UExCameraModeParam> GodSlashCameraParam;

	// ModeClass = UAirChargeDashExCameraMode。通常カメラに割り込ませるため
	// CommonParams.Priority を通常（0）より高く設定しておく
	UPROPERTY( EditAnywhere, Category = "Tide|Charging" )
	TObjectPtr<class UExCameraModeParam> AirChargeDashCameraParam;

private:
	UPROPERTY( Transient ) TArray<TObjectPtr<class UTidePlayerModule>> Modules;
	UPROPERTY( Transient ) TObjectPtr<class UChargeActionPlayerModule> CachedChargeModule;	// Deprecated
	UPROPERTY( Transient ) TObjectPtr<class UChargeActionPlayerModule_V2> CachedChargeModuleV2;
	UPROPERTY( Transient ) TObjectPtr<UDodgeActionPlayerModule> CachedDodgeModule;
	UPROPERTY( Transient ) TObjectPtr<UDashActionPlayerModule> CachedDashModule;
	UPROPERTY( Transient ) TObjectPtr<UAttackActionPlayerModule> CachedAttackModule;
	UPROPERTY( Transient ) TObjectPtr<class ULockOnPlayerModule> CachedLockOnModule;
	UPROPERTY( Transient ) TObjectPtr<class UHitReactionPlayerModule> CachedHitReactionModule;
	UPROPERTY( Transient ) TObjectPtr<class UGlideActionPlayerModule> CachedGlideModule;
	UPROPERTY( Transient ) TObjectPtr<UJumpActionPlayerModule> CachedJumpModule;
	UPROPERTY( Transient ) TObjectPtr<UFallActionPlayerModule> CachedFallModule;
	UPROPERTY( Transient ) TObjectPtr<UGodActionPlayerModule> CachedGodActionModule;
	UPROPERTY( Transient ) TObjectPtr<USlidePassivePlayerModule> CachedSlidePassiveModule;
	UPROPERTY( Transient ) TObjectPtr<UFinisherPlayerModule> CachedFinisherModule;
	UPROPERTY( Transient ) TObjectPtr<ULaunchActionLockPlayerModule> CachedLaunchLockModule;
	UPROPERTY( Transient ) TObjectPtr<UBoostDashPlayerModule> CachedBoostDashModule;
	UPROPERTY( Transient ) TObjectPtr<class UGodBirdPlayerModule> CachedGodBirdModule;
	UPROPERTY( Transient ) TObjectPtr<class UNiagaraComponent> WeaponTrailVFX;

	UPROPERTY( VisibleAnywhere ) TObjectPtr<UInputBufferComponent> InputBufferComponent;
	UPROPERTY( VisibleAnywhere ) TObjectPtr<ULockOnComponent> LockOnComponent;
	UPROPERTY( VisibleAnywhere ) TObjectPtr<UFallRecoveryComponent> FallRecoveryComponent;
	UPROPERTY( VisibleAnywhere ) TObjectPtr<URestartComponent> RestartComponent;

	TArray<FGameplayTag> BasicPermissionTags;
	FTimerHandle RestartDelayTimerHandle;	// 秒数は PlayerParamData->PlayerRestartDelay
	FCameraModeHandle FirstJumpCameraHandle;
	FVector2D CachedMovementInput = {};

	bool bIsDead = false;
	bool bIsDodgeInputHeld = false;
	bool bIsDashInputHeld = false;
	bool bIsCameraResetRequested = false;
	bool bIsPushingPawn = false;
	FVector PushTargetSafetyLocation = FVector::ZeroVector;

	FVector SmoothedFloorNormal = FVector::UpVector;
	bool bWasMovingOnGround = false;	// 床法線のスナップを着地の瞬間だけに限定するための前フレーム接地状態

	// --- 面沿い移動モード ---
	bool bIsSurfaceRiding = false;
	// 縮退域で暴れないよう持ち回し＋レート制限した接平面での前方
	FVector SurfaceRideInputForward = FVector::ZeroVector;
	float SurfaceRideInputDegenerateSin = 1.0f;

	FVector CurrentGravityUp = FVector::UpVector;	// 補間中の上方向（重力方向の逆）
	// ライド中は true、解除後は「立ち切る」まで true のまま。
	// 重力方向が戻り切ったかだけで打ち切ると傾きが取り残される（UpdateSurfaceRide の早期 return 参照）
	bool bSurfaceRidePostureApplied = false;
	// 天井・壁から降りたときのチャージジャンプモーションの予約時刻（負で予約なし）。
	// 解除フレームはまだ接地しているので、落下が始まるのを待ってから流す
	float SurfaceRideFallMotionRequestTime = -100.0f;
	float LastSurfaceRideGroundTime = -100.0f;
	FSurfaceRideZoneExitRecord SurfaceRideZoneExitRecord;
	bool bWasInSurfaceRideZoneAllowed = false;		// 前フレームのエリア在籍（退出フレームの検出用）
	int32 SurfaceRideZoneCount = 0;					// 現在重なっている ASurfaceRideZone の数
	int32 SurfaceRideInvertZoneCount = 0;			// うち左右反転（原案）を要求するエリアの数
	int32 SurfaceRideInvertForwardZoneCount = 0;	// うち上下（前後）反転を要求するエリアの数
	int32 SurfaceRideScreenRelativeZoneCount = 0;	// うち画面基準入力（B案）を要求するエリアの数
	int32 SurfaceRideCameraRollZoneCount = 0;		// うちカメラ上方向ロール追従（A案）を要求するエリアの数
	int32 SurfaceRideChaseZoneCount = 0;			// うち進行方向追従カメラ（方式1）を要求するエリアの数
	int32 SurfaceRideTubeRelativeZoneCount = 0;		// うちチューブ基準入力（方式2）を要求するエリアの数
	int32 SurfaceRideUphillAssistZoneCount = 0;		// うち上り坂アシストを要求するエリアの数

	// --- 「ガクッ」計測用（区間の境目の速度と実移動量。ラッチは新しいものが先頭・最大 8 件）---
	FVector MovementSpikePrevLocation = FVector::ZeroVector;		// 前フレームの Tick 頭
	FVector MovementSpikePrevTickEndLocation = FVector::ZeroVector;	// 前フレームの Tick 末（Tick 内／外の切り分け用）
	FVector MovementSpikeMidTickLocation = FVector::ZeroVector;		// 面沿い処理の直後（押し出し・風の手前）
	FVector MovementSpikeActualMove = FVector::ZeroVector;
	FVector MovementSpikeVelIn = FVector::ZeroVector;
	FVector MovementSpikeVelAfterModules = FVector::ZeroVector;
	FVector MovementSpikeVelAfterRide = FVector::ZeroVector;
	FVector MovementSpikeVelPrevOut = FVector::ZeroVector;
	float MovementSpikeOutsideTickSpeed = 0.0f;	// 前フレーム末 → 今フレーム頭（＝CMC・ルートモーション側）
	float MovementSpikeActualSpeed = 0.0f;
	float MovementSpikePrevDeltaTime = 0.0f;
	bool bMovementSpikeFrameValid = false;
	bool bMovementSpikePrevOnGround = false;	// 床から離れた瞬間を必ずラッチするための前フレーム接地状態
	TArray<FMovementSpikeSample> MovementSpikeLog;

	// --- チャージ滑走 ---
	// 旋回の継続ランプ用。同方向へ旋回し続けている時間と、その旋回方向の符号（+1/-1、0=未旋回）
	float ChargingTurnContinuousTimer = 0.0f;
	float ChargingTurnContinuousSign = 0.0f;
	// 維持目標は「ドリフト開始時の速度」を確保して持つ
	// （現速度を毎フレーム読み直すと CMC の摩擦ロスが複利で積んで落ちていく）
	bool bIsDriftSpeedMaintaining = false;
	float DriftMaintainSpeed = 0.0f;
	// 目標へ補間した後の坂道倍率。目標は面沿いモードの ON/OFF や上り／下り判定の反転で階段状に飛び、
	// そのまま使うと歩行上限と摩擦が一気に変わって「ガクッ」となる
	float ChargingSlopeMultiplier = 1.0f;
	// ImGui 計測用
	float ChargingSpeedIn = 0.0f, ChargingSpeedOut = 0.0f, ChargingSpeedPrevOut = 0.0f;
	float ChargingAppliedGroundFriction = 0.0f;

	// 摩擦復帰（UpdateFrictionRecovery）の開始値
	float RecoveryStartFriction = 0.0f;
	float RecoveryStartBrakingWalking = 0.0f;
	float RecoveryStartBrakingFalling = 0.0f;
	float RecoveryStartAirControl = 0.0f;
	float RecoveryStartGravityScale = 1.0f;

	float LastHitReactionRecoveredTime = -100.0f;	// 起き上がり（吹き飛びやられ復帰）が終わった時刻
	float LastCameraInputTime = -100.0f;

	// フレーミング有効（専用カメラ用）と入力ロック（寄せ演出中のみ）は分離して管理する
	FRotator AirChargeDashCamTargetRotation = FRotator::ZeroRotator;
	bool bAirChargeDashCamFramingValid = false;
	bool bAirChargeDashCamInputLocked = false;

	// --- 風・地面の吸い込み ---
	int32 WindSourceCount = 0;			// 同時に巻き込まれている風源の数（参照カウント）
	float CurrentWindJumpBoost = 1.0f;
	// 竜巻は常にゼロを渡すため WindZone の向かい風判定に使える。風源が無くなればゼロへ戻る
	FVector CurrentHorizontalWindVelocity = FVector::ZeroVector;
	// 実際の押し出しに使う補間済みの値。風に入った瞬間の「一気に押される」感を無くし、
	// 抜けた後も数フレームで滑らかに減衰させる
	FVector SmoothedHorizontalWindVelocity = FVector::ZeroVector;
	// 遮蔽の有無に関わらずエリア内なら加算される（＝実際の押し出しとは独立）。IsInHeadwindZone() が参照する
	int32 WindZoneAreaCount = 0;
};

