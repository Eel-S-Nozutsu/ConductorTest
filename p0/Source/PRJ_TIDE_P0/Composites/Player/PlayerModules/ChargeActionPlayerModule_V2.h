// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

#include "PRJ_TIDE_P0/Composites/Player/PlayerModules/TidePlayerModule.h"
#include "PRJ_TIDE_P0/Utilities/AutomaticTimer.h"
#include "PRJ_TIDE_P0/Data/Camera/ExCameraTypes.h"

#include "ChargeActionPlayerModule_V2.generated.h"

class UNiagaraComponent;
class UChargeEffectSubModule;
class UChargeGuardBrakeSubModule;
class UPlayerAnimInstance;
class UAnimMontage;
class ULockOnTargetComponent;

// チャージ解放後に派生する現在のアクション種別。None はチャージ溜め中・未アクション状態。
enum class EChargeActionV2Type : uint8
{
	None,
	Dash,
	Jump,
	Attack,
	GuardBrake,
};

// ドリフトの旋回方向（進行方向に対して入力がどちら側か）。ギア直上昇モードの「切り返し」判定に使う。
enum class EDriftTurnDirection : uint8
{
	None,
	Left,
	Right,
};

UCLASS()
class UChargeActionPlayerModule_V2 : public UTidePlayerModule
{
	GENERATED_BODY()

public:
	// --- ライフサイクル ---
	virtual void Initialize( ATidePlayerCharacter* InOwner ) override;
	virtual void OnModuleUpdate( float DeltaTime ) override;

	// --- 入力 API（キャラクター本体から呼ばれる） ---
	void BeginCharge();
	void ReleaseCharge();

	// 非チャージの空中攻撃ボタンで振り下ろし（縦ダイブ）を開始する。
	// AttackActionPlayerModule から地上ライトコンボの代わりに呼ばれる
	bool TryStartAirNormalDiveAttack();

	// 打ち上げ封印中のリリースは発動せず、溜めを凍結保持したまま封印解除時まで後回しにする
	void DeferChargeReleaseForLaunchLock();
	void FlushPendingChargeRelease();
	void ResumeHeldChargeAfterLaunchLock();	// R2 を押し続けていれば解除時に溜め直す

	// バッファ／Disable を介さず押下イベントで直接更新するため、打ち上げ封印中に押し始めたケースも拾える
	void SetChargeInputHeldRaw( bool bHeld )
	{
		bChargeInputPhysicallyHeld = bHeld;
		// 離した時点で押し直し待ちは解消する（次の押下は自主的な溜め直しになる）
		if ( !bHeld )
		{
			bRequireChargeRepress = false;
			bRequireChargeRepressByEvent = false;
		}
	}

	// 死亡をまたいで残る R2 の押下ラッチをクリアし、復帰直後の握りっぱなしで再チャージが始まるのを防ぐ
	void ResetChargeInputOnRevive()
	{
		bChargeInputHeld = false;
		bChargeInputPhysicallyHeld = false;
		bBlockChargeInputUntilRelease = false;
		bRequireChargeRepress = false;
		bRequireChargeRepressByEvent = false;
	}

	// 被弾でアクションが強制終了されたとき。押し直し要求（部分適用版）の待ちを立てる
	void NotifyChargeInterruptedByDamage() { RequestChargeRepressByEvent(); }

	void CancelCharge( bool bRestoreDash = true );
	void ExecuteManualCancel();
	void ReleaseManualCancel();

	bool RequestChargeJump();

	// CanJump タグによる攻撃キャンセルジャンプ。実行中のチャージアクション状態を破棄し、R2 保持中なら
	// チャージジャンプを発動して true。離していれば破棄だけ行い false（呼び出し側で通常ジャンプ）
	bool TryChargeAttackCancelJump();

	// DA: bBlockChargeJumpAfterNormalJump ＋ この滞空中にジャンプ済み。着地で自動的に解除される
	bool IsChargeJumpBlockedAfterJump() const;

	// --- 状態クエリ ---
	bool IsCharging() const { return bIsCharging; }
	bool IsPlayingChargeAction() const;
	bool IsPlayingChargeDash() const { return CurrentChargeActionType == EChargeActionV2Type::Dash; }
	bool IsPlayingChargeJump() const { return CurrentChargeActionType == EChargeActionV2Type::Jump; }
	bool IsPlayingChargeAttack() const { return CurrentChargeActionType == EChargeActionV2Type::Attack; }
	bool IsAnyChargeActionTypeSet() const { return CurrentChargeActionType != EChargeActionV2Type::None; }
	bool IsPlayingChargeDashStartMontage() const;
	float GetChargedActionLockRemainingTime() const;
	float GetChargeDashRemainingTime() const;	// ジャンプ中の退避時間も含む

	// 空中で開始した（着地後の ED 待機・入力なし継続を含む）チャージダッシュ中か。地上チャージダッシュ中の
	// ジャンプ割り込み（検証用機能）とは区別し、こちらは回避以外の割り込みを禁止する対象
	bool IsAirChargeDashing() const { return CurrentChargeActionType == EChargeActionV2Type::Dash && bIsAirChargeDash; }
	int32 GetAirChargeDashStage() const { return AirChargeDashStageCount; }	// 0=なし / 1=1回目。着地でリセット
	// 使い切った後の空中アクション制限（ATidePlayerCharacter::IsAirActionLimitedAfterAirCharge）の判定に使う
	bool IsAirChargeDashExhausted() const;
	// 出したあとの空中は空中チャージダッシュ使い切り後と同じ制限にする。着地でリセット
	bool IsAirChargeAttackUsedThisAirtime() const { return bAirChargeAttackUsedThisAirtime; }
	// 空中チャージダッシュ中はフレネルを点滅させず点灯し続けるか（チカチカ防止）
	bool ShouldKeepAirDashFresnelSolid() const;

	// 面沿い（天井・壁）から降りた落下を、ダッシュを継続したままチャージジャンプのモーションで見せる。
	// ダッシュ中は JumpActionPlayerModule のモーション制御が止まるため、ST→LP と着地をこちらで回す
	bool TryStartChargeDashFallJumpMotion();

	// 縦ダイブ機械をチャージ攻撃と共用しているため演出側（フレネル／神鳥／スキッド）が「チャージアクション中」と
	// 誤認する。その除外判定に使う（移動パラメータはダイブに必要なので除外しないこと）
	bool IsAirNormalDiveAttack() const { return CurrentChargeActionType == EChargeActionV2Type::Attack && bIsAirNormalAttack; }
	// 空中チャージ攻撃（横ダイブ）の行動封印中か。ST／LP と着地 ED のキャンセル窓前が対象
	bool IsAirChargeAttackActionLocked() const;
	// チャージジャンプ／幅跳び中の攻撃入力を振り下ろしへ回す窓か（R2 を握っていれば空中チャージ攻撃）
	bool IsChargeJumpNormalAttackWindow() const { return IsPlayingChargeJump() && !bChargeInputHeld && !bIsCharging; }
	// チャージホップ中か（着地後の CHARGE_HOP_ED 待機を含む）。JumpActionPlayerModule の崖落下フォールバックが
	// LP 中の IsPlayingChargeAction()==false に乗じてホップ専用モーションを上書きしないためのガードに使う
	bool IsPlayingChargeHopJump() const { return CurrentChargeActionType == EChargeActionV2Type::Jump && bIsChargeHopJump; }

	// ロックタイマーに依存しない確実な判定
	bool IsPlayingChargeAttackMontage() const;
	// 地上チャージ攻撃（CHARGE_0N_ATK）中か。コンボ受付（CanCombo）を待たせる判定に使う
	bool IsPlayingGroundChargeAttackMontage() const;
	// CHARGE_DASH_ED の硬直中で、まだ MoveCancelable が付いていない区間か（移動・ダッシュのキャンセルを禁止する）
	bool IsChargeDashEndMovementLocked() const;

