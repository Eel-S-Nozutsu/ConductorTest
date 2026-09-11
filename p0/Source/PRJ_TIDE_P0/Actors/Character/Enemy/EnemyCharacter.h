// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PRJ_TIDE_P0/Actors/Character/TideCharacter.h"
#include "PRJ_TIDE_P0/Components/Combat/HitReactionComponent.h"
#include "PRJ_TIDE_P0/Components/Equipment/HaloComponent.h"
#include "PRJ_TIDE_P0/Data/Enemy/EnemyWaitSettings.h"
#include "PRJ_TIDE_P0/Data/Combat/DeathRagdollTypes.h"
#include "PRJ_TIDE_P0/Interfaces/Environment/IWindAffectable.h"
#include "PRJ_TIDE_P0/Interfaces/Environment/IGroundPullAffectable.h"
#include "Components/SphereComponent.h"
#include "EnemyCharacter.generated.h"

/**
 * 敵
 */
UCLASS(Abstract)
class PRJ_TIDE_P0_API AEnemyCharacter : public ATideCharacter, public IWindAffectable, public IGroundPullAffectable
{
	GENERATED_BODY()

public:

	AEnemyCharacter();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void FellOutOfWorld(const UDamageType& DmgType) override;
	virtual EDamageResult ReceiveDamage(const FDamageInfo& DamageInfo) override;
	virtual void OnModifyDamageInfo(FDamageInfo& DamageInfo, FGameplayTag AttackTypeTag, AActor* Target = nullptr) override;
	virtual void DrawImGuiInspector() override;

	class UEnemyBattleComponent* GetBattleComponent() const { return BattleComponent; }
	class UMotionWarpingComponent* GetMotionWarping() const { return MotionWarping; }

	// DAから構成される光輪メッシュ実体。HaloComponentがこれを操作する
	// (未構成ならnullptr)
	class UStaticMeshComponent* GetHaloMeshComponent() const { return HaloMeshComponent; }

	// 実行中の攻撃 (FAttackEntry) のダメージ実数値
	// UEnemyAIState_Attackが攻撃開始時にセットし終了/中断でクリアする
	// OnModifyDamageInfoと弾スポーンがこの値を唯一の真実として参照する
	// 負値 = 攻撃非実行中 (フォールバック扱い)
	void SetCurrentAttackDamage(float InDamage) { CurrentAttackDamage = InDamage; }
	void ClearCurrentAttackDamage() { CurrentAttackDamage = -1.0f; }
	float GetCurrentAttackDamage() const { return CurrentAttackDamage; }

	// 実行中の攻撃 (FAttackEntry) が被弾側へ与えるヒットリアクションタグ
	// Damageと同じくUEnemyAIState_Attackが開始でセット・終了/中断でクリアし、
	// 近接(OnModifyDamageInfo)と弾スポーンがこの値を参照する
	// 空 = 未設定 (フォールバック扱い)
	void SetCurrentAttackHitReactionTag(FGameplayTag InTag) { CurrentAttackHitReactionTag = InTag; }
	void ClearCurrentAttackHitReactionTag() { CurrentAttackHitReactionTag = FGameplayTag(); }
	FGameplayTag GetCurrentAttackHitReactionTag() const { return CurrentAttackHitReactionTag; }

	// 実行中の攻撃 (FAttackEntry) が「スポナー範囲外で弾をフェード」指定か
	// Damageと同じくUEnemyAIState_Attackが開始でセット・終了/中断でクリアし、
	// 弾スポーンがActivateProjectileで参照する
	void SetCurrentAttackFadeOutsideRange(bool bInFade) { bCurrentAttackFadeOutsideRange = bInFade; }
	void ClearCurrentAttackFadeOutsideRange() { bCurrentAttackFadeOutsideRange = false; }
	bool GetCurrentAttackFadeOutsideRange() const { return bCurrentAttackFadeOutsideRange; }

