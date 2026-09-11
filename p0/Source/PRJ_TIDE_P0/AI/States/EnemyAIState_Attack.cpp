// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/AI/States/EnemyAIState_Attack.h"
#include "PRJ_TIDE_P0/AI/Components/Enemy/EnemyBrainComponent.h"
#include "PRJ_TIDE_P0/AI/Controllers/Enemy/EnemyAIController.h"
#include "PRJ_TIDE_P0/AI/Director/AIDirector.h"
#include "PRJ_TIDE_P0/Actors/Character/Enemy/EnemyCharacter.h"
#include "PRJ_TIDE_P0/Components/AI/EnemyBattleComponent.h"
#include "PRJ_TIDE_P0/Data/Enemy/AttackExecution/TideAttackExecution.h"
#include "PRJ_TIDE_P0/Data/Enemy/EnemyDataAsset.h"

#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"

void UEnemyAIState_Attack::Enter(UEnemyBrainComponent& Brain)
{
	OwningBrain = &Brain;

	AEnemyCharacter* Enemy = Brain.GetEnemy();
	UEnemyBattleComponent* Battle = Brain.GetBattleComponent();
	const int32 AttackIndex = Brain.GetActiveAttackIndex();
	if (!Enemy || !Battle || AttackIndex < 0)
	{
		Brain.ConsumeActiveAttackRequest();
		return;
	}

	const FAttackEntry* EntryPtr = Battle->GetAttackEntry(AttackIndex);
	if (!EntryPtr || !EntryPtr->ExecutionClass)
	{
		// 無効なインデックスのまま毎ティック選び直されないよう取り下げる
		Brain.ConsumeActiveAttackRequest();
		return;
	}
	const FAttackEntry& Entry = *EntryPtr;

	// 当選はReactやカットシーン、強制攻撃の裏などで保留され、
	// その間条件が凍結されたまま持ち越される
	// 発動直前にもう一度、抽選時と同じ判定で距離グループと角度を確かめ、
	// 外れていたら不発にして次ティックの抽選へ戻す
	// (条件を満たさなくなった技をそのまま出す理由はないので常に行う)
	// 例外は「条件を無視して撃つ」約束のもの: 強制攻撃 (指名/デバッグ) と反撃(反撃トリガータグ)
	// なお発動後のモーション中にPCが動くぶんは対象外(振り始めた技は途中で止めない)
	const bool bBypassRecheck = Brain.IsActiveAttackForced() || Entry.CounterTriggerTag.IsValid();
	if (!bBypassRecheck
		&& (!Battle->IsWithinRangeGroups(Entry, Enemy->GetDistToTarget())
			|| !Battle->IsWithinAngleLimits(Entry, Enemy->GetAngleToTarget())))
	{
		Brain.ConsumeActiveAttackRequest();
		return;
	}

	// 強制攻撃 (協力攻撃の徴集・デバッグ) と好機攻撃はトークン制限をスキップし、
	// 取得できた場合のみ返却する
	bTokenAcquired = false;
	if (UAIDirector* Director = Enemy->GetWorld()->GetSubsystem<UAIDirector>())
	{
		const bool bAcquired = Director->TryAcquireAttackToken(Enemy, Entry.TokenHoldTime);
		const bool bBypassToken = Brain.IsActiveAttackForced() || Entry.bIgnoreAttackToken;
		if (bBypassToken)
		{
			bTokenAcquired = bAcquired;
		}
		else if (!bAcquired)
		{
			Brain.ConsumeActiveAttackRequest();
			return;
		}
		else
		{
			bTokenAcquired = true;
		}
	}

	CachedEnemy = Enemy;
	ActiveAttackIndex = AttackIndex;
	bActive = true;
	bExecutionStarted = false;
	bAttackEndNotified = false;
	bTurningToTarget = false;

	// この攻撃のダメージ実数値を敵へキャッシュし、ANSと弾スポーンの双方が参照する
	Enemy->SetCurrentAttackDamage(Entry.Damage);
	Enemy->SetCurrentAttackHitReactionTag(Entry.HitReactionTag);
	Enemy->SetCurrentAttackFadeOutsideRange(Entry.bFadeProjectilesOutsideSpawnerRange);

	// 攻撃実行中フラグで予知回避などの割り込みを抑止 ※FinishAttackで確実に下ろす
	Enemy->SetExecutingAttack(true);

	AnimInstance = Enemy->GetMesh() ? Enemy->GetMesh()->GetAnimInstance() : nullptr;
	ActiveExecution = NewObject<UTideAttackExecution>(this, Entry.ExecutionClass);
	AttackMontage = ActiveExecution->GetMontage();

	// 前モーション (リアクション/復帰等) のブレンドアウト中に残っているCanTransitionを
	// 引き継いで初手で早期遷移するのを防ぐ。攻撃は自分のモンタージュが立てた窓だけを根拠にする
	Enemy->ClearCanTransition();

	// 攻撃本体の前にPCの方へ向き直るか判定する
	// 挟むなら向き直りモンタージュを再生し、その終了
	// (OnMontageEnded)でBeginExecutionへ繋ぐ
	// 既に正面付近or未設定なら即攻撃へ
	// 実際の回頭は向き直りモンタージュ側の「ターゲット方向へ回転」Notifyが駆動
	if (UAnimMontage* Turn = GetTurnToTargetMontage(Enemy, Entry))
	{
		// 向き直り中はfocusを触らない
		// (攻撃本体直前のBeginExecutionでClearFocusする)
		// Play → 登録の順はBeginExecutionと同じ理由
		// (旧モンタージュ同期停止での誤検知回避)。再生に成功したときだけ向き直りへ入る
		// 失敗 (長さ0等) なら攻撃本体へフォールバックし、
		// OnMontageEndedが来ずbTurningToTargetが立ちっぱなしで固まるのを防ぐ
		if (AnimInstance->Montage_Play(Turn) > 0.0f)
		{
			TurnMontage = Turn;
			bTurningToTarget = true;
			AnimInstance->OnMontageEnded.AddDynamic(this, &UEnemyAIState_Attack::OnMontageEnded);
			return;
		}
	}

	BeginExecution();
}

