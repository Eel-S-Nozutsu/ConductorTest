// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "EnemyAnimInstance.generated.h"

/**
 * 検証敵用アニメーションインスタンス
 */
UCLASS()
class PRJ_TIDE_P0_API UEnemyAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:

	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	// 地上速度
	UPROPERTY(BlueprintReadOnly, Category = "Tide|Movement")
	float GroundSpeed = 0.0f;

	// ストレイフ移動用X軸速度
	UPROPERTY(BlueprintReadOnly, Category = "Tide|Movement")
	float ForwardSpeed = 0.0f;

	// ストレイフ移動用Y軸速度
	UPROPERTY(BlueprintReadOnly, Category = "Tide|Movement")
	float RightSpeed = 0.0f;

	// 落下中か
	UPROPERTY(BlueprintReadOnly, Category = "Tide|Movement")
	bool bIsFalling = false;

	// 戦闘対象がいるか (BB.TargetActor)
	UPROPERTY(BlueprintReadOnly, Category = "Tide|Movement")
	bool bHasTarget = false;

	// ストレイフ移動中か。BB.IsStrafingをそのまま映す
	// 距離・攻撃者かどうかの判定はフロー制御側が持つ。ここで再判定すると条件がズレて足滑りする
	UPROPERTY(BlueprintReadOnly, Category = "Tide|Movement")
	bool bIsStrafing = false;

	// LookAtターゲットのワールド位置(AnimBPのLook Atノードに渡す)
	UPROPERTY(BlueprintReadOnly, Category = "Tide|LookAt")
	FVector LookAtWorldPosition = FVector::ZeroVector;

	// LookAtターゲットの高さオフセット
	UPROPERTY(BlueprintReadOnly, Category = "Tide|LookAt")
	FVector LookAtHeightOffset = FVector(0.0f, 0.0f, 40.0f);

	// LookAtブレンド率(0=無効, 1=完全追従)。フォーカス有無でスムーズに遷移する
	UPROPERTY(BlueprintReadOnly, Category = "Tide|LookAt")
	float LookAtAlpha = 0.0f;

	// 視線IK(LookAt)を抑制する。trueの間LookAtAlphaは0へフェードする
	// 火炎放射など、頭部の追従が本体の照準と競合する攻撃中に立てる(攻撃全体で有効)
	UPROPERTY(BlueprintReadOnly, Category = "Tide|LookAt")
	bool bSuppressLookAtIK = false;

	// 火炎放射:ループ噴射中フラグ。AnimBPはこれがtrueの間ループBlendSpaceを再生する
	// 開始/終了モンタージュはスロットで上書き再生され、
	// 噴射ループだけがAnimGraphのブレンドで表現される
	// UFlamethrowerAttackExecutionがbegin/endで切り替える
	UPROPERTY(BlueprintReadOnly, Category = "Tide|Flamethrower")
	bool bFlamethrowerActive = false;

	// 火炎放射: 照準ブレンド値。-1=左 / 0=正面 / +1=右
	// ループBlendSpaceのX軸へ渡す
	// EM→PCのヨー差から算出され、PCの左右位置に応じて左右前モーションがブレンドされる
	UPROPERTY(BlueprintReadOnly, Category = "Tide|Flamethrower")
	float FlamethrowerAimBlend = 0.0f;

	// 部位ごとの揺れ角度(度)。AnimBPのModify Boneで参照する
	// インデックスはBossDataAsset::Partsの順番に対応
	UPROPERTY(BlueprintReadOnly, Category = "Tide|HitShake")
	TArray<float> PartShakeAngles;

	// 部位揺れ角度を取得
	// ※範囲外は0を返すAnimBPはPartShakeAnglesの直接インデックスではなくこれを使う
	// Parts未設定(部位揺れを使わない敵 / 24xボス等)でも配列長0で安全に0が返る
	// AnimGraph(ワーカースレッド)から呼ぶためBlueprintThreadSafeを付与
	// (配列を読むだけで安全)
	UFUNCTION(BlueprintPure, Category = "Tide|HitShake", meta = (BlueprintThreadSafe))
	float GetPartShakeAngle(int32 Index) const
	{
		return PartShakeAngles.IsValidIndex(Index) ? PartShakeAngles[Index] : 0.0f;
	}

	// BeginPlayで部位数分の配列を事前確保する
	// BossCharacter::BeginPlayから呼ぶ
	void InitHitShake(int32 PartCount);

	// 部位ヒット時にPartDestructionComponentから呼ぶ
	// PartIndex = BossDataAsset::Partsの配列インデックス
	void TriggerHitShake(int32 PartIndex, float Impulse, float Duration);

private:

	struct FHitShakeState
	{
		float Angle = 0.0f;
		float Velocity = 0.0f;
		float Remaining = 0.0f;

	};
	TArray<FHitShakeState> ShakeStates;

};