	// 攻撃実行中フラグ。UEnemyAIState_Attackが攻撃開始でセット・終了/中断でクリアする
	// 。ダメージ値と違い攻撃を定義していない攻撃でも確実に立つので、
	// 予知回避など「攻撃中は割り込ませたくない」判定に使う
	virtual void SetExecutingAttack(bool bInExecuting)
	{
		bIsExecutingAttack = bInExecuting;
		// 攻撃が終われば向きロックも必ず終わり。実行側の外し漏れで
		// 以後ずっと向き補正が効かない個体になるのを防ぐ保険
		if (!bInExecuting) bActionFacingLocked = false;
	}
	bool IsExecutingAttack() const { return bIsExecutingAttack; }

	// 攻撃ロジックが向きを握っている間true。立っている間、モーション側の向き補正
	// (UEnemyNotifyBehavior_RotateToTarget) は何もしない
	// 協調攻撃の背中合わせのように「PCを向いてはいけない」配置を、共用モンタージュに
	// 仕込まれた「PCを向く」補正から守るためのスイッチ
	void SetActionFacingLocked(bool bInLocked) { bActionFacingLocked = bInLocked; }
	bool IsActionFacingLocked() const { return bActionFacingLocked; }
	void ApplyWaitSettings(const FEnemyWaitSettings& InSettings);

	const FEnemyWaitSettings& GetWaitSettings() const { return WaitSettings; }

	// [旧/移行用] タイプ別DAへ移す前の攻撃テーブル上書き
	// CharacterData未注入時のフォールバック
	void SetAttackTableOverride(class UDataTable* InTable) { AttackTableOverride = InTable; }

	// スポナー等が生成時にAI視線半径を上書きする
	// OnPossessで参照するためFinishSpawningより前に呼ぶこと
	void SetSightRadiusOverride(float InRadius) { SightRadiusOverride = InRadius; }
	float GetSightRadiusOverride() const { return SightRadiusOverride; }

	float GetDistToTarget() const;
	float GetAngleToTarget() const;

	// ターゲットへの上下角 (絶対値, 度)。水平=0, 真上/真下=90。ターゲットなしは-1
	// 攻撃選定のMaxPitchToTarget判定や、真下/真上の除外に使う
	float GetPitchAngleToTarget() const;
	bool IsVisibleToPlayerCamera() const { return bVisibleToPlayerCamera; }
	float GetContinuousVisibleToPlayerCameraTime() const { return ContinuousVisibleToPlayerCameraTime; }

	bool CanTransitionToNextAction() const;

	// State.Enemy.CanTransitionをカウントごと強制クリアする
	// 攻撃開始時に呼び、前モーション (リアクション等) のブレンドアウト中に残る窓を
	// 引き継いで初手で早期遷移してしまうのを防ぐ
	void ClearCanTransition();

	// 予知回避。AttackDirectionに垂直な方向にステップする
	void TriggerPredictiveDodge(FVector AttackDirection);

	bool IsDodging() const { return ActiveDodgeMontage != nullptr; }

	// 被弾リアクション等で行動不能か
	// UEnemyBrainComponentのReact状態の入場条件であり、
	// 「いま行動できない」の唯一の真実。BT運用では対で管理するStopLogic
	// /RestartLogicが担っていたもの
	// (9種の理由が1つの非参照カウントなスイッチを奪い合っていた)
	// 立てる側は必ず対応する解除点も用意すること
	bool IsReacting() const;

	// 現在の戦闘対象。実体はUEnemyThreatComponentが持つ
	// (BBはBT運用中のミラー)。EQSコンテキストや攻撃抽選から引かれる
	AActor* GetTargetActor() const;

	// スポナーのテリトリー。半径0 = テリトリー無し
	// 実体はGetTargetActorと同じくThreatが持つ
	// 徘徊圏(InnerVolume)と交戦圏(OuterVolume)の2段。交戦中の行動範囲は交戦圏
	FVector GetPatrolOrigin() const;
	float GetPatrolRadius() const;
	float GetCombatRadius() const;

