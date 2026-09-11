// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/Components/Combat/PartDestructionComponent.h"
#include "PRJ_TIDE_P0/Components/Combat/DamageSystemComponent.h"
#include "PRJ_TIDE_P0/Data/Enemy/BossDataAsset.h"
#include "PRJ_TIDE_P0/Data/Tags/TideGameplayTags.h"
#include "PRJ_TIDE_P0/Utilities/TideCombatUtils.h"
#include "PRJ_TIDE_P0/Interfaces/Combat/IDamageable.h"
#include "PRJ_TIDE_P0/Animation/Instances/Character/Enemy/EnemyAnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "Field/FieldSystemComponent.h"
#include "Field/FieldSystemObjects.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "PRJ_TIDE_P0/Core/TideGameSettings.h"
#include "Engine/OverlapResult.h"
#include "DrawDebugHelpers.h"

namespace
{
	// 破片用コリジョンプロファイル ※背景(WorldStatic
	// /WorldDynamic)のみブロック
	// DefaultEngine.iniの[/Script
	// /Engine.CollisionProfile] に定義
	const FName PartHaloBreakFragmentProfileName(TEXT("HaloFragment"));
}

UPartDestructionComponent::UPartDestructionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UPartDestructionComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	bool bNeedTick = false;
	for (int32 i = 0; i < PartHaloRuntimes.Num(); ++i)
	{
		FPartHaloRuntimeData& HR = PartHaloRuntimes[i];
		const FPartHaloConfig& Cfg = Parts[i].Entry.HaloConfig;

		// 破壊破片ディザフェード (DitherAlpha 0->1)
		if (HR.bFading)
		{
			UGeometryCollectionComponent* GCC = PartHaloGCComps.IsValidIndex(i) ? PartHaloGCComps[i] : nullptr;
			if (GCC)
			{
				bNeedTick = true;
				HR.FadeElapsed += DeltaTime;
				const float t = FMath::Clamp(HR.FadeElapsed / Cfg.BreakFadeDuration, 0.0f, 1.0f);

				for (int32 m = 0; m < GCC->GetNumMaterials(); ++m)
				{
					if (UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(GCC->GetMaterial(m)))
					{
						MID->SetScalarParameterValue(Cfg.DitherParamName, t);
					}
				}

				if (t >= 1.0f)
				{
					HR.bFading = false;
					GCC->SetVisibility(false);
					GCC->SetSimulatePhysics(false);
				}
			}
		}

		// 復活ディザ (DitherAlpha 1->0のフェードイン)。完了時に部位反応を解禁する
		if (HR.bReviving)
		{
			UStaticMeshComponent* HaloComp = PartHaloMeshComps.IsValidIndex(i) ? PartHaloMeshComps[i] : nullptr;
			if (HaloComp)
			{
				bNeedTick = true;
				HR.ReviveElapsed += DeltaTime;
				const float Dur   = FMath::Max(Cfg.ReviveDissolveDuration, KINDA_SMALL_NUMBER);
				const float t     = FMath::Clamp(HR.ReviveElapsed / Dur, 0.0f, 1.0f);
				const float Alpha = 1.0f - t;

				for (int32 m = 0; m < HaloComp->GetNumMaterials(); ++m)
				{
					if (UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(HaloComp->GetMaterial(m)))
					{
						MID->SetScalarParameterValue(Cfg.DitherParamName, Alpha);
					}
				}

				if (t >= 1.0f)
				{
					HR.bReviving = false;
					Parts[i].bDestroyed = false;
					RefreshPartHitDetection();
				}
			}
		}

		// 攻撃中の一時非表示/再表示 (DitherAlphaを0<->1でフェード)
		if (HR.bHideFading)
		{
			UStaticMeshComponent* HaloComp = PartHaloMeshComps.IsValidIndex(i) ? PartHaloMeshComps[i] : nullptr;
			if (HaloComp)
			{
				bNeedTick = true;
				HR.HideElapsed += DeltaTime;
				const float Dur = FMath::Max(Cfg.HaloHideDissolveDuration, KINDA_SMALL_NUMBER);
				const float t   = FMath::Clamp(HR.HideElapsed / Dur, 0.0f, 1.0f);
				HR.CurrentDither = FMath::Lerp(HR.HideFrom, HR.HideTo, t);

				for (int32 m = 0; m < HaloComp->GetNumMaterials(); ++m)
				{
					if (UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(HaloComp->GetMaterial(m)))
					{
						MID->SetScalarParameterValue(Cfg.DitherParamName, HR.CurrentDither);
					}
				}

				if (t >= 1.0f)
				{
					HR.bHideFading = false;
					// 消滅方向 (1) のフェード完了時は描画も止める
					if (HR.HideTo >= 1.0f) HaloComp->SetVisibility(false);
				}
			}
		}

		// ヒットしたが壊れなかったときの3軸ブレ
		if (HR.HaloShake.IsActive())
		{
			UStaticMeshComponent* HaloComp = PartHaloMeshComps.IsValidIndex(i) ? PartHaloMeshComps[i] : nullptr;
			HR.HaloShake.Tick(DeltaTime, HaloComp);
			if (HR.HaloShake.IsActive()) bNeedTick = true;
		}
	}

	// ダウン復帰中の接触ダメージ (窓が開いている間だけ)
	if (bRecoveryTouchActive)
	{
		TickRecoveryTouchDamage(DeltaTime);
		bNeedTick = true;
	}

	if (!bNeedTick) SetComponentTickEnabled(false);
}

