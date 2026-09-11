// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/Actors/Prop/BreakableProp.h"

#include "Components/StaticMeshComponent.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "Field/FieldSystemComponent.h"
#include "Field/FieldSystemObjects.h"
#include "NiagaraFunctionLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"

#include "DrawDebugHelpers.h"
#include "Kismet/KismetMathLibrary.h"

#include "PRJ_TIDE_P0/Actors/Environment/WindZone.h"

#include "PRJ_TIDE_P0/Components/Targeting/LockOnTargetComponent.h"

#include "PRJ_TIDE_P0/Data/Combat/DamageInfo.h"
#include "PRJ_TIDE_P0/Data/Tags/TideGameplayTags.h"
#include "PRJ_TIDE_P0/Actors/Character/Player/TidePlayerCharacter.h"

#if !UE_BUILD_SHIPPING
#include "PRJ_TIDE_P0/Integration/ImGui/Gameplay/DamageNumberDebug.h"
#endif

namespace
{
	const FName FragmentCollisionProfileName(TEXT("HaloFragment"));
}

ABreakableProp::ABreakableProp()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	VisualMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisualMesh"));
	VisualMesh->SetCollisionObjectType(ECC_WorldDynamic);
	VisualMesh->SetCollisionResponseToAllChannels(ECR_Block);
	VisualMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	VisualMesh->ComponentTags.Add(TEXT("DamageLayer"));
	RootComponent = VisualMesh;

	GCComp = CreateDefaultSubobject<UGeometryCollectionComponent>(TEXT("GCComp"));
	GCComp->SetupAttachment(RootComponent);
	GCComp->SetSimulatePhysics(false);
	GCComp->SetVisibility(false);
	// 破片はプレイヤー・カメラをブロックしない
	GCComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	GCComp->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	FieldComp = CreateDefaultSubobject<UFieldSystemComponent>(TEXT("FieldComp"));
	FieldComp->SetupAttachment(RootComponent);

	// 攻撃吸着のターゲット部位。コリジョンは持たず位置マーカーとして機能する
	// (吸着検出はVisualMeshのECC_Pawn応答で行われる)
	LockOnTargetComp = CreateDefaultSubobject<ULockOnTargetComponent>(TEXT("LockOnTargetComp"));
	LockOnTargetComp->SetupAttachment(RootComponent);

	BlowoffReactionTag = TAG_HitReaction_Blowoff;
}

void ABreakableProp::BeginPlay()
{
	Super::BeginPlay();
	CurrentHP = MaxHP;

	// 破片をVisibilityから外す。足IK・視線判定・接地エフェクトのトレースが破片を地面や遮蔽物と
	// して拾ってしまうため。BPのコリジョンプリセットに勝ち、かつChaosプロキシ生成
	// (Break)より前に反映させる必要があるのでコンストラクタではなくここで設定する
	GCComp->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);

	// 無傷のまま剛体として落とす場合はここでシミュレートを開始する
	// エディタのSimulate Physicsチェックは実体に効かないことがあるため、
	// コードで確定させる。破壊時はBreak
	// ()がVisualMeshを隠してGCComp側の物理へ切り替える
	if (bSimulatePhysicsWhenIntact)
	{
		// 同位置・非表示のGCCompが剛体VisualMeshとめり込み、開始直後に弾き飛ばされるため、
		// 破壊までGCCompの当たりを切る(破壊時Break() で戻す)
		GCComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		VisualMesh->SetSimulatePhysics(true);
	}

	for (int32 i = 0; i < VisualMesh->GetNumMaterials(); ++i)
	{
		UMaterialInstanceDynamic* MID = VisualMesh->CreateAndSetMaterialInstanceDynamic(i);
		if (MID)
		{
			MID->SetScalarParameterValue(FadeParameterName, 1.0f);
			FadeMIDs.Add(MID);
		}
	}
#if !UE_BUILD_SHIPPING
	if (WindZone)
	{
		// デバッグ用：風の遮蔽判定を可視化する
		bShowDebugWindExclusion = true;
		SetActorTickEnabled(true);
	}
#endif
}

void ABreakableProp::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

#if !UE_BUILD_SHIPPING
	if (bShowDebugWindExclusion)
	{
		DebugDrawWindExclusion();
	}
