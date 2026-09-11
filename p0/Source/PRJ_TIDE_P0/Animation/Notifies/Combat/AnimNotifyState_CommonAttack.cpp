// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/Animation/Notifies/Combat/AnimNotifyState_CommonAttack.h"
#include "PRJ_TIDE_P0/Interfaces/Combat/IDamageable.h"
#include "PRJ_TIDE_P0/Components/Combat/HaloSystemComponent.h"
#include "PRJ_TIDE_P0/Utilities/HitStopUtils.h"
#include "PRJ_TIDE_P0/Utilities/TideCombatUtils.h"
#include "PRJ_TIDE_P0/Actors/Character/TideCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"

#include "PRJ_TIDE_P0/Core/TideGameSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/OverlapResult.h"

namespace CommonAttack
{
	// Boxブロードフェーズのヒットが円柱の扇形に入っているか返す
	// UEのFCollisionShapeに円柱・扇形が無いため、Box(R,R,H)
	// で広めに取ってからここで削る。BoxのZ範囲は円柱と同一なので高さは判定済み、
	// 水平の円と角度だけ見ればよい
	static bool IsInCylinderFan(const FHitResult& Hit, const FVector& Center, const FQuat& ShapeRot,
		float Radius, float AngleDeg)
	{
		UPrimitiveComponent* Prim = Hit.GetComponent();
		if (!Prim) return false;

		// コライダー上の最近点で判定する。中心座標だと巨大・細長い相手を取りこぼす
		FVector ClosestPoint = FVector::ZeroVector;
		const float Dist = Prim->GetClosestPointOnCollision(Center, ClosestPoint);
		if (Dist < 0.0f) return false;	// コリジョン無しで判定不能
		if (Dist == 0.0f) return true;	// 扇の中心がコライダー内部＝密着なので当てる

		const FVector Local = ShapeRot.UnrotateVector(ClosestPoint - Center);
		const FVector2D LocalXY(Local.X, Local.Y);
		if (LocalXY.SizeSquared() > FMath::Square(Radius)) return false;	// 円柱の外(Boxの角)

		if (AngleDeg >= 360.0f) return true;

		const FVector2D Dir = LocalXY.GetSafeNormal();
		if (Dir.IsNearlyZero()) return true;	// 中心軸上は常に扇の内側
		return Dir.X >= FMath::Cos(FMath::DegreesToRadians(AngleDeg * 0.5f));
	}

	static bool ExecuteHit(USkeletalMeshComponent* MeshComp, const FName SocketName,
		const FCommonAttackHitSettings& Settings, TSet<TWeakObjectPtr<AActor>>& HitActors,
		const FVector& PrevLocation, bool bUseOwnerForwardForKnockback,
		const FTransform* FixedTransform = nullptr)
	{
		if (!MeshComp) return false;

		AActor* Owner = MeshComp->GetOwner();
		if (!Owner) return false;

		UWorld* World = Owner->GetWorld();
		if (!World) return false;

		const FTransform SockXf = FixedTransform
			? *FixedTransform
			: (SocketName != NAME_None)
				? MeshComp->GetSocketTransform(SocketName, RTS_World)
				: MeshComp->GetComponentTransform();

		const FVector AttackLocation = SockXf.TransformPosition(Settings.Offset);

		// 形状の姿勢。Offsetと対でソケット基準のローカル回転を掛ける(既定0なら従来どおり)
		const FQuat ShapeRot = SockXf.GetRotation() * Settings.Rotation.Quaternion();

		FCollisionObjectQueryParams ObjQuery = FCollisionObjectQueryParams::AllDynamicObjects;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(AttackHitNotify), false, Owner);

		const FVector ActorScale = MeshComp->GetComponentScale();
		const float ScaleXY = FMath::Max(FMath::Abs(ActorScale.X), FMath::Abs(ActorScale.Y));
		const float ScaleZ = FMath::Abs(ActorScale.Z);
		const float UseRadius = Settings.Radius * ScaleXY;
		const FVector UseBoxExtent = Settings.BoxExtent * ActorScale;
		const float UseHalfHeight = Settings.HalfHeight * ScaleZ;

