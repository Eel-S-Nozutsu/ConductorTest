// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "NiagaraSystem.h"
#include "PRJ_TIDE_P0/Data/Enemy/DamageReactionTables.h"
#include "HitReactionComponent.generated.h"

class UDataTable;

UENUM()
enum class EBlowbackPhase : uint8
{
	None,
	Start,
	Loop,
	WallHitStart,
	WallHitLoop,
	Land,
	Recovery,
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBlowbackPhaseChanged, EBlowbackPhase, NewPhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBlowbackEnded);

/**
 * 竜巻の巻き上げリアクションのアニメフェーズ。
 * Launching(打ち上げ開始) -> Airborne(上昇~滞空) -> Falling(落下) -> Landing(着地) -> Recovery(起き上がり)
 *
 * 物理・捕捉(速度オーバーライド／捕捉の参照カウント／ガード破壊待ち)はIWindAffectable実装側
 * (AEnemyCharacter) が持ち、こちらはアニメの進行だけを担当する
 */
UENUM()
enum class EWindReactionPhase : uint8
{
	None,
	Launching,
	Airborne,
	Falling,
	Landing,
	Recovery,
};

// 巻き上げリアクションの完全終了時に発火 (AI再開等は所有側でバインド)
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnWindReactionEnded);

/**
 * ヒットリアクション エントリ (Knockback等の単発再生用)
 */
USTRUCT(BlueprintType)
struct FHitReactionEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, meta=(Categories="HitReaction"))
	FGameplayTag ReactionTag;

	// 再生するモンタージュ(未設定ならmontage無し = VFXのみ)
	UPROPERTY(EditAnywhere)
	TObjectPtr<UAnimMontage> Montage = nullptr;

	UPROPERTY(EditAnywhere)
	TObjectPtr<UNiagaraSystem> HitEffect = nullptr;

	UPROPERTY(EditAnywhere)
	float LaunchForce = 0.0f;

	UPROPERTY(EditAnywhere)
	float LaunchUpForce = 0.0f;

	UPROPERTY(EditAnywhere)
	bool bFaceAttacker = false;

};

/**
 * ヒットリアクション 設定
 */
USTRUCT(BlueprintType)
struct FHitReactionSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	TArray<FHitReactionEntry> Reactions;

};

/**
 * ヒットリアクション コンポーネント
 * - Knockback系: HitReactionTag → FHitReactionEntryでモンタージュ再生
 * - Blowoff系:  st-lp-edステートマシンを内包し、壁バウンドにも対応
 */