#endif

	if (bDamageShaking)
	{
		DamageShakeElapsed += DeltaTime;
		const float OffsetX = FMath::Sin(DamageShakeElapsed * DamageShakeFrequency) * DamageShakeAmplitude;
		SetActorLocation(FVector(ShakeBaseLocation.X + OffsetX, ShakeBaseLocation.Y, ShakeBaseLocation.Z));
	}

	if (!bFading) return;

	FadeElapsed += DeltaTime;
	const float Alpha = 1.0f - FMath::Clamp(FadeElapsed / FadeDuration, 0.0f, 1.0f);

	for (UMaterialInstanceDynamic* MID : FadeMIDs)
	{
		if (MID)
		{
			MID->SetScalarParameterValue(FadeParameterName, Alpha);
		}
	}

	if (FadeElapsed >= FadeDuration)
	{
		Destroy();
	}
}

bool ABreakableProp::AcceptsDamage(const FDamageInfo& DamageInfo) const
{
	if (CurrentHP <= 0) return false;

	// プレイヤーの攻撃でのみ破壊可能。敵の攻撃では壊れない(ヒット演出も出さない)
	// bCanBreakPropsを立てた攻撃だけは例外(環境ギミック由来。アリジゴクのコア爆発など)
	if (!DamageInfo.bCanBreakProps
		&& (!DamageInfo.Instigator.IsValid() || !DamageInfo.Instigator->IsA<ATidePlayerCharacter>()))
	{
		return false;
	}

	// 落下(空中チャージ)攻撃のみで壊れる足場: それ以外の攻撃は受け付けない
	// (ヒット演出も出さない)空中チャージ攻撃(AirChargeAttack) と
	// 空中チャージダッシュ(AirChargeDash) の両方を落下攻撃として扱う
	if (bOnlyBreakByAirChargeAttack
		&& DamageInfo.AttackTypeTag != TAG_AttackType_Player_AirChargeAttack
		&& DamageInfo.AttackTypeTag != TAG_AttackType_Player_AirChargeDash)
	{
		return false;
	}

	return true;
}

EDamageResult ABreakableProp::ReceiveDamage(const FDamageInfo& DamageInfo)
{
	if (!AcceptsDamage(DamageInfo)) return EDamageResult::Immune;

	// 耐久は整数・最低1減算。表示もこの実減算量に合わせる
	const int32 AppliedDamage = FMath::Max(1, FMath::FloorToInt(DamageInfo.BaseDamage));
	CurrentHP -= AppliedDamage;

#if !UE_BUILD_SHIPPING
	// デバッグ用ダメージ数値オーバーレイへ通知(フラグOFF時はPush側で無視される)。
	// 壊れ物はUDamageSystemComponentを持たないため、キャラと同じ通知をここで行う
	{
		FVector PopupLocation = GetActorLocation();
		if (DamageInfo.HitResult.bBlockingHit)
		{
			PopupLocation = DamageInfo.HitResult.ImpactPoint;
		}
		PopupLocation.Z += 50.0f;	// 少し上に出す

		FDamageNumberDebug::Push(PopupLocation, static_cast<float>(AppliedDamage), false);
	}
#endif

	{
		const float HPRatioForLog = static_cast<float>(FMath::Max(0, CurrentHP)) / static_cast<float>(MaxHP);
		float StageForLog = 0.0f;
		if (HPRatioForLog <= CrackLargeThreshold) StageForLog = CrackLargeStageValue;
		else if (HPRatioForLog <= CrackSmallThreshold) StageForLog = CrackSmallStageValue;
		UE_LOG(LogTemp, Log, TEXT("[BreakableProp] HP: %d/%d  DamageStage: %.1f"), CurrentHP, MaxHP, StageForLog);
	}

	if (HitEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, HitEffect, DamageInfo.HitResult.ImpactPoint);
	}

	if (CurrentHP <= 0)
	{
		FVector AttackDir = FVector::ForwardVector;
		if (DamageInfo.Instigator.IsValid())
		{
			AttackDir = (DamageInfo.HitResult.ImpactPoint - DamageInfo.Instigator->GetActorLocation()).GetSafeNormal2D();
		}

		// 砕けるヒットではプレイヤーを押し返さない(敵にとどめを刺したときと同じ挙動)。
		// ヒットバックは攻撃側のOnAttackHit(同フレーム)で仕込まれているため、ここで解決すれば
		// 実移動が始まる前に差し替わる。停止か前進(切り抜け)かはプレイヤー側のDAフラグが決める。
		// ヒットストップはとどめスローに置き換わらないのでそのまま残す
		if (bSuppressPlayerKnockbackOnBreak)
		{
			if (ATidePlayerCharacter* Player = Cast<ATidePlayerCharacter>(DamageInfo.Instigator.Get()))
			{
				Player->ResolveAttackHitBackOnLethalHit();
			}
		}

		Break(DamageInfo.HitResult.ImpactPoint, AttackDir);
	}
	else
	{
		const float HPRatio = static_cast<float>(CurrentHP) / static_cast<float>(MaxHP);
		float NewStage = 0.0f;
		if (HPRatio <= CrackLargeThreshold) NewStage = CrackLargeStageValue;
		else if (HPRatio <= CrackSmallThreshold) NewStage = CrackSmallStageValue;

		if (!FMath::IsNearlyEqual(NewStage, CurrentDamageStage))
		{
			CurrentDamageStage = NewStage;
			OnDamageStageChanged(NewStage);
		}
	}

	return EDamageResult::Hit;
}

