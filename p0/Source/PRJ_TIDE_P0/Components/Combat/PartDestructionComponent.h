// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PRJ_TIDE_P0/Data/Combat/DamageInfo.h"
#include "PRJ_TIDE_P0/Data/Enemy/BossDataAsset.h"
#include "PRJ_TIDE_P0/Utilities/HaloShake.h"
#include "PartDestructionComponent.generated.h"

class UGeometryCollectionComponent;
class UFieldSystemComponent;
class UStaticMeshComponent;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnPartDestroyed, FName /*PartTag*/)
DECLARE_MULTICAST_DELEGATE_OneParam(FOnPartComboDestroyed, int32 /*ComboIndex*/)
DECLARE_MULTICAST_DELEGATE_OneParam(FOnPartRevived, FName /*PartTag*/)

/**
 * 破壊部位の自動復活を保留する要因。攻撃モーション中とダウン中に光輪が生え直すのは不自然なので、
 * クールダウンが明けても要因が残っている間は復活を遅らせる。
 * 要因は交錯する (リアクションが攻撃を中断する) ためビットマスクで独立に管理する
 */
enum class EPartRegenBlockReason : uint8
{
	Attack   = 1 << 0,	// 攻撃実行中 (AEnemyCharacter::SetExecutingAttackの期間)
	Reaction = 1 << 1,	// 部位破壊リアクション/ダウン中 (ABossCharacterが駆動)
};

/**
 * 部位破壊を管理するコンポーネント
 * DamageSystemComponent::OnDamageReceivedを購読し、HitResult.GetComponent()のタグから
 * どの部位がヒットしたかを判断して部位HPを管理する
 */
UCLASS()
class PRJ_TIDE_P0_API UPartDestructionComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UPartDestructionComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ABossCharacter::BeginPlayから呼ぶ。PartsArrayが空ならno-op
	void Initialize(const TArray<FPartEntry>& InParts, const TArray<FPartComboReaction>& InCombos,
		const FBossRecoveryTouch& InRecoveryTouch);

	bool IsPartDestroyed(FName PartTag) const;

	// PartTagを含む連動グループのうち、全メンバー破壊済みのものを返す
	// (なければINDEX_NONE)
	int32 FindCompletedComboForPart(FName PartTag) const;

	// 攻撃開始: 対象部位の光輪を発光させ、攻撃中の部位無敵を開始する
	// (bExcludeFromHaloHideの部位・破壊済み部位は対象外)
	void BeginPartHaloAttackGlow();

	// 叩きつけ等: 対象部位の光輪をディザフェードで消す (無敵は継続)
	void HidePartHalosForAttack();

	// 攻撃終了: 発光を消し、対象部位の光輪をディザフェードで再表示し、無敵を解除する
	// 再表示時は発光しないようにする
	void RestorePartHalosAfterAttack();

	// ダウン復帰中の接触ダメージ窓を開く: 破壊されていない光輪を発光させ、
	// 以降Tickでアクター中心のAOE内の敵対Pawnに周期ダメージを与える
	// 無敵化 (bPartHaloSuppressed) はしない
	void BeginPartHaloRecoveryDamage();

	// 復帰中の接触ダメージ窓を閉じる: 発光を消し、AOE判定を止める
	void EndPartHaloRecoveryDamage();

	// 全部位を即時に強制復活させる (クールダウン・復活保留RegenBlockMaskを無視)
	// ダウン明けの反撃で全光輪を出してから攻撃させる用途。ディザは挟まず即表示する
	void ReviveAllPartsImmediate();

	// 復活の保留要因を立て下げする。全要因が解除された瞬間に、
	// 保留していた部位をまとめて復活させる
	// 攻撃・リアクションのどちらもABossCharacterが駆動する
	void SetRegenBlocked(EPartRegenBlockReason Reason, bool bBlocked);

	// --- デバッグ表示用 (ImGui) ---破壊後の復活までの残り秒。タイマー非稼働なら負値
	float GetPartRegenRemaining(int32 Index) const;
	// 設定上の復活クールダウン秒 (BreakRegenCooldown)
	float GetPartRegenCooldown(int32 Index) const;
	// 復活タイマーは経過したが攻撃/リアクションで保留されているか
	bool IsPartRevivePending(int32 Index) const;

	// 指定コンポーネント群 (またはその祖先) が属する部位を、HPに関係なく一括で一撃破壊する
	// 神技 (一閃) のように物理ヒット情報を持たず、ロックオン対象から部位を特定したい経路で使う
	// 連動 (combo) 成立を正しく判定させるため、
	// 通知前に対象を全て破壊状態にしてからまとめて通知する
	// (1つずつ破壊すると連動が成立する前に個別モンタージュが流れてしまう)。1つでも破壊したらtrue
	bool DestroyPartsByComponents(const TArray<USceneComponent*>& HitComponents);

	int32 GetPartCount() const { return Parts.Num(); }
	FName GetPartTag(int32 Index) const;
	bool IsPartDestroyedByIndex(int32 Index) const;

	// BossPhaseComponent等が購読する
	FOnPartDestroyed OnPartDestroyed;

	// 連動グループ成立時に発火する (ComboIndexを渡す)
	FOnPartComboDestroyed OnPartComboDestroyed;

	// 部位が復活したとき発火する (PartTagを渡す)
	// 共通光輪システムが状態をDeployedへ戻すのに使う
	FOnPartRevived OnPartRevived;

