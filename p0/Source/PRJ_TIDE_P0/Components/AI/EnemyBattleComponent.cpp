// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/Components/AI/EnemyBattleComponent.h"
#include "PRJ_TIDE_P0/AI/Director/AIDirector.h"
#include "PRJ_TIDE_P0/Actors/Character/Enemy/EnemyCharacter.h"
#include "PRJ_TIDE_P0/Core/TideGameSettings.h"

void UEnemyBattleComponent::Initialize(const UEnemyDataAsset* InData, const UDataTable* AttackTableOverride)
{
	Data = InData;

	// 距離グループ境界プロフィールを解決 ※未設定時はデフォルト
	CachedNearMidBoundary = 200.0f;
	CachedMidFarBoundary  = 400.0f;
	if (Data && !Data->AISettings.RangeProfile.IsNull())
	{
		static const FString ProfileContext(TEXT("UEnemyBattleComponent::Initialize RangeProfile"));
		if (const FAttackRangeProfile* Profile = Data->AISettings.RangeProfile.GetRow<FAttackRangeProfile>(ProfileContext))
		{
			CachedNearMidBoundary = Profile->NearMidBoundary;
			CachedMidFarBoundary  = Profile->MidFarBoundary;
		}
	}

	CachedAttacks.Reset();
	// レベル配置の上書きがあればそれを、なければDAのAttackTableを使う ※丸ごと差し替え
	const UDataTable* AttackTable = AttackTableOverride ? AttackTableOverride : (Data ? Data->AttackTable.Get() : nullptr);
	if (AttackTable)
	{
		// AttackTableの行から攻撃リストを構築 ※行名=攻撃名
		static const FString Context(TEXT("UEnemyBattleComponent::Initialize"));
		const TArray<FName> RowNames = AttackTable->GetRowNames();
		CachedAttacks.Reserve(RowNames.Num());
		for (const FName& RowName : RowNames)
		{
			if (const FAttackEntry* Row = AttackTable->FindRow<FAttackEntry>(RowName, Context))
			{
				FAttackEntry Entry = *Row;
				Entry.RowName = RowName;
				CachedAttacks.Add(Entry);
			}
		}
	}

	AttackLastUsedTimes.SetNumZeroed(CachedAttacks.Num());
	AttackEnabledFlags.Init(1, CachedAttacks.Num());
}

bool UEnemyBattleComponent::MeetsPlayerCameraVisibilityRequirement(
	const FAttackEntry& Attack, const AEnemyCharacter* Enemy) const
{
	if (Attack.MinVisibleToPlayerCameraTime <= 0.0f) return true;
	if (!Enemy) return false;
	return Enemy->GetContinuousVisibleToPlayerCameraTime() >= Attack.MinVisibleToPlayerCameraTime;
}