	// --- 加速ギミック（ブースト）連携。いずれも中断／再開できたら true ---
	bool PauseChargeDashForBoost();		// 残り時間を退避する
	bool CancelChargeJumpForBoost();	// 退避・再開なし（ブーストへ主導権を譲る）
	bool ResumeChargeDashAfterBoost();
	bool PauseChargeForBoost();			// 溜め中のみ。ギア／コンボ段を退避する
	bool ResumeChargeAfterBoost();		// 長押し継続時のみ、退避ギアのまま再開する

	// 溜め・進行中のチャージアクションを畳んで竜巻ジャンプへ主導権を渡す（長押し中の自動再チャージも止める）
	bool CancelChargeActionForWindJump();

	// --- ガードブレーキ ---
	bool IsGuardBraking() const { return CurrentChargeActionType == EChargeActionV2Type::GuardBrake; }
	// 被弾を1回だけ防御する。1回目なら true（防御成立）を返し、以降は false
	bool TryConsumeGuardBrakeBlock();
	// ブレーキ滑り・旋回は共有の摩擦回復タイマーに連動するため、進行状況を SubModule へ公開する
	bool IsFrictionRecoveryFinished() const { return FrictionRecoveryTimer.IsFinish(); }
	float GetFrictionRecoveryRate() const { return FrictionRecoveryTimer.GetRate(); }
	void EndGuardBrakeAction() { CurrentChargeActionType = EChargeActionV2Type::None; }

	// ヒットキャンセルチャージ直後の移動入力倍率（制限窓が無効なら 1.0）
	float GetHitCancelPropulsionScale() const;

	// 真の間はキャラ側がレバー方向への回頭をロックし、対象へ向け続ける（到達でロック解除）
	bool IsChargeDashHomingRotationLocked() const { return bChargeDashHomingRotationLock; }

	// --- エフェクト補助クエリ（SubModule から使用） ---
	FLinearColor GetCurrentGearColor() const;
	float GetCurrentChargeEffectSpeed() const;
	bool IsEffectivelyInAir() const;
	float GetDynamicChargeMultiplier() const;

	// --- ドリフト ---
	// 「n°以上のドリフトを t 秒継続」のゲート判定結果。チャージボーナスと演出が共有して On/Off を完全一致させる
	bool IsDriftBoostActive() const { return bIsDriftBoostActive; }
	float GetDriftBoostAlpha() const { return CurrentDriftBoostAlpha; }	// ドリフト強度（0..1、角度由来）
	// 継続 t 秒のゲート前＝今この瞬間ドリフト条件を満たしているか。ドリフト中の速度維持
	// （ATidePlayerCharacter::UpdateChargingMovementParams）は曲がり始めから効かせたいのでこちらを見る
	bool IsRawDrifting() const { return bIsRawDrifting; }
	EDriftTurnDirection GetDriftTurnDirection() const { return CurrentDriftTurnDirection; }
	// ヒットバックをキャンセルして溜め直し、後退で流されている間か（足元へ小さい火花。演出側が参照）
	bool IsHitbackChargeSparkActive() const;

	// --- イベント通知（キャラクター本体から呼ばれる） ---
	void OnCharacterHit( const FHitResult& Hit );
	void OnLanded();
	void ExecuteRebound( FVector HitNormal );
	void OnAttackHit( AActor* TargetActor, bool bIsRebounded ) override;

	// とどめヒット時に同フレームで呼ばれ、LaunchCharacter で与えた水平速度を消す
	void CancelHitBack();

	// 後退ローンチを前進ローンチへ差し替える（とどめ・壊れ物破壊で「前へ抜ける」用途）。
	// このモジュールがヒットバックを張っていないときは何もしない（前進の二重適用を防ぐ）
	void ApplyBreakthroughMove( const FVector& Direction, float Speed );

	// 対象詰め（TimedApproach）は SetActorLocation 駆動で CMC の Velocity に出ないため、
	// 慣性を見る側はこの実測水平速度（cm/s）も参照する
	float GetChargeAttackApproachSpeed2D() const { return ChargeApproachMeasuredSpeed2D; }

	// 重力ロックと真下プランジは打ち上げと競合して無限に跳ね続けるため、
	// 発動済みの空中ダイブだけを終了させる（溜め・チャージジャンプ・地上アクションは対象外）
	bool CancelAirDiveActionForLaunch();

	// --- コンボ／ギア ---
	// bFromManualInput: R1 由来の手動シフト（bManualGearUpOnly が ON の間、手動以外はすべて弾かれる）
	void ShiftUpGear( bool bFromManualInput = false );
	void ShiftDownGear();	// L1。bEnableManualGearDown が ON かつ溜め中のみ
	// 最大段階へ引き上げる（突風から呼ばれる）。すでに最大／R1 限定モードで弾かれた場合は false
	bool SetGearToMax();

	// --- スライドパッシブ「突風」連携 ---
	void NotifyDriftSparkForcedBurst();	// ギアアップ時と同じ火花（ドリフトしていなくても出す）
	void ArmGustChargeBuff();			// 次の1チャージアクションに範囲攻撃バフを arm（単発消費）
	bool IsGustChargeBuffArmed() const { return bGustChargeBuffArmed; }
	// 未消費ぶんを破棄する（神技など別アクションへの移行時）。次のアクションでの突風バースト暴発を防ぐ
	void ClearGustChargeBuff();
	bool IsGustBuffedActionActive() const { return bGustBuffedActionActive; }	// 消費〜アクション終了まで
	// 突風の後始末用。ガードブレーキはキャンセルの受け皿（派生ではない）なので None と同じ扱いにする
	bool IsGustCarryingChargeActionActive() const { return IsAnyChargeActionTypeSet() && !IsGuardBraking(); }

	int32 GetCurrentChargeComboIndex() const { return CurrentChargeComboIndex; }
	int32 GetCurrentChargeGearIndex() const { return CurrentChargeGearIndex; }
	FName GetChargeAttackAnimTag() const;

	// --- デバッグ ---
	void DrawDebugImGui();

private:
	struct FChargePropulsionSettings
	{
		float Power = 0.0f;
		float Speed = 0.0f;
		float LockTime = 0.0f;
		float GhostTrailDuration = 0.0f;
		float GravityLockTime = 0.0f;
		float ZUpSpeed = 0.0f;			// 滞空中の上昇速度（cm/s）。0 で水平維持
	};

	// --- モジュール更新（OnModuleUpdate から毎フレーム呼ばれる Update 系） ---

	// --- 入力と開始判定 ---
	bool UpdateInputCommands();
	void UpdateChargeBeginCheck();

	// --- 継続ステート・タイマー ---
	void UpdateComboReset();
	void UpdatePostActionChargeWindow( float DeltaTime );
	void UpdateChargeKeptByDodge();
	void UpdateChargeShift( float DeltaTime );
	void UpdateChargingState( float DeltaTime );
	void UpdateDriftBoostState( float DeltaTime );	// 回転ブーストの継続判定とゲート更新（単一ソース）
	void UpdateDriftAttack( float DeltaTime );		// ドリフトブースト中、周囲へ攻撃判定（火花と同タイミング）
	void UpdateDriftGearUp( float DeltaTime );		// 同方向ドリフトの継続でギアを1段上げ、同方向はクールダウン
	void TryDriftGearUp();
	void ResetDriftGearUpState();
	void UpdateFrictionRecoveryState( float DeltaTime );
	void UpdateGravityLock( float DeltaTime );
	void UpdateReservedDashOnLanding();

	// --- アニメーション状態 ---
	void UpdateChargeAnimState( float DeltaTime );
	void UpdateChargeJumpAnimState( float DeltaTime );
	void UpdateAirChargeAttackAnimState( float DeltaTime );
	// 空中チャージ攻撃の移動：ST=滞空（その場で待機）／LP=地面へ斜め突進。GravityScale を自前で 0 にして制御する
	void UpdateAirChargeAttackDive( float DeltaTime );
	void UpdateAirChargeAttackEndCheck();