private:

	UFUNCTION()
	void HandleDamageReceived(const FDamageInfo& DamageInfo);

	void DestroyPart(int32 Index);
	// 部位光輪の破片演出 (GCC破断・散布・フェード) を起動する
	// DestroyPart / 一括破壊から共用する
	void StartPartHaloBreakVisual(int32 Index);
	// 部位光輪の破片GCCを新規生成して登録する (HaloBreakGC未設定ならnullptr)
	// Chaos GCは一度破断するとクラスタ状態を初期へ戻せないため、
	// 破壊のたびに作り直してリセットする
	UGeometryCollectionComponent* CreatePartHaloGCC(int32 Index);
	// HitComponent (またはその祖先) が属する部位インデックスを返す
	// なければINDEX_NONE
	int32 FindPartIndexForComponent(const USceneComponent* HitComponent) const;
	void RevivePart(int32 Index);
	// 破壊後BreakRegenCooldown秒で部位を個別復活させるタイマーを仕込む
	// (0以下なら自動復活しない)
	void SchedulePartRegen(int32 Index);
	void ApplyPartHaloScatterForce(int32 PartIndex);
	void StartPartHaloBreakFade(int32 PartIndex);

	// 部位のヒビ状態を光輪マテリアルへ反映する
	void UpdatePartHaloCrack(int32 Index);

	// 部位の攻撃ヒット検出 (DamageLayerタグ) を一括で有効/無効化する
	// コリジョン自体は残すのでPawnブロックは維持される
	void SetPartHitDetectionEnabled(int32 Index, bool bEnabled);

	// 全部位のDamageLayerタグを現在の破壊状態から計算し直す。破壊・復活のたびに呼ぶ
	// 破壊時にしか評価しないと、兄弟部位が後から壊れたり復活したりしても状態が古いまま残る
	void RefreshPartHitDetection();

	// 破壊されていない光輪の発光 (AttackGlow) を一括でON/OFFする (無敵化はしない)
	void SetPartHalosGlow(bool bGlow);

	// 復帰接触ダメージの1フレーム判定
	// (bRecoveryTouchActiveの間だけTickComponentから呼ぶ)
	void TickRecoveryTouchDamage(float DeltaTime);

	// 部位のダメージコリジョン (PartTagを持つプリミティブ)
	// 破壊されても "DamageLayer" タグは原則そのまま残し、壊れた部位も殴れる状態に保つ
	// (コリジョンがあるのに攻撃だけ素通りするのを防ぐ)
	// 外すのはHitAbsorbGroupに生きた部位が残っている間だけで、
	// 重なった生きた光輪へのヒットを壊れた部位が吸わないようにする
	// コリジョン自体は触らないのでPawnブロックは常時維持される
	struct FPartCollisionCache
	{
		TWeakObjectPtr<UPrimitiveComponent> Comp;
		bool bHadDamageLayer = false;

	};

	struct FPartRuntimeData
	{
		FPartEntry Entry;
		bool bDestroyed = false;
		bool bCracked   = false;	// ヒビあり (次のヒットで破壊)

		TArray<FPartCollisionCache> CollisionComps;

	};

	TArray<FPartRuntimeData> Parts;

	// 連動破壊リアクション定義 (DataAssetからコピー)
	TArray<FPartComboReaction> Combos;

	// Partsと同インデックス。光輪なし部位はnull
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> PartHaloMeshComps;

	UPROPERTY()
	TArray<TObjectPtr<UGeometryCollectionComponent>> PartHaloGCComps;

	UPROPERTY()
	TArray<TObjectPtr<UFieldSystemComponent>> PartHaloFieldComps;

	struct FPartHaloRuntimeData
	{
		bool    bFading       = false;
		float   FadeElapsed   = 0.0f;
		bool    bReviving     = false;
		float   ReviveElapsed = 0.0f;
		FVector BreakCenter   = FVector::ZeroVector;
		FTimerHandle ScatterTimerHandle;
		FTimerHandle FadeTimerHandle;
		FTimerHandle RegenTimerHandle;	// 破壊後の個別復活タイマー
		bool         bRevivePending = false;	// タイマーは発火したが保留要因で復活を待っている

		// 攻撃中の一時非表示/再表示フェード
		bool    bHideFading     = false;
		float   HideElapsed     = 0.0f;
		float   HideFrom        = 0.0f;	// 開始DitherAlpha
		float   HideTo          = 0.0f;	// 目標DitherAlpha (1=消滅, 0=表示)
		float   CurrentDither   = 0.0f;	// 現在のDitherAlpha値 (中断時の開始値に使う)

		// ヒットしたが壊れなかったときの3軸ブレ
		FHaloShakeState HaloShake;

	};

	TArray<FPartHaloRuntimeData> PartHaloRuntimes;

	// 攻撃中(BeginPartHaloAttackGlow〜
	// RestorePartHalosAfterAttack)は除外部位以外のダメージを受け付けない
	bool bPartHaloSuppressed = false;

	// EPartRegenBlockReasonのビットマスク。0以外の間は復活を保留する
	uint8 RegenBlockMask = 0;

	// ダウン復帰中の接触ダメージ設定 (Initializeでコピー)
	FBossRecoveryTouch RecoveryTouch;

	// 復帰接触ダメージ窓が開いているか (TickComponentが判定を回す)
	bool bRecoveryTouchActive = false;

	// 対象アクターごとの再ヒットまでの残り秒。0以下でその対象へ再度ダメージを与える
	TMap<TWeakObjectPtr<AActor>, float> RecoveryTouchCooldowns;

	// VFX発生の全体マージン残り秒。0以下のときだけ次のヒットVFXを出す (多重起動防止)
	float RecoveryTouchVfxCooldown = 0.0f;

};
