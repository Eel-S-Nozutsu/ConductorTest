// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/AI/Director/AIDirector.h"
#include "PRJ_TIDE_P0/Actors/Character/Enemy/EnemyCharacter.h"

void UAIDirector::SubmitAttackBid(AEnemyCharacter* Bidder, float DistToTarget)
{
	if (!Bidder) return;

	PruneBids();

	FAttackBid& Bid = AttackBids.FindOrAdd(Bidder);
	Bid.DistToTarget = DistToTarget;
	Bid.SubmitTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
}

void UAIDirector::PruneBids()
{
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	for (auto It = AttackBids.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid() || (Now - It.Value().SubmitTime) > BidLifetime)
		{
			It.RemoveCurrent();
		}
	}
}

void UAIDirector::PruneAttackers()
{
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	ActiveAttackers.RemoveAllSwap([Now](const FActiveAttacker& A)
	{
		if (!A.Enemy.IsValid()) return true;
		return A.ExpireTime > 0.0f && Now >= A.ExpireTime;
	});
}

bool UAIDirector::IsClosestBidder(const AEnemyCharacter* Requester) const
{
	if (!Requester) return false;

	const FAttackBid* MyBid = AttackBids.Find(Requester);
	if (!MyBid) return false;

	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	if ((Now - MyBid->SubmitTime) > BidLifetime) return false;

	for (const auto& [WeakEnemy, Bid] : AttackBids)
	{
		const AEnemyCharacter* Rival = WeakEnemy.Get();
		if (!Rival || Rival == Requester) continue;
		if ((Now - Bid.SubmitTime) > BidLifetime) continue;

		// 僅差では入れ替わらない ※マージンを超えて近い敵にだけ競り負ける
		if (Bid.DistToTarget < MyBid->DistToTarget - BidDistanceMargin) return false;
	}

	return true;
}

bool UAIDirector::TryAcquireAttackToken(AEnemyCharacter* Requester, float HoldTime)
{
	if (!Requester) return false;

	PruneAttackers();

	// 既にトークンを保持中 (AbortTask→再ExecuteTaskの即時再試行への対応)
	const bool bAlreadyHolds = ActiveAttackers.ContainsByPredicate(
		[Requester](const FActiveAttacker& A) { return A.Enemy.Get() == Requester; });
	if (bAlreadyHolds) return true;

	if (ActiveAttackers.Num() >= MaxSimultaneousAttackers) return false;

	// 抽選(TryPickAttack)からタスク実行までの間に、より近い敵が入札していた場合は譲る
	if (!IsClosestBidder(Requester)) return false;

	// 攻撃に入ったら入札は取り下げる ※攻撃中の敵の古い入札が他の敵を足止めしないようにする
	AttackBids.Remove(Requester);

	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	ActiveAttackers.Add({ Requester, HoldTime > 0.0f ? Now + HoldTime : 0.0f });
	return true;
}

void UAIDirector::ReleaseAttackToken(AEnemyCharacter* Requester)
{
	ActiveAttackers.RemoveAllSwap([Requester](const FActiveAttacker& A)
	{
		return !A.Enemy.IsValid() || A.Enemy.Get() == Requester;
	});
}

bool UAIDirector::HasTokenAvailable(const AEnemyCharacter* Requester) const
{
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	int32 ValidCount = 0;
	for (const FActiveAttacker& A : ActiveAttackers)
	{
		if (!A.Enemy.IsValid()) continue;
		if (A.Enemy.Get() == Requester) return true;  // 既に保持中
		if (A.ExpireTime > 0.0f && Now >= A.ExpireTime) continue;  // 保持時間切れ

		++ValidCount;
	}

	if (ValidCount >= MaxSimultaneousAttackers) return false;

	// 空きがあっても、より近い敵が攻撃したがっているなら譲る (遠い敵はストレイフに回る)
	return IsClosestBidder(Requester);
}

void UAIDirector::RegisterCombatant(AEnemyCharacter* Enemy)
{
	if (!Enemy) return;

	PruneCombatants();
	Combatants.Add(Enemy);
}

void UAIDirector::UnregisterCombatant(AEnemyCharacter* Enemy)
{
	if (!Enemy) return;
	Combatants.Remove(Enemy);
}

void UAIDirector::PruneCombatants()
{
	for (auto It = Combatants.CreateIterator(); It; ++It)
	{
		if (!It->IsValid()) It.RemoveCurrent();
	}
}

void UAIDirector::RegisterDesiredPosition(AEnemyCharacter* Enemy, FVector Position)
{
	if (Enemy) DesiredPositions.Add(Enemy, Position);
}

