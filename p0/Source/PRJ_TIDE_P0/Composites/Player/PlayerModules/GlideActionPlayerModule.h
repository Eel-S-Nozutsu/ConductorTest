// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "PRJ_TIDE_P0/Composites/Player/PlayerModules/TidePlayerModule.h"
#include "PRJ_TIDE_P0/Utilities/AutomaticTimer.h"
#include "GlideActionPlayerModule.generated.h"

// 滑空モジュール。空中でジャンプボタンを長押しすると滑空へ移る（ジャンプが残っていればそれが先に出て、
// 上昇が終わってから始まる）。ボタンを離すと落下へ戻り、押し直せば残り時間ぶん再滑空できる。
// 持続時間は「空中1回ぶんの予算」で滑空⇄落下を往復しても引き継ぎ、着地でリセットする。
//
// 空中チャージダッシュを使い切った空中では R2 長押しでも滑空でき、bAutoGlideAfterAirChargeDash が ON なら
// 空中チャージダッシュが空中のまま終わった時点で押下なしで自動展開する。
//
// 滑空中は移動・回頭・重力をこのモジュールが握るため JumpActionPlayerModule のモーション制御は抑止され、
// 着地の JUMP_ED は OnLanded から明示的に引き渡す
UCLASS()
class PRJ_TIDE_P0_API UGlideActionPlayerModule : public UTidePlayerModule
{
	GENERATED_BODY()

public:
	virtual void OnModuleUpdate( float DeltaTime ) override;

	void OnLanded();	// キャラクターの Landed() から呼ぶ

	// 押下／離しイベントから直接更新する（入力バッファを介さない）
	void SetJumpInputHeldRaw( bool bHeld );
	void SetChargeInputHeldRaw( bool bHeld );	// R2。使い切った空中でのみ滑空の入力になる

	// 空中チャージダッシュが空中のまま終わった瞬間の引き継ぎ。ジャンプ／R2 を長押し中ならその押下ぶんとして
	// （＝離すと落下）、押していなければ bAutoGlideAfterAirChargeDash が ON のときだけ自動展開する。
	// false を返したら、呼び出し側は従来どおり落下ループへ引き渡す
	bool RequestGlideAfterAirChargeDash();

	// 竜巻の強制ジャンプなど、外部の打ち上げへ主導権を渡す。滑空中は落下速度を毎フレーム
	// 上書きするため、畳まないと打ち上げが消える
	void CancelForWindJump();

	// ジャンプパッド等の打ち上げ。畳んだうえで持続時間の予算も張り直す（＝新しい滞空の始まり）
	void ResetForLaunch();

	bool IsGliding() const { return bIsGliding; }
	float GetRemainingTime() const;	// GlideMaxDuration <= 0（無制限）／非滑空時は 0
	bool IsDurationExhausted() const { return bDurationExhausted; }

	void DrawDebugImGui();

private:
	// bAutoDeploy=true ならボタン押下と上昇の終わり（Velocity.Z<=0）を待たない
	bool CanStartGlide( bool bAutoDeploy ) const;
	bool CanKeepGliding() const;
	bool IsGlideInputHeld() const;			// 継続判定に使う
	bool IsGlideInputHeldEnough() const;	// 押下時間が GlideJumpHoldTime に達したか＝開始判定

	// R2 を滑空に使えるのは使い切った空中だけ（それ以外の R2 は溜めに使われる）
	bool IsChargeHoldGlideAllowed() const;

	// 他アクション（攻撃・回避・チャージ・ブースト・神技等）が進行中か。開始・継続を弾く
	bool IsBlockedByOtherAction() const;

	void StartGlide( bool bAutoDeploy );

	// bEndedInAir=true（ボタン離し／持続時間切れ）のときだけ、落下ループへの引き渡しと
	// 解除直後の落下速度ランプを行う
	void StopGlide( bool bEndedInAir );

	// 止めずに次のモーションを再生しても、スロットが違えばループし続けて滑空ポーズが残る
	void StopGlideMontages();

	void UpdateGlideMontage();
	void UpdateGlideMovement( float DeltaTime );
	void UpdateFallRamp( float DeltaTime );

	bool HasGlideStartMontageFinished() const;

	const class UTidePlayerParamDataAsset* GetPlayerParams() const;
	class UCharacterMovementComponent* GetCharacterMovement() const;

private:
	bool bIsGliding = false;

	bool bJumpInputHeld = false;
	bool bChargeInputHeld = false;

	// 自動展開で開いた滑空か。押していなくても継続し、ボタンを押した時点で通常操作へ戻す
	bool bAutoGlideActive = false;

	float JumpHeldTime = 0.0f;		// 離すと 0 へ戻す
	float ChargeHeldTime = 0.0f;

	bool bBlockUntilJumpRelease = false;	// 着地時に押しっぱなしだった場合の暴発を防ぐ
	bool bDurationExhausted = false;		// 着地までは押し直しても再滑空できない

	// 開始直後は AnimInstance の更新遅れで GetCurrentMontage() が「AirGlideStart 再生前」の
	// 古いモーションを返すため、その間は ST 終了判定を行わない
	float GlideElapsedTime = 0.0f;

	FAutomaticTimer DurationTimer;	// 空中1回ぶんの予算。滑空⇄落下の往復では引き継ぐ
	FAutomaticTimer FallRampTimer;	// 解除直後の落下速度の上限を開放していくランプ
};