void UEnemyAIState_Attack::BeginExecution()
{
	AEnemyCharacter* Enemy = CachedEnemy;
	if (!bActive || !Enemy || !ActiveExecution) return;

	// 攻撃本体は自前で向きを制御する。向き直りで使ったfocusはここで落とす
	if (OwningBrain)
	{
		if (AEnemyAIController* AIController = OwningBrain->GetAIController())
		{
			AIController->ClearFocus(EAIFocusPriority::Gameplay);
		}
	}

	bExecutionStarted = true;

	// asyncモード (GetMontage() == nullptr) の完了コールバック
	// NewObject直後なのでFinishDelegateは必ず空から始まる
	ActiveExecution->FinishDelegate = [this](bool bSuccess)
	{
		if (!bActive) return;
		FinishAttack(/*bInterrupted=*/false);
	};

	ActiveExecution->OnAttackBegin(Enemy);

	// OnAttackBeginが同期的にFinishDelegateを呼んだ場合、ここで既に終わっている
	if (!bActive) return;

	// AnimNotify_AttackEventをExecutionへ転送する
	if (UEnemyBattleComponent* Battle = Enemy->GetBattleComponent())
	{
		Battle->OnAttackEvent.AddUObject(this, &UEnemyAIState_Attack::ForwardAttackEvent);
	}

	if (!AttackMontage || !AnimInstance) return;

	// Montage_Playを先に呼ぶ: Play内で同スロットの旧モンタージュ (向き直り含む)
	// が同期停止しOnMontageEndedが発火することがある。デリゲート登録前なら誤検知しない
	AnimInstance->Montage_Play(AttackMontage);
	AnimInstance->OnMontageEnded.AddDynamic(this, &UEnemyAIState_Attack::OnMontageEnded);
}

UAnimMontage* UEnemyAIState_Attack::GetTurnToTargetMontage(AEnemyCharacter* Enemy, const FAttackEntry& Entry) const
{
	if (!Entry.bTurnToTargetBeforeAttack || !AnimInstance || !Enemy) return nullptr;

	const UEnemyDataAsset* Data = Cast<UEnemyDataAsset>(Enemy->GetCharacterData());
	if (!Data || !Data->AISettings.TurnToTargetMontage) return nullptr;

	const AActor* Target = Enemy->GetTargetActor();
	if (!Target) return nullptr;

	const FVector ToTarget = (Target->GetActorLocation() - Enemy->GetActorLocation()).GetSafeNormal2D();
	if (ToTarget.IsNearlyZero()) return nullptr;

	// PCが向きから十分ズレているときだけ向き直る (正面付近ならテンポ優先で即攻撃)
	const float TargetYaw = ToTarget.Rotation().Yaw;
	const float YawDiff = FMath::Abs(FRotator::NormalizeAxis(TargetYaw - Enemy->GetActorRotation().Yaw));
	return YawDiff > Data->AISettings.TurnToTargetThreshold ? Data->AISettings.TurnToTargetMontage.Get() : nullptr;
}