bool UEnemyBattleComponent::TryPickAttack(float DistToTarget, float AngleToTarget, int32& OutAttackIndex)
{
	if (!Data || CachedAttacks.IsEmpty()) return false;

	AEnemyCharacter* Self = Cast<AEnemyCharacter>(GetOwner());

	// ターゲットへの上下角 (絶対値)。MaxPitchToTargetで真下/真上を除外する判定に使う
	const float PitchToTarget = Self ? Self->GetPitchAngleToTarget() : -1.0f;

	// トークンの空き状況。トークン無視攻撃
	// (bIgnoreAttackToken)はこれに関係なく候補に含める(ExecuteTaskで失敗
	// →即再抽選によるガタつき防止のため、
	// 通常攻撃は空きが無ければ候補外)抽選に来た時点で「攻撃したい」意思表示なので、
	// 距離つきで入札してから空きを問い合わせる
	// トークンは先着順ではなく最もターゲットに近い入札者に与えられ、遠い敵はストレイフに回る
	bool bHasToken = true;
	if (UAIDirector* Director = GetWorld()->GetSubsystem<UAIDirector>())
	{
		// 持続攻撃はTokenHoldTimeで途中からトークンを手放す。その間に本人が入札して
		// 自分の枠を取り直さないよう、攻撃実行中は入札しない
		if (Self && !Self->IsExecutingAttack())
		{
			Director->SubmitAttackBid(Self, DistToTarget);
		}
		bHasToken = Director->HasTokenAvailable(Self);
	}

	const float Now = GetWorld()->GetTimeSeconds();

	// グローバルCDの状態。bIgnoreGlobalCooldownの攻撃はこれを無視する
	const float GlobalCD = Data->AISettings.GlobalAttackCooldown;
	const bool bGlobalCDActive = GlobalCD > 0.0f && (Now - GlobalAttackLastUsedTime) < GlobalCD;

	// デバッグ: 個別クールダウンを0扱い ※グローバルCDはそのまま
	bool bIgnoreIndividualCD = false;
#if !UE_BUILD_SHIPPING
	if (const UTideGameSettings* DebugSettings = UTideGameSettings::Get())
		bIgnoreIndividualCD = DebugSettings->bDebugEnemyNoAttackCooldown;
#endif

	// 現在距離から抽選対象の距離グループを決定する
	const int32 AcceptableMask = GetAcceptableRangeMask(DistToTarget);

	// 1. 候補を集めつつ最高Priorityを求める
	TArray<int32> ValidIndices;
	int32 MaxPriority = TNumericLimits<int32>::Min();

	for (int32 i = 0; i < CachedAttacks.Num(); ++i)
	{
		const FAttackEntry& Attack = CachedAttacks[i];
		if (!Attack.bEnabled) continue;
		if (Attack.CounterTriggerTag.IsValid()) continue;
		if (Attack.bCoordinatedOnly) continue;
		if (AttackEnabledFlags.IsValidIndex(i) && AttackEnabledFlags[i] == 0) continue;
		if (!MeetsPlayerCameraVisibilityRequirement(Attack, Self)) continue;

		const bool bInRange      = (Attack.RangeGroups & AcceptableMask) != 0;
		const bool bOffCooldown  = bIgnoreIndividualCD
			|| !AttackLastUsedTimes.IsValidIndex(i)
			|| (Now - AttackLastUsedTimes[i]) >= Attack.Cooldown;
		const bool bPhaseMatch   = (Attack.PhaseIndex < 0) || (Attack.PhaseIndex == CurrentPhaseIndex);
		// 角度不明 (ターゲットなし) の時は素通りさせる。攻撃開始時の再チェックと同じ判定を使う
		const bool bInAngle      = IsWithinAngleLimits(Attack, AngleToTarget);
		// 上下角 (ピッチ) フィルタ。真下/真上を除外する。ターゲットなし (-1) は素通り
		const bool bInPitch      = PitchToTarget < 0.0f
			|| Attack.MaxPitchToTarget <= 0.0f
			|| PitchToTarget <= Attack.MaxPitchToTarget;
		const bool bStateAllowed = !Self->HasAnyStateTag(Attack.BlockedStateTags);
		const bool bGlobalOk     = !bGlobalCDActive || Attack.bIgnoreGlobalCooldown;
		const bool bTokenOk      = bHasToken || Attack.bIgnoreAttackToken;
		// 攻撃ロジック固有の発動可否 (例: 部位破壊による使用制限)。CDOへ問い合わせる
		const bool bActivatable  = !Attack.ExecutionClass
			|| Attack.ExecutionClass.GetDefaultObject()->CanActivate(Self);

		if (bInRange && bOffCooldown && bPhaseMatch && bInAngle && bInPitch && bStateAllowed && bGlobalOk && bTokenOk && bActivatable)
		{
			ValidIndices.Add(i);
			MaxPriority = FMath::Max(MaxPriority, Attack.Priority);
		}
	}

	if (ValidIndices.IsEmpty()) return false;

	// 2. 最高Priorityティアのみ残す (好機攻撃が通常攻撃を押しのける)
	ValidIndices.RemoveAll([&](int32 Idx) { return CachedAttacks[Idx].Priority < MaxPriority; });

	// 3. 連続選択防止: 他に候補があれば直前と同じ攻撃を除外する
	if (ValidIndices.Num() > 1)
	{
		ValidIndices.Remove(LastUsedAttackIndex);
	}

	// 4. 重み付き抽選
	float TotalWeight = 0.0f;
	for (int32 i : ValidIndices)
	{
		TotalWeight += CachedAttacks[i].Weight;
	}

	float Roll = FMath::FRandRange(0.0f, TotalWeight);
	float Accumulated = 0.0f;
	OutAttackIndex = ValidIndices.Last();

	for (int32 i : ValidIndices)
	{
		Accumulated += CachedAttacks[i].Weight;
		if (Roll < Accumulated)
		{
			OutAttackIndex = i;
			break;
		}
	}

	return true;
}

bool UEnemyBattleComponent::IsWithinRangeGroups(const FAttackEntry& Attack, float DistToTarget) const
{
	// 距離不明 (ターゲットなし) は素通りさせる
	if (DistToTarget < 0.0f) return true;

	return (Attack.RangeGroups & GetAcceptableRangeMask(DistToTarget)) != 0;
}

bool UEnemyBattleComponent::IsWithinAngleLimits(const FAttackEntry& Attack, float AngleToTarget) const
{
	// 角度不明 (ターゲットなし) は素通りさせる
	if (AngleToTarget < 0.0f) return true;

	// 正面側は「以内」、背面側は「以上」。0 = 無制限
	const bool bFrontOk = Attack.MaxAngleToTarget <= 0.0f || AngleToTarget <= Attack.MaxAngleToTarget;
	return bFrontOk && AngleToTarget >= Attack.MinAngleToTarget;
}

