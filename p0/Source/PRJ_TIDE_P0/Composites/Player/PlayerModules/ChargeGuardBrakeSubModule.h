// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

#include "ChargeGuardBrakeSubModule.generated.h"

class ATidePlayerCharacter;
class UChargeActionPlayerModule_V2;
class UAnimMontage;

/**
 * チャージのガードブレーキ（急制動ガード）サブモジュール。
 *
 * ChargeActionPlayerModule_V2 からガードブレーキ固有の状態（入力ホールド／防御消費／ブレーキ速度）と
 * 毎フレームの状態機械（減速移動・旋回・モンタージュ遷移・1回防御）を切り出したもの。
 * 開始／終了のオーケストレーション（ExecuteManualCancel / ReleaseManualCancel）と、複数アクションで
 * 共有する摩擦回復タイマー・アクション状態トークンは OwnerModule 側が保持し、
 * 本クラスはその public API 経由で参照する。
 */
UCLASS()
class UChargeGuardBrakeSubModule : public UObject
{
	GENERATED_BODY()

public:
	void Initialize( UChargeActionPlayerModule_V2* InOwnerModule, ATidePlayerCharacter* InOwnerCharacter );

	// 毎フレーム更新（ガードブレーキ中のみ実処理する）
	void Update( float DeltaTime );

	// 入力ホールド状態の設定（キャンセルボタンの押下／離し）
	void SetInputHeld( bool bHeld ) { bInputHeld = bHeld; }

	// ガードブレーキ開始時に呼ぶ：防御フラグのリセット／キャンセル時の初速保持
	void ResetBlockConsumed() { bBlockConsumed = false; }
	void SetBrakeVelocity( const FVector& InVelocity ) { CurrentBrakeVelocity = InVelocity; }

	// ガードブレーキ中の被弾を1回だけ防御する。1回目の被弾なら true（防御成立）を返し、以降は false。
	bool TryConsumeBlock();

private:
	void UpdateMovement( float DeltaTime );
	void UpdateRotation( float DeltaTime );
	void UpdateMontageState();
	bool HasStartMontageFinished( UAnimMontage* CurrentMontage, UAnimMontage* BrakeSt, UAnimMontage* BrakeLp ) const;

private:
	UPROPERTY( Transient )
	TObjectPtr<UChargeActionPlayerModule_V2> OwnerModule;

	UPROPERTY( Transient )
	TObjectPtr<ATidePlayerCharacter> OwnerCharacter;

	bool bInputHeld = false;			// キャンセルボタンが現在ホールドされているか
	bool bBlockConsumed = false;		// 現在のガードブレーキで防御（1回耐え）を消費済みか
	FVector CurrentBrakeVelocity = FVector::ZeroVector;	// ブレーキ中の現在の慣性速度
};