void UAIDirector::UnregisterDesiredPosition(AEnemyCharacter* Enemy)
{
	if (!Enemy) return;
	DesiredPositions.Remove(Enemy);
}

float UAIDirector::GetOccupancyScore(FVector Position, AEnemyCharacter* Requester, float Radius) const
{
	const float EffRadius = (Radius > 0.0f) ? Radius : SlotRadius;

	// 半径内で最も近い登録位置が減点を決める ※最初の一致ではなく最近傍
	float Score = 1.0f;
	for (const auto& [WeakEnemy, DesiredPos] : DesiredPositions)
	{
		AEnemyCharacter* Enemy = WeakEnemy.Get();
		if (!Enemy || Enemy == Requester) continue;

		const float Dist = FVector::Dist2D(Position, DesiredPos);
		if (Dist < EffRadius)
		{
			// 距離が近いほど減点
			Score = FMath::Min(Score, FMath::Clamp(Dist / EffRadius, 0.0f, 1.0f));
		}
	}
	return Score;
}

TArray<FVector> UAIDirector::GetLaneAnchors(const AEnemyCharacter* Requester) const
{
	TArray<FVector> Anchors;

	for (const auto& [WeakEnemy, DesiredPos] : DesiredPositions)
	{
		const AEnemyCharacter* Enemy = WeakEnemy.Get();
		if (!Enemy || Enemy == Requester) continue;

		Anchors.Add(DesiredPos);
	}

	// 攻撃中の敵は移動先を登録しないため、現在位置を別途拾う
	for (const FActiveAttacker& A : ActiveAttackers)
	{
		AEnemyCharacter* Enemy = A.Enemy.Get();
		if (!Enemy || Enemy == Requester) continue;
		if (DesiredPositions.Contains(Enemy)) continue;

		Anchors.Add(Enemy->GetActorLocation());
	}

	return Anchors;
}

TArray<AEnemyCharacter*> UAIDirector::GetCombatants(const AEnemyCharacter* Requester) const
{
	TArray<AEnemyCharacter*> Result;

	for (const TWeakObjectPtr<AEnemyCharacter>& WeakEnemy : Combatants)
	{
		AEnemyCharacter* Enemy = WeakEnemy.Get();
		if (!Enemy || Enemy == Requester) continue;

		Result.Add(Enemy);
	}
	return Result;
}

TArray<FVector> UAIDirector::GetCombatantLocations(const AEnemyCharacter* Requester) const
{
	TArray<FVector> Locations;
	for (AEnemyCharacter* Enemy : GetCombatants(Requester))
	{
		Locations.Add(Enemy->GetActorLocation());
	}
	return Locations;
}

bool UAIDirector::ReserveCoordinator(AEnemyCharacter* Coop, AEnemyCharacter* Leader, FVector SlotLocation,
	float SlotYaw)
{
	if (!Coop || !Leader) return false;

	// 予約の寿命秒数。指名→協力者が攻撃開始までのラグをカバーする短めの値
	// 着地後はMarkCoordinatorArrivedが延長する
	const float CoordinationLifetime = 5.0f;
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	// 既に生きている別リーダーの有効な予約があれば取り合いにしない
	if (const FCoordinationSlot* Existing = CoordinationReservations.Find(Coop))
	{
		if (Existing->Leader.IsValid() && Existing->Leader.Get() != Leader && Now < Existing->ExpireTime)
			return false;
	}

	FCoordinationSlot Slot;
	Slot.Leader = Leader;
	Slot.SlotLocation = SlotLocation;
	Slot.ExpireTime = Now + CoordinationLifetime;
	Slot.SlotYaw = SlotYaw;
	CoordinationReservations.Add(Coop, Slot);
	return true;
}

bool UAIDirector::GetCoordinationSlotYaw(AEnemyCharacter* Coop, float& OutSlotYaw) const
{
	const FCoordinationSlot* Slot = CoordinationReservations.Find(Coop);
	if (!Slot) return false;
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	if (Now >= Slot->ExpireTime) return false;
	OutSlotYaw = Slot->SlotYaw;
	return true;
}

AEnemyCharacter* UAIDirector::GetCoordinationLeader(AEnemyCharacter* Coop) const
{
	const FCoordinationSlot* Slot = CoordinationReservations.Find(Coop);
	if (!Slot) return nullptr;
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	if (Now >= Slot->ExpireTime) return nullptr;
	return Slot->Leader.Get();
}

void UAIDirector::MarkCoordinatorArrived(AEnemyCharacter* Coop, float ExtraLifetime)
{
	FCoordinationSlot* Slot = CoordinationReservations.Find(Coop);
	if (!Slot) return;

	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	Slot->bArrived = true;
	Slot->ExpireTime = FMath::Max(Slot->ExpireTime, Now + ExtraLifetime);
}