	void SetReacting(bool bInReacting, FName Reason = NAME_None);

	// 行動抑止の理由ラベル。React状態のデバッグ表示専用で挙動には影響しない
	// BT運用のStopLogic(Reason) が持っていた情報を残すためのもの
	// (Reactは6種類の理由を1つの状態に畳んでいるため、これが無いと切り分けできない)
	FName GetReactionReason() const;

	// 反撃攻撃を要求する。ステートマシン運用ではUEnemyBrainComponentへ、
	// BT運用ではBlackboardへ振り分ける (書き先の差をここ1箇所に閉じる)
	void RequestCounterAttack(int32 AttackIndex);

	// 強制攻撃を要求する (協力攻撃の徴集・ImGuiデバッグ)
	// 書き先の差はRequestCounterAttackと同じ
	// 要求できなければfalse (呼び側が予約を巻き戻せるように)
	bool RequestForceAttack(int32 AttackIndex);

	// 暫定HUDでボス用HPゲージを使うか。既定は通常ゲージ
	// ABossCharacter派生(EM0010含む)でtrueを返す
	virtual bool UsesBossHpGauge() const { return false; }

	// パトロール原点へ帰還中か。帰還中は完全無敵 + HPゲージをグレーアウト表示する
	// UEnemyAIState_ReturnToOriginが開始/終了で切り替える
	void SetReturningHome(bool bInReturningHome) { bIsReturningHome = bInReturningHome; }
	bool IsReturningHome() const { return bIsReturningHome; }

	// --- IWindAffectable(竜巻などの巻き上げ) ---
	virtual void OnWindEnter(const FWindInfluence& Wind) override;
	virtual void OnWindTick(const FWindInfluence& Wind, float DeltaTime) override;
	virtual void OnWindExit() override;

	// --- IGroundPullAffectable(アリジゴクの渦などの地面の吸い込み
	// IWindAffectableとは別現象) ---
	virtual void OnGroundPullTick(const FGroundPullInfluence& Pull, float DeltaTime) override;

	USceneComponent* AttachHaloToSocket(FName SocketName, FName HaloComponentName = FName("Halo"), float Scale = 1.0f);
	void AttachHaloForAttack(FName SocketName, FName HaloComponentName = FName("Halo"));
	void RestoreHaloToStateSocket(FName HaloComponentName = FName("Halo"));
	void BeginGuard(FName HaloComponentName = FName("Halo"));
	void StartGuardAutoRelease();
	void SetHaloBarrierActive(bool bEnable);
	void SetHaloDeployed(bool bActive);
	void SetHaloThrown(bool bThrown);
	bool IsHaloAway() const;
	void StartHaloThrowRegenIfAway();
	void SetHaloAttackGlow(bool bBright);
	// 光輪攻撃の区間中true。共通仕様として攻撃中は光輪を破壊させない
	// (HaloAttachノーティファイから駆動)
	void SetHaloAttackSuppressed(bool bSuppressed);
	void SetArmamentActive(bool bActive);
	bool IsArmamentActive() const;

	// 殴れる窓: 硬化を解除して攻撃を通せる状態にする。攻撃の開始時に呼ぶ
	void BeginArmamentSoftWindow();
	// 殴れる窓を閉じて再硬化する。ただし部位破壊リアクション再生中なら、
	// リアクション終了まで再硬化を遅延する (殴れる窓をリアクションで途切れさせない)
	void EndArmamentSoftWindow();
	// 部位破壊/連動リアクション終了時に呼ぶ。遅延していた再硬化があれば実行する
	void NotifyPartReactionEnded();
	// 部位破壊/連動リアクションのモンタージュが再生中か(ABossCharacterでoverride)
	virtual bool IsPlayingPartReaction() const { return false; }

	// 光輪の発光強度を直接書き込む。ガードチャージのフリッカー演出などから毎フレーム呼ぶ
	void SetHaloGlowIntensity(float Intensity);