int32 UEnemyBattleComponent::GetAcceptableRangeMask(float DistToTarget) const
{
	// 現在距離に対応する単一の距離グループを返す。攻撃はそのグループを含む場合のみ抽選される
	// (近距離→近 / 中距離→中 / 遠距離→遠)
	if (DistToTarget < CachedNearMidBoundary) return 1 << static_cast<int32>(EAttackRangeGroup::Near);
	if (DistToTarget < CachedMidFarBoundary)  return 1 << static_cast<int32>(EAttackRangeGroup::Mid);
	return 1 << static_cast<int32>(EAttackRangeGroup::Far);
}

void UEnemyBattleComponent::RecordAttackEnd(int32 AttackIndex, bool bReleaseToken)
{
	const float Now = GetWorld()->GetTimeSeconds();
	if (AttackLastUsedTimes.IsValidIndex(AttackIndex))
	{
		AttackLastUsedTimes[AttackIndex] = Now;

		// CD共有グループの攻撃も同時に伏せる。1つの攻撃を人数違いなどで複数行に分けたとき、
		// 片方を撃った直後にもう片方が撃ててしまうのを防ぐ
		const FName Group = CachedAttacks[AttackIndex].CooldownGroup;
		if (!Group.IsNone())
		{
			for (int32 i = 0; i < CachedAttacks.Num(); ++i)
			{
				if (CachedAttacks[i].CooldownGroup == Group)
				{
					AttackLastUsedTimes[i] = Now;
				}
			}
		}
	}
	GlobalAttackLastUsedTime = Now;
	LastUsedAttackIndex = AttackIndex;

	if (bReleaseToken)
	{
		AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(GetOwner());
		if (UAIDirector* Director = GetWorld()->GetSubsystem<UAIDirector>())
		{
			if (Enemy) Director->ReleaseAttackToken(Enemy);
		}
	}
}

void UEnemyBattleComponent::DispatchAttackEvent(FGameplayTag Tag)
{
	OnAttackEvent.Broadcast(Tag);
}

bool UEnemyBattleComponent::HasReadyAttack(float DistToTarget, float AngleToTarget) const
{
	if (!Data || CachedAttacks.IsEmpty()) return false;

	AEnemyCharacter* Self = Cast<AEnemyCharacter>(GetOwner());
	const float Now = GetWorld()->GetTimeSeconds();

	// ターゲットへの上下角 (絶対値)。MaxPitchToTargetで真下/真上を除外する判定に使う
	const float PitchToTarget = Self ? Self->GetPitchAngleToTarget() : -1.0f;

	// グローバルCDの状態。bIgnoreGlobalCooldownの攻撃はこれを無視する
	const float GlobalCD = Data->AISettings.GlobalAttackCooldown;
	const bool bGlobalCDActive = GlobalCD > 0.0f && (Now - GlobalAttackLastUsedTime) < GlobalCD;

	// デバッグ: 個別クールダウンを0扱い ※グローバルCDはそのまま
	bool bIgnoreIndividualCD = false;
#if !UE_BUILD_SHIPPING
	if (const UTideGameSettings* DebugSettings = UTideGameSettings::Get())
		bIgnoreIndividualCD = DebugSettings->bDebugEnemyNoAttackCooldown;
#endif

	// 現在距離から抽選対象の距離グループを決定する
	const int32 AcceptableMask = GetAcceptableRangeMask(DistToTarget);

	for (int32 i = 0; i < CachedAttacks.Num(); ++i)
	{
		const FAttackEntry& Attack = CachedAttacks[i];
		if (!Attack.bEnabled) continue;
		if (Attack.CounterTriggerTag.IsValid()) continue;
		if (Attack.bCoordinatedOnly) continue;
		if (AttackEnabledFlags.IsValidIndex(i) && AttackEnabledFlags[i] == 0) continue;
		if (!MeetsPlayerCameraVisibilityRequirement(Attack, Self)) continue;

		const bool bInRange      = (Attack.RangeGroups & AcceptableMask) != 0;
		const bool bOffCooldown  = bIgnoreIndividualCD
			|| !AttackLastUsedTimes.IsValidIndex(i)
			|| (Now - AttackLastUsedTimes[i]) >= Attack.Cooldown;
		const bool bPhaseMatch   = (Attack.PhaseIndex < 0) || (Attack.PhaseIndex == CurrentPhaseIndex);
		// 角度不明 (ターゲットなし) の時は素通りさせる。
		// 抽選・攻撃開始時の再チェックと同じ判定を使う
		const bool bInAngle      = IsWithinAngleLimits(Attack, AngleToTarget);
		// 上下角 (ピッチ) フィルタ。真下/真上を除外する。ターゲットなし (-1) は素通り
		const bool bInPitch      = PitchToTarget < 0.0f
			|| Attack.MaxPitchToTarget <= 0.0f
			|| PitchToTarget <= Attack.MaxPitchToTarget;
		const bool bStateAllowed = Self && !Self->HasAnyStateTag(Attack.BlockedStateTags);
		const bool bGlobalOk     = !bGlobalCDActive || Attack.bIgnoreGlobalCooldown;
		// 攻撃ロジック固有の発動可否 (例: 部位破壊による使用制限)。CDOへ問い合わせる
		const bool bActivatable  = !Attack.ExecutionClass
			|| Attack.ExecutionClass.GetDefaultObject()->CanActivate(Self);

		if (bInRange && bOffCooldown && bPhaseMatch && bInAngle && bInPitch && bStateAllowed && bGlobalOk && bActivatable)
			return true;
	}
	return false;
}