	// --- 計測のみ（挙動は変えない）---
	void UpdateAirChargeAttackEdAirborneLog();	// 着地ED 中に再び MOVE_Falling へ戻った瞬間
	void UpdateAirborneTransitionLog();			// 接地→落下の瞬間と、そのとき流れていたモンタージュ
	void RecordChargeAttackTriggerLog();		// SetupAirChargeAttackState の入力値
	// チャージ攻撃のモンタージュタグが選ばれた瞬間（発動経路を通らない再生の検出用）
	void RecordChargeAttackMontagePickLog( const FName& PickedTag ) const;
	// 真下の地面までの距離（130cm まで。見つからなければ -1）。「自前射出の浮き」を地上扱いへ
	// 戻す判定（SetupAirChargeAttackState）と計測ログで共用する
	float MeasureGroundDistanceBelow() const;

	// CHARGE_DASH_ED 再生中、MoveCancelable が付いた区間で移動入力があれば ED を打ち切って移動へ返す
	void UpdateChargeDashEndCancel();
	void UpdateChargeDashMontageState( float DeltaTime );
	// CHARGE_HOP_ED の再生終了を監視し、移動入力の有無でダッシュ継続／終了を判定する
	void UpdateChargeHopEndCheck();
	// 上記の判定本体。ブレンドアウト前の末尾到達検知・自然終了フォールバックの両方から呼ぶ
	void ResolveChargeHopLandingDecision();

	// --- 幅跳び（ホップ）の水平速度維持＝MoveSpeed 方式 ---
	// InPreLaunchSpeed は発射直前の水平速度（計測用）
	void BeginChargeHopSpeedMaintain( float InLaunchSpeed, const FVector& InDirection, float InPreLaunchSpeed );
	// 毎フレーム、水平速度の「大きさ」だけ維持目標へ当て直す（向きはエアコントロールでの操舵結果を優先）。
	// あわせて区間別の速度収支（CMC が食った量／自前で戻した量）を計測しログへ積む
	void UpdateChargeHopSpeedMaintain( float DeltaTime );
	void ResetChargeHopSpeedMaintain();		// ホップ終了・別アクション開始・キャンセル時
	float ClampChargeHopHorizontalSpeed( float InSpeed ) const;
	float GetChargeHopMoveSpeedForGear( int32 GearIndex1Based ) const;		// 0 以下＝発射初速を維持
	float GetChargeHopMaxMoveSpeedForGear( int32 GearIndex1Based ) const;	// 0 以下＝上限なし
	// 坂対応（bEnableChargeHopSlopeAlign）が有効なら床法線を上方向として射出フレームごと傾ける
	FVector BuildChargeHopLaunchVelocity( const FVector& ForwardDirection, float HorizontalSpeed, float JumpZ ) const;

	// --- 物理・移動ロック ---
	void UpdatePropulsionLock( float DeltaTime );

	// --- チャージ攻撃の対象詰め（攻撃判定タイミングに合わせて対象手前まで移動する） ---
	// 成立で true（呼び元は弾道射出をスキップする）
	bool TrySetupChargeAttackTimedApproach( const FVector& DashDirection );
	void UpdateChargeAttackTimedApproach( float DeltaTime );	// 到達点へ事前計算イーズで駆動する
	void ClearChargeAttackTimedApproach();					// 終了・キャンセル・リバウンド時
	// 対象（ロックオン優先、無ければ扇形サーチ）。GetHomingDirection と同じ優先順位
	ULockOnTargetComponent* FindHomingTargetComponent( const FVector& InDefaultDir ) const;
	// モンタージュ中で最初の CommonAttack 判定枠の開始秒（無ければ -1）
	float GetFirstHitWindowTriggerTime( UAnimMontage* Montage ) const;

	// --- 演出 ---
	// チャージアクション中の専用カメラ。開始時に発動ギア別キー（ChargeDashGear1~3 / ChargeAttackGear1~3）を
	// Push し、終了・キャンセル時（ResetChargedActionState）に Pop する
	void SetChargeActionCamera( const FName& RowName );	// 既存があれば張り替えて Push
	void PopChargeActionCamera();
	FName MakeChargeActionCameraKey( const TCHAR* Prefix ) const;	// "<Prefix>Gear<1-3>" を作る
	void UpdateGearShiftCamera();
	// 遅延 Pop の進行（地上チャージ攻撃の出し切り／幅跳びの着地）。ホールド時間が経過したら Pop する
	void UpdateDeferredChargeActionCameraPop( float DeltaTime );

	// 空中チャージダッシュ専用カメラ（斜め下ダイブに合わせて背後へ回り込む）。発動時に直進方向から目標回転を作って
	// キャラへ渡し専用モードを Push、寄せ演出が終わったら ControlRotation を目標へ同期して Pop し通常操作へ返す
	void StartAirChargeDashSwingCamera();
	void UpdateAirChargeDashSwingCamera( float DeltaTime );
	void StopAirChargeDashSwingCamera( bool bSyncControlRotation );	// 安全側の後始末にも使う

	// --- 継続維持ルール ---
	bool ShouldSkipComboReset() const;
	bool ShouldPreserveChargeComboState() const;
	void ResetChargeComboAndGearState();
	bool ShouldClearPostActionChargeWindowForNormalAttack() const;
	bool IsPostActionChargeWindowActive() const;
	bool IsCurrentMontageKeepingPostActionWindowAlive() const;
	bool ShouldKeepChargeByDodge() const;
	bool ShouldReleaseChargeKeptByDodge() const;

	// --- チャージ開始アニメ遷移 ---
	bool HasChargeStartMontageFinished( UPlayerAnimInstance* AnimInst ) const;
	void HandleFinishedChargeStartState( UPlayerAnimInstance* AnimInst );
	void UpdateChargeBlendSpaceState( UPlayerAnimInstance* AnimInst, float DeltaTime );
	void UpdateChargeBlendSpaceDirection( UPlayerAnimInstance* AnimInst, float DeltaTime ) const;
	void UpdateChargeBlendSpacePhase( UPlayerAnimInstance* AnimInst, float DeltaTime ) const;

	// 地上チャージダッシュ Loop の BlendSpace 駆動（Dash と同方式）
	void UpdateChargeDashBlendSpace( float DeltaTime );
	void StopChargeDashBlendSpace();

	// --- チャージジャンプ／空中チャージ攻撃の遷移 ---
	// 指定タグのモンタージュが再生し終わったか（別モンタージュへ切り替わった／末尾で止まっている）
	bool HasMontageFinished( FName Tag ) const;
	bool HasChargeJumpStartMontageFinished() const;
	void TransitionChargeJumpToLoop();
	bool HasAirChargeAttackStartMontageFinished() const;
	void TransitionAirChargeAttackToLoop();

	// --- 入力ディスパッチ ---
	void ConsumeBufferedChargeInput( float ChargeBufferTime );
	bool DispatchChargePriorityCommands( const class UTidePlayerParamDataAsset* PlayerParams );
	bool HandleChargePriorityJumpInput( float JumpBufferTime );
	bool HandleChargePriorityAttackInput( float AttackBufferTime );
	bool HandleChargeComboAttackInput( float AttackBufferTime );

