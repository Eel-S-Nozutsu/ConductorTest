// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "PRJ_TIDE_P0/Data/Enemy/AttackExecution/TideAttackExecution.h"
#include "TideAttackExecution_EM0020.generated.h"

class AEnemyProjectile;
class UProjectileProfile;
class UNiagaraSystem;
class UNiagaraComponent;

/**
 * ガード開始
 * 
 * モンタージュ先頭のAnimNotifyがGuardOnタグを発火することを前提
 * GuardOn受信時にState.Enemy.Guardタグを付与しDataAssetのHaloWaistSocketNameソケットへ光輪を固定
 * HaloReadyEventを設定した場合そのタグが発火された時点でbHaloDeployedを立てて接触反応を有効化
 * ガード解除はHandleGuardHit側(チャージヒット時)で行う
 */
UCLASS(meta = (DisplayName = "ガード開始"))
class PRJ_TIDE_P0_API UGuardActivateAttackExecution : public UTideAttackExecution
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere)
	TObjectPtr<UAnimMontage> Montage;

	UPROPERTY(EditAnywhere)
	FName HaloComponentName = FName("Halo");

	// 展開アニメーション完了を通知するイベントタグ。発火時に接触反応が有効になる
	// SA (HaloOpen含む) の区間内に置くこと。SAが無い区間では被弾でモンタージュが中断され、
	// ガードだけ成立して接触反応が死んだまま残る
	UPROPERTY(EditAnywhere)
	FGameplayTag HaloReadyEvent = TAG_AttackEvent_HaloReady;

	// --- ガード開始からChargeDelay秒後の暴発 (チャージ) ---
	// 0以下なら暴発しない (純粋なガードのみ)

	// ガード開始から暴発までの秒数
	UPROPERTY(EditAnywhere, Category = "Charge")
	float ChargeDelay = 6.0f;

	// 暴発VFX (BurstVFXScale倍のスケールで再生)
	UPROPERTY(EditAnywhere, Category = "Charge")
	TObjectPtr<UNiagaraSystem> BurstVFX;

	UPROPERTY(EditAnywhere, Category = "Charge")
	float BurstVFXScale = 2.0f;

	// 広範囲ダメージ
	UPROPERTY(EditAnywhere, Category = "Charge")
	float BlastRadius = 600.0f;

	UPROPERTY(EditAnywhere, Category = "Charge")
	float BlastDamage = 20.0f;

	UPROPERTY(EditAnywhere, Category = "Charge")
	FGameplayTag BlastReactionTag;

	// 水平方向へ放射状に発射する弾プロファイル。角度は360/ProjectileCountで等分する
	UPROPERTY(EditAnywhere, Category = "Charge")
	TObjectPtr<UProjectileProfile> ProjectileProfile;

	UPROPERTY(EditAnywhere, Category = "Charge", meta = (ClampMin = "0"))
	int32 ProjectileCount = 8;

	// 弾ダメージ (負値で弾BPの既定値を尊重)
	UPROPERTY(EditAnywhere, Category = "Charge")
	float ProjectileDamage = -1.0f;

	// 発射/VFX/AOEの原点ソケット (未指定ならactor位置 + OriginHeight)
	UPROPERTY(EditAnywhere, Category = "Charge")
	FName MuzzleSocketName = NAME_None;

	UPROPERTY(EditAnywhere, Category = "Charge")
	float OriginHeight = 40.0f;

	// 発光フリッカー: 時間経過で点滅周波数をStart -> Endへ上げ、振幅も終盤ほど強くする
	UPROPERTY(EditAnywhere, Category = "Charge")
	float GlowMaxIntensity = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Charge")
	float FlickerStartHz = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Charge")
	float FlickerEndHz = 8.0f;

	virtual UAnimMontage* GetMontage() const override { return Montage; }
	virtual void OnAttackEvent(AEnemyCharacter* Enemy, FGameplayTag EventTag) override;
	virtual void OnAttackEnd(AEnemyCharacter* Enemy) override;