void UPartDestructionComponent::Initialize(const TArray<FPartEntry>& InParts, const TArray<FPartComboReaction>& InCombos,
	const FBossRecoveryTouch& InRecoveryTouch)
{
	if (InParts.IsEmpty()) return;

	// ストリーミング再入でBeginPlay→Initializeが再度走ると、
	// 部位・光輪が二重生成される
	// (OnDamageReceivedをAddUniqueで守っているのと同じ再入対策)
	// 既に初期化済みなら生成済みコンポーネント・タイマーを破棄し、配列を空にしてから作り直す
	// (冪等化)
	if (Parts.Num() > 0)
	{
		if (AActor* Owner = GetOwner())
		{
			for (FPartHaloRuntimeData& HR : PartHaloRuntimes)
			{
				Owner->GetWorldTimerManager().ClearTimer(HR.RegenTimerHandle);
				Owner->GetWorldTimerManager().ClearTimer(HR.ScatterTimerHandle);
				Owner->GetWorldTimerManager().ClearTimer(HR.FadeTimerHandle);
			}
		}
		for (UStaticMeshComponent* Comp : PartHaloMeshComps)        if (Comp) Comp->DestroyComponent();
		for (UGeometryCollectionComponent* Comp : PartHaloGCComps)  if (Comp) Comp->DestroyComponent();
		for (UFieldSystemComponent* Comp : PartHaloFieldComps)      if (Comp) Comp->DestroyComponent();

		PartHaloMeshComps.Reset();
		PartHaloGCComps.Reset();
		PartHaloFieldComps.Reset();
		PartHaloRuntimes.Reset();
		Parts.Reset();
	}

	Combos        = InCombos;
	RecoveryTouch = InRecoveryTouch;

	Parts.Reserve(InParts.Num());
	for (const FPartEntry& Entry : InParts)
	{
		FPartRuntimeData& Runtime = Parts.AddDefaulted_GetRef();
		Runtime.Entry = Entry;
	}

	// 各部位のダメージコリジョン (PartTagを持つプリミティブ) を集める
	// 破壊時に "DamageLayer" タグを外し復活で戻すことで、コリジョン (Pawnブロック)
	// は残したまま攻撃の対象選択からだけ外す (重なった部位への吸い込み防止＋すり抜け防止)
	if (AActor* Owner = GetOwner())
	{
		static const FName DamageLayerTag(TEXT("DamageLayer"));
		TArray<UPrimitiveComponent*> PrimComps;
		Owner->GetComponents<UPrimitiveComponent>(PrimComps);
		for (FPartRuntimeData& Runtime : Parts)
		{
			for (UPrimitiveComponent* Prim : PrimComps)
			{
				if (Prim && Prim->ComponentHasTag(Runtime.Entry.PartTag))
				{
					FPartCollisionCache Cache;
					Cache.Comp            = Prim;
					Cache.bHadDamageLayer = Prim->ComponentHasTag(DamageLayerTag);
					Runtime.CollisionComps.Add(Cache);
				}
			}
		}
	}

	// DamageSystemComponentからダメージを受け取り、
	// HitResultのコンポーネントタグで部位を特定
	if (UDamageSystemComponent* DmgSys = GetOwner()->FindComponentByClass<UDamageSystemComponent>())
	{
		// ストリーミング再入で残留束縛と衝突して二重束縛のensureが出るのを防ぐ
		DmgSys->OnDamageReceived.AddUniqueDynamic(this, &UPartDestructionComponent::HandleDamageReceived);
	}

	// 部位光輪メッシュ・GCC・FieldCompを事前生成
	PartHaloMeshComps.SetNum(Parts.Num());
	PartHaloGCComps.SetNum(Parts.Num());
	PartHaloFieldComps.SetNum(Parts.Num());
	PartHaloRuntimes.SetNum(Parts.Num());

	USkeletalMeshComponent* SkelMesh = GetOwner()->FindComponentByClass<USkeletalMeshComponent>();
	for (int32 i = 0; i < Parts.Num(); ++i)
	{
		const FPartHaloConfig& Cfg = Parts[i].Entry.HaloConfig;

		if (Cfg.HaloMesh && SkelMesh)
		{
			UStaticMeshComponent* HaloComp = NewObject<UStaticMeshComponent>(GetOwner());
			HaloComp->SetStaticMesh(Cfg.HaloMesh);
			HaloComp->RegisterComponent();
			HaloComp->AttachToComponent(SkelMesh,
				FAttachmentTransformRules::SnapToTargetNotIncludingScale, Cfg.HaloSocket);
			HaloComp->SetRelativeScale3D(FVector(Cfg.HaloScale));
			HaloComp->SetCollisionProfileName(TEXT("NoCollision"));
			PartHaloMeshComps[i] = HaloComp;

			// 以降のヒビ・発光・ディゾルブはMID経由で駆動するので、ここで用意して初期値を入れる
			for (int32 m = 0; m < HaloComp->GetNumMaterials(); ++m)
			{
				if (UMaterialInstanceDynamic* MID = HaloComp->CreateAndSetMaterialInstanceDynamic(m))
				{
					MID->SetScalarParameterValue(Cfg.CrackParamName, 0.0f);
					MID->SetScalarParameterValue(Cfg.AttackGlowParamName, 0.0f);
					MID->SetScalarParameterValue(Cfg.DitherParamName, 0.0f);
				}
			}
		}

		if (Cfg.HaloBreakGC)
		{
			// BreakablePropと同じく事前生成・非表示で待機させ、
			// 破壊時にSetSimulatePhysics(true) で起動する
			// (破壊のたびに作り直して破断状態をリセットするので、ここでは初期分を1つ用意するだけ)
			PartHaloGCComps[i] = CreatePartHaloGCC(i);

			UFieldSystemComponent* FieldComp = NewObject<UFieldSystemComponent>(GetOwner());
			FieldComp->RegisterComponent();
			FieldComp->AttachToComponent(GetOwner()->GetRootComponent(),
				FAttachmentTransformRules::KeepRelativeTransform);
			PartHaloFieldComps[i] = FieldComp;
		}
	}
}

bool UPartDestructionComponent::IsPartDestroyed(FName PartTag) const
{
	for (const FPartRuntimeData& Runtime : Parts)
	{
		if (Runtime.Entry.PartTag == PartTag) return Runtime.bDestroyed;
	}
	return false;
}

FName UPartDestructionComponent::GetPartTag(int32 Index) const
{
	return Parts.IsValidIndex(Index) ? Parts[Index].Entry.PartTag : NAME_None;
}

bool UPartDestructionComponent::IsPartDestroyedByIndex(int32 Index) const
{
	return Parts.IsValidIndex(Index) ? Parts[Index].bDestroyed : false;
}

void UPartDestructionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Initializeで張った束縛を対称に解除する(残留による二重束縛ensureを防ぐ)
	if (AActor* Owner = GetOwner())
	{
		if (UDamageSystemComponent* DmgSys = Owner->FindComponentByClass<UDamageSystemComponent>())
		{
			DmgSys->OnDamageReceived.RemoveDynamic(this, &UPartDestructionComponent::HandleDamageReceived);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void UPartDestructionComponent::HandleDamageReceived(const FDamageInfo& DamageInfo)
{
	if (Parts.IsEmpty()) return;

	UPrimitiveComponent* HitComp = DamageInfo.HitResult.GetComponent();
	if (!HitComp) return;

	for (int32 i = 0; i < Parts.Num(); ++i)
	{
		FPartRuntimeData& Runtime = Parts[i];
		if (!HitComp->ComponentHasTag(Runtime.Entry.PartTag)) continue;

		// 攻撃中の一時無敵: 除外部位 (頭など) 以外はダメージ・ヒット反応を一切受けない
		if (bPartHaloSuppressed && !Runtime.Entry.bExcludeFromHaloHide) return;

		if (Runtime.Entry.HitVFX)
		{
			// 着弾法線向きでスポーンする (リアクションVFXと出方を揃える)
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(
				GetOwner(), Runtime.Entry.HitVFX, DamageInfo.HitResult.ImpactPoint,
				DamageInfo.HitResult.ImpactNormal.Rotation());
		}

		if (Runtime.bDestroyed) break;

		if (Runtime.Entry.HitShakeImpulse > 0.0f)
		{
			if (USkeletalMeshComponent* Mesh = GetOwner()->FindComponentByClass<USkeletalMeshComponent>())
			{
				if (UEnemyAnimInstance* AnimInst = Cast<UEnemyAnimInstance>(Mesh->GetAnimInstance()))
				{
					AnimInst->TriggerHitShake(i, Runtime.Entry.HitShakeImpulse, Runtime.Entry.HitShakeDuration);
				}
			}
		}

		// 神技 (UGodActionPlayerModule) はギア・ヒビ状態に関係なく一撃で破壊する
		if (TideCombatUtil::IsGodActionDamage(DamageInfo))
		{
			DestroyPart(i);
			break;
		}

		// 壊れ方はザコの光輪 (UHaloComponent::HandleGuardHit)
		// と共通のルールにする。軽ギア (無/壱/弍) かつヒビなし → ヒビのみで部位は残る
		const FGameplayTag Gear = DamageInfo.ChargeGearTag;
		const bool bHeavyGear   = (Gear == TAG_Charge_Gear3 || Gear == TAG_Charge_Gear4);

		if (!bHeavyGear && !Runtime.bCracked)
		{
			Runtime.bCracked = true;
			UpdatePartHaloCrack(i);

			// 壊れなかった → 部位光輪を3軸ブレ
			UStaticMeshComponent* HaloComp = PartHaloMeshComps.IsValidIndex(i) ? PartHaloMeshComps[i] : nullptr;
			if (HaloComp && PartHaloRuntimes.IsValidIndex(i))
			{
				PartHaloRuntimes[i].HaloShake.Trigger(HaloComp, Runtime.Entry.HaloConfig.HitShake);
				SetComponentTickEnabled(true);
			}
			break;
		}

		// 重ギア、もしくはヒビ中の再ヒット → 破壊
		DestroyPart(i);
		break;
	}
}

int32 UPartDestructionComponent::FindPartIndexForComponent(const USceneComponent* HitComponent) const
{
	if (!HitComponent) return INDEX_NONE;

	// HitComponentまたはその祖先が持つPartTagから部位を特定する
	// (ロックオン対象は部位コリジョン配下に付くため、親をたどればタグに行き当たる)
	for (int32 i = 0; i < Parts.Num(); ++i)
	{
		const FName PartTag = Parts[i].Entry.PartTag;
		for (const USceneComponent* C = HitComponent; C; C = C->GetAttachParent())
		{
			if (C->ComponentHasTag(PartTag)) return i;
		}
	}
	return INDEX_NONE;
}

bool UPartDestructionComponent::DestroyPartsByComponents(const TArray<USceneComponent*>& HitComponents)
{
	if (Parts.IsEmpty()) return false;

	// 対象部位を集める (重複・破壊済みは除外)
	TArray<int32> Indices;
	for (const USceneComponent* C : HitComponents)
	{
		const int32 Idx = FindPartIndexForComponent(C);
		if (Idx != INDEX_NONE && !Parts[Idx].bDestroyed)
		{
			Indices.AddUnique(Idx);
		}
	}
	if (Indices.Num() == 0) return false;

	// 連動 (combo) の成立を一斉に判定させるため、通知の前に対象を全て破壊状態にしておく
	// (1つずつDestroyPartすると最初の部位の時点で連動が未成立となり、
	// 個別モンタージュが流れてしまう)先に全対象を破壊状態にしてからDamageLayerを再計算する
	// (同時破壊ぶんも「破壊済み」として見るように)
	for (int32 Idx : Indices)
	{
		Parts[Idx].bDestroyed = true;
	}
	RefreshPartHitDetection();

	for (int32 Idx : Indices)
	{
		StartPartHaloBreakVisual(Idx);
		SchedulePartRegen(Idx);
	}

	// (1) まず個別破壊を通知する。連動メンバは購読側 (ボス)
	// が連動成立を見て個別モンタージュをスキップし、    非連動部位 (頭など)
	// はここで個別DestroyMontageを再生する
	for (int32 Idx : Indices)
	{
		OnPartDestroyed.Broadcast(Parts[Idx].Entry.PartTag);
	}

	// (2) 連動成立通知は個別通知の「後」にまとめて発火する
	// これでComboMontageが個別モンタージュより    後に再生され、
	// 部位の処理順に依存せず確実に連動ダウン演出が優先される (同一グループは一度だけ)
	// 復活は連動では行わず、各部位がSchedulePartRegenの個別タイマーで復活する
	TSet<int32> FiredCombos;
	for (int32 Idx : Indices)
	{
		const int32 ComboIndex = FindCompletedComboForPart(Parts[Idx].Entry.PartTag);
		if (ComboIndex != INDEX_NONE && !FiredCombos.Contains(ComboIndex))
		{
			FiredCombos.Add(ComboIndex);
			OnPartComboDestroyed.Broadcast(ComboIndex);
		}
	}
	return true;
}

void UPartDestructionComponent::DestroyPart(int32 Index)
{
	FPartRuntimeData& Runtime = Parts[Index];
	Runtime.bDestroyed = true;

	RefreshPartHitDetection();

	// 先に個別破壊を通知 (フェーズ判定・個別リアクション)。連動成立判定は購読側が
	// FindCompletedComboForPartで行い、成立部位は個別モンタージュをスキップする
	OnPartDestroyed.Broadcast(Runtime.Entry.PartTag);

	// 連動グループが成立したら通知する (個別通知の後に発火し、ComboMontageを優先させる)
	const int32 ComboIndex = FindCompletedComboForPart(Runtime.Entry.PartTag);
	if (ComboIndex != INDEX_NONE)
	{
		OnPartComboDestroyed.Broadcast(ComboIndex);
	}

	StartPartHaloBreakVisual(Index);
	SchedulePartRegen(Index);
}

UGeometryCollectionComponent* UPartDestructionComponent::CreatePartHaloGCC(int32 Index)
{
	if (!Parts.IsValidIndex(Index)) return nullptr;
	const FPartHaloConfig& Cfg = Parts[Index].Entry.HaloConfig;
	if (!Cfg.HaloBreakGC) return nullptr;

	UGeometryCollectionComponent* GCC = NewObject<UGeometryCollectionComponent>(GetOwner());
	GCC->SetRestCollection(Cfg.HaloBreakGC);
	// 破片は背景(WorldStatic/WorldDynamic)のみブロックし、Pawn
	// /PhysicsBodyとの接触による不自然な加速を防ぐ
	// 全レベルへHaloFragmentプロファイルを適用する
	// 後でSetSimulatePhysics
	// (true)してもOnPostCreateParticlesで自動再適用される
	GCC->SetPerLevelCollisionProfileNames({PartHaloBreakFragmentProfileName});
	GCC->SetSimulatePhysics(false);
	GCC->SetVisibility(false);
	// アタッチせず登録するので破壊まではワールド原点に居座る 当たりを残すと原点付近への
	// オブジェクトクエリが「持ち主(遠くにいる敵)へのヒット」として返ってしまう
	// (SweepMultiByObjectTypeはObjectTypeしか見ないためプロファイルのIgnoreでは防げない)
	GCC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GCC->RegisterComponent();
	return GCC;
}

void UPartDestructionComponent::StartPartHaloBreakVisual(int32 Index)
{
	// 部位光輪の破壊
	const FPartHaloConfig& Cfg = Parts[Index].Entry.HaloConfig;

	// 2回目以降も破片がきちんと飛散するよう、GCCを作り直して破断状態をリセットする
	// (Chaos GCは一度破断するとクラスタ状態を初期へ戻せず、
	// 使い回すと「飛散済み」の状態で出てしまう)
	if (Cfg.HaloBreakGC && PartHaloGCComps.IsValidIndex(Index))
	{
		if (UGeometryCollectionComponent* OldGCC = PartHaloGCComps[Index])
		{
			OldGCC->DestroyComponent();
		}
		PartHaloGCComps[Index] = CreatePartHaloGCC(Index);
	}

	UGeometryCollectionComponent* GCC = PartHaloGCComps.IsValidIndex(Index) ? PartHaloGCComps[Index] : nullptr;
	UFieldSystemComponent* FieldComp  = PartHaloFieldComps.IsValidIndex(Index) ? PartHaloFieldComps[Index] : nullptr;
	UStaticMeshComponent*  HaloComp   = PartHaloMeshComps.IsValidIndex(Index) ? PartHaloMeshComps[Index] : nullptr;
	if (!GCC || !FieldComp) return;

	if (HaloComp) HaloComp->SetVisibility(false);

	// リングの現在位置にGCCとFieldCompを配置してシミュレーション開始
	const FVector BreakCenter = HaloComp ? HaloComp->GetComponentLocation() : GetOwner()->GetActorLocation();
	if (HaloComp) GCC->SetWorldTransform(HaloComp->GetComponentTransform());
	FieldComp->SetWorldLocation(BreakCenter);

	GCC->SetVisibility(true);
	// ワールド位置を入れた後に当たりを戻す。SetSimulatePhysicsより前に置けば
	// OnPostCreateParticles時にHaloFragmentプロファイルが各破片へ焼き込まれる
	GCC->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	GCC->SetSimulatePhysics(true);

	// 復活後の再破壊に備える。前回フェードでGCCマテリアルがDMI化されDitherAlpha=消滅
	// (1) のまま残るため、破片が最初から消えた状態でスポーンしないよう表示(0) に戻しておく
	for (int32 m = 0; m < GCC->GetNumMaterials(); ++m)
	{
		if (UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(GCC->GetMaterial(m)))
		{
			MID->SetScalarParameterValue(Cfg.DitherParamName, 0.0f);
		}
	}

	PartHaloRuntimes[Index].bFading     = false;
	PartHaloRuntimes[Index].FadeElapsed = 0.0f;
	PartHaloRuntimes[Index].BreakCenter = BreakCenter;

	// Strainでクラスターを砕く
	URadialFalloff* StrainField = NewObject<URadialFalloff>(FieldComp);
	StrainField->Magnitude = Cfg.BreakStrainMagnitude;
	StrainField->Radius    = Cfg.BreakFieldRadius;
	StrainField->Position  = BreakCenter;
	StrainField->Falloff   = EFieldFalloffType::Field_FallOff_None;
	StrainField->MinRange  = 0.0f;
	StrainField->MaxRange  = 1.0f;
	StrainField->Default   = 0.0f;
	FieldComp->ApplyPhysicsField(true, EFieldPhysicsType::Field_ExternalClusterStrain, nullptr, StrainField);

	// 散布力 (0なら重力のみ)
	if (Cfg.BreakScatterForce > 0.0f)
	{
		FTimerDelegate ScatterDelegate;
		ScatterDelegate.BindUObject(this, &UPartDestructionComponent::ApplyPartHaloScatterForce, Index);
		GetOwner()->GetWorldTimerManager().SetTimer(
			PartHaloRuntimes[Index].ScatterTimerHandle, ScatterDelegate, 0.05f, false);
	}

	// フェード開始タイマー
	if (Cfg.BreakFadeDuration > 0.0f)
	{
		const float FadeDelay = FMath::Max(Cfg.BreakLifeSpan - Cfg.BreakFadeDuration, 0.0f);
		FTimerDelegate FadeDelegate;
		FadeDelegate.BindUObject(this, &UPartDestructionComponent::StartPartHaloBreakFade, Index);
		GetOwner()->GetWorldTimerManager().SetTimer(
			PartHaloRuntimes[Index].FadeTimerHandle, FadeDelegate, FadeDelay, false);
	}
}

int32 UPartDestructionComponent::FindCompletedComboForPart(FName PartTag) const
{
	// 成立コンボが複数ある場合はメンバー数が最大のものを優先する
	// 片足コンボ (2メンバー=やられ) と全足コンボ (4メンバー=ダウン) が同時成立したとき、
	// ダウンを勝たせるため (配列順で最初の成立を返すと片足やられが漏れて出てしまう)
	int32 BestIndex = INDEX_NONE;
	int32 BestSize  = 0;
	for (int32 c = 0; c < Combos.Num(); ++c)
	{
		const FPartComboReaction& Combo = Combos[c];
		if (!Combo.MemberPartTags.Contains(PartTag)) continue;

		bool bAllDestroyed = true;
		for (const FName& MemberTag : Combo.MemberPartTags)
		{
			if (!IsPartDestroyed(MemberTag))
			{
				bAllDestroyed = false;
				break;
			}
		}
		if (bAllDestroyed && Combo.MemberPartTags.Num() > BestSize)
		{
			BestIndex = c;
			BestSize  = Combo.MemberPartTags.Num();
		}
	}
	return BestIndex;
}

void UPartDestructionComponent::SchedulePartRegen(int32 Index)
{
	if (!Parts.IsValidIndex(Index) || !PartHaloRuntimes.IsValidIndex(Index)) return;

	// 0以下は自動復活しない (破壊しっぱなし)
	const float Cooldown = Parts[Index].Entry.HaloConfig.BreakRegenCooldown;
	if (Cooldown <= 0.0f) return;

	FTimerDelegate ReviveDelegate;
	ReviveDelegate.BindUObject(this, &UPartDestructionComponent::RevivePart, Index);
	GetOwner()->GetWorldTimerManager().SetTimer(
		PartHaloRuntimes[Index].RegenTimerHandle, ReviveDelegate, Cooldown, false);
}

void UPartDestructionComponent::RevivePart(int32 Index)
{
	if (!Parts.IsValidIndex(Index) || !PartHaloRuntimes.IsValidIndex(Index)) return;

	// 攻撃中・ダウン中に光輪が生え直すと、技の分岐 (UStompAttackExecutionなど)
	// と食い違ううえ反撃機会も潰れる。クールダウンは進めたまま復活だけ保留し、
	// 要因解除時にSetRegenBlockedが実行する
	if (RegenBlockMask != 0)
	{
		PartHaloRuntimes[Index].bRevivePending = true;
		return;
	}
	PartHaloRuntimes[Index].bRevivePending = false;

	FPartRuntimeData& Runtime  = Parts[Index];
	const FPartHaloConfig& Cfg = Runtime.Entry.HaloConfig;

	// 作り直された光輪なのでヒビは無傷に戻る
	// bDestroyedはディザ完了までtrueのまま維持し、部位反応を抑止する
	Runtime.bCracked = false;
	UpdatePartHaloCrack(Index);

	OnPartRevived.Broadcast(Runtime.Entry.PartTag);

	// 進行中の破壊破片演出 (散布・フェード・物理) を止める
	FPartHaloRuntimeData& HR = PartHaloRuntimes[Index];
	GetOwner()->GetWorldTimerManager().ClearTimer(HR.ScatterTimerHandle);
	GetOwner()->GetWorldTimerManager().ClearTimer(HR.FadeTimerHandle);
	HR.bFading = false;

	if (UGeometryCollectionComponent* GCC = PartHaloGCComps.IsValidIndex(Index) ? PartHaloGCComps[Index] : nullptr)
	{
		GCC->SetSimulatePhysics(false);
		GCC->SetVisibility(false);
		// 破片は飛散先に残るので、隠すのと同時に当たりも切る
		GCC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	UStaticMeshComponent* HaloComp = PartHaloMeshComps.IsValidIndex(Index) ? PartHaloMeshComps[Index] : nullptr;

	// 光輪なしorディザなし → 即復活
	if (!HaloComp || Cfg.ReviveDissolveDuration <= 0.0f)
	{
		if (HaloComp) HaloComp->SetVisibility(true);
		HR.bReviving = false;
		Runtime.bDestroyed = false;
		RefreshPartHitDetection();
		return;
	}

	// 光輪を再表示し、DitherAlpha=1 (消滅) からフェードインさせる
	HaloComp->SetVisibility(true);
	for (int32 m = 0; m < HaloComp->GetNumMaterials(); ++m)
	{
		if (UMaterialInstanceDynamic* MID = HaloComp->CreateAndSetMaterialInstanceDynamic(m))
		{
			MID->SetScalarParameterValue(Cfg.DitherParamName, 1.0f);
			// MIDを作り直しているのでヒビも明示的に戻す
			MID->SetScalarParameterValue(Cfg.CrackParamName, 0.0f);
		}
	}

	HR.bReviving     = true;
	HR.ReviveElapsed = 0.0f;
	SetComponentTickEnabled(true);
}

void UPartDestructionComponent::ReviveAllPartsImmediate()
{
	AActor* Owner = GetOwner();

	for (int32 i = 0; i < Parts.Num(); ++i)
	{
		if (!PartHaloRuntimes.IsValidIndex(i)) continue;

		FPartRuntimeData&     Runtime = Parts[i];
		FPartHaloRuntimeData& HR      = PartHaloRuntimes[i];
		const FPartHaloConfig& Cfg    = Runtime.Entry.HaloConfig;

		// 進行中の破片演出・復活タイマー・各種フェードを止める (クールダウン
		// /保留を無視して即復活)
		if (Owner)
		{
			Owner->GetWorldTimerManager().ClearTimer(HR.RegenTimerHandle);
			Owner->GetWorldTimerManager().ClearTimer(HR.ScatterTimerHandle);
			Owner->GetWorldTimerManager().ClearTimer(HR.FadeTimerHandle);
		}
		HR.bFading        = false;
		HR.bReviving      = false;
		HR.bHideFading    = false;
		HR.bRevivePending = false;

		if (UGeometryCollectionComponent* GCC = PartHaloGCComps.IsValidIndex(i) ? PartHaloGCComps[i] : nullptr)
		{
			GCC->SetSimulatePhysics(false);
			GCC->SetVisibility(false);
			// 破片は飛散先に残るので、隠すのと同時に当たりも切る
			GCC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}

		// 光輪メッシュを再表示。ディザ設定があれば消滅(1)から表示(0)へフェードインさせ、
		// 無ければ即表示(0)。反撃中の発光を効かせるためbDestroyedは下で即falseにする
		const bool bUseDither = Cfg.ReviveDissolveDuration > 0.0f;
		if (UStaticMeshComponent* HaloComp = PartHaloMeshComps.IsValidIndex(i) ? PartHaloMeshComps[i] : nullptr)
		{
			HaloComp->SetVisibility(true);
			const float StartDither = bUseDither ? 1.0f : 0.0f;
			for (int32 m = 0; m < HaloComp->GetNumMaterials(); ++m)
			{
				UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(HaloComp->GetMaterial(m));
				if (!MID) MID = HaloComp->CreateAndSetMaterialInstanceDynamic(m);
				if (MID)
				{
					MID->SetScalarParameterValue(Cfg.DitherParamName, StartDither);
					MID->SetScalarParameterValue(Cfg.CrackParamName, 0.0f);
				}
			}
			HR.CurrentDither = StartDither;

			// ディザフェードイン (1->0) をTickComponentのbReviving経路に駆動させる
			if (bUseDither)
			{
				HR.bReviving     = true;
				HR.ReviveElapsed = 0.0f;
				SetComponentTickEnabled(true);
			}
		}

		const bool bWasDestroyed = Runtime.bDestroyed;
		Runtime.bCracked   = false;
		Runtime.bDestroyed = false;

		// 破壊されていた部位だけ復活を通知する (共通光輪システムが状態をDeployedへ戻す)
		if (bWasDestroyed)
		{
			OnPartRevived.Broadcast(Runtime.Entry.PartTag);
		}
	}

	RefreshPartHitDetection();
}

void UPartDestructionComponent::SetRegenBlocked(EPartRegenBlockReason Reason, bool bBlocked)
{
	const uint8 Bit  = static_cast<uint8>(Reason);
	const uint8 Prev = RegenBlockMask;

	if (bBlocked) RegenBlockMask |= Bit;
	else          RegenBlockMask &= static_cast<uint8>(~Bit);

	// 全要因が解除された瞬間に、保留していた部位をまとめて復活させる
	// 攻撃解除とリアクション解除は交錯する
	// (リアクションが攻撃を中断してOnAttackEndを走らせる) ため、
	// マスクが0になるまで待つ必要がある
	if (Prev != 0 && RegenBlockMask == 0)
	{
		for (int32 i = 0; i < PartHaloRuntimes.Num(); ++i)
		{
			// RevivePartがbRevivePendingをクリアする
			if (PartHaloRuntimes[i].bRevivePending) RevivePart(i);
		}
	}
}

float UPartDestructionComponent::GetPartRegenRemaining(int32 Index) const
{
	if (!PartHaloRuntimes.IsValidIndex(Index)) return -1.0f;

	const AActor* Owner = GetOwner();
	if (!Owner) return -1.0f;

	// 非稼働のタイマーは負値が返る
	return Owner->GetWorldTimerManager().GetTimerRemaining(PartHaloRuntimes[Index].RegenTimerHandle);
}

float UPartDestructionComponent::GetPartRegenCooldown(int32 Index) const
{
	return Parts.IsValidIndex(Index) ? Parts[Index].Entry.HaloConfig.BreakRegenCooldown : 0.0f;
}

bool UPartDestructionComponent::IsPartRevivePending(int32 Index) const
{
	return PartHaloRuntimes.IsValidIndex(Index) ? PartHaloRuntimes[Index].bRevivePending : false;
}

void UPartDestructionComponent::UpdatePartHaloCrack(int32 Index)
{
	if (!Parts.IsValidIndex(Index)) return;

	UStaticMeshComponent* HaloComp = PartHaloMeshComps.IsValidIndex(Index) ? PartHaloMeshComps[Index] : nullptr;
	if (!HaloComp) return;

	const FPartRuntimeData& Runtime = Parts[Index];
	const FPartHaloConfig& Cfg      = Runtime.Entry.HaloConfig;
	const float CrackValue          = Runtime.bCracked ? 1.0f : 0.0f;

	for (int32 m = 0; m < HaloComp->GetNumMaterials(); ++m)
	{
		if (UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(HaloComp->GetMaterial(m)))
		{
			MID->SetScalarParameterValue(Cfg.CrackParamName, CrackValue);
		}
	}
}

void UPartDestructionComponent::SetPartHitDetectionEnabled(int32 Index, bool bEnabled)
{
	if (!Parts.IsValidIndex(Index)) return;

	static const FName DamageLayerTag(TEXT("DamageLayer"));
	for (const FPartCollisionCache& Cache : Parts[Index].CollisionComps)
	{
		UPrimitiveComponent* Comp = Cache.Comp.Get();
		if (!Comp || !Cache.bHadDamageLayer) continue;

		// コリジョンは触らずDamageLayerタグだけ付け外しする
		// 外れている間は攻撃の対象選択 (AnimNotifyState_CommonAttack)
		// から除外されるが、Pawnブロックは残るのですり抜けない
		if (bEnabled) Comp->ComponentTags.AddUnique(DamageLayerTag);
		else          Comp->ComponentTags.Remove(DamageLayerTag);
	}
}

void UPartDestructionComponent::RefreshPartHitDetection()
{
	for (int32 i = 0; i < Parts.Num(); ++i)
	{
		const FPartRuntimeData& Runtime = Parts[i];
		const FName Group = Runtime.Entry.HitAbsorbGroup;

		// 生きている部位と、束ねられていない部位は常に殴れる
		if (!Runtime.bDestroyed || Group == NAME_None)
		{
			SetPartHitDetectionEnabled(i, true);
			continue;
		}

		// 同じ足の上下のように重なった部位が生きて残っている間だけ、壊れた側を対象選択から外す
		// 最後の1つ (生きたメンバーが居ない) は外さず、その足を攻撃で殴れる状態に保つ
		bool bLiveMember = false;
		for (int32 j = 0; j < Parts.Num(); ++j)
		{
			if (j == i || Parts[j].bDestroyed) continue;
			if (Parts[j].Entry.HitAbsorbGroup != Group) continue;

			bLiveMember = true;
			break;
		}
		SetPartHitDetectionEnabled(i, !bLiveMember);
	}
}

void UPartDestructionComponent::BeginPartHaloAttackGlow()
{
	// 攻撃中はずっと部位無敵
	bPartHaloSuppressed = true;

	for (int32 i = 0; i < PartHaloMeshComps.Num(); ++i)
	{
		UStaticMeshComponent* HaloComp = PartHaloMeshComps[i];
		if (!HaloComp || !Parts.IsValidIndex(i)) continue;
		// 発光は頭を含む全部位で行う (破壊済みのみ対象外)
		if (Parts[i].bDestroyed) continue;

		const FPartHaloConfig& Cfg = Parts[i].Entry.HaloConfig;
		for (int32 m = 0; m < HaloComp->GetNumMaterials(); ++m)
		{
			UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(HaloComp->GetMaterial(m));
			if (!MID) MID = HaloComp->CreateAndSetMaterialInstanceDynamic(m);
			if (MID) MID->SetScalarParameterValue(Cfg.AttackGlowParamName, 1.0f);
		}
	}
}

void UPartDestructionComponent::HidePartHalosForAttack()
{
	bool bAnyFade = false;
	for (int32 i = 0; i < PartHaloMeshComps.Num(); ++i)
	{
		UStaticMeshComponent* HaloComp = PartHaloMeshComps[i];
		if (!HaloComp || !Parts.IsValidIndex(i) || Parts[i].bDestroyed) continue;

		const FPartHaloConfig& Cfg = Parts[i].Entry.HaloConfig;

		// 消去対象外の部位 (頭など) は消さず、発光だけ消す
		if (Parts[i].Entry.bExcludeFromHaloHide)
		{
			for (int32 m = 0; m < HaloComp->GetNumMaterials(); ++m)
			{
				UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(HaloComp->GetMaterial(m));
				if (!MID) MID = HaloComp->CreateAndSetMaterialInstanceDynamic(m);
				if (MID) MID->SetScalarParameterValue(Cfg.AttackGlowParamName, 0.0f);
			}
			continue;
		}

		// DitherAlphaを駆動するためDMI化しておく (発光は維持したまま消す)
		for (int32 m = 0; m < HaloComp->GetNumMaterials(); ++m)
		{
			if (!Cast<UMaterialInstanceDynamic>(HaloComp->GetMaterial(m)))
			{
				HaloComp->CreateAndSetMaterialInstanceDynamic(m);
			}
		}

		FPartHaloRuntimeData& HR = PartHaloRuntimes[i];
		HR.bHideFading = true;
		HR.HideElapsed = 0.0f;
		HR.HideFrom    = HR.CurrentDither;	// 現在値から滑らかに消す
		HR.HideTo      = 1.0f;					// 消滅
		bAnyFade = true;
	}

	if (bAnyFade) SetComponentTickEnabled(true);
}

void UPartDestructionComponent::RestorePartHalosAfterAttack()
{
	// 無敵解除
	bPartHaloSuppressed = false;

	bool bAnyFade = false;
	for (int32 i = 0; i < PartHaloMeshComps.Num(); ++i)
	{
		UStaticMeshComponent* HaloComp = PartHaloMeshComps[i];
		if (!HaloComp || !Parts.IsValidIndex(i)) continue;
		// 発光解除は頭を含む全部位 (破壊済みのみ対象外)。消去された部位だけディザで戻る
		if (Parts[i].bDestroyed) continue;

		const FPartHaloConfig& Cfg = Parts[i].Entry.HaloConfig;
		for (int32 m = 0; m < HaloComp->GetNumMaterials(); ++m)
		{
			UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(HaloComp->GetMaterial(m));
			if (!MID) MID = HaloComp->CreateAndSetMaterialInstanceDynamic(m);
			// 再表示時は発光させない
			if (MID) MID->SetScalarParameterValue(Cfg.AttackGlowParamName, 0.0f);
		}

		// 先に可視化してからディザをフェードインする
		// (消えていない頭はCurrentDither=0で実質変化なし)
		HaloComp->SetVisibility(true);

		FPartHaloRuntimeData& HR = PartHaloRuntimes[i];
		HR.bHideFading = true;
		HR.HideElapsed = 0.0f;
		HR.HideFrom    = HR.CurrentDither;	// 現在値から滑らかに表示
		HR.HideTo      = 0.0f;					// 表示
		bAnyFade = true;
	}

	if (bAnyFade) SetComponentTickEnabled(true);
}

void UPartDestructionComponent::ApplyPartHaloScatterForce(int32 PartIndex)
{
	UFieldSystemComponent* FieldComp = PartHaloFieldComps.IsValidIndex(PartIndex) ? PartHaloFieldComps[PartIndex] : nullptr;
	UGeometryCollectionComponent* GCC = PartHaloGCComps.IsValidIndex(PartIndex) ? PartHaloGCComps[PartIndex] : nullptr;
	if (!FieldComp || !GCC) return;

	const FPartHaloConfig& Cfg = Parts[PartIndex].Entry.HaloConfig;

	const FVector Center = PartHaloRuntimes[PartIndex].BreakCenter;
	FieldComp->SetWorldLocation(Center);

	URadialVector* ForceField = NewObject<URadialVector>(FieldComp);
	ForceField->Magnitude = Cfg.BreakScatterForce;
	ForceField->Position  = Center;
	FieldComp->ApplyPhysicsField(true, EFieldPhysicsType::Field_LinearForce, nullptr, ForceField);
}

void UPartDestructionComponent::StartPartHaloBreakFade(int32 PartIndex)
{
	if (!PartHaloRuntimes.IsValidIndex(PartIndex)) return;
	UGeometryCollectionComponent* GCC = PartHaloGCComps.IsValidIndex(PartIndex) ? PartHaloGCComps[PartIndex] : nullptr;
	if (!GCC) return;

	for (int32 i = 0; i < GCC->GetNumMaterials(); ++i)
	{
		GCC->CreateAndSetMaterialInstanceDynamic(i);
	}

	FPartHaloRuntimeData& HR = PartHaloRuntimes[PartIndex];
	HR.bFading     = true;
	HR.FadeElapsed = 0.0f;
	SetComponentTickEnabled(true);
}

void UPartDestructionComponent::SetPartHalosGlow(bool bGlow)
{
	// 発光のみを駆動する (無敵化bPartHaloSuppressedはしない)。破壊済み部位は対象外
	for (int32 i = 0; i < PartHaloMeshComps.Num(); ++i)
	{
		UStaticMeshComponent* HaloComp = PartHaloMeshComps[i];
		if (!HaloComp || !Parts.IsValidIndex(i) || Parts[i].bDestroyed) continue;

		const FPartHaloConfig& Cfg = Parts[i].Entry.HaloConfig;
		for (int32 m = 0; m < HaloComp->GetNumMaterials(); ++m)
		{
			UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(HaloComp->GetMaterial(m));
			if (!MID) MID = HaloComp->CreateAndSetMaterialInstanceDynamic(m);
			if (MID) MID->SetScalarParameterValue(Cfg.AttackGlowParamName, bGlow ? 1.0f : 0.0f);
		}
	}
}

void UPartDestructionComponent::BeginPartHaloRecoveryDamage()
{
	SetPartHalosGlow(true);
	bRecoveryTouchActive = true;
	RecoveryTouchCooldowns.Reset();
	RecoveryTouchVfxCooldown = 0.0f;
	SetComponentTickEnabled(true);
}

void UPartDestructionComponent::EndPartHaloRecoveryDamage()
{
	bRecoveryTouchActive = false;
	RecoveryTouchCooldowns.Reset();
	SetPartHalosGlow(false);
	// Tickは他要因 (フェード等) が無ければTickComponent末尾の判定で自動停止する
}

void UPartDestructionComponent::TickRecoveryTouchDamage(float DeltaTime)
{
	AActor* Owner = GetOwner();
	UWorld* World = Owner ? Owner->GetWorld() : nullptr;
	if (!World) return;

	// 既存クールダウンを進め、切れた/無効な対象は掃除する
	for (auto It = RecoveryTouchCooldowns.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
			continue;
		}
		It.Value() -= DeltaTime;
		if (It.Value() <= 0.0f) It.RemoveCurrent();
	}

	// VFX発生の全体マージンを進める
	if (RecoveryTouchVfxCooldown > 0.0f) RecoveryTouchVfxCooldown -= DeltaTime;

	const float Radius = RecoveryTouch.AoeRadius;
	if (Radius <= 0.0f) return;

	FCollisionObjectQueryParams ObjQuery(ECC_Pawn);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(PartHaloRecoveryTouch), false, Owner);
	Params.AddIgnoredActor(Owner);
	const FCollisionShape Sphere = FCollisionShape::MakeSphere(Radius);

	// 「光輪に触れるとダメージ」なので、発光している (＝破壊されていない) 光輪ごとに、
	// その部位アタッチ先の光輪メッシュ位置を中心に接触判定する
	// 対象はRecoveryTouchCooldownsで間隔内1回に絞られるため、複数光輪の範囲が
	// 重なっても二重ヒットしない
	for (int32 i = 0; i < PartHaloMeshComps.Num(); ++i)
	{
		UStaticMeshComponent* HaloComp = PartHaloMeshComps[i];
		if (!HaloComp || !Parts.IsValidIndex(i) || Parts[i].bDestroyed || !HaloComp->IsVisible()) continue;

		const FVector Center = HaloComp->GetComponentLocation();

	#if !UE_BUILD_SHIPPING
		if (UTideGameSettings::Get()->bDebugDrawAttackHitbox)
		{
			DrawDebugSphere(World, Center, Radius, 16, FColor::Red, false, -1.0f, 0, 1.5f);
		}
	#endif

		TArray<FOverlapResult> Overlaps;
		World->OverlapMultiByObjectType(Overlaps, Center, FQuat::Identity, ObjQuery, Sphere, Params);

		for (const FOverlapResult& Overlap : Overlaps)
		{
			AActor* Actor = Overlap.GetActor();
			if (!Actor || RecoveryTouchCooldowns.Contains(Actor)) continue;
			if (!TideCombatUtil::IsHostileTo(Owner, Actor)) continue;
			if (FVector::DistSquared(Center, Actor->GetActorLocation()) > FMath::Square(Radius)) continue;

			IDamageable* Target = Cast<IDamageable>(Actor);
			if (!Target) continue;

			FDamageInfo DamageInfo;
			DamageInfo.BaseDamage     = RecoveryTouch.Damage;
			DamageInfo.HitReactionTag = RecoveryTouch.HitReactionTag;
			DamageInfo.Instigator     = Owner;
			Target->ReceiveDamage(DamageInfo);

			RecoveryTouchCooldowns.Add(Actor, RecoveryTouch.RehitInterval);

			// ヒットVFX: 触れた光輪の位置でバースト。全体マージン中は多重起動を避けて出さない
			if (RecoveryTouch.TouchEffect && RecoveryTouchVfxCooldown <= 0.0f)
			{
				UNiagaraComponent* NC = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
					Owner, RecoveryTouch.TouchEffect, Center);
				if (NC) NC->SetWorldScale3D(FVector(RecoveryTouch.TouchEffectScale));
				RecoveryTouchVfxCooldown = RecoveryTouch.TouchEffectMinInterval;
			}
		}
	}
}