int32 UEnemyBattleComponent::FindCounterAttackIndex(FGameplayTag HitReactionTag) const
{
	if (!HitReactionTag.IsValid()) return -1;
	const AEnemyCharacter* Self = Cast<AEnemyCharacter>(GetOwner());

	for (int32 i = 0; i < CachedAttacks.Num(); ++i)
	{
		if (!CachedAttacks[i].bEnabled) continue;
		if (!MeetsPlayerCameraVisibilityRequirement(CachedAttacks[i], Self)) continue;
		if (CachedAttacks[i].CounterTriggerTag == HitReactionTag)
			return i;
	}
	return -1;
}

int32 UEnemyBattleComponent::FindAttackIndexByRowName(FName RowName) const
{
	for (int32 i = 0; i < CachedAttacks.Num(); ++i)
	{
		if (CachedAttacks[i].RowName == RowName) return i;
	}
	return -1;
}

const FAttackEntry* UEnemyBattleComponent::GetAttackEntry(int32 Index) const
{
	if (!CachedAttacks.IsValidIndex(Index)) return nullptr;
	return &CachedAttacks[Index];
}

int32 UEnemyBattleComponent::GetAttackCount() const
{
	return CachedAttacks.Num();
}

FString UEnemyBattleComponent::GetAttackName(int32 Index) const
{
	if (CachedAttacks.IsValidIndex(Index) && !CachedAttacks[Index].RowName.IsNone())
		return CachedAttacks[Index].RowName.ToString();
	return FString::Printf(TEXT("Attack %d"), Index);
}

float UEnemyBattleComponent::GetCooldownRemaining(int32 Index) const
{
	if (!CachedAttacks.IsValidIndex(Index) || !AttackLastUsedTimes.IsValidIndex(Index))
		return 0.0f;
	const float Elapsed = GetWorld()->GetTimeSeconds() - AttackLastUsedTimes[Index];
	return FMath::Max(0.0f, CachedAttacks[Index].Cooldown - Elapsed);
}

float UEnemyBattleComponent::GetCooldownTotal(int32 Index) const
{
	return CachedAttacks.IsValidIndex(Index) ? CachedAttacks[Index].Cooldown : 0.0f;
}

FString UEnemyBattleComponent::GetAttackRangeGroupLabel(int32 Index) const
{
	if (!CachedAttacks.IsValidIndex(Index)) return TEXT("-");

	const int32 Groups = CachedAttacks[Index].RangeGroups;
	FString Label;
	if (Groups & (1 << static_cast<int32>(EAttackRangeGroup::Near))) Label += TEXT("近");
	if (Groups & (1 << static_cast<int32>(EAttackRangeGroup::Mid)))  Label += TEXT("中");
	if (Groups & (1 << static_cast<int32>(EAttackRangeGroup::Far)))  Label += TEXT("遠");
	return Label.IsEmpty() ? TEXT("-") : Label;
}

float UEnemyBattleComponent::GetGlobalCooldownRemaining() const
{
	if (!Data || Data->AISettings.GlobalAttackCooldown <= 0.0f) return 0.0f;
	const float Elapsed = GetWorld()->GetTimeSeconds() - GlobalAttackLastUsedTime;
	return FMath::Max(0.0f, Data->AISettings.GlobalAttackCooldown - Elapsed);
}

float UEnemyBattleComponent::GetGlobalCooldownTotal() const
{
	return Data ? Data->AISettings.GlobalAttackCooldown : 0.0f;
}

bool UEnemyBattleComponent::IsAttackEnabled(int32 Index) const
{
	return AttackEnabledFlags.IsValidIndex(Index) && AttackEnabledFlags[Index] != 0;
}

void UEnemyBattleComponent::SetAttackEnabled(int32 Index, bool bEnabled)
{
	if (AttackEnabledFlags.IsValidIndex(Index))
		AttackEnabledFlags[Index] = bEnabled ? 1 : 0;
}