UCLASS()
class PRJ_TIDE_P0_API UHitReactionComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// プレイヤー用。基底DA
	// (UTideCharacterDataAsset::HitReactionSettings)
	// の単発リアクションを流し込む。敵はInitializeReactionTableのテーブル定義を使う
	void InitializeFromData(const FHitReactionSettings& Settings);
	void Initialize(class UDamageSystemComponent* InDamageSystem, class UStatusComponent* InStatusComponent = nullptr);

	// リアクション定義テーブル (行型FHitReactionRow) を流し込む
	// AEnemyCharacter::BeginPlayから呼ぶ
	void InitializeReactionTable(const UDataTable* InTable);

	// テーブルから多段シーケンスを解決する。見つからなければnullptr
	// (＝DA直設定へフォールバック)
	const FBlowbackAnimSequence* FindTableSequence(FGameplayTag Tag) const;

	// --- 竜巻の巻き上げリアクション(アニメ側のみ。物理・捕捉は所有側が持つ) ---

	// 巻き上げに使うモンタージュ／打ち上げ力をテーブルのHitReaction.Wind行から解決する
	// InitializeReactionTableの後にBeginPlayから呼ぶ
	void InitializeWindReaction();

	// 巻き上げ開始(初回捕捉時)。物理打ち上げを行う場合は所有側がLaunchCharacterするため、
	// 適用すべき打ち上げ力を返す(0なら竜巻の持続揚力に任せる＝撃ち出さない)
	float BeginWindReaction();

	// 風から外れた(捕捉解除)。着地待ちへ入る
	void OnWindReleased();

	// 巻き上げリアクション進行中か
	bool IsInWindReaction() const { return WindPhase != EWindReactionPhase::None; }

	// 物理打ち上げの初速がまだ効いている区間か(所有側が速度オーバーライドを抑制するのに使う)
	// 上昇を確認したのち落下へ転じた時点でfalseになる
	bool IsWindPhysicsLaunching() const { return bWindPhysicsLaunch; }
	void NotifyWindLaunchRising() { bWindLaunchPeaked = true; }
	bool HasWindLaunchPeaked() const { return bWindLaunchPeaked; }
	void EndWindPhysicsLaunch() { bWindPhysicsLaunch = false; }

	// 竜巻に捕捉中か(空中アニメをAirLoopに固定するかの判定に使う)。所有側が出入りで更新する
	void SetWindCaptured(bool bInCaptured) { bWindCaptured = bInCaptured; }

	// 巻き上げを中断して状態を畳む(死亡・吹き飛びに奪われた場合)。RM抑制も解除する
	void AbortWindReaction();

	// 巻き上げシーケンス完全終了時に発火 (AI再開等は所有側でバインド)
	UPROPERTY(BlueprintAssignable)
	FOnWindReactionEnded OnWindReactionEnded;

	// 死亡ラグドール開始時にEnemyCharacterから呼ぶ
	// コライダーを有効化して衝突ダメージを設定する
	void BeginRagdollDamage(class USphereComponent* Collider, float Damage, FGameplayTag HitReactionTag, AActor* Instigator);

	// Blowoffヒット時にEnemyCharacter::ReceiveDamageから事前計算用に呼
	// ぶ
	FVector GetLaunchVelocity(const struct FDamageInfo& DamageInfo) const;

	EBlowbackPhase GetBlowbackPhase() const { return BlowbackPhase; }
	bool IsInBlowback() const { return BlowbackPhase != EBlowbackPhase::None; }

	// この被弾で実際にリアクション(montage/吹き飛び)が走るか返す
	// 行動抑止SetReactingを解除保証時だけに限定するため
	// montage未再生だと解除通知が来ず抑止が下りない
	// ※OnDamageReceivedの分岐と対称に保つ
	bool WillReact(FGameplayTag Tag) const;

	// フェーズ変化時に発火 (StateTag操作等はEnemyCharacter側でバインド)
	UPROPERTY(BlueprintAssignable)
	FOnBlowbackPhaseChanged OnBlowbackPhaseChanged;

	// Blowbackシーケンス完全終了時に発火(AI再起動等はEnemyCharacter側でバインド)
	UPROPERTY(BlueprintAssignable)
	FOnBlowbackEnded OnBlowbackEnded;