bool ABreakableProp::CanBeDamaged() const
{
	return CurrentHP > 0;
}

void ABreakableProp::Break(const FVector& ImpactLocation, const FVector& AttackDirection)
{
	// コリジョンを消す前に、上に乗っている対象を吹き飛ばす (位置がまだ天面にあるうちに拾う)
	BlowoffActorsOnTop();

	VisualMesh->SetVisibility(false);
	VisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GCComp->SetVisibility(true);

	// 破壊後は吸着対象から外す
	// (VisualMeshのNoCollisionで検出からも外れるが明示しておく)
	if (LockOnTargetComp)
	{
		LockOnTargetComp->bIsTargetable = false;
	}

	CachedImpactLocation  = ImpactLocation;
	CachedAttackDirection = AttackDirection;

	// 画面が見えなくなる副作用があったので、P0は光輪とは別で地面すり抜けのままで
	/*GCComp->SetPerLevelCollisionProfileNames({FragmentCollisionProfileName});
	GCComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);*/

	if (bSimulatePhysicsWhenIntact)
	{
		GCComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}

	// Chaosシミュレーション開始
	GCComp->SetSimulatePhysics(true);

	// GCCompのMIDを作成して破壊前のDamageStageを引き継ぐ
	FadeMIDs.Empty();
	for (int32 i = 0; i < GCComp->GetNumMaterials(); ++i)
	{
		UMaterialInstanceDynamic* MID = GCComp->CreateAndSetMaterialInstanceDynamic(i);
		if (MID)
		{
			MID->SetScalarParameterValue(DamageStageParameterName, CurrentDamageStage);
			MID->SetScalarParameterValue(FadeParameterName, 1.0f);
			FadeMIDs.Add(MID);
		}
	}

	// GCのDamage Thresholdを超えるStrainを均一に与えてクラスターを砕く
	URadialFalloff* StrainField = NewObject<URadialFalloff>(this);
	StrainField->Magnitude  = BreakStrainMagnitude;
	StrainField->Radius     = BreakFieldRadius;
	StrainField->Position   = ImpactLocation;
	StrainField->Falloff    = EFieldFalloffType::Field_FallOff_None;
	StrainField->MinRange   = 0.0f;
	StrainField->MaxRange   = 1.0f;
	StrainField->Default    = 0.0f;

	FieldComp->ApplyPhysicsField(true, EFieldPhysicsType::Field_ExternalClusterStrain,
		nullptr, StrainField);

	// 破片は次の物理ステップでフリーボディになるため、力場は遅らせて加える
	GetWorldTimerManager().SetTimer(ScatterTimerHandle, this,
		&ABreakableProp::ApplyScatterForce, 0.05f, false);

	if (BreakEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, BreakEffect, GetActorLocation());
	}

	if (DropClass)
	{
		UWorld* World = GetWorld();
		const FVector Origin = GetActorLocation();

		for (int32 i = 0; i < DropCount; ++i)
		{
			const float Angle = FMath::FRandRange(0.0f, 2.0f * PI);
			const float Dist  = FMath::FRandRange(0.0f, DropScatterRadius);
			const FVector Offset = FVector(FMath::Cos(Angle) * Dist, FMath::Sin(Angle) * Dist, 0.0f);
			World->SpawnActor<AActor>(DropClass, Origin + Offset, FRotator::ZeroRotator);
		}
	}

	// フェードあり：DebrisLifetime - FadeDuration後にフェード開始、
	// フェード完了時にDestroyフェードなし：SetLifeSpanで即時消去
	if (DebrisLifetime > 0.0f)
	{
		if (FadeDuration > 0.0f)
		{
			const float FadeStartDelay = FMath::Max(0.0f, DebrisLifetime - FadeDuration);
			GetWorldTimerManager().SetTimer(FadeStartTimerHandle, this,
				&ABreakableProp::StartFade, FadeStartDelay, false);
		}
		else
		{
			SetLifeSpan(DebrisLifetime);
		}
	}

	// デバッグ描画無効化
	bShowDebugWindExclusion = false;
}