		// デバッグ描画
#if !UE_BUILD_SHIPPING
		bool bDrawDebug = false;
		if (UTideGameSettings* GameSettings = UTideGameSettings::Get())
		{
			bDrawDebug = GameSettings->bDebugDrawAttackHitbox;
		}
#if WITH_EDITOR
		// モンタージュ編集中(アニメエディタのプレビュー)ではフラグに関係なく常に表示する
		if (World->WorldType == EWorldType::EditorPreview)
		{
			bDrawDebug = true;
		}
#endif
		if (bDrawDebug)
		{
			const FVector SweepVec = AttackLocation - PrevLocation;
			const float SweepDist = SweepVec.Size();
			const FVector SweepCenter = (PrevLocation + AttackLocation) * 0.5f;
			// カプセルのデフォルト上向き(Z)をSweep方向に向ける
			const FQuat SweepRot = SweepDist > KINDA_SMALL_NUMBER
				? FQuat::FindBetweenNormals(FVector::UpVector, SweepVec / SweepDist)
				: SockXf.GetRotation();

			switch (Settings.Shape)
			{
			case EHitShape::Sphere:
				// Sweepの軌跡をカプセル1本で表現
				DrawDebugCapsule(World, SweepCenter, SweepDist * 0.5f + UseRadius, UseRadius,
					SweepRot, FColor::Red);
				break;
			case EHitShape::Box:
				DrawDebugLine(World, PrevLocation, AttackLocation, FColor::Yellow);
				DrawDebugBox(World, AttackLocation, UseBoxExtent, ShapeRot, FColor::Red);
				DrawDebugBox(World, PrevLocation, UseBoxExtent, ShapeRot, FColor::Orange);
				break;
			case EHitShape::Capsule:
				// Sweepの軌跡をカプセル1本で表現
				DrawDebugCapsule(World, SweepCenter, SweepDist * 0.5f + UseHalfHeight, UseRadius,
					SweepRot, FColor::Red);
				break;
			case EHitShape::Cylinder:
			{
				const FVector AxisHalf = ShapeRot.GetUpVector() * UseHalfHeight;
				if (Settings.AngleDeg >= 360.0f)
				{
					DrawDebugCylinder(World, AttackLocation - AxisHalf, AttackLocation + AxisHalf,
						UseRadius, 16, FColor::Red);
					break;
				}

				// DrawDebugCylinderは常に全周になりAngleDegを反映できないので円弧を自前で引く
				const float HalfRad = FMath::DegreesToRadians(Settings.AngleDeg * 0.5f);
				constexpr int32 ArcSegments = 16;
				auto ArcPoint = [&](float Rad, const FVector& Axis)
				{
					return AttackLocation + Axis + ShapeRot.RotateVector(
						FVector(FMath::Cos(Rad), FMath::Sin(Rad), 0.0f)) * UseRadius;
				};

				FVector PrevTop    = ArcPoint(-HalfRad, AxisHalf);
				FVector PrevBottom = ArcPoint(-HalfRad, -AxisHalf);
				for (int32 Seg = 1; Seg <= ArcSegments; ++Seg)
				{
					const float Rad = FMath::Lerp(-HalfRad, HalfRad, static_cast<float>(Seg) / ArcSegments);
					const FVector Top    = ArcPoint(Rad, AxisHalf);
					const FVector Bottom = ArcPoint(Rad, -AxisHalf);
					DrawDebugLine(World, PrevTop, Top, FColor::Red);
					DrawDebugLine(World, PrevBottom, Bottom, FColor::Red);
					PrevTop    = Top;
					PrevBottom = Bottom;
				}

				// 扇の左右端(中心→円弧)と、上下の円弧をつなぐ縦線
				for (const float EdgeRad : { -HalfRad, HalfRad })
				{
					const FVector Top    = ArcPoint(EdgeRad, AxisHalf);
					const FVector Bottom = ArcPoint(EdgeRad, -AxisHalf);
					DrawDebugLine(World, AttackLocation + AxisHalf, Top, FColor::Red);
					DrawDebugLine(World, AttackLocation - AxisHalf, Bottom, FColor::Red);
					DrawDebugLine(World, Top, Bottom, FColor::Red);
				}
				break;
			}
			}
		}
#endif

		// 形状ごとの判定
		FCollisionShape CollisionShape;
		switch (Settings.Shape)
		{
		case EHitShape::Sphere:
			CollisionShape = FCollisionShape::MakeSphere(UseRadius);
			break;
		case EHitShape::Box:
			CollisionShape = FCollisionShape::MakeBox(UseBoxExtent);
			break;
		case EHitShape::Capsule:
			CollisionShape = FCollisionShape::MakeCapsule(UseRadius, UseHalfHeight);
			break;
		case EHitShape::Cylinder:
			// 円柱を最小限に包含するBox。カプセルだと上下の縁が丸く落ちて円柱の角を取りこぼす
			CollisionShape = FCollisionShape::MakeBox(FVector(UseRadius, UseRadius, UseHalfHeight));
			break;
		}

		TArray<FHitResult> Hits;
		bool bAnyHit = World->SweepMultiByObjectType(Hits, PrevLocation, AttackLocation,
			ShapeRot, ObjQuery, CollisionShape, Params);

		if (!bAnyHit)
		{
			return false;
		}