private:

	void StartCharge(AEnemyCharacter* Enemy);
	void TickFlicker();
	void Detonate();
	void CancelCharge();
	FVector GetChargeOrigin(const AEnemyCharacter* Enemy) const;

	TWeakObjectPtr<AEnemyCharacter> CachedEnemy;
	FTimerHandle FlickerTimerHandle;
	FTimerHandle DetonateTimerHandle;
	float ChargeElapsed = 0.0f;
	float FlickerPhase = 0.0f;

};

/**
 * 光輪投擲攻撃
 * 
 * HaloThrowStart: 光輪を非表示にして同位置にAHaloProjectileをスポーンし発射
 * HaloReturn: 弾を強制削除して光輪を再表示・背中ソケットへ戻す
 */
UCLASS(meta = (DisplayName = "光輪投擲攻撃"))
class PRJ_TIDE_P0_API UHaloThrowAttackExecution : public UTideAttackExecution
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere)
	TObjectPtr<UAnimMontage> Montage;

	// 投擲する光輪の弾プロファイル
	UPROPERTY(EditAnywhere)
	TObjectPtr<UProjectileProfile> HaloProfile;

	// 光輪メッシュコンポーネントの名前
	UPROPERTY(EditAnywhere)
	FName HaloComponentName = FName("Halo");

	virtual UAnimMontage* GetMontage() const override { return Montage; }
	virtual void OnAttackEvent(AEnemyCharacter* Enemy, FGameplayTag EventTag) override;
	virtual void OnAttackEnd(AEnemyCharacter* Enemy) override;

private:

	TWeakObjectPtr<AEnemyProjectile> SpawnedProjectile;
	TWeakObjectPtr<USceneComponent> HaloComp;

};

/**
 * 光輪ステップ
 * ランダムに左右を決定し、ナビメッシュ到達チェックを行ってから横移動する。
 * 到達不可なら即時完了 (FinishDelegate(false)) し、モンタージュを再生しない。
 * GetMontage() はnullptrを返すasyncモードで動作し、モンタージュ終了時にFinishDelegateを呼ぶ。
 */
UCLASS(meta = (DisplayName = "光輪ステップ"))
class PRJ_TIDE_P0_API UHaloStepAttackExecution : public UTideAttackExecution
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere)
	TObjectPtr<UAnimMontage> LeftMontage = nullptr;

	UPROPERTY(EditAnywhere)
	TObjectPtr<UAnimMontage> RightMontage = nullptr;

	// ナビメッシュ到達チェック用の横移動距離
	UPROPERTY(EditAnywhere)
	float StepNavCheckDistance = 300.0f;

	// ステップ中の移動速度
	UPROPERTY(EditAnywhere)
	float StepSpeed = 500.0f;

	virtual UAnimMontage* GetMontage() const override { return nullptr; }
	virtual void OnAttackBegin(AEnemyCharacter* Enemy) override;
	virtual void OnAttackTick(AEnemyCharacter* Enemy, float DeltaTime) override;
	virtual void OnAttackEvent(AEnemyCharacter* Enemy, FGameplayTag EventTag) override;
	virtual void OnAttackEnd(AEnemyCharacter* Enemy) override;

private:

	UFUNCTION()
	void OnStepMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	void RestoreRotation(AEnemyCharacter* Enemy);

	// UPROPERTYを付けないメンバ: 攻撃ごとに生成されるので常に初期値から始まる
	bool bStepping = false;
	bool bStepRight = false;
	bool bRotationOverridden = false;
	bool bSavedUseControllerDesiredRotation = false;
	float PreStepMaxWalkSpeed = 0.0f;
	FVector CachedStepDirection = FVector::ZeroVector;
	TWeakObjectPtr<UAnimInstance> CachedAnimInstance;
	TWeakObjectPtr<UAnimMontage>  ActiveMontage;

};

/**
 * 光輪回転攻撃
 * AnimNotifyがTAG_AttackEvent_HaloAttackStartタグを発火することを前提とする。
 * TAG_AttackEvent_HaloAttackStartで回転ループが始まるので、ループを制御する
 */