TArray<AEnemyCharacter*> UAIDirector::GetCoordinators(const AEnemyCharacter* Leader) const
{
	TArray<AEnemyCharacter*> Result;
	if (!Leader) return Result;

	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	for (const auto& Pair : CoordinationReservations)
	{
		if (Pair.Value.Leader.Get() != Leader) continue;
		if (Now >= Pair.Value.ExpireTime) continue;
		if (AEnemyCharacter* Coop = Pair.Key.Get())
		{
			Result.Add(Coop);
		}
	}
	return Result;
}

bool UAIDirector::AreAllCoordinatorsArrived(const AEnemyCharacter* Leader) const
{
	if (!Leader) return true;

	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	for (const auto& Pair : CoordinationReservations)
	{
		if (Pair.Value.Leader.Get() != Leader) continue;
		if (Now >= Pair.Value.ExpireTime) continue;
		// 死亡・破棄された協力者は待たない (待つと全員がスタンバイのまま固まる)
		if (!Pair.Key.IsValid()) continue;
		if (!Pair.Value.bArrived) return false;
	}
	return true;
}

bool UAIDirector::IsCoordinatorArrived(AEnemyCharacter* Coop) const
{
	const FCoordinationSlot* Slot = CoordinationReservations.Find(Coop);
	if (!Slot) return false;
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	return Now < Slot->ExpireTime && Slot->bArrived;
}

void UAIDirector::SetCoordinatorFireDelay(AEnemyCharacter* Coop, float Delay)
{
	if (FCoordinationSlot* Slot = CoordinationReservations.Find(Coop))
	{
		Slot->FireDelay = Delay;
	}
}

bool UAIDirector::GetCoordinatorFireDelay(AEnemyCharacter* Coop, float& OutDelay) const
{
	const FCoordinationSlot* Slot = CoordinationReservations.Find(Coop);
	if (!Slot || Slot->FireDelay < 0.0f) return false;
	OutDelay = Slot->FireDelay;
	return true;
}

bool UAIDirector::IsCoordinationReserved(AEnemyCharacter* Coop) const
{
	const FCoordinationSlot* Slot = CoordinationReservations.Find(Coop);
	if (!Slot || !Slot->Leader.IsValid()) return false;
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	return Now < Slot->ExpireTime;
}

bool UAIDirector::GetCoordinationSlot(AEnemyCharacter* Coop, FVector& OutSlot) const
{
	const FCoordinationSlot* Slot = CoordinationReservations.Find(Coop);
	if (!Slot) return false;
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	if (Now >= Slot->ExpireTime) return false;
	OutSlot = Slot->SlotLocation;
	return true;
}

void UAIDirector::ReleaseCoordinator(AEnemyCharacter* Coop)
{
	CoordinationReservations.Remove(Coop);
}

int32 UAIDirector::GetCurrentAttackerCount() const
{
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	int32 Count = 0;
	for (const FActiveAttacker& A : ActiveAttackers)
	{
		if (!A.Enemy.IsValid()) continue;
		if (A.ExpireTime > 0.0f && Now >= A.ExpireTime) continue;

		++Count;
	}
	return Count;
}

TArray<AEnemyCharacter*> UAIDirector::GetActiveAttackers() const
{
	TArray<AEnemyCharacter*> Result;
	for (const FActiveAttacker& A : ActiveAttackers)
	{
		if (AEnemyCharacter* Enemy = A.Enemy.Get()) Result.Add(Enemy);
	}
	return Result;
}

TArray<TPair<AEnemyCharacter*, FVector>> UAIDirector::GetDesiredPositions() const
{
	TArray<TPair<AEnemyCharacter*, FVector>> Result;
	for (const auto& [WeakEnemy, Pos] : DesiredPositions)
	{
		if (AEnemyCharacter* Enemy = WeakEnemy.Get()) Result.Add({ Enemy, Pos });
	}
	return Result;
}

TArray<TPair<AEnemyCharacter*, float>> UAIDirector::GetActiveBids() const
{
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	TArray<TPair<AEnemyCharacter*, float>> Result;
	for (const auto& [WeakEnemy, Bid] : AttackBids)
	{
		AEnemyCharacter* Enemy = WeakEnemy.Get();
		if (!Enemy || (Now - Bid.SubmitTime) > BidLifetime) continue;

		Result.Add({ Enemy, Bid.DistToTarget });
	}

	Result.Sort([](const TPair<AEnemyCharacter*, float>& A, const TPair<AEnemyCharacter*, float>& B)
	{
		return A.Value < B.Value;
	});

	return Result;
}