	// ガードを即時終了する (チャージ暴発後などに呼ぶ)
	void EndGuardNow(bool bRestartAI = true);

	// ガード開始モーションを守っているHaloOpen SAをカウントごと落とす
	// ガードを割られた後まで残ると貫通リアクションがSAゲートに消され、割った側に手応えが返らない
	void ClearGuardMotionArmor();

	// ReceiveDamageの処理中か (被弾由来の同期的なタスク中断の判別用)
	bool IsProcessingDamage() const { return bProcessingDamage; }

	// ガードチャージ暴発のExecutionを生存させる
	// (ExecutionはOnAttackEnd後にGCされうるため、
	// 暴発までの間Enemyがハードリファレンスで保持する)
	void HoldChargeExecution(UObject* Execution) { PendingChargeExecution = Execution; }
	void ReleaseChargeExecution() { PendingChargeExecution = nullptr; }

protected:

	UPROPERTY(VisibleAnywhere, Category = "Tide|Components")
	TObjectPtr<class UEnemyBattleComponent> BattleComponent = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Tide|Components")
	TObjectPtr<class ULockOnTargetComponent> LockOnTarget = nullptr;

	// 攻撃モンタージュのroot motionをターゲットへ伸縮/回転補正する (間合い・向きの吸着)
	UPROPERTY(VisibleAnywhere, Category = "Tide|Components")
	TObjectPtr<class UMotionWarpingComponent> MotionWarping = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Tide|Components")
	TObjectPtr<UHaloComponent> HaloComponent = nullptr;

	// 光輪のメッシュ実体。DA
	// (HaloMesh/HaloBackSocketName/HaloNormalScale)
	// からOnConstructionで構成し、エディタでも実物が見える
	// 挙動 (ガード/背後被弾/破壊など) はHaloComponentが握る
	UPROPERTY(VisibleAnywhere, Category = "Tide|Components")
	TObjectPtr<class UStaticMeshComponent> HaloMeshComponent = nullptr;

	// 全敵共通の光輪システム(肉質・無防備などの共通挙動)
	// 既存のHalo/BackHalo/Partを段階移行する
	UPROPERTY(VisibleAnywhere, Category = "Tide|Components")
	TObjectPtr<class UHaloSystemComponent> HaloSystemComponent = nullptr;

	// 死亡ラグドール時の衝突ダメージ用コライダー。平常時はNoCollision
	UPROPERTY(VisibleAnywhere, Category = "Tide|Components")
	TObjectPtr<USphereComponent> RagdollDamageCollider = nullptr;

	// 暫定HUDの頭上HPゲージを頭上へ何ピクセル上に置くか ※画面ピクセル固定で距離によらず一定
	UPROPERTY(EditAnywhere, Category = "Tide|UI")
	float HpGaugeScreenOffsetY = 60.0f;

	// ボス用HPゲージに表示する名前。空なら名前なし
	UPROPERTY(EditAnywhere, Category = "Tide|UI")
	FString HpGaugeBossName = TEXT("NoName");

	UPROPERTY(EditInstanceOnly, Category = "Tide|AI|Wait")
	FEnemyWaitSettings WaitSettings;

	// [旧/移行用] タイプ別DAへ移す前の攻撃テーブル上書き。タイプ別DAへ移したら削除予定
	// 既存マップの値を消さないため残置し、CharacterData未注入のときだけフォールバックで使う
	UPROPERTY(EditInstanceOnly, Category = "Tide|AI|Attacks", meta = (DisplayName = "[旧] 攻撃テーブル(移行用)", RowType = "/Script/PRJ_TIDE_P0.AttackEntry"))
	TObjectPtr<class UDataTable> AttackTableOverride = nullptr;

	// レベル配置インスタンスだけAI視線半径を上書きする。-1で無効(DAのSightRadiusを使う)
	UPROPERTY(EditInstanceOnly, Category = "Tide|AI", meta = (ClampMin = "-1.0"))
	float SightRadiusOverride = -1.0f;