	// --- 開始可否判定 ---
	bool IsAttackButtonChargeComboEnabled() const;
	bool IsChargePriorityActive() const;
	// ロック開始直後、タグ（CanAttack / CanCharge 等）が残留して誤発火する短い窓の中か
	// （ChargeV2TagResidueIgnoreTime）。アクション種別を問わず使う汎用判定
	bool IsWithinChargeActionTagResidueWindow() const;
	bool CanStartChargeAttackFromCurrentState() const;
	bool CanStartChargeAttackComboFromCurrentState() const;
	bool IsAirChargeAttackEndCancelable() const;
	bool CanBeginChargeFromCurrentState() const;
	// R2 を握りっぱなしのままの自動再チャージを止めるか（全経路の押し直し要求／部分適用の押し直し要求のどちらか）
	bool IsAutoChargeRestartBlocked() const;
	// 部分適用版（チャージ攻撃最終段・被弾）の押し直し待ちを立てる
	void RequestChargeRepressByEvent();
	// 押し直し待ちのまま溜めが再開しておらず、R2 の長押しが無効な状態か（溜め中は対象外）
	bool IsChargeHoldStale() const;
	// 無効になった長押しラッチ（bChargeInputHeld）を落として「離している」扱いに揃える
	void UpdateStaleChargeHoldRelease();
	bool CanRestartChargeFromCurrentAction( bool bIsAirChargeAttackEdCancelable ) const;
	bool IsChargeStartBlockedByDashStartMontage() const;
	bool IsChargeActionStateFree( bool bIsAirChargeAttackEdCancelable ) const;
	bool IsChargeStartStateValid( bool bIsAirChargeAttackEdCancelable ) const;
	bool CanExecuteChargedAction() const;
	// 攻撃ノックバックをチャージでキャンセルした直後の、チャージアクション禁止時間中か
	bool IsHitCancelChargeActionLocked() const;
	// ヒットバック由来の溜めが後退区間（ステアリング制限中）にいるか（小さい火花＋ギアアップの計測条件）
	bool IsHitbackChargeHoldActive() const;
	void UpdateHitbackChargeGearUp( float DeltaTime );	// 規定秒数の継続でギアを1段上げる（大きい火花）

	// R2 を押さない連打時のみ次の攻撃を遅らせるインターバル
	bool IsHitbackComboIntervalActive() const;
	// インターバルが明けたら、飲み込んでいた攻撃入力をチャージ攻撃コンボとして自動継続する
	void UpdatePendingHitbackComboAttack();
	// 自動継続の「次の一手」。通常のチャージコンボ入力と同じ分岐（最終段は弱攻撃で締める）
	void ContinueHitbackChargeCombo();
	bool ShouldContinueChargeCombo() const;

	// --- アクション要求 ---
	bool RequestChargeDash();
	bool RequestChargeAttack();
	bool RequestChargeAttackCombo();
	bool RequestChargeAttackLegacy();
	bool TryStartChargeAttackFromDash();
	bool TryStartChargeAttackFromCombo();
	bool TryStartFreshChargeAttack();
	bool TryStartChargeJumpFromDash();
	bool TryStartFreshChargeJump();
	// 通常攻撃からの派生時、条件付きでコンボ段数のみリセットする（攻撃は開始しない）
	void ResetChargeComboForNormalAttackDerivation();

	// --- チャージ状態の遷移・開始/終了処理 ---
	void ClearChargeState();
	void ResetChargedActionState();
	// bFromLanding：ACharacter::Landed は MOVE_Falling のまま呼ばれるため OnEndAction 内の IsFalling() では
	// 着地を判別できない。空中終了専用の後始末を弾くために渡す
	void OnEndAction( bool bSkipEdAnimation = false, bool bFromLanding = false );
	void InterruptCurrentChargeAction();
	void PrepareOwnerForCharge();

	// --- 開始準備 ---
	void InitializeChargeStartFlags();
	void UpdateChargeStartComboState();
	void ActivateChargeStartPresentation();
	void StartChargeState();
	void PlayChargeStartMontage();
	void ApplyChargeStartMovement();

	// --- 解放・キャンセル・終了 ---
	void ExecuteChargeReleaseAction();
	// 構えが終わった時点で凍結保持していた溜めを解決する（R2 保持なら継続、離していれば畳む）
	void UpdateGodArtStanceChargeFreeze();
	// 溜めの見た目（モーション／エフェクト／ギアカメラ）だけを落とす。
	// 進行中のチャージダッシュは構え中の凍結で温存するので触らない
	void DiscardChargeForGodArtStance();
	void ResetChargeRuntimeState();
	void RestoreDashIfNeeded( bool bRestoreDash );
	void FinishChargeCancel( bool bRestoreDash );
	void StopFinishedChargeJumpMontages();
	void HandleFinishedChargeDashEnd( bool bSkipEdAnimation );
	void StartPostActionFrictionRecovery( EChargeActionV2Type FinishedActionType );
	void StopCurrentChargeMontages();

	// --- 進行・段数 ---
	void IncrementChargeComboIndex();
	void ResetChargeComboIndex();

	// --- 接地補助 ---
	void ApplyGroundSnap();

	// --- タグ・モンタージュ取得 ---
	FName GetChargeStartAnimTag() const;
	FName GetChargeDashStartAnimTag() const;
	FName GetChargeDashEndAnimTag() const;

	// --- アクション開始時の初期化 ---
	void InitializeChargeDashAction();
	void InitializeChargeAttackAction( bool bForceAirAttack );
	// EffectiveGearIndex: 実効ギア（ダッシュ由来のギア壱固定も反映済み）。ギア別ジャンプカメラの選択に使う
	void InitializeChargeJumpAction( int32 EffectiveGearIndex );

	// --- 開始演出・開始条件の補助 ---
	void SetupAirChargeAttackState( bool bForceAirAttack );
	void SetupChargeJumpDirection( FVector& OutForwardDirection, float& OutInputScale ) const;
	// 面沿い中の射出方向をワールド水平へ倒す（接平面基準の入力をそのまま使うと真上へ跳ね上がるため）
	FVector FlattenSurfaceRideLaunchDirection( const FVector& InDirection ) const;
	void SetupChargeJumpCameraAndAirState( int32 EffectiveGearIndex );
	// bIsHop: 引き継ぎ速度の倍率・上限をホップ用パラメータで参照する
	void ApplyChargeJumpLaunch( const FVector& ForwardDirection, float FinalForwardPower, float FinalJumpZ, bool bIsHop );
	void ApplyChargeJumpActionLock( int32 GearIndex1Based );
	float GetChargeJumpPlayRateForGear( int32 GearIndex1Based ) const;

	// 以下 3 つはギア段階（1 始まり）別の配列を引く。未設定なら Min〜Max のギア補間へフォールバックする
	float GetChargeJumpLockTimeForGear( int32 GearIndex1Based ) const;			// 発動後の移動ロック時間（秒）
	float GetChargeJumpZPowerForGear( int32 GearIndex1Based, bool bIsHop ) const;	// Z 方向初速
	// 前方初速（XY＝飛距離）。幅跳びは持たない（水平速度は ChargeHopMoveSpeedForGear ＋引き継ぎで決まる）
	float GetChargeJumpMovePowerForGear( int32 GearIndex1Based ) const;

	// --- 実際のアクション開始 ---
	void OnStartChargeDash();
	void OnStartChargeAttack( bool bForceAirAttack = false );
	// bForceGearOne: チャージダッシュ由来のジャンプ。現在ギアに依らずギア壱固定の力・再生レートにする
	void OnStartChargeJump( bool bForceGearOne = false );
	void OnStartLightAttack();

	void ResumeChargeDashAfterLanding();	// チャージダッシュ中ジャンプの着地後（検証用）
	// 退避しておいた残り持続時間から走行ループを再構築する。着地復帰・ブースト終了復帰の両方から使う
	void RestartChargeDashWithRemainingTime( float RemainingTime );

	// 突風バフが arm されていれば VFX ＋ 範囲ダメージ実体を発生させて消費する（各アクション開始時）
	void ConsumeGustChargeBuffIfArmed();
	void DestroyActiveGustBurst();			// 追従中のバーストを即破棄（多重スポーン防止）
	void BeginFadeOutActiveGustBurst();		// 即破棄せず "Alpha" を 1→0 にフェードさせてから破棄する
	void UpdateGustBurstFadeOut( float DeltaTime );