private:

	UFUNCTION()
	void OnDamageReceived(const struct FDamageInfo& DamageInfo);

	// Knockback系
	const FHitReactionEntry* FindEntryByTag(FGameplayTag Tag) const;
	void PlayEntry(const FHitReactionEntry& Entry, const FDamageInfo& DamageInfo);

	// Blowoff系
	void StartBlowbackSequence(const FDamageInfo& DamageInfo);
	void StartWallHitSequence();
	void AdvanceBlowbackPhase();

	UFUNCTION()
	void OnBlowbackMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted);

	UFUNCTION()
	void OnBlowbackLanded(const FHitResult& Hit);

	UFUNCTION()
	void OnBlowbackWallHit(UPrimitiveComponent* HitComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	UFUNCTION()
	void OnRagdollSphereOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	static UAnimMontage* PickRecoveryMontage(const TArray<FBlowbackRecoveryEntry>& Recoveries,
		int32& LastIndex, const ACharacter* Enemy, float NavCheckDistance);

	void SetBlowbackPhase(EBlowbackPhase NewPhase);
	void EndBlowback();

	// 空中フェーズで抑制したroot motion移動スケールを元に戻す(着地時・終了時に呼ぶ。冪等)
	void RestoreAirborneRootMotion();

	// --- 巻き上げリアクション(内部) ---

	// 空中フェーズ (Airborne/Falling) を実際の上下速度で切り替える
	// TickComponentから毎フレーム呼ぶ
	void UpdateWindAerialAnim();
	// 接地したままWindPhaseが畳まれない取りこぼし(着地/ブレンドアウトイベント欠落)を
	// 検出して強制終了する。TickComponentから毎フレーム呼ぶ
	void UpdateWindReactionWatchdog(float DeltaTime);
	// 巻き上げのモンタージュ連結(ループ再トリガ / Land->Recovery /
	// Recovery->終了)
	void AdvanceWindPhase(UAnimMontage* Montage);
	// 着地して着地モーションへ遷移する
	void OnWindLanded();
	// 巻き上げで抑制したroot motion移動スケールを元に戻す(冪等)
	void RestoreWindRootMotion();
	void PlayWindMontage(UAnimMontage* Montage);
	// 巻き上げを終了してAI再開を通知する
	void EndWindReaction();

	// 巻き上げのアニメ状態
	EWindReactionPhase WindPhase = EWindReactionPhase::None;

	UPROPERTY() TObjectPtr<UAnimMontage> WindStartMontage    = nullptr;
	UPROPERTY() TObjectPtr<UAnimMontage> WindAirLoopMontage  = nullptr;
	UPROPERTY() TObjectPtr<UAnimMontage> WindFallLoopMontage = nullptr;
	UPROPERTY() TObjectPtr<UAnimMontage> WindLandMontage     = nullptr;
	UPROPERTY() TObjectPtr<UAnimMontage> WindRecoveryMontage = nullptr;

	// 打ち上げ力。>0で物理打ち上げモード(インパルスで撃ち出して重力で落ちる)
	float WindLaunchUpForce = 0.0f;

	bool bWindCaptured       = false;	// 竜巻に捕捉中か(所有側が更新)
	bool bWindPhysicsLaunch  = false;	// 物理打ち上げの初速が効いている区間か
	bool bWindLaunchPeaked   = false;	// 撃ち出しが速度へ適用されたのを確認したか
	bool bWindRMSuppressed   = false;
	float PreWindRMScale     = 1.0f;

	// 空中アニメ切り替えの最小ドウェル(境界での高速トグル抑制)
	double LastWindAnimSwitchTime = -1.0;

	// 風の外で「落下」「上昇」と判定する上下速度しきい値
	// 両しきい値の間 (apex付近) は現状維持してチラつきを防ぐ
	UPROPERTY(EditDefaultsOnly, Category = "Wind")
	float WindFallVelThreshold = 100.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Wind")
	float WindRiseVelThreshold = 100.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Wind")
	float WindAnimMinDwell = 0.2f;

	// 接地したままWindPhaseが終了しないときに強制終了するまでの猶予秒。
	// 着地/ブレンドアウトイベントの取りこぼしでReact(Wind)へ居座る棒立ちの最後の砦
	UPROPERTY(EditDefaultsOnly, Category = "Wind")
	float WindStuckTimeout = 1.0f;

	// 接地しているのにWindPhaseが畳まれない状態の継続時間(WindStuckTimeoutの計時用)
	float WindGroundedStuckTime = 0.0f;

	// プレイヤー用の単発リアクション(基底DA由来)。敵は下のTableSingleSettingsを使う
	FHitReactionSettings HitReactionSettings;

	// --- テーブル由来のリアクション定義(敵) ---

	// 行から組み立てたエントリ。FindEntryByTagが先に検索する
	// Sequence行も含める(HitEffectとLaunchForce
	// /LaunchUpForceはKindによらず引かれるため)
	FHitReactionSettings TableSingleSettings;

	// 多段シーケンス。タグ→シーケンスの対応 (行の並び順を保持するため配列)
	TArray<TPair<FGameplayTag, FBlowbackAnimSequence>> TableSequences;

	// 現在再生中の多段シーケンス(TableSequencesの要素を指す)
	const FBlowbackAnimSequence* BlowbackSeq = nullptr;

	UPROPERTY()
	UAnimInstance* AnimInstance = nullptr;

	UPROPERTY()
	TObjectPtr<class UStatusComponent> StatusComponent = nullptr;

	// EndPlayでOnDamageReceivedを解除するためにInitializeで束縛した相手を
	// 保持する
	UPROPERTY()
	TObjectPtr<class UDamageSystemComponent> CachedDamageSystem = nullptr;

	EBlowbackPhase BlowbackPhase = EBlowbackPhase::None;
	bool bBlowbackLanded = false;
	bool bWallHitOccurred = false;

	// 空中フェーズでroot motion移動を抑制中か + 抑制前のtranslationスケール
	// (着地で復帰)
	bool bAirborneRMSuppressed = false;
	float PreAirborneRMScale = 1.0f;
	int32 LastRecoveryIndex = -1;
	int32 LastWallHitRecoveryIndex = -1;
	FVector BlowbackVelocityCache = FVector::ZeroVector;

	// 吹き飛ばしを起こした元の加害者。衝突ダメージのInstigatorに使用
	TWeakObjectPtr<AActor> CachedBlowbackInstigator;

	// 死亡ラグドール衝突ダメージ
	float RagdollCollisionDamage = 0.0f;
	FGameplayTag RagdollCollisionHitReactionTag;
	TWeakObjectPtr<AActor> CachedRagdollInstigator;
	TWeakObjectPtr<class USphereComponent> RagdollDamageCollider;

};