	// プレイヤーカメラへの可視状態は毎フレームではなくこの間隔で再サンプルする
	// 画面投影 + Visibility遮蔽の判定回数を抑えつつ、連続可視時間だけは毎フレーム加算する
	UPROPERTY(EditDefaultsOnly, Category = "Tide|AI", meta = (ClampMin = "0.0"))
	float PlayerCameraVisibilitySampleInterval = 0.2f;

	UPROPERTY(EditDefaultsOnly, Category = "Tide|Death")
	float RagdollDeathDelay = 1.0f;

private:

	// 殴れる窓 (硬化解除) の実行時状態
	bool bArmamentSoftWindowOpen = false;  // 窓が開いているか
	bool bArmamentRestorePending = false;  // リアクション終了時に再硬化する予約

	// この一撃でHPが0以下になる(=とどめ)なら、加害プレイヤーへ通知してフィニッシュ演出を起動する
	// 通常被弾・スーパーアーマー被弾の両方から呼ばれる(GetCurrentHPは被弾前HP)
	void TryNotifyFinishingBlow(const struct FDamageInfo& Info);

	UFUNCTION()
	void OnDamageReceived(const struct FDamageInfo& DamageInfo);

	UFUNCTION()
	void OnDeath();

	UFUNCTION()
	void OnReactionMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	// HitReactionComponentのデリゲートを受けてステートタグ・AIを操作する
	UFUNCTION()
	void OnBlowbackPhaseChanged(EBlowbackPhase NewPhase);

	UFUNCTION()
	void OnBlowbackEnded();

	bool IsBackAttack(const FDamageInfo& DamageInfo) const;
	void StartRagdoll();

	// ラグドールへ吹き飛び速度と回転を与える
	// 物理初期化に上書きされるのでStartRagdollの次フレームに呼ぶ
	void ApplyRagdollLaunch(const FVector& LaunchVelocity);

	void PlayDeathExplosion();
	void UpdatePlayerCameraVisibility(float DeltaSeconds);
	bool SampleVisibleToPlayerCamera() const;

	bool CanDodge() const;
	bool TryPlayDodgeMontage(FVector DodgeDir, UAnimMontage* Montage);

	float LastDodgeTime = -1e9f;
	int32 PendingDodgeCounterAttackIndex = -1;

	// 実行中攻撃のダメージ。負値 = 非実行中
	float CurrentAttackDamage = -1.0f;

	// 実行中攻撃のヒットリアクションタグ。空 = 非実行中/未設定
	FGameplayTag CurrentAttackHitReactionTag;

	// 実行中攻撃が「スポナー範囲外で弾をフェード」指定か。非実行中はfalse
	bool bCurrentAttackFadeOutsideRange = false;

	// 攻撃を実行中か。UEnemyAIState_Attackが開始でセット・終了/中断でクリアする
	bool bIsExecutingAttack = false;

	// 攻撃ロジックが向きを握っているか (SetActionFacingLocked)
	bool bActionFacingLocked = false;
	bool bVisibleToPlayerCamera = false;
	float ContinuousVisibleToPlayerCameraTime = 0.0f;
	float PlayerCameraVisibilitySampleRemaining = 0.0f;

	// 暫定HUDの追従ゲージ(遅れて減る残像バー)の現在値。負値 = 未初期化
	// EnemyHudPlaceholder::Drawにin/outで渡し、敵ごとに状態を保持する
	float HpGaugeTrailingHP = -1.0f;

	// 暫定HUDの表示in/outアニメ進行 [0,1]。0=非表示, 1=表示
	// EnemyHudPlaceholder::Drawが補間する
	float HpGaugeShowAlpha = 0.0f;

	bool bInRagdoll = false;

	bool bIsReacting = false;

	// bIsReactingを立てた理由。デバッグ表示専用 (下ろすとクリアされる)
	FName ReactionReason;

	// ReceiveDamageの処理中か。被弾リアクションが攻撃タスクを同期中断した場合と、
	// スポナー外等による中断をガード側が見分けるために使う
	bool bProcessingDamage = false;