	// --- 推進・移動ロック ---
	void ExecuteChargePropulsion();
	bool UpdateReboundSliding( float DeltaTime );
	void UpdateChargeDashLockedMovement( float DeltaTime );
	// ターン（急な逆方向入力）を走り／ダッシュと同じ判定で処理する。
	// ターン中はロック移動を止めてルートモーションの旋回に任せるため true を返す
	bool TryUpdateChargeDashTurn( float DeltaTime );
	// ターン明けの出口方向を入力方向へ微調整できるようにする
	void UpdateChargeDashTurnSteering( float DeltaTime );
	bool ShouldDeferChargedActionEnd() const;
	// 神技の構え中（移動禁止モード）は推進と寿命タイマーを凍結するか
	bool ShouldFreezeChargeDashForGodArt() const;
	void BeginGodArtDashResumeEase();	// 凍結解除時。落ちた速度からダッシュ速度へ戻すイージング
	void UpdateDashLoopAllowance( const FChargePropulsionSettings& Settings );
	// OutTarget に吸着対象（無ければ nullptr）を返す。対象へ当たるまで回頭を制御するため呼び出し側が保持する
	FVector BuildChargeDashDirection( ULockOnTargetComponent*& OutTarget ) const;
	void ApplyChargeLaunchVelocity( const FVector& DashDirection, float FinalDashPower, bool bShouldLaunch );
	void BuildDashPropulsionSettings( FChargePropulsionSettings& OutSettings, int32 ComboIdx, int32 GearIdx ) const;
	void BuildAttackPropulsionSettings( FChargePropulsionSettings& OutSettings, int32 ComboIdx, int32 GearIdx ) const;
	FChargePropulsionSettings BuildChargePropulsionSettings() const;

	// --- 空中ダイブ攻撃（縦=振り下ろし / 横=空中チャージ攻撃）・空中チャージダッシュ ---
	bool IsRevampAirDiveAttack() const;	// 現在のアクションが横ダイブ（空中チャージ攻撃）か
	bool IsAnyAirDiveAttackActive() const;
	bool ShouldAirDivePlunge() const;	// 縦ダイブ（真下プランジ）か。横ダイブは false
	// ST/LP/ED のモンタージュタグ（通常=AIR_ATK / チャージ=AIRCHARGE_ATK）
	FName GetAirDiveAttackStartTag() const;
	FName GetAirDiveAttackLoopTag() const;
	FName GetAirDiveAttackEndTag() const;
	void OnStartAirNormalDiveAttack();	// TryStartAirNormalDiveAttack から呼ぶ本体
	// 突進（ヒット or 持続時間満了）の終了。速度・重力強制をやめ通常落下へ返す。
	// HorizontalSpeedRate は残す水平速度の割合（1.0＝突進速度をそのまま慣性として残す＝ロック突進の従来挙動）
	void EndAirChargeAttackDive( float HorizontalSpeedRate, const TCHAR* Reason );
	// 突進方向へ傾けた姿勢（ピッチ）を直立へ戻す（落下・着地・壁ヒット時）
	void RestoreUprightAfterAirChargeAttackDive();

	// 突進ログ（ImGui 表示専用。挙動には影響しない）
	void BeginAirChargeAttackDiveLog();
	void RecordAirChargeAttackDiveEnd( const TCHAR* Reason, float SpeedRate );
	void RecordAirChargeAttackDiveLanding();

	bool CanStartAirChargeDashNow() const;	// MaxAirChargeDashCount までの回数制限
	// ダッシュの寿命はロック時間（秒）で決まりモーション長を見ないため、ST の尺をデータ側で詰めるのに使う
	float GetAirChargeDashStPlayRate() const;
	// ST の実再生時間（秒。再生速度を考慮。取得できなければ 0）。
	// AirChargeDashLockTimesForGear が 0 以下のとき、ダッシュ寿命をこの長さへ自動追従させる
	float GetAirChargeDashStMontageDuration() const;

	// 空中チャージダッシュ終了時（→通常落下）の水平慣性減衰。OnEndAction から空中で終わった瞬間に開始し、
	// 水平速度の大きさだけを EaseOut で減衰させる（時間 0 なら何もしない）。着地・滑空開始で自動終了
	void BeginAirDashEndInertia();
	void UpdateAirDashEndInertia( float DeltaTime );

	// --- 計算・判定ヘルパー ---
	float GetSlopeChargeTimeMultiplier() const;
	// IsEffectivelyInAir の判定結果の描画（bDrawDebug が true のときのみ動作）
	void DrawAirCheckDebug( bool bInAir, float ZVel, const FVector& TraceStart, const FVector& TraceEnd, bool bGroundHit ) const;
	FVector GetHomingDirection( const FVector& InDefaultDir, float InDebugDuration = 0.0f ) const;
	// チャージダッシュ専用の吸着方向（チャージ攻撃とは別の距離／角度パラメータを使う）
	FVector GetChargeDashHomingDirection( const FVector& InDefaultDir, float InDebugDuration = 0.0f, ULockOnTargetComponent** OutTargetComp = nullptr ) const;
	// 吸着方向計算の共通コア（ロックオン優先→扇形サーチ→デバッグ描画）。距離・角度は呼び出し側が解決して渡す。
	// ApexBackOffset は扇の頂点を後方へ下げる量、MaxHeightDiff は吸着を許す高さ差の上限（0 で無制限）、
	// bPreferLockOnTarget=false ならロックオン中でも扇形サーチで吸着する（OutTargetComp に対象を返す）
	FVector GetHomingDirectionWithParams( const FVector& InDefaultDir, float MaxDist, float Angle, float ApexBackOffset = 0.0f, float MaxHeightDiff = 0.0f, float InDebugDuration = 0.0f, FColor DebugAreaColor = FColor::Cyan, ULockOnTargetComponent** OutTargetComp = nullptr, bool bPreferLockOnTarget = true ) const;
	FVector GetActorYawForwardDirection() const;
	FVector ProjectDirectionToGround( const FVector& InDirection ) const;

	// --- アクセサ ---
	const class UTidePlayerParamDataAsset* GetPlayerParams() const;
	class UCharacterMovementComponent* GetCharacterMovement() const;
	class UExCameraSubsystem* GetCameraSubsystem() const;

private:
	// Niagara／残像／スキッド／ブースト／ギアUI等の生成・描画を担う
	UPROPERTY( Transient )
	TObjectPtr<UChargeEffectSubModule> EffectSubModule;

	// ブレーキ固有状態と毎フレームの状態機械を担う
	UPROPERTY( Transient )
	TObjectPtr<UChargeGuardBrakeSubModule> GuardBrakeSubModule;

	// --- 入力・継続状態 ---

	bool bChargeInputHeld = false;		// ボタンが現在ホールド（長押し）されているか
	bool bIsChargeKeptByDodge = false;
	bool bBlockChargeInputUntilRelease = false;	// 手動キャンセル後、離されるまで再チャージを禁止する
	// 溜め直しに R2 の押し直しを要求する状態か（DA の bRequireChargeRepressToRestart が ON のときだけ効く）。
	// BeginCharge で立ち、新規押下の消費／物理離しで下りる。立っている間は途切れても自動では溜め直さない
	bool bRequireChargeRepress = false;
	// 上記の部分適用版（DA: bRequireChargeRepressAfterComboFinishAndDamage）。
	// チャージ攻撃の最終段発動時と被弾時だけ立て、解除条件は上と同じ
	bool bRequireChargeRepressByEvent = false;
	// 打ち上げ封印中のリリースを後回しにする予約。リリース時点のギア／溜め状態は bIsCharging を凍結して
	// 保持し、封印解除時に FlushPendingChargeRelease で発動する
	bool bPendingLaunchChargeRelease = false;
	// bChargeInputHeld はバッファ消費経由で立つため封印中の押下を拾えないが、こちらは押下イベント直結で拾える
	bool bChargeInputPhysicallyHeld = false;

	bool bIsCharging = false;
	bool bIsShortenedChargeActive = false;
	bool bHasReachedMaxCharge = false;

	FAutomaticTimer PostActionChargeTimer;
	FAutomaticTimer CurrentChargeTimer;
	FAutomaticTimer ChargeShiftTimer;		// ギアシフト（段階上昇）管理用タイマー