void ABreakableProp::BlowoffActorsOnTop()
{
	if (!bBlowoffActorsOnBreak || !VisualMesh) return;

	UWorld* World = GetWorld();
	if (!World) return;

	// VisualMeshのバウンズを基準に、天面 +マージン上を覆うカプセルでポーンをスイープ検出する
	// メッシュのピボット位置に依存しないようBounds.Originを中心にする
	const FBoxSphereBounds B = VisualMesh->Bounds;
	const float Radius     = FMath::Max(B.BoxExtent.X, B.BoxExtent.Y) + BlowoffDetectPadding;
	const float HalfHeight = B.BoxExtent.Z + BlowoffDetectPadding;

	FCollisionObjectQueryParams ObjQuery(ECC_Pawn);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BreakablePropBlowoff), false, this);
	Params.AddIgnoredActor(this);

	const FCollisionShape Shape = FCollisionShape::MakeCapsule(Radius, HalfHeight);
	const FVector SweepStart = B.Origin - FVector(0.0f, 0.0f, 1.0f);
	const FVector SweepEnd   = B.Origin + FVector(0.0f, 0.0f, 1.0f);

	TArray<FHitResult> Hits;
	World->SweepMultiByObjectType(
		Hits, SweepStart, SweepEnd, FQuat::Identity, ObjQuery, Shape, Params);

	// 同一アクターに複数コンポーネントがヒットしても1回に絞る
	TSet<AActor*> Processed;
	for (const FHitResult& Hit : Hits)
	{
		AActor* Other = Hit.GetActor();
		if (!Other || Other == this) continue;
		if (Processed.Contains(Other)) continue;

		// 【仮対処】チャージダッシュで横から壊した張本人のプレイヤーが、
		// 横方向に広い検出カプセルに
		// 入って吹き飛ばし(のけぞり)を食らうため、プレイヤーは天面Blowoffの対象から除外する
		// 本来は壊したInstigatorをBreak経由で受け取って除外するのが筋
		if (Other->IsA<ATidePlayerCharacter>()) continue;

		IDamageable* Damageable = Cast<IDamageable>(Other);
		if (!Damageable) continue;

		Processed.Add(Other);

		// 柱中心から対象へ向かう水平方向へ吹き飛ばす
		// (上方向のアークはBlowoffシーケンス側が付ける)
		FVector LaunchDir = (Other->GetActorLocation() - B.Origin).GetSafeNormal2D();
		if (LaunchDir.IsNearlyZero()) LaunchDir = GetActorForwardVector().GetSafeNormal2D();

		FDamageInfo DamageInfo;
		DamageInfo.BaseDamage                = BlowoffDamage;
		DamageInfo.Instigator                = this;
		DamageInfo.HitReactionTag            = BlowoffReactionTag;
		DamageInfo.KnockbackDirectionOverride = LaunchDir;
		DamageInfo.HitResult                 = Hit;

		Damageable->ReceiveDamage(DamageInfo);
	}
}

void ABreakableProp::ApplyScatterForce()
{
	// ラジアル散布の原点を攻撃者側へオフセットする→ 原点より遠い側(攻撃方向の先)に破片が飛ぶ
	const FVector RadialOrigin = CachedImpactLocation - CachedAttackDirection * BreakFieldRadius;

	URadialVector* ForceField = NewObject<URadialVector>(FieldComp);
	ForceField->Magnitude = BreakScatterForce;
	ForceField->Position  = RadialOrigin;

	FieldComp->ApplyPhysicsField(true, EFieldPhysicsType::Field_LinearForce,
		nullptr, ForceField);
}

void ABreakableProp::OnDamageStageChanged(float NewStage)
{
	for (UMaterialInstanceDynamic* MID : FadeMIDs)
	{
		if (MID) MID->SetScalarParameterValue(DamageStageParameterName, NewStage);
	}

	// 剛体シミュレート中はSetActorLocationベースの振動が物理と競合するので振動はしない
	if (VisualMesh->IsSimulatingPhysics()) return;

	ShakeBaseLocation = GetActorLocation();
	bDamageShaking = true;
	DamageShakeElapsed = 0.0f;
	SetActorTickEnabled(true);
	GetWorldTimerManager().SetTimer(DamageShakeTimerHandle, this,
		&ABreakableProp::OnDamageShakeEnd, DamageShakeDuration, false);
}

