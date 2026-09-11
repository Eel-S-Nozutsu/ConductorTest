// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "PRJ_TIDE_P0/Data/Enemy/EnemyDataAsset.h"
#include "EnemyBattleComponent.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnAttackEvent, FGameplayTag)

/**
 * 敵の戦闘状態を管理するコンポーネント
 * 攻撃CD管理・攻撃選択ロジックを集約し、BT側は薄いブリッジにとどめる
 */
UCLASS()
class PRJ_TIDE_P0_API UEnemyBattleComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	// AEnemyCharacter::BeginPlayから呼ぶ
	// AttackTableOverrideを渡すとDAのAttackTableの代わりに使う
	// ※レベル配置インスタンスだけ攻撃テーブルを丸ごと差し替える用
	void Initialize(const UEnemyDataAsset* InData, const UDataTable* AttackTableOverride = nullptr);

	// 距離・角度チェック・CD・重み付き抽選をまとめて実行選択できればtrue、
	// OutAttackIndexに選んだインデックスを書き込む
	bool TryPickAttack(float DistToTarget, float AngleToTarget, int32& OutAttackIndex);

	// UEnemyAIState_Attackがモンタージュ終了時に呼ぶ (CDはここから開始)
	// bReleaseToken=falseはトークンを取得しなかったforce攻撃用
	void RecordAttackEnd(int32 AttackIndex, bool bReleaseToken = true);

	// AnimNotify_AttackEventから呼ぶ
	// OnAttackEventデリゲートへブロードキャスト
	void DispatchAttackEvent(FGameplayTag Tag);

	// デリゲート購読口 (BossPhaseComponent・召喚ハンドラ等がBindする)
	FOnAttackEvent OnAttackEvent;

	// BossPhaseComponentからフェーズ移行時に呼ぶ
	// TryPickAttackのフィルターに使用
	void SetCurrentPhase(int32 InPhaseIndex)  { CurrentPhaseIndex = InPhaseIndex; }

	// HitReactionTagに対応する反撃攻撃インデックス。なければ-1
	int32 FindCounterAttackIndex(FGameplayTag HitReactionTag) const;

	// RowNameに一致する攻撃インデックス。なければ-1
	// 協調攻撃でリーダーが協力者エントリをindex順に依らず指名する用
	int32 FindAttackIndexByRowName(FName RowName) const;

	// 現在の距離・角度でCDが完了した攻撃があるか。ガード解除判定用 ※トークンチェックなし
	bool HasReadyAttack(float DistToTarget, float AngleToTarget) const;

	// インデックスに対応する攻撃エントリ。範囲外ならnullptr
	const FAttackEntry* GetAttackEntry(int32 Index) const;

	// 指定距離がその攻撃の距離グループに入っているか。距離不明 (負値 = ターゲットなし) は素通り
	bool IsWithinRangeGroups(const FAttackEntry& Attack, float DistToTarget) const;

	// 指定角度がその攻撃の角度条件 (正面側MaxAngleToTarget /
	// 背面側MinAngleToTarget) を満たすか
	// 角度不明 (負値 = ターゲットなし) は素通り
	bool IsWithinAngleLimits(const FAttackEntry& Attack, float AngleToTarget) const;

	// 距離グループ境界。デバッグ表示用
	float GetNearMidBoundary() const { return CachedNearMidBoundary; }
	float GetMidFarBoundary() const  { return CachedMidFarBoundary; }

	// デバッグ用アクセサ
	int32 GetAttackCount() const;
	FString GetAttackName(int32 Index) const;
	float GetCooldownRemaining(int32 Index) const;  // 残りCD秒 (0 = 使用可能)
	float GetCooldownTotal(int32 Index) const;
	FString GetAttackRangeGroupLabel(int32 Index) const;  // "近中" のような距離グループ表示
	float GetGlobalCooldownRemaining() const;
	float GetGlobalCooldownTotal() const;

	// DataAsset。未初期化時はnullptr
	const UEnemyDataAsset* GetDataAsset() const { return Data; }

	// 抽選に使う攻撃リスト (デバッグ描画用)
	const TArray<FAttackEntry>& GetCachedAttacks() const { return CachedAttacks; }

	// DataAssetのAI設定。未初期化時はnullptr
	const FEnemyAISettings* GetAISettings() const { return Data ? &Data->AISettings : nullptr; }

	// デバッグ: 抽選有効フラグ (ImGuiチェックボックスから操作)
	bool IsAttackEnabled(int32 Index) const;
	void SetAttackEnabled(int32 Index, bool bEnabled);

private:

	UPROPERTY()
	TObjectPtr<const UEnemyDataAsset> Data;

	// 抽選に使う攻撃リスト。InitializeでDataTableかインラインAttacksから構築する
	UPROPERTY()
	TArray<FAttackEntry> CachedAttacks;

	// インデックス対応のCDタイムスタンプ
	TArray<float> AttackLastUsedTimes;

	// デバッグ用: 抽選から除外するフラグ (Initialize時に全1)
	// TArray<bool>のビットパック問題を避けuint8で保持
	TArray<uint8> AttackEnabledFlags;

	// グローバルCD: 攻撃終了時刻 (初期値は十分な過去時刻)
	float GlobalAttackLastUsedTime = -1e9f;

	// 現在フェーズ番号。雑魚敵は常に0。BossPhaseComponentが更新する
	int32 CurrentPhaseIndex = 0;

	// 直前に使用した攻撃インデックス (-1 = 未使用)。同グループ内での連続選択防止に使用
	int32 LastUsedAttackIndex = -1;

	// RangeProfileから解決した距離グループ境界。Initializeで設定
	float CachedNearMidBoundary = 200.0f;
	float CachedMidFarBoundary  = 400.0f;

	// 現在距離から抽選対象となる距離グループのビットマスク
	int32 GetAcceptableRangeMask(float DistToTarget) const;
	bool MeetsPlayerCameraVisibilityRequirement(const FAttackEntry& Attack, const AEnemyCharacter* Enemy) const;

};