	// --- アクション状態 ---

	EChargeActionV2Type CurrentChargeActionType = EChargeActionV2Type::None;

	bool bWasDashingBeforeCharge = false;

	// --- アクション中タイマー・物理キャッシュ ---

	FAutomaticTimer ChargedActionLockTimer;
	FAutomaticTimer FrictionRecoveryTimer;	// ダッシュ後の摩擦回復
	FAutomaticTimer GravityLockTimer;
	float GravityLockZUpSpeed = 0.0f;	// 滞空維持中に与える上昇速度（cm/s）。0 で水平維持（Velocity.Z=0）

	bool bIsAirFrictionRecovery = false;

	FVector CachedChargeDashDirection = FVector::ZeroVector;	// 突進中の等速移動を維持するため
	float CachedChargeDashSpeed = 0.0f;

	// --- 空中チャージダッシュ終了時（1回目→通常落下）の水平慣性減衰 ---
	FAutomaticTimer AirDashEndInertiaTimer;				// 動作中のみ Velocity.XY を制御する
	float AirDashEndInertiaInitialSpeed = 0.0f;
	FVector AirDashEndInertiaDir = FVector::ZeroVector;	// 速度が消えたときのフォールバック

	float CachedChargeDashPower = 0.0f;
	// 開始時に ChargedActionLockTimer へ設定した「本来の」持続時間。中断からの再開（着地／ブースト終了）時、
	// 消費済みの残り時間ではなくこの値で bAllowDashLoop を判定するために使う
	float CachedChargeDashLockTime = 0.0f;
	float CurrentDashSpeed = 0.0f;

	bool bAllowDashLoop = false;		// LP（ループ）の再生を許可するか
	bool bReserveDashOnLanding = false;	// 空中アクション終了後の着地ダッシュ予約
	bool bHasUsedChargeJump = false;

	// --- チャージホップ（チャージダッシュ中ジャンプ → 着地後ダッシュ継続 / bEnableChargeHop）---
	// ホップ（ギア壱固定・CHARGE_HOP_ST/LP）か。ST->LP 遷移監視がどちらのモンタージュを見るかの判定に使う
	bool bIsChargeHopJump = false;
	bool bChargeHopEdMontageStarted = false;	// 自然終了の取りこぼし検知用
	// 連続幅跳びの回数（1回目＝1）。ResetChargedActionState でリセットされるため、
	// 再ホップ側は OnEndAction を挟む前に退避して復元する
	int32 ChargeHopChainCount = 0;

	// --- 幅跳びの水平速度維持（MoveSpeed 方式）と速度収支の計測 ---
	bool bChargeHopSpeedMaintainActive = false;		// 発射で ON、ホップ終了で OFF
	float ChargeHopMaintainSpeed = 0.0f;			// 維持目標（cm/s）。現速度がこれを下回っていたら引き上げる
	FVector ChargeHopMaintainDir = FVector::ZeroVector;	// 速度が消えて向きが取れないときのフォールバック
	float ChargeHopLastExitSpeed = 0.0f;			// 前フレーム出口（自前補正後）。CMC ロスの計測用
	bool bChargeHopBlockedBySolid = false;			// 壁に当たって速度が潰されている＝押し付け続けないため維持を止める

	// 幅跳び1回ぶんの速度収支（ImGui）。慣性がどこで落ちているかの切り分け用。
	// CmcLoss＝前フレーム出口 → 今フレーム入口で減った量。SelfGain＝入口 → 出口で戻した量
	struct FChargeHopSpeedLog
	{
		int32 Index = 0;				// 連続幅跳びの何回目か（1始まり）
		float PreLaunchSpeed = 0.0f;	// 発射直前＝引き継ぎ元
		float LaunchSpeed = 0.0f;		// 発射時に与えた水平速度（上限クランプ後）
		float AirCmcLoss = 0.0f, AirSelfGain = 0.0f;		// 滞空中の累計（cm/s）
		float LandingSpeed = 0.0f;
		float GroundCmcLoss = 0.0f, GroundSelfGain = 0.0f;	// 着地〜ED の累計（cm/s）
		float EndSpeed = 0.0f;			// ホップ終了時点（次ホップ発射／ダッシュ継続）
		float AirTime = 0.0f, GroundTime = 0.0f;
	};
	static constexpr int32 ChargeHopSpeedLogMax = 6;	// 直近何回ぶん残すか（先頭が最新）
	TArray<FChargeHopSpeedLog> ChargeHopSpeedLogs;
	int32 ChargeHopSequenceIndex = 0;				// ダッシュへ戻る／終了でリセット

	bool bChargeDashHomingRotationLock = false;		// 到達まで回頭をロックして対象へ向け続ける
	TWeakObjectPtr<ULockOnTargetComponent> ChargeDashHomingTarget;	// 到達判定・回頭追従に使う
	bool bResumeChargeDashOnLanding = false;
	float ResumeChargeDashRemainingTime = 0.0f;		// ジャンプ時に退避したダッシュ持続タイマーの残り秒数

	// ホップ中はダッシュ用カメラを維持し、ジャンプ用へ切り替えない（途中の Pop/Push＝カットを防ぐ）。
	// ダッシュへ復帰／ホップが継続せず終了した時点で false へ戻し、以後は通常どおり Pop させる
	bool bKeepDashCameraDuringHop = false;

	// 1回目の空中チャージダッシュが空中で終わったとき、ダッシュ用カメラを着地まで保持する。終了の瞬間は
	// 水平慣性＋ZUp頂点で動きが最大なので、そこにカメラ切替のブレンドが重なるとガクついて見える
	bool bKeepChargeDashCameraUntilLanding = false;

	// --- 加速ギミック（ブースト）へ触れた場合の中断 → 再開 ---
	bool bResumeChargeDashAfterBoost = false;
	float ChargeDashResumeTimeAfterBoost = 0.0f;	// ブースト開始時に退避したダッシュ持続タイマーの残り秒数
	// 再開直後、進行方向を「中断前のブースト速度の向き」から機体前方向へ寄せるイージング。ハードセットだと
	// 低摩擦の滑りで向きがズレていた場合に再開の瞬間へ直角スナップしてしまう
	FAutomaticTimer BoostResumeTurnEaseTimer;
	FVector BoostResumeInitialVelDir = FVector::ZeroVector;	// 再開時のブースト速度の向き（水平）
	float BoostResumeInitialSpeed = 0.0f;					// 同・大きさ。ここからダッシュ速度へ滑らかに戻す
	// 溜め中に触れた場合。再開時にギア／コンボ段を復元する
	bool bResumeChargeHoldAfterBoost = false;
	int32 SavedChargeGearForBoostResume = 1;
	int32 SavedChargeComboForBoostResume = 1;

	// --- 神技の構え中はチャージダッシュの推進を凍結（キャンセルはしない）---
	// 速度のハードセットが構えブレーキを打ち消すため。寿命タイマーも止めて残り時間を保持し、解除すれば続きへ戻る
	bool bChargeDashFrozenByGodArt = false;
	FAutomaticTimer GodArtDashResumeEaseTimer;	// 凍結解除後、落ちた速度からダッシュ速度へ戻す
	float GodArtDashResumeInitialSpeed = 0.0f;
	// 構え中に R2 を離した溜めの凍結保持中か（発動もキャンセルもせず維持し、構えの終わりで畳む）
	bool bChargeFrozenByGodArtStance = false;

	// --- はじかれ（リバウンド） ---

	bool bIsReboundSliding = false;		// はじかれ中の強制滑り
	bool bIsReboundDash = false;
	FVector CurrentReboundVelocity = FVector::ZeroVector;
	FAutomaticTimer ReboundRecoveryTimer;
	FAutomaticTimer HitCancelChargingSteeringTimer;	// ヒットキャンセルチャージ直後のステアリング制限