UCLASS(meta = (DisplayName = "光輪回転攻撃"))
class PRJ_TIDE_P0_API UHaloRotationAttackExecution : public UTideAttackExecution
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere)
	TObjectPtr<UAnimMontage> Montage;

	// 区間の最大継続秒数 ※追い越しを検知しなくてもこの秒数で強制切り返し(フォールバック)
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.05"))
	float TurnInterval = 1.5f;

	// 折り返し区間の回数の下限
	UPROPERTY(EditAnywhere, meta = (ClampMin = "1"))
	int32 RepeatCountMin = 1;

	// 折り返し区間の回数の上限
	UPROPERTY(EditAnywhere, meta = (ClampMin = "1"))
	int32 RepeatCountMax = 2;

	// 攻撃中の常駐VFX
	UPROPERTY(EditAnywhere, Category = "VFX")
	TObjectPtr<UNiagaraSystem> LoopVFX;

	// 進行方向を+Xとしたアクター相対オフセット (例: X=前方, Z=上方)
	UPROPERTY(EditAnywhere, Category = "VFX")
	FVector LoopVFXOffset = FVector::ZeroVector;

	// 進行方向基準の相対回転 (VFXの向き調整用)
	UPROPERTY(EditAnywhere, Category = "VFX")
	FRotator LoopVFXRotation = FRotator::ZeroRotator;

	// LoopVFXのスケール。NiagaraのUser floatパラメータ "Scale" に流し込む
	UPROPERTY(EditAnywhere, Category = "VFX", meta = (ClampMin = "0.0"))
	float LoopVFXScale = 1.0f;

	// 移動速度
	UPROPERTY(EditAnywhere)
	float MoveSpeed = 800.0f;

	// lp中に前方どこまでをNav到達チェックするか
	UPROPERTY(EditAnywhere)
	float NavCheckDistance = 200.0f;

	// Navチェックの秒間隔
	UPROPERTY(EditAnywhere)
	float NavCheckInterval = 0.1f;

	// 足元からの落差がこの値を超えたら危険と判定
	UPROPERTY(EditAnywhere)
	float NavCheckMaxFallHeight = 60.0f;

	// 各突進区間の開始からHomingDuration秒だけ、
	// 毎フレームPC方向へChargeDirectionをゆるく寄せる
	// (初回だけでなく切り返し後の各突進にも効く)
	UPROPERTY(EditAnywhere, Category = "Tracking", meta = (ClampMin = "0.0"))
	float HomingDuration = 0.3f;

	// ホーミング中の最大旋回速度 (deg/s)。小さいほど避けやすい
	UPROPERTY(EditAnywhere, Category = "Tracking", meta = (ClampMin = "0.0"))
	float HomingTurnRate = 180.0f;

	// PCとの距離がこの値以下ならホーミング／溜め追従を切る
	// 近距離での理不尽な食いつきを防ぎ、コミット直進にして回避を成立させる
	UPROPERTY(EditAnywhere, Category = "Tracking", meta = (ClampMin = "0.0"))
	float HomingMinDistance = 400.0f;

	// 照準時にPCの速度から何秒先の位置を予測して狙うか (0で現在位置)
	UPROPERTY(EditAnywhere, Category = "Tracking", meta = (ClampMin = "0.0"))
	float PredictLeadTime = 0.3f;

	// 進行方向と「実際の現在PC方向」の角度がこれを超えたらPCを追い越した(外した)と判断する
	// (deg)
	UPROPERTY(EditAnywhere, Category = "Tracking", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float AbortAngle = 90.0f;

	// 追い越し(角度超過)がこの秒数続いたら切り返す ※すれ違った瞬間の一時的な超過では切り返さない
	UPROPERTY(EditAnywhere, Category = "Tracking", meta = (ClampMin = "0.0"))
	float AbortHoldTime = 0.5f;

	// 各突進区間の最小継続時間 (秒)。この秒数までは追い越し判定で切り返さない
	// (即切り返しでの往復ブレ防止)
	UPROPERTY(EditAnywhere, Category = "Tracking", meta = (ClampMin = "0.0"))
	float MinCommitTime = 0.3f;

	// 切り返し時の「溜め」時間 (秒)。この間その場で停止し、傾きながら徐々に新方向へ旋回する
	// 光輪(モンタージュ)の回転は止めない。0で溜めなし(即座に向き直す)
	UPROPERTY(EditAnywhere, Category = "Tracking", meta = (ClampMin = "0.0"))
	float CutbackWindupTime = 1.0f;

	// 最大バンク角 (deg) 旋回方向へメッシュをロール
	UPROPERTY(EditAnywhere, Category = "Banking", meta = (ClampMin = "0.0"))
	float MaxBankAngle = 25.0f;

	// この旋回速度 (deg/s) で最大バンクに達する。小さいほど軽い旋回でも大きく傾く
	UPROPERTY(EditAnywhere, Category = "Banking", meta = (ClampMin = "1.0"))
	float BankSensitivity = 200.0f;

	// バンクの応答の速さ(バネの固有振動数)。大きいほどキビキビ、小さいほどゆったり慣性が乗る
	UPROPERTY(EditAnywhere, Category = "Banking", meta = (ClampMin = "0.0"))
	float BankInterpSpeed = 8.0f;

	// バンクの減衰比。1.0 = 臨界減衰(オーバーシュートなし)、<1 = 揺り戻し(バネっぽく)、
	// >1 = もっさり
	UPROPERTY(EditAnywhere, Category = "Banking", meta = (ClampMin = "0.0"))
	float BankDamping = 1.0f;

	// 傾く左右を反転する (メッシュの向きに応じて調整)
	UPROPERTY(EditAnywhere, Category = "Banking")
	bool bInvertBank = true;

	virtual UAnimMontage* GetMontage() const override { return Montage; }
	virtual void OnAttackEvent(AEnemyCharacter* Enemy, FGameplayTag EventTag) override;
	virtual void OnAttackTick(AEnemyCharacter* Enemy, float DeltaTime) override;
	virtual void OnAttackEnd(AEnemyCharacter* Enemy) override;

private:

	void JumpToEnd();
	// 常駐VFXの生成
	void StartLoopVFX(AEnemyCharacter* Enemy);
	// 常駐VFXの破棄
	void StopLoopVFX();
	// 敵の現在ターゲット(PC)方向をXY正規化で返す。取得できなければ現在の前方
	FVector GetTargetDirection(AEnemyCharacter* Enemy) const;

	// PredictLeadTime秒先のPC予測位置への方向をXY正規化で返す
	// 取得できなければ現在の前方
	FVector GetPredictedTargetDirection(AEnemyCharacter* Enemy) const;

	// PCとのXY距離を返す。ターゲットが取得できなければ -1
	float GetTargetDistance2D(AEnemyCharacter* Enemy) const;

	// 追い越し検知時の切り返し (残り回数があれば再照準、無ければedへ)
	void AdvanceSegment(AEnemyCharacter* Enemy);
	// 旋回方向へメッシュをバンク (ロール) させる
	void UpdateBanking(AEnemyCharacter* Enemy, float DeltaTime);
	// バンクを初期姿勢へ戻す
	void RestoreMeshBank(AEnemyCharacter* Enemy);

	TWeakObjectPtr<AEnemyCharacter> CachedEnemy;
	TWeakObjectPtr<UNiagaraComponent> LoopVFXComp;
	float PreMoveMaxWalkSpeed = 0.0f;
	float PreAnimRMTranslationScale = -1.0f;
	float NavCheckTimer = 0.0f;
	float ChargeElapsed = 0.0f;
	float SegmentElapsed = 0.0f;
	float AbortTimer = 0.0f;
	FVector ChargeDirection = FVector::ForwardVector;

	// 切り返しの溜め (その場で徐々に旋回するフェーズ)
	bool bWindingUp = false;
	float WindupElapsed = 0.0f;
	FVector WindupStartDir = FVector::ForwardVector;
	FVector WindupTargetDir = FVector::ForwardVector;
	bool bMoving = false;
	bool bRotationOverridden = false;
	bool bSavedUseControllerDesiredRotation = false;

	// バンク (メッシュのロール)
	float CurrentBankRoll = 0.0f;
	float BankVelocity = 0.0f;
	float PrevChargeYaw = 0.0f;
	FRotator MeshBaseRelRot = FRotator::ZeroRotator;
	bool bMeshBankActive = false;

	// 突進中にすり抜けた他敵 (終了時に移動無視を解除する)
	TArray<TWeakObjectPtr<AActor>> IgnoredEnemies;

	int32 RepeatsRemaining = 0;
	bool  bCycleInitialized = false;

};