void UEnemyAIState_Attack::Tick(UEnemyBrainComponent& Brain, float DeltaSeconds)
{
	if (!bActive || !CachedEnemy) return;

	// 向き直りモーション中は攻撃本体がまだ始まっていない。Executionを触らず、早期遷移も見ない
	if (bTurningToTarget) return;

	if (ActiveExecution)
	{
		ActiveExecution->OnAttackTick(CachedEnemy, DeltaSeconds);
	}

	// asyncモードはFinishDelegateで完了するので早期遷移は見ない
	if (!AttackMontage) return;
	if (!bAllowEarlyTransitionByTag) return;

	if (CachedEnemy->CanTransitionToNextAction())
	{
		FinishAttack(/*bInterrupted=*/false);
	}
}

void UEnemyAIState_Attack::Exit(UEnemyBrainComponent& Brain)
{
	// 正常終了済みならbActiveはfalse。ここへ来るのは
	// リアクション・死亡・強制攻撃による割り込みだけ
	if (bActive)
	{
		FinishAttack(/*bInterrupted=*/true);
	}

	OwningBrain = nullptr;
}

void UEnemyAIState_Attack::FinishAttack(bool bInterrupted)
{
	if (!bActive) return;
	bActive = false;

	if (AnimInstance)
	{
		AnimInstance->OnMontageEnded.RemoveDynamic(this, &UEnemyAIState_Attack::OnMontageEnded);

		// 中断時のみモンタージュを止める。正常終了は自然にブレンドアウトさせる
		// 向き直り中の中断もあり得るので、再生中の可能性がある両方を止める
		if (bInterrupted)
		{
			if (AttackMontage) AnimInstance->Montage_Stop(0.2f, AttackMontage);
			if (TurnMontage)   AnimInstance->Montage_Stop(0.2f, TurnMontage);
		}
	}

	// asyncモードでExecutionが後からFinishDelegateを呼んでも無視されるようにす
	// る
	if (ActiveExecution)
	{
		ActiveExecution->FinishDelegate = nullptr;
	}

	NotifyAttackEnd(bInterrupted);

	if (CachedEnemy)
	{
		if (UEnemyBattleComponent* Battle = CachedEnemy->GetBattleComponent())
		{
			Battle->OnAttackEvent.RemoveAll(this);
			Battle->RecordAttackEnd(ActiveAttackIndex, bTokenAcquired);
		}

		// ダメージ・リアクションのキャッシュを次の攻撃へ持ち越さない
		CachedEnemy->ClearCurrentAttackDamage();
		CachedEnemy->ClearCurrentAttackHitReactionTag();
		CachedEnemy->ClearCurrentAttackFadeOutsideRange();
		CachedEnemy->SetExecutingAttack(false);
	}

	// 自分が選ばれる条件を無効化する。これで次ティックの優先度評価が別の状態へ移す
	if (OwningBrain)
	{
		OwningBrain->ConsumeActiveAttackRequest();
	}

	CachedEnemy = nullptr;
	AnimInstance = nullptr;
	AttackMontage = nullptr;
	TurnMontage = nullptr;
	ActiveExecution = nullptr;
	ActiveAttackIndex = -1;
	bExecutionStarted = false;
	bAttackEndNotified = false;
	bTurningToTarget = false;
	bTokenAcquired = false;
}

void UEnemyAIState_Attack::OnMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	// 向き直りモーションの終了。中断でなければ攻撃本体へ繋ぐ
	if (bTurningToTarget && Montage == TurnMontage)
	{
		bTurningToTarget = false;
		TurnMontage = nullptr;

		// BeginExecutionが攻撃モンタージュ再生後に登録し直すので、ここで一度外す
		if (AnimInstance)
		{
			AnimInstance->OnMontageEnded.RemoveDynamic(this, &UEnemyAIState_Attack::OnMontageEnded);
		}

		// 中断で止められた場合はExit→FinishAttackが処理する (bActiveで弾かれる)
		if (!bActive || bInterrupted) return;

		BeginExecution();
		return;
	}

	if (Montage != AttackMontage) return;

	// 中断で止められた場合はFinishAttackが既に走っている (bActiveで弾かれる)
	FinishAttack(/*bInterrupted=*/false);
}

void UEnemyAIState_Attack::NotifyAttackEnd(bool bInterrupted)
{
	if (bAttackEndNotified) return;
	if (!bExecutionStarted || !ActiveExecution || !CachedEnemy) return;

	bAttackEndNotified = true;
	ActiveExecution->bAttackInterrupted = bInterrupted;
	ActiveExecution->OnAttackEnd(CachedEnemy);
}

void UEnemyAIState_Attack::ForwardAttackEvent(FGameplayTag Tag)
{
	if (ActiveExecution && CachedEnemy)
	{
		ActiveExecution->OnAttackEvent(CachedEnemy, Tag);
	}
}

FString UEnemyAIState_Attack::GetDebugText() const
{
	return bActive ? FString::Printf(TEXT("#%d"), ActiveAttackIndex) : TEXT("-");
}