		// Boxで広めに取った分を円柱の扇形へ削る
		if (Settings.Shape == EHitShape::Cylinder)
		{
			Hits.RemoveAll([&](const FHitResult& Hit)
			{
				return !IsInCylinderFan(Hit, AttackLocation, ShapeRot, UseRadius, Settings.AngleDeg);
			});
			if (Hits.Num() == 0) return false;
		}

		// ----------------------------------------------------
		// 1. ノックバック方向など、ターゲット非依存の情報を事前構築する
		// ----------------------------------------------------
		FDamageInfo BaseInfo;
		BaseInfo.Instigator = Owner;

		if (bUseOwnerForwardForKnockback)
		{
			BaseInfo.KnockbackDirectionOverride = Owner->GetActorForwardVector();
		}

		// 部位タグ(Part.*)を持つDamageLayerコンポーネントかどうか判定する
		auto ComponentHasPartTag = [](const UPrimitiveComponent* Comp) -> bool
		{
			if (!Comp) return false;
			for (const FName& Tag : Comp->ComponentTags)
			{
				if (Tag == TEXT("DamageLayer")) continue;
				const FGameplayTag PT = FGameplayTag::RequestGameplayTag(
					FName(*FString::Printf(TEXT("Part.%s"), *Tag.ToString())), false);
				if (PT.IsValid()) return true;
			}
			return false;
		};

		// 同一アクターで複数のDamageLayerに当たったときの優先度
		// 光輪が生きている部位 > 壊れた部位 > ボディ(部位ではないコリジョン)
		// アクターにつき1ヒットしか通さないので、優先度を付けないと  -
		// 巨大ボスは部位カプセルを外側から覆うボディカプセルが常に先にヒットする  -
		// 壊れた部位のコリジョンが隣の生きた光輪へのヒットを奪うのどちらでも、
		// 狙った光輪を殴れなくなる
		// 部位の生死は共通光輪システムに問い合わせる (コンポーネントタグ＝PartTagで照合するので、
		// Part.* GameplayTagの登録有無に依存しない)
		auto HitPriority = [&ComponentHasPartTag](const AActor* Target, const UPrimitiveComponent* Comp) -> int32
		{
			if (!Comp) return 0;

			if (const UHaloSystemComponent* Halo = Target->FindComponentByClass<UHaloSystemComponent>())
			{
				const FName UnitTag = Halo->GetUnitPartTagForComponent(Comp);
				if (UnitTag != NAME_None) return Halo->IsPartDefenseless(UnitTag) ? 1 : 2;
			}
			return ComponentHasPartTag(Comp) ? 1 : 0;
		};

		// ----------------------------------------------------
		// 2. アクターごとに最良のDamageLayerヒットを選択 (HitPriorityが最大のもの)
		// ----------------------------------------------------
		struct FActorBestHit
		{
			FHitResult Hit;
			int32      Priority = 0;

		};

		TMap<AActor*, FActorBestHit> BestHitPerActor;
		for (const FHitResult& Hit : Hits)
		{
			AActor* Other = Hit.GetActor();
			if (!Other || Other == Owner) continue;
			if (!TideCombatUtil::IsHostileTo(Owner, Other)) continue;

			UPrimitiveComponent* Prim = Hit.GetComponent();
			if (!Prim || !Prim->ComponentHasTag(TEXT("DamageLayer"))) continue;

			const int32 Priority = HitPriority(Other, Prim);

			FActorBestHit* Existing = BestHitPerActor.Find(Other);
			if (!Existing)
			{
				BestHitPerActor.Add(Other, FActorBestHit{Hit, Priority});
			}
			else if (Priority > Existing->Priority)
			{
				// 同優先度ならスイープ順 (手前のヒット) を維持する
				Existing->Hit      = Hit;
				Existing->Priority = Priority;
			}
		}

		// ----------------------------------------------------
		// 3. 当たり判定のループ処理
		// ----------------------------------------------------
		bool bAnyDamaged = false;
		FDamageInfo LastHitInfo;