void ABreakableProp::OnDamageShakeEnd()
{
	bDamageShaking = false;
	DamageShakeElapsed = 0.0f;
	SetActorLocation(ShakeBaseLocation);

	if (!bFading) SetActorTickEnabled(bShowDebugWindExclusion);
}

void ABreakableProp::DebugDrawWindExclusion()
{
	if (WindZone && WindZone->OcclusionDirectionMode == EWindDirectionMode::Fixed)
	{
		// 1. 風の直交平面(のSide軸)を定義
		FVector NormalizedWind = WindZone->GetActorForwardVector();
		FRotator WindRotation = UKismetMathLibrary::MakeRotFromX(NormalizedWind);
		FVector SideDir = UKismetMathLibrary::GetRightVector(WindRotation);

		// 2. 岩の【ローカル】のOBB情報と【ワールド】トランスフォームを取得
		FVector LocalOrigin, LocalExtents;
		VisualMesh->GetLocalBounds(LocalOrigin, LocalExtents);

		FTransform MeshTransform = VisualMesh->GetComponentTransform();
		FVector WorldOrigin = MeshTransform.TransformPosition(LocalOrigin); // 正しいワールド中心

		// 3. ローカルの8頂点を定義し、ワールド空間へ変換する
		FVector Vertices[8];
		Vertices[0] = MeshTransform.TransformPosition(LocalOrigin + FVector(LocalExtents.X, LocalExtents.Y, LocalExtents.Z));
		Vertices[1] = MeshTransform.TransformPosition(LocalOrigin + FVector(LocalExtents.X, LocalExtents.Y, -LocalExtents.Z));
		Vertices[2] = MeshTransform.TransformPosition(LocalOrigin + FVector(LocalExtents.X, -LocalExtents.Y, LocalExtents.Z));
		Vertices[3] = MeshTransform.TransformPosition(LocalOrigin + FVector(LocalExtents.X, -LocalExtents.Y, -LocalExtents.Z));
		Vertices[4] = MeshTransform.TransformPosition(LocalOrigin + FVector(-LocalExtents.X, LocalExtents.Y, LocalExtents.Z));
		Vertices[5] = MeshTransform.TransformPosition(LocalOrigin + FVector(-LocalExtents.X, LocalExtents.Y, -LocalExtents.Z));
		Vertices[6] = MeshTransform.TransformPosition(LocalOrigin + FVector(-LocalExtents.X, -LocalExtents.Y, LocalExtents.Z));
		Vertices[7] = MeshTransform.TransformPosition(LocalOrigin + FVector(-LocalExtents.X, -LocalExtents.Y, -LocalExtents.Z));

		// 4. 岩の中心からの相対位置にしてから、Side軸に投影して最小/最大を求める
		float MinSide = FLT_MAX;
		float MaxSide = -FLT_MAX;

		for (const FVector& Vertex : Vertices)
		{
			FVector RelativeVertex = Vertex - WorldOrigin;

			float ProjectedLeft = FVector::DotProduct(RelativeVertex, SideDir);
			if (ProjectedLeft < MinSide) MinSide = ProjectedLeft;
			if (ProjectedLeft > MaxSide) MaxSide = ProjectedLeft;
		}

		// 純粋な横幅
		float Side = (MaxSide - MinSide) * 0.9f;
		// 5. 判定エリアの定義
		float Depth = WindZone->OcclusionTraceDistance * WindZone->OcclusionFalloffStartRatio;
		// ベースとなる中心位置(風の順方向に押し出した位置)
		FVector Center = VisualMesh->Bounds.Origin + WindZone->GetActorForwardVector() * Depth * 0.5f;

		// 横幅(Side)と高さ(Z)の半幅を設定
		FVector Extent(Depth * 0.5f, Side * 0.5f, VisualMesh->Bounds.BoxExtent.Z);

		// 6. 風の向き(WindRotation)に合わせて赤箱を描画する
		DrawDebugBox(GetWorld(), Center, Extent, WindRotation.Quaternion(), FColor::Red);
	}
}

void ABreakableProp::StartFade()
{
	bFading = true;
	FadeElapsed = 0.0f;
	SetActorTickEnabled(true);
}