	// --- ドリフトブースト（回転ブースト）状態：単一ソース ---
	float DriftSustainTimer = 0.0f;			// n°以上のドリフトの継続秒数（途切れたら即0へ）
	bool bIsRawDrifting = false;			// 継続 t 秒のゲート前。速度維持側が参照する
	bool bIsDriftBoostActive = false;		// t 秒継続を満たし回転ブーストが有効か
	float CurrentDriftBoostAlpha = 0.0f;	// ドリフト強度（0..1、角度由来）
	EDriftTurnDirection CurrentDriftTurnDirection = EDriftTurnDirection::None;

	// --- ギア直上昇モード（bUseDriftGearUpBonus）の状態 ---
	float DriftGearUpSustainTimer = 0.0f;	// 同方向ドリフトの継続秒数（方向が変わる／切れると0へ）
	float DriftGearUpCooldownTimer = 0.0f;	// 同方向での再上昇を塞ぐ残り秒数（実時間で消化。切り返しでリセット）
	EDriftTurnDirection DriftGearUpSustainDirection = EDriftTurnDirection::None;
	EDriftTurnDirection LastDriftGearUpDirection = EDriftTurnDirection::None;	// 切り返し判定用
	TMap<TWeakObjectPtr<AActor>, double> DriftAttackLastHitTimes;	// 再アーム間隔の判定用

	// --- ヒットキャンセル・ヒットバック ---
	FAutomaticTimer HitCancelChargeActionLockTimer;	// 直後のチャージアクション禁止
	FAutomaticTimer HitbackComboIntervalTimer;	// R2 を押さない連続チャージ攻撃の発動を遅らせる
	bool bPendingChargeComboAttack = false;	// インターバル中に飲み込んだ攻撃入力の予約
	bool bIsHitCancelableToCharge = false;	// ヒットバック発生時、チャージによるモーションキャンセルを許可する
	bool bIsChargeFromHitCancel = false;	// 現在の溜めがヒットバックキャンセル経由か
	// 傾斜地で摩擦に消えないよう保持する引き継ぎ速度
	FVector CachedHitCancelVelocity = FVector::ZeroVector;
	// 後退区間での溜め継続によるギアアップ
	FAutomaticTimer HitbackChargeHoldTimer;		// 0 到達でギアアップ
	bool bWasHitbackChargeHold = false;			// 立ち上がりでタイマーを張るための前フレーム値
	bool bHitbackChargeGearUpDone = false;		// 1回の溜めにつき1回に制限する

	// --- コンボ／ギア ---

	int32 CurrentChargeComboIndex = 1;
	int32 CurrentChargeGearIndex = 1;
	bool bIsGearShiftedByComboFinish = false;		// コンボ出し切りでギアが上がった直後か
	// 加算経路が複数あり速い連打では二重加算で段が飛ぶ（1→3）ため、IncrementChargeComboIndex が
	// このフラグでガードし、攻撃発動（OnStartChargeAttack）で解除する
	bool bChargeComboAdvancedThisAttack = false;

	// --- スライドパッシブ「突風」バフ ---
	bool bGustChargeBuffArmed = false;		// 次の1チャージアクションに乗っているか（単発消費）
	bool bGustBuffedActionActive = false;	// 消費したアクションが進行中か（消費〜終了まで）

	// バフ付きアクション中、プレイヤーに追従させる範囲攻撃バースト（効果中ずっと持続。終了時に破棄）
	UPROPERTY( Transient )
	TObjectPtr<class ASlidePassiveGustBurst> ActiveGustBurst;
	UPROPERTY( Transient )
	TObjectPtr<class UNiagaraComponent> ActiveGustBurstVFX;	// クラス未設定時のフォールバック VFX

	bool bGustBurstFadingOut = false;		// 消滅時の Alpha フェードアウト進行中か
	float GustBurstFadeElapsed = 0.0f;

	// --- チャージジャンプ ---

	bool bIsChargeJumpInLoop = false;

	// 面沿い（天井・壁）から降りた落下を、通常の落下ループではなくチャージジャンプのモーションで見せている間 true
	// （地上チャージダッシュを継続したままの落下が対象。TryStartChargeDashFallJumpMotion 参照）
	bool bDashFallChargeJumpMotion = false;
	bool bDashFallChargeJumpStStarted = false;	// ST の再生開始を観測したか（更新遅れで即 LP へ飛ぶのを防ぐ）
	bool bDashFallChargeJumpInLoop = false;
	bool bDashFallChargeJumpLanding = false;	// 着地して CHARGE_JUMP_ED を一瞬だけ見せている間
	FAutomaticTimer DashFallLandingEdTimer;		// 上記の残り時間（SurfaceRideFallLandingEdTime）

	// --- 空中チャージ攻撃 ---
	bool bIsAirChargeAttack = false;
	bool bIsAirChargeAttackInLoop = false;
	// 滞空→突進（ST→LP）の切替タイマー。AirChargeAttackHoverTime>0 でセットし、経過で突進へ移す。
	// 未設定（パラメータ 0 or 縦ダイブ）なら従来どおり ST モーション終了で切替える
	FAutomaticTimer AirChargeAttackHoverTimer;
	// 着地ED（AIRCHARGE_ATK_ED）が一度でも再生開始されたか（自然終了の取りこぼし検知用）。
	// ED 中は CanMove まで移動不可、CanMove 中の移動入力で早期キャンセル、入力無しなら自然終了する
	bool bAirChargeAttackEdMontageStarted = false;
	// 突進（LP）中に壁へ当たったか。突進は Velocity/GravityScale を毎フレーム強制するため、壁に当たると
	// 落ちも着地もできずハングする。立ったら速度強制をやめて通常落下へ移し、着地で ED を出させる
	bool bAirChargeAttackDiveWallStuck = false;
	// 突進を「ヒット or 持続時間」で終えたか。壁ヒットと同じく速度強制をやめるが、
	// こちらは水平慣性を残したまま自然に落として着地（ED）まで繋ぐ
	bool bAirChargeAttackDiveEnded = false;
	// ロックオン突進か。true の間は角度に依らずロック部位へ直接突進し、毎フレーム対象を追う
	bool bIsAirChargeAttackLockOnRush = false;
	// 追従先のロック部位。毎フレーム GetTargetLocation() を参照して突進方向を作る
	TWeakObjectPtr<class ULockOnTargetComponent> AirChargeAttackRushTarget;
	// 突進の持続。満了で当たらなくても終了（＝通常落下へ）。秒数はロック突進＝AirChargeAttackLockOnRushMaxTime／
	// 角度ダイブ＝AirChargeAttackDiveMaxTime。どちらも 0 なら未設定＝無制限
	FAutomaticTimer AirChargeAttackDiveTimer;

	// 突進1回ぶんのログ（ImGui）。突進中は実機で数値を読めないため、
	// 着地後に「持続時間で終わったのか／終わった後も速度が残っていたのか」を見る
	struct FAirChargeAttackDiveLog
	{
		bool bLockOnRush = false;		// false＝角度ダイブ
		float MaxTime = 0.0f;			// 設定した持続時間（0＝無制限）
		float LoopTime = 0.0f;			// LP（突進）が続いた秒数
		float EndSpeed2D = -1.0f;		// -1＝まだ終わっていない
		float EndSpeedRate = 1.0f;		// 終了時に掛けた水平速度の割合
		float LandSpeed2D = -1.0f;		// 着地時点（終了後に減ったか）
		const TCHAR* EndReason = TEXT( "---" );
	};
	static constexpr int32 AirChargeAttackDiveLogMax = 4;	// 直近何回ぶん残すか（先頭が最新）
	TArray<FAirChargeAttackDiveLog> AirChargeAttackDiveLogs;