	// 被弾硬直 (stagger) でStopLogicした直後に立てる
	// staggerのCanTransitionウィンドウに入った時点でBTを再開してコンボに割り込み反撃
	// させるためのワンショットフラグ。Tickで消費し、次の被弾でまた立つ
	bool bHitReactionCounterArmed = false;

	FTransform SpawnTransform;
	FTimerHandle DeathFadeTimerHandle;
	FTimerHandle DeathPoseTimerHandle;
	TWeakObjectPtr<AActor> LastDamageInstigator;

	TObjectPtr<UAnimMontage> ActiveDodgeMontage = nullptr;

	FVector DeathVelocity = FVector::ZeroVector;

	// 致死ヒットが指定した死亡吹き飛び方向 (水平)
	// OnDeathはDamageInfoを持たないためここへ退避する
	FVector DeathLaunchDirOverride = FVector::ZeroVector;

	// 致死ヒットが指定した回転モード・速度・吹き飛び力 (敵既定より優先する。速度/力<0=未指定)
	bool bDeathSpinModeOverridden = false;
	EDeathRagdollSpinMode DeathSpinModeOverride = EDeathRagdollSpinMode::Corkscrew;
	float DeathSpinSpeedOverride = -1.0f;
	float DeathLaunchForceOverride = -1.0f;
	float DeathLaunchUpForceOverride = -1.0f;

	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> BodyDMIs;

	bool bIsReturningHome = false;

	// ガードチャージ暴発Executionの延命用ハードリファレンス
	UPROPERTY()
	TObjectPtr<UObject> PendingChargeExecution = nullptr;


	void ApplyArmamentVisual(float Value);

	// DAのBodyColorを胴体DMIへ適用する
	void ApplyBodyColor();

	// この敵が竜巻で浮かない (ボス等) か
	// データアセットのWindReaction.bAnchoredを見る
	bool IsWindAnchored() const;

	// 浮かない敵が竜巻 (大) のスリップを受けたときののけぞり
	// 常時SAでも見えるよう直接モンタージュ再生する
	// 竜巻内にいる間は繰り返し呼ばれ、ブレンドアウトのたびに再生し直してループする
	void TriggerAnchoredWindFlinch();
	// のけぞりループを終了し、行動を再開する
	void EndAnchoredWindFlinch();

	// のけぞりループ中か
	bool bAnchoredWindFlinching = false;
	// 最後に竜巻 (大) のスリップを受けた時刻。ループ継続判定に使う
	double LastAnchoredWindHitTime = -1.0e9;

	// 巻き上げ捕捉の開始／終了(実際にAI・移動を制御するのは初回／最後の風源のときだけ)
	void BeginWindCapture();
	void EndWindCapture();

	int32 WindCaptureCount = 0;		// 同時に巻き込んでいる風源の数(参照カウント)
	bool bWindCaptured = false;		// 実際にAI・移動を竜巻側へ明け渡しているか

	// ガード中に竜巻へ触れたら、光輪を破壊してからこの秒数だけ待って巻き込む (0で即時)
	UPROPERTY(EditDefaultsOnly, Category = "Tide|Wind")
	float WindGuardBreakDelay = 0.2f;

	bool bWindGuardBreakPending = false;	// 光輪破壊後、巻き込み開始までの待機中か
	FTimerHandle WindGuardBreakTimerHandle;

	// 竜巻リアクションのアニメ進行はUHitReactionComponentが持つ
	// (吹き飛びと共通のドライバ)。ここに残るのはIWindAffectableの実装
	// (速度オーバーライド・捕捉の参照カウント・ガード破壊待ち)だけ
	// 巻き上げ終了のAI再開はOnWindReactionEndedデリゲート経由で受ける

	// リアクション終了後にAIを再開する(死亡・ガード中は再開しない)
	UFUNCTION()
	void RestartAIAfterReaction();

};