		for (auto& [Other, Best] : BestHitPerActor)
		{
			if (HitActors.Contains(Other)) continue;
			HitActors.Add(Other);

			if (IDamageable* Damageable = Cast<IDamageable>(Other))
			{
				// ターゲットごとにDamageInfoを構築(空中判定等はキャラ側で解決)
				FDamageInfo TargetDamageInfo = BaseInfo;
				TargetDamageInfo.HitResult = Best.Hit;

				// ヒットコンポーネントのタグから "Part.*" GameplayTagを解決
				if (UPrimitiveComponent* HitComp = Best.Hit.GetComponent())
				{
					for (const FName& CompTag : HitComp->ComponentTags)
					{
						if (CompTag == TEXT("DamageLayer")) continue;
						const FGameplayTag PartTag = FGameplayTag::RequestGameplayTag(
							FName(*FString::Printf(TEXT("Part.%s"), *CompTag.ToString())), false);
						if (PartTag.IsValid())
						{
							TargetDamageInfo.HitPartTag = PartTag;
							break;
						}
					}
				}

				if (ATideCharacter* TideChar = Cast<ATideCharacter>(Owner))
				{
					TideChar->OnModifyDamageInfo(TargetDamageInfo, Settings.AttackTypeTag, Other);
				}

				// フォールバック上書き設定
				if (Settings.bUseFallback)
				{
					TargetDamageInfo.BaseDamage      = Settings.Damage;
					TargetDamageInfo.bUseHitStop     = Settings.bUseHitStop;
					TargetDamageInfo.HitStopDuration = Settings.HitStopDuration;
					TargetDamageInfo.HitStopDilation = Settings.HitStopDilation;

					// ヒットリアクションタグは設定されている時だけ上書きする
					// (NoneのままならOnModifyDamageInfo等で解決済みの値を維持する)
					if (Settings.HitReactionTag.IsValid())
					{
						TargetDamageInfo.HitReactionTag = Settings.HitReactionTag;
					}
				}

				const EDamageResult Result = Damageable->ReceiveDamage(TargetDamageInfo);
				if (Result != EDamageResult::Hit)
				{
					// ヒットしなかった(回避された等)場合は次のActorへ
					continue;
				}

				// ダメージ確定: 攻撃側キャラへ通知(ログ等)
				if (ATideCharacter* TideOwner = Cast<ATideCharacter>(Owner))
				{
					TideOwner->OnAttackHitConfirmed(TargetDamageInfo);
				}
				LastHitInfo = TargetDamageInfo;
			}
			else
			{
				// IDamageableを持たない(ダメージを与えられない)Actorへの処理
				// ※壁などを殴った時にヒットストップをかけたい場合はここを調整してください
				continue;
			}

			// --- ここから下は「完全にヒットが確定した」場合のみ実行される ---

			// ここでのヒットストップは「殴られた側」のみにかける
			if (LastHitInfo.bUseHitStop)
			{
				HitStopUtil::ApplyHitStop(Other, LastHitInfo.HitStopDuration, LastHitInfo.HitStopDilation);
			}

			bAnyDamaged = true;
		}

		// ----------------------------------------------------
		// 4. ループ終了後、攻撃側へのヒットストップを1回だけ適用
		// ----------------------------------------------------
		if (bAnyDamaged && LastHitInfo.bUseHitStop)
		{
			HitStopUtil::ApplyHitStop(Owner, LastHitInfo.HitStopDuration, LastHitInfo.HitStopDilation);
		}

		return false;
	}
}

void UAnimNotifyState_CommonAttack::NotifyBegin(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	if (MeshComp)
	{
		// そのコンポーネント用のヒットリストを初期化
		ComponentHitMap.FindOrAdd(MeshComp).Empty();

		const FTransform SockXf = (SocketName != NAME_None)
			? MeshComp->GetSocketTransform(SocketName, RTS_World)
			: MeshComp->GetComponentTransform();

		PrevSocketLocationMap.Add(MeshComp, SockXf.TransformPosition(HitSettings.Offset));

		if (bFixWorldPosition)
		{
			FixedWorldTransformMap.Add(MeshComp, SockXf);
		}
	}
}

void UAnimNotifyState_CommonAttack::NotifyTick(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference)
{
	if (!MeshComp) return;

	const FTransform* FixedTransform = bFixWorldPosition ? FixedWorldTransformMap.Find(MeshComp) : nullptr;

	const FTransform SockXf = FixedTransform
		? *FixedTransform
		: (SocketName != NAME_None)
			? MeshComp->GetSocketTransform(SocketName, RTS_World)
			: MeshComp->GetComponentTransform();

	const FVector CurrentLocation = SockXf.TransformPosition(HitSettings.Offset);

	// 前回位置を取得(なければ現在地で初期化)
	FVector& PrevLocation = PrevSocketLocationMap.FindOrAdd(MeshComp, CurrentLocation);

	TSet<TWeakObjectPtr<AActor>>& HitActors = ComponentHitMap.FindOrAdd(MeshComp);
	CommonAttack::ExecuteHit(MeshComp, SocketName, HitSettings, HitActors, PrevLocation, bUseOwnerForwardForKnockback, FixedTransform);

	// 固定位置モードではPrevLocationを更新しない(毎フレーム同一位置でOverlap判定)
	if (!bFixWorldPosition)
	{
		PrevLocation = CurrentLocation;
	}
}