	// --- 計測ログ：チャージ攻撃を「空中／地上」どちらで発動したか ---
	// 判定は SetupAirChargeAttackState の IsFalling() 一本だが、実機では発動の瞬間の値を読めないため
	// 判定に絡む変数をまとめて残す。「着地ED 中なのに空中チャージ攻撃が出る」の切り分け用
	struct FChargeAttackTriggerLog
	{
		bool bResultAir = false;			// 判定結果（true＝空中チャージ攻撃）
		bool bFalling = false;				// 判定の入力そのもの
		bool bEffectivelyInAir = false;		// 真下130cm・Z速度も見る方
		bool bLaunchAirborne = false;		// 自前射出の浮き
		bool bUsedThisAirtime = false;		// 1滞空1回の制限
		bool bDuringAirDiveEd = false;		// 発動時に着地ED が流れていたか
		bool bChargeHeld = false;			// R2 の長押しラッチ
		const TCHAR* Source = TEXT( "---" );	// 発動経路（コンボ／新規／ダッシュ派生 等）
		float VelocityZ = 0.0f;
		float Speed2D = 0.0f;
		float GroundDistance = -1.0f;		// -1＝130cm 以内に地面なし
		float TimeSinceLanded = -1.0f;		// -1＝未着地
		FName PrevMontage;					// 発動直前に流れていたモンタージュ
		FName MovementMode;					// Walking / Falling 等
		FString Tags;						// 発動時に付いていた受付タグ
	};
	static constexpr int32 ChargeAttackTriggerLogMax = 4;	// 1件3行で表示するため控えめに
	TArray<FChargeAttackTriggerLog> ChargeAttackTriggerLogs;

	// --- 計測ログ：着地ED 中に再び浮いたか（浮けば以降の発動判定が空中扱いになる）---
	struct FAirChargeAttackEdAirborneLog
	{
		float EdPosition = 0.0f;		// ED モンタージュの再生位置（何秒目で浮いたか）
		float TimeSinceLanded = 0.0f;
		float Speed2D = 0.0f;			// 突進の残り速度が原因かの判断材料
		float VelocityZ = 0.0f;
	};
	static constexpr int32 AirChargeAttackEdAirborneLogMax = 4;
	TArray<FAirChargeAttackEdAirborneLog> AirChargeAttackEdAirborneLogs;

	// --- 計測ログ：チャージ攻撃のモンタージュが選ばれた瞬間（GetChargeAttackAnimTag の呼び出し）---
	// ここに残るのに発動判定ログへ対応する行が無ければ、V2 の発動経路を通らずにモーションが張られている
	struct FChargeAttackMontagePickLog
	{
		FName Tag;
		bool bAirFlag = false;			// そのときの bIsAirChargeAttack
		bool bNormalDive = false;		// そのときの bIsAirNormalAttack（振り下ろし）
		bool bFalling = false;
		float TimeSinceLanded = -1.0f;
	};
	static constexpr int32 ChargeAttackMontagePickLogMax = 5;
	mutable TArray<FChargeAttackMontagePickLog> ChargeAttackMontagePickLogs;	// const な取得関数から積むため

	// --- 計測ログ：接地→落下（MOVE_Falling）へ移った瞬間。「何のモーションで浮いたか」を押さえる ---
	// 溜め開始や攻撃のルートモーションで一瞬浮くと、その隙の入力が空中チャージ攻撃へ化けるため
	struct FAirborneTransitionLog
	{
		FName Montage;					// 浮いた瞬間に流れていたモンタージュ
		float VelocityZ = 0.0f;
		float Speed2D = 0.0f;
		float GroundDistance = -1.0f;	// -1＝130cm 以内に地面なし
		float TimeSinceLanded = -1.0f;
		bool bCharging = false;
		const TCHAR* ActionType = TEXT( "None" );
	};
	static constexpr int32 AirborneTransitionLogMax = 5;
	TArray<FAirborneTransitionLog> AirborneTransitionLogs;
	bool bAirborneTransitionLastFalling = false;	// 立ち上がり検出用の前フレーム値

	bool bAirChargeAttackEdAirborneLogged = false;	// ED 1回につき1度だけ記録するためのラッチ
	float LastLandedTimeSeconds = -1.0f;			// 経過秒の算出用
	// 次に記録する発動判定ログの経路名。OnStartChargeAttack を呼ぶ直前に各 Try* が入れる
	const TCHAR* ChargeAttackTriggerSource = TEXT( "---" );
	// 縦ダイブ機械（bIsAirChargeAttack）を流用しつつ、
	// これで「通常＝AIR_ATK タグ・常に真下プランジ」を識別する
	bool bIsAirNormalAttack = false;
	// 地上からのチャージ射出（LaunchCharacter）で浮いているか。空中／地上の分類は真下トレースへ移したため
	// 現在は計測ログ表示のみ（射出しない経路＝詰め・ルートモーションの浮きを拾えず判定の根拠にならなかった）
	bool bAirborneByGroundChargeLaunch = false;
	// 出したあとは空中チャージダッシュ使い切りと同じ空中アクション制限を掛ける。着地でクリア
	bool bAirChargeAttackUsedThisAirtime = false;
	// 空中チャージ攻撃の終了から振り下ろしを禁止する残り時間（AirChargeAttackToDiveAttackCooldown）
	FAutomaticTimer AirChargeAttackDiveAttackCooldownTimer;

	// --- 空中チャージダッシュ（回数 MaxAirChargeDashCount まで＝既定1回。ST 終了でそのまま通常落下へ）---
	bool bIsAirChargeDash = false;
	int32 AirChargeDashStageCount = 0;	// 0=なし / 1=1回目。着地でリセットする

	// --- チャージ攻撃の対象詰め ---
	bool bChargeAttackTimedApproachActive = false;
	TWeakObjectPtr<AActor> ChargeApproachTargetActor;	// 被弾面トレースの絞り込み・生存確認用
	// 追従先のロック部位（毎フレーム GetTargetLocation を参照）
	TWeakObjectPtr<class ULockOnTargetComponent> ChargeApproachTargetComp;
	FVector ChargeApproachStartLocation = FVector::ZeroVector;	// 補間の始点
	float ChargeApproachStandoff = 0.0f;	// 対象位置から手前に取る到達距離（被弾面オフセット込み）
	float ChargeApproachDuration = 0.0f;	// 判定発生までの実時間（ロック時間の確保に使う）
	FAutomaticTimer ChargeApproachTimer;	// GetRate が 0..1 の補間 alpha
	// 詰めは SetActorLocation 駆動で Velocity に出ないため、とどめの切り抜けが慣性として読めるよう
	// 実移動量から毎フレーム計測する（詰め終了では消さず、次のチャージ攻撃の開始で消す）
	float ChargeApproachMeasuredSpeed2D = 0.0f;

	// --- カメラハンドル ---
	// 空中攻撃（振り下ろし／空中チャージ攻撃共通）の開始・着地
	FCameraModeHandle AirAttackStartCameraHandle;
	FCameraModeHandle AirAttackEndCameraHandle;
	// ダッシュ／地上チャージ攻撃で共用（同時に発生しないため1本で張り替える）
	FCameraModeHandle ChargeActionCameraHandle;
	FName CurrentChargeActionCameraKey = NAME_None;	// 同一キーなら張り替えない判定用
	FCameraModeHandle GearShiftCameraHandle;
	// 空中チャージダッシュの寄せ演出。発動時に Push、寄せ切ったら Pop
	FCameraModeHandle AirChargeDashSwingCameraHandle;
	FAutomaticTimer AirChargeDashSwingTimer;	// 残り時間（モードの SwingDuration を Set）

	// 終了時に即 Pop せず一定時間保持する。用途は「地上チャージ攻撃の出し切り（戻りが早すぎるのを後ろ倒し）」と
	// 「幅跳びの着地（続けて跳ぶ可能性があるので待つ）」の 2 つ。保持中に次のアクションが同じキーを要求すれば
	// SetChargeActionCamera が予約だけ解除してカメラを維持する（＝Pop→Push のブレンドが起きない）
	bool bPendingChargeActionCameraPop = false;
	FAutomaticTimer ChargeActionCameraHoldTimer;

};
