// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/Components/Equipment/HaloComponent.h"
#include "PRJ_TIDE_P0/Actors/Character/Enemy/EnemyCharacter.h"
#include "PRJ_TIDE_P0/Components/AI/EnemyBattleComponent.h"
#include "PRJ_TIDE_P0/Data/Enemy/EnemyDataAsset.h"
#include "PRJ_TIDE_P0/Data/Tags/TideGameplayTags.h"
#include "PRJ_TIDE_P0/Utilities/TideCombatUtils.h"
#include "PRJ_TIDE_P0/Interfaces/Combat/IDamageable.h"
#include "PRJ_TIDE_P0/Core/TideGameSettings.h"

#include "AIController.h"
#include "BrainComponent.h"
#include "Engine/OverlapResult.h"
#include "GeometryCollection/GeometryCollectionActor.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "Field/FieldSystemComponent.h"
#include "Field/FieldSystemObjects.h"
#include "Kismet/GameplayStatics.h"
#include "Components/SphereComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"

namespace
{
	const FName HaloCameraSweepIgnoreTag(TEXT("CameraSweepIgnore"));
	// 破片用コリジョンプロファイル。背景(WorldStatic)のみブロックし他は無視する
	// DefaultEngine.iniの[/Script
	// /Engine.CollisionProfile] に定義
	const FName HaloBreakFragmentProfileName(TEXT("HaloFragment"));
#if !UE_BUILD_SHIPPING
	// ランタイム設定 (コンソール/ini) で光輪当たり判定のデバッグ描画を切り替える
	bool IsHaloDebugDraw()
	{
		const UTideGameSettings* Settings = UTideGameSettings::Get();
		return Settings && Settings->bDebugDrawHaloHitbox;
	}
#endif
}

UHaloComponent::UHaloComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// 製品ビルドは必要時のみTickを有効化する。非シッピングはImGuiでデバッグ描画を
	// いつでも切り替えられるよう常時Tickさせる
#if UE_BUILD_SHIPPING
	PrimaryComponentTick.bStartWithTickEnabled = false;
#else
	PrimaryComponentTick.bStartWithTickEnabled = true;
#endif
}

void UHaloComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

#if !UE_BUILD_SHIPPING
	if (IsHaloDebugDraw()) DrawHaloDebug();
#endif

	// 破壊破片フェード
	if (bHaloBreakFading && Data && ActiveHaloBreakGCA.IsValid())
	{
		HaloBreakFadeElapsed += DeltaTime;
		const float t = FMath::Clamp(HaloBreakFadeElapsed / Data->HaloBreakFadeDuration, 0.0f, 1.0f);

		if (UGeometryCollectionComponent* GCC = ActiveHaloBreakGCA->GetGeometryCollectionComponent())
		{
			for (int32 i = 0; i < GCC->GetNumMaterials(); ++i)
			{
				if (UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(GCC->GetMaterial(i)))
				{
					MID->SetScalarParameterValue(Data->HaloDitherParamName, t);
				}
			}
		}
		if (t >= 1.0f) bHaloBreakFading = false;
	}

	// 胴体フレネルのフェード ※光輪の健在/無防備に連動
	if (bBodyFresnelFading && Data && SkelMesh)
	{
		const float Duration = FMath::Max(Data->HaloBreakFresnelFadeOutTime, KINDA_SMALL_NUMBER);
		const float Speed    = FMath::Max(Data->HaloBreakFresnelIntensity, KINDA_SMALL_NUMBER) / Duration;
		FresnelCurrent = FMath::FInterpConstantTo(FresnelCurrent, FresnelTarget, DeltaTime, Speed);
		SkelMesh->SetCustomPrimitiveDataFloat(0, FresnelCurrent);
		if (FMath::IsNearlyEqual(FresnelCurrent, FresnelTarget)) bBodyFresnelFading = false;
	}

	// ガード解除ディザ
	if (bHaloDithering && Data && HasHaloDMI())
	{
		HaloDitherElapsed += DeltaTime;
		const float t     = FMath::Clamp(HaloDitherElapsed / Data->HaloDitherTime, 0.0f, 1.0f);
		const float Alpha = bHaloDitherOut ? t : (1.0f - t);
		SetHaloScalar(Data->HaloDitherParamName, Alpha);

		if (t >= 1.0f)
		{
			HaloDitherElapsed = 0.0f;
			if (bHaloDitherOut)
			{
				// 消え終わり → アタッチ変更してから出現アニメ開始
				RestoreHaloToStateSocket();
				SetHaloScalar(Data->HaloDitherParamName, 1.0f);
				bHaloDitherOut = false;
			}
			else
			{
				// 出現終わり → 完全表示で終了
				bHaloDithering  = false;
				bHaloDitherOut = true;
				SetHaloScalar(Data->HaloDitherParamName, 0.0f);
			}
		}
	}

	// ダメージウィンドウ中: 毎フレーム範囲内を取り直して当てる (1体につき1回)
	// 時間切れで閉じて再受付クールダウンへ入る
	if (bHaloTouchWindowActive)
	{
		ApplyHaloTouchDamage();
		if (bHaloTouchWindowActive)
		{
			HaloTouchWindowRemaining -= DeltaTime;
			if (HaloTouchWindowRemaining <= 0.0f) CloseHaloTouchWindow();
		}
	}
	// 接触ダメージ: Deploy中はトリガー球
	// (HaloTouchTriggerRadius)内の敵対Pawnを毎フレーム拾って再トリガーする
	// Overlapの開始イベントだけだと「範囲内に居続ける」間の再発火ができないため
	// クールダウン中/計測中はスキップし、OnHaloOverlapBegin側の判定(敵対
	// /Deploy) を再利用する
	else if (bHaloDeployed && HaloTouchVolumeComp && !bHaloDamageCooling
		&& !GetOwner()->GetWorldTimerManager().IsTimerActive(HaloTouchTimerHandle))
	{
		TArray<AActor*> Overlapping;
		HaloTouchVolumeComp->GetOverlappingActors(Overlapping);
		for (AActor* Actor : Overlapping)
		{
			OnHaloOverlapBegin(HaloTouchVolumeComp, Actor, nullptr, 0, false, FHitResult());
			if (GetOwner()->GetWorldTimerManager().IsTimerActive(HaloTouchTimerHandle)) break;
		}
	}

	// ヒットしたが壊れなかったときの3軸ブレ
	if (HaloShakeState.IsActive())
	{
		HaloShakeState.Tick(DeltaTime, HaloMeshComp);
	}

	// tickが不要かチェック
	bool bNeedTick = bHaloDithering || bHaloBreakFading || bHaloDeployed || HaloShakeState.IsActive()
		|| bBodyFresnelFading || bHaloTouchWindowActive;
#if !UE_BUILD_SHIPPING
	// 非シッピングは常時Tickを維持し、ImGuiでデバッグ描画をいつでもON/OFFできるようにする
	bNeedTick = true;
#endif
	if (!bNeedTick) SetComponentTickEnabled(false);
}

void UHaloComponent::Initialize(const UEnemyDataAsset* InData, USkeletalMeshComponent* InSkelMesh, UEnemyBattleComponent* InBattle)
{
	Data            = InData;
	SkelMesh        = InSkelMesh;
	BattleComponent = InBattle;

	// 操作対象の光輪プリミティブをここで一度だけ解決し、以降は使い回す
	HaloMeshComp = ResolveHaloPrimitive();
}

UPrimitiveComponent* UHaloComponent::ResolveHaloPrimitive() const
{
	// AEnemyCharacterがDA (HaloMesh)
	// からOnConstructionで構成した光輪メッシュ
	// メッシュ未設定 (BackHalo運用など光輪を持たない敵) はnullptrを返し、
	// 以降の光輪処理が無効化される
	if (const AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(GetOwner()))
	{
		if (UStaticMeshComponent* Mesh = Enemy->GetHaloMeshComponent(); Mesh && Mesh->GetStaticMesh())
		{
			return Mesh;
		}
	}
	return nullptr;
}

void UHaloComponent::InitializeDMI()
{
	// 破片と同じく全マテリアルスロットをMID化する。スロット0だけだと、DitherAlpha等を
	// 持つマテリアルが別スロットにある光輪でパラメータが効かない
	HaloDMIs.Reset();
	if (HaloMeshComp)
	{
		const int32 NumMaterials = HaloMeshComp->GetNumMaterials();
		for (int32 i = 0; i < NumMaterials; ++i)
		{
			if (UMaterialInstanceDynamic* MID = HaloMeshComp->CreateAndSetMaterialInstanceDynamic(i))
			{
				HaloDMIs.Add(MID);
			}
		}
	}

	// 初期状態は通常待機なので暗めにしておく
	SetHaloAttackGlow(false);

	// 常時ヒビの背中光輪は初期状態からヒビを張っておく
	if (ShouldBackHaloStayCracked())
	{
		SetHaloCracked(true);
	}

	// 常時フレネル: 光輪が健在な初期状態から点灯させる
	// (光輪を持たない敵はSetFresnelActive内で無効)
	SetFresnelActive(true, /*bImmediate=*/true);
}

void UHaloComponent::SetHaloScalar(FName Param, float Value)
{
	// 全スロットのMIDへ同じパラメータを流す
	for (const TObjectPtr<UMaterialInstanceDynamic>& MID : HaloDMIs)
	{
		if (MID) MID->SetScalarParameterValue(Param, Value);
	}
}

void UHaloComponent::SetHaloAttackGlow(bool bBright)
{
	// 破壊破片の色に流用するため、DMI無しでも状態は保持する
	bHaloAttackGlow = bBright;
	if (!HasHaloDMI() || !Data) return;
	SetHaloScalar(Data->HaloAttackGlowParamName, bBright ? 1.0f : 0.0f);
}

void UHaloComponent::SetHaloGlowIntensity(float Intensity)
{
	if (!HasHaloDMI() || !Data) return;
	SetHaloScalar(Data->HaloAttackGlowParamName, Intensity);
}

void UHaloComponent::BindHaloOverlap(FName HaloComponentName)
{
	// Initializeで解決済み。未解決 (名前不一致・メッシュ未構成) なら何もしない
	UPrimitiveComponent* Prim = HaloMeshComp;
	if (!Prim) return;

	// ストリーミング再入・再バインドで残留束縛と衝突して二重束縛のensureが出るのを防ぐ
	Prim->OnComponentBeginOverlap.AddUniqueDynamic(this, &UHaloComponent::OnHaloOverlapBegin);

	USphereComponent* Barrier = NewObject<USphereComponent>(GetOwner(), TEXT("HaloBarrier"));
	Barrier->RegisterComponent();
	Barrier->AttachToComponent(Prim, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	Barrier->SetSphereRadius(Data ? Data->HaloBarrierRadius : 60.0f);
	Barrier->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Barrier->SetCollisionObjectType(ECC_WorldDynamic);
	Barrier->SetCollisionResponseToAllChannels(ECR_Ignore);
	Barrier->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	// ガード中はこのバリア球を攻撃の被弾面にもする
	// プレイヤー攻撃のスイープ
	// (SweepMultiByObjectType)はDamageLayerタグ付き
	// コンポーネントのみヒット対象にするため、
	// タグを付けて光輪リング位置で被弾させる
	// 部位タグ (Part.*) は持たせないのでボディ扱い -> ガード割り込みに乗る
	Barrier->ComponentTags.Add(TEXT("DamageLayer"));
	HaloBarrierComp = Barrier;

	// 接触トリガー用オーバーラップ球。攻撃の被弾(バリア)とは別系統
	// ブロックしないので間合いはバリアが、接触トリガーはこの球が担当する
	// 半径はHaloTouchTriggerRadius
	// ダメージ範囲 (HaloTouchAoeRadius) とは別物なので混同しないこと
	// 絶対スケールにしてモンタージュの光輪拡縮に追従させない(サイズ安定)
	// コリジョンはSetDeployedで開閉する。常時QueryOnlyだと、竜巻など
	// AllDynamicObjectsのオブジェクトクエリ (コンポーネントのレスポンス設定を無視し
	// ObjectTypeだけで拾う) が待機中の光輪位置でこの球を掴み、見た目より広く巻き込む
	USphereComponent* TouchVolume = NewObject<USphereComponent>(GetOwner(), TEXT("HaloTouchVolume"));
	TouchVolume->RegisterComponent();
	TouchVolume->AttachToComponent(Prim, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	TouchVolume->SetAbsolute(false, false, true);
	TouchVolume->SetSphereRadius(Data ? Data->HaloTouchTriggerRadius : 210.0f);
	TouchVolume->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TouchVolume->SetCollisionObjectType(ECC_WorldDynamic);
	TouchVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	TouchVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	TouchVolume->SetGenerateOverlapEvents(true);
	TouchVolume->OnComponentBeginOverlap.AddUniqueDynamic(this, &UHaloComponent::OnHaloOverlapBegin);
	HaloTouchVolumeComp = TouchVolume;
}

void UHaloComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// BindHaloOverlapで張った束縛を対称に解除する
	// GC任せだとOnComponentBeginOverlapのスパースデリゲート束縛が残留し、
	// ストリーミング再入時に二重束縛のensureを招く
	if (HaloMeshComp)
	{
		HaloMeshComp->OnComponentBeginOverlap.RemoveDynamic(this, &UHaloComponent::OnHaloOverlapBegin);
	}
	if (HaloTouchVolumeComp)
	{
		HaloTouchVolumeComp->OnComponentBeginOverlap.RemoveDynamic(this, &UHaloComponent::OnHaloOverlapBegin);
	}

	Super::EndPlay(EndPlayReason);
}

USceneComponent* UHaloComponent::AttachHaloToSocket(FName SocketName, FName HaloComponentName, float Scale)
{
	if (SocketName.IsNone() || !HaloMeshComp) return nullptr;

	bBackHaloActive = false;

	HaloMeshComp->AttachToComponent(
		SkelMesh.Get(),
		FAttachmentTransformRules::SnapToTargetNotIncludingScale,
		SocketName);
	HaloMeshComp->SetRelativeScale3D(FVector(Scale));
	return HaloMeshComp;
}

void UHaloComponent::AttachHaloForAttack(FName SocketName, FName HaloComponentName)
{
	if (SocketName.IsNone()) return;

	// 進行中のガード解除ディザを中止する
	// (ディザ完了時のRestoreHaloToStateSocketが攻撃ソケットを背中へ戻すのを防ぐ)
	bHaloDithering = false;
	if (HasHaloDMI() && Data)
	{
		SetHaloScalar(Data->HaloDitherParamName, 0.0f);
	}
	bBackHaloActive = false;

	if (!HaloMeshComp) return;

	// ガード中に設定された回転絶対モードをリセットしてからアタッチする
	HaloMeshComp->SetAbsolute(false, false, false);
	HaloMeshComp->AttachToComponent(
		SkelMesh.Get(),
		FAttachmentTransformRules::SnapToTargetNotIncludingScale,
		SocketName);
	// 攻撃時はモーション側でスケールを駆動するため基準は中立(1.0)にする
	// HaloNormalScaleを入れるとモーションのスケールに乗算され意図より小さくなる
	HaloMeshComp->SetRelativeScale3D(FVector::OneVector);
}

void UHaloComponent::RestoreHaloToStateSocket(FName HaloComponentName)
{
	if (!Data) return;

	// 光輪が手元に無い間 (投擲中・破壊〜再生待ち) は待機ソケットへ戻さない
	// バリアと背面被弾判定を復活させると、非表示の光輪が当たり判定だけ生き返ってしまう
	// 再生時は各再生ハンドラが先に無防備を解いてからここへ来る
	if (IsHaloAway()) return;

	AEnemyCharacter* Enemy  = Cast<AEnemyCharacter>(GetOwner());
	const bool bGuard       = Enemy && Enemy->HasStateTag(TAG_State_Enemy_Guard);
	const FName Socket      = bGuard ? Data->HaloWaistSocketName : Data->HaloBackSocketName;
	const float Scale       = bGuard ? Data->HaloGuardScale      : Data->HaloNormalScale;

	// ガード(腰)も通常(背中)も暗めに戻す
	SetHaloAttackGlow(false);

	SetHaloBarrierActive(bGuard);
	if (!bGuard && bHaloCracked)
	{
		// 常時ヒビの背中光輪 (bHaloBackAlwaysCracked) は無傷へ戻さず、ヒビを維持する
		SetHaloCracked(false);
	}

	USceneComponent* Comp = AttachHaloToSocket(Socket, HaloComponentName, Scale);
	if (!Comp) return;

	if (bGuard)
	{
		Comp->SetAbsolute(false, true, false);
		Comp->SetWorldRotation(FRotator(0.0f, GetOwner()->GetActorRotation().Yaw, 0.0f));
	}
	else
	{
		Comp->SetAbsolute(false, false, false);
		bBackHaloActive = true;
	}
}

void UHaloComponent::BeginGuard(FName HaloComponentName)
{
	if (AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(GetOwner()))
	{
		Enemy->AddStateTag(TAG_State_Enemy_Guard);
	}
	RestoreHaloToStateSocket(HaloComponentName);

#if !UE_BUILD_SHIPPING
	if (IsHaloDebugDraw())
	{
		SetComponentTickEnabled(true);
	}
#endif
}

void UHaloComponent::StartGuardAutoRelease()
{
	if (!Data) return;

	if (Data->GuardAutoReleaseDuration > 0.0f)
	{
		GetOwner()->GetWorldTimerManager().SetTimer(GuardMaxDurationTimerHandle, this,
			&UHaloComponent::OnGuardAutoReleaseExpired, Data->GuardAutoReleaseDuration, false);
	}

	GetOwner()->GetWorldTimerManager().SetTimer(GuardCheckTimerHandle, this,
		&UHaloComponent::CheckGuardRelease, 0.1f, true);
}

void UHaloComponent::OnGuardAutoReleaseExpired()
{
	// SetTimerがvoid() のメンバ関数ポインタしか取れないためEndGuard(bool)
	// を包む
	EndGuard();
}

void UHaloComponent::SetHaloBarrierActive(bool bEnable)
{
	if (!HaloBarrierComp) return;
	HaloBarrierComp->SetCollisionEnabled(
		bEnable ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
}

void UHaloComponent::SetRecoveryArmor(bool bActive)
{
	// 発光＋接触ダメージ判定のON/OFFのみ
	// 「SA中は壊れない」はオーナーSAを見る既存ゲート
	// (EnemyCharacter::ReceiveDamage) が担うため、
	// ここでは破壊不可フラグを持たない (復帰中ずっと破壊不可になるのを防ぐ)光輪が手元に無い間
	// (投擲中・破壊〜再生待ち) は復帰アーマーを張れない
	// 無防備の定義と矛盾するうえ、見えない光輪で接触ダメージが出てしまう
	const bool bEnable = bActive && !IsHaloAway();
	SetHaloAttackGlow(bEnable);
	SetDeployed(bEnable);

#if !UE_BUILD_SHIPPING
	// 復帰中のタッチAOEをデバッグ描画するためtickを起こす
	// (TickComponent側で維持判定する)
	if (bEnable && IsHaloDebugDraw()) SetComponentTickEnabled(true);
#endif
}

void UHaloComponent::SetDeployed(bool bActive)
{
	bHaloDeployed = bActive;

	// 使う区間だけコリジョンを開ける。他システムのオブジェクトクエリに待機中の球を拾わせない
	// フラグを立てた後に有効化する順序を守ること
	// SetCollisionEnabledがUpdateOverlapsを同期で走らせ、既に内側にいる相手へBeginOverlapを飛ばすため
	if (HaloTouchVolumeComp)
	{
		HaloTouchVolumeComp->SetCollisionEnabled(
			bActive ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	}

	if (!bActive) return;

	// Deploy中は近接判定のためtickを回す (TickComponent側で維持/停止を判定)
	SetComponentTickEnabled(true);

	// 有効化した時点で既に範囲内にいる相手も拾う。接触トリガーは専用球を使う
	UPrimitiveComponent* OverlapSrc = HaloTouchVolumeComp ? HaloTouchVolumeComp.Get() : HaloMeshComp.Get();
	if (!OverlapSrc) return;

	TArray<AActor*> Overlapping;
	OverlapSrc->GetOverlappingActors(Overlapping);
	for (AActor* Actor : Overlapping)
	{
		OnHaloOverlapBegin(OverlapSrc, Actor, nullptr, 0, false, FHitResult());
	}
}

void UHaloComponent::SetHaloThrown(bool bThrown)
{
	bHaloThrown = bThrown;

	// 手元へ戻ったなら技クールの再生待ちも用済み
	if (!bThrown) GetOwner()->GetWorldTimerManager().ClearTimer(HaloThrowRegenTimerHandle);

	UpdateHaloAwayTag();
}

void UHaloComponent::StartThrowRegenIfAway()
{
	// HaloReturnを経て既に戻っている
	if (!bHaloThrown) return;

	// 破壊されたなら再生は破壊クール側が握る
	// 投擲フラグだけ下ろす (無防備はbHaloBrokenが維持する)
	if (bHaloBroken)
	{
		bHaloThrown = false;
		UpdateHaloAwayTag();
		return;
	}

	const float RegenCooldown = Data ? Data->HaloThrowRegenCooldown : 0.0f;
	if (RegenCooldown <= 0.0f)
	{
		OnHaloThrowRegen();
		return;
	}

	// 技クールを消化するまで光輪は飛ばされたまま＝無防備を継続する
	GetOwner()->GetWorldTimerManager().SetTimer(HaloThrowRegenTimerHandle, this,
		&UHaloComponent::OnHaloThrowRegen, RegenCooldown, false);
}

void UHaloComponent::OnHaloThrowRegen()
{
	// クール中に壊された場合は破壊側の再生に任せる
	if (bHaloBroken)
	{
		bHaloThrown = false;
		UpdateHaloAwayTag();
		return;
	}

	// RestoreHaloToStateSocketは無防備中は何もしないので、先に無防備を解く
	bHaloThrown = false;
	UpdateHaloAwayTag();

	if (HaloMeshComp) HaloMeshComp->SetVisibility(true, true);
	RestoreHaloToStateSocket();

	// 破壊からの復帰と同じくディザでフェードインさせる
	if (HasHaloDMI() && Data && Data->HaloDitherTime > 0.0f)
	{
		SetHaloScalar(Data->HaloDitherParamName, 1.0f);
		bHaloDithering     = true;
		bHaloDitherOut    = false;
		HaloDitherElapsed = 0.0f;
		SetComponentTickEnabled(true);
	}
}

void UHaloComponent::UpdateHaloAwayTag()
{
	AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(GetOwner());
	if (!Enemy) return;

	// StateTagComponentは参照カウント制。BackHaloなど他の付与者と共存できるよう、
	// このコンポーネントが持つカウントは常に0か1に保つ (状態が変化したときだけ付け外す)
	const bool bWantAway = bHaloThrown || bHaloBroken;
	if (bWantAway == bHaloAwayTagApplied) return;

	bHaloAwayTagApplied = bWantAway;

	// 光輪が手元にある間だけ胴体フレネルを点灯する (破壊/投擲で無防備なら消す)
	SetFresnelActive(!bWantAway);

	if (bWantAway)
	{
		Enemy->AddStateTag(TAG_State_Enemy_HaloAway);
	}
	else
	{
		Enemy->RemoveStateTag(TAG_State_Enemy_HaloAway);
	}
}

void UHaloComponent::HandleGuardHit(const FDamageInfo& DamageInfo, bool bSuppressReaction)
{
	// 光輪攻撃中 (発光中) は共通仕様として壊れない・ヒビも入らない
	if (bHaloAttackSuppressed) return;

	const FGameplayTag Gear      = DamageInfo.ChargeGearTag;
	const bool bHeavyGear        = (Gear == TAG_Charge_Gear3 || Gear == TAG_Charge_Gear4);
	// 神技 (UGodActionPlayerModule) とスライドパッシブ (突風・竜巻) は
	// ギア・ヒビ状態に関係なく一撃で割る
	const bool bGodAction        = TideCombatUtil::IsHaloBreakerDamage(DamageInfo);

	// ひびなし && 軽いギア (壱・弍) かつ一撃破壊系でない → ひびが入る。ガード継続
	if (!bGodAction && !bHaloCracked && !bHeavyGear)
	{
		bHaloCracked = true;
		if (HasHaloDMI() && Data)
		{
			SetHaloScalar(Data->HaloCrackParamName, 1.0f);
		}

		// 壊れずヒビが入っただけ → 光輪を3軸ブレ
		if (HaloMeshComp && Data)
		{
			HaloShakeState.Trigger(HaloMeshComp, Data->HaloHitShake);
			SetComponentTickEnabled(true);
		}

		FDamageInfo CrackInfo        = DamageInfo;
		CrackInfo.BaseDamage         = 0.0f;
		CrackInfo.HitReactionTag     = (Gear == TAG_Charge_Gear1) ? TAG_HitReaction_Knockback_S : TAG_HitReaction_Knockback_M;
		if (OnGuardPenetrated) OnGuardPenetrated(CrackInfo);
		return;
	}

	// 即割れ (参・極orひびあり状態で再ヒット)
	bHaloCracked = false;
	SetHaloBarrierActive(false);
	GetOwner()->GetWorldTimerManager().ClearTimer(HaloTouchTimerHandle);
	StartHaloTouchFlicker(false);
	// 予告中/判定中に割られたら接触ダメージは無かったことにする
	bHaloTouchWindowActive   = false;
	HaloTouchWindowRemaining = 0.0f;
	HaloTouchDamagedActors.Reset();

	if (Data && HasHaloDMI())
	{
		SetHaloScalar(Data->HaloCrackParamName, 0.0f);
	}

	if (AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(GetOwner()))
	{
		Enemy->RemoveStateTag(TAG_State_Enemy_Guard);
		// 割られた以上、ガード開始モーションを守る理由はもう無い
		// 残すと直後の突破リアクションがHitReactionComponentのSAゲートに消される
		Enemy->ClearGuardMotionArmor();
	}

	// ガード突破でガードを終える。OnAttackEndのStopLogic("Guard")
	// が宙に浮かないよう、突破リアクションが走らない(SA等)場合に備えて停止中ならAIを再開しておく
	// この後OnGuardPenetratedでリアクションが走れば、
	// そちらが改めてStopLogic/RestartLogicする
	ClearGuardReaction();

	FVector ImpulseDir = FVector::ZeroVector;
	if (DamageInfo.HitResult.bBlockingHit)
		ImpulseDir = -DamageInfo.HitResult.ImpactNormal;
	else if (DamageInfo.Instigator.IsValid() && HaloMeshComp)
		ImpulseDir = (HaloMeshComp->GetComponentLocation() - DamageInfo.Instigator->GetActorLocation()).GetSafeNormal();

	float InstigatorDistance = -1.0f;
	if (DamageInfo.Instigator.IsValid() && HaloMeshComp)
	{
		InstigatorDistance = FVector::Distance(HaloMeshComp->GetComponentLocation(), DamageInfo.Instigator->GetActorLocation());
	}
	StartHaloBreak(ImpulseDir, InstigatorDistance);

	if (Data && Data->HaloGuardBreakEffect && DamageInfo.HitResult.bBlockingHit)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(),
			Data->HaloGuardBreakEffect,
			DamageInfo.HitResult.ImpactPoint,
			FRotator::ZeroRotator,
			Data->HaloGuardBreakEffectScale);
	}

	// ガードは割った。リアクションは呼び出し側が後段で通すので、ここでは起こさない
	if (bSuppressReaction) return;

	FDamageInfo PenetratedInfo = DamageInfo;

	if (Gear == TAG_Charge_Gear1)
	{
		PenetratedInfo.BaseDamage    = 0.0f;
		PenetratedInfo.HitReactionTag = TAG_HitReaction_Knockback_S;
	}
	else if (Gear == TAG_Charge_Gear2)
	{
		PenetratedInfo.BaseDamage    = 0.0f;
		PenetratedInfo.HitReactionTag = TAG_HitReaction_Knockback_M;
	}
	else if (Gear == TAG_Charge_Gear3)
	{
		PenetratedInfo.HitReactionTag = TAG_HitReaction_Knockback_L;
	}
	else if (Gear == TAG_Charge_Gear4)
	{
		PenetratedInfo.HitReactionTag = TAG_HitReaction_Blowoff_L;
		// DeathVelocityの計算はOnGuardPenetratedコールバック側で行う
	}
	else
	{
		return;
	}

	if (Data && Data->HaloGuardBreakReactionTag.IsValid())
	{
		PenetratedInfo.HitReactionTag = Data->HaloGuardBreakReactionTag;
	}

#if !UE_BUILD_SHIPPING
	if (UTideGameSettings::Get()->bDebugEnemyNoDamage)
	{
		PenetratedInfo.BaseDamage = 0.0f;
	}
#endif

	if (OnGuardPenetrated) OnGuardPenetrated(PenetratedInfo);
}

bool UHaloComponent::BreakGuardForWind(const FVector& WindCenter)
{
	// 光輪攻撃中 (発光中) は共通仕様として壊れない
	if (bHaloAttackSuppressed) return false;

	AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(GetOwner());
	if (!Enemy || !Enemy->HasStateTag(TAG_State_Enemy_Guard)) return false;

	// 接触ダメージ系の後始末 (HandleGuardHitの即割れ経路と同じ)
	bHaloCracked = false;
	SetHaloBarrierActive(false);
	GetOwner()->GetWorldTimerManager().ClearTimer(HaloTouchTimerHandle);
	StartHaloTouchFlicker(false);
	// 予告中/判定中に割られたら接触ダメージは無かったことにする
	bHaloTouchWindowActive   = false;
	HaloTouchWindowRemaining = 0.0f;
	HaloTouchDamagedActors.Reset();

	if (Data && HasHaloDMI())
	{
		SetHaloScalar(Data->HaloCrackParamName, 0.0f);
	}

	// ガード状態と自動解除タイマーを止める (EndGuardと同じ)
	Enemy->RemoveStateTag(TAG_State_Enemy_Guard);
	GetOwner()->GetWorldTimerManager().ClearTimer(GuardCheckTimerHandle);
	GetOwner()->GetWorldTimerManager().ClearTimer(GuardMaxDurationTimerHandle);

	// StopLogic("Guard") が宙に浮かないよう、停止中なら復帰させる
	// この後に風の巻き上げがStopLogic("Wind") するならそちらが改めて停止/復帰を握る
	ClearGuardReaction();

	// 竜巻中心から外側へ破片を散らす。中心が光輪と一致して方向が出ない場合は真上へ
	FVector ImpulseDir = FVector::UpVector;
	if (HaloMeshComp)
	{
		const FVector Outward = (HaloMeshComp->GetComponentLocation() - WindCenter).GetSafeNormal();
		if (!Outward.IsNearlyZero()) ImpulseDir = Outward;
	}

	const FVector BreakLocation = HaloMeshComp ? HaloMeshComp->GetComponentLocation() : Enemy->GetActorLocation();
	StartHaloBreak(ImpulseDir);

	// ガード突破とは異なりヒット情報が無いので、光輪位置で破壊エフェクトを出す
	if (Data && Data->HaloGuardBreakEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(),
			Data->HaloGuardBreakEffect,
			BreakLocation,
			FRotator::ZeroRotator,
			Data->HaloGuardBreakEffectScale);
	}

	return true;
}

void UHaloComponent::ClearAllTimers()
{
	// 直書きせずSetDeployedを通す (接触トリガー球のコリジョンを閉じるため)
	SetDeployed(false);
	GetOwner()->GetWorldTimerManager().ClearTimer(HaloTouchTimerHandle);
	GetOwner()->GetWorldTimerManager().ClearTimer(HaloVfxTimerHandle);
	GetOwner()->GetWorldTimerManager().ClearTimer(HaloCooldownTimerHandle);
	GetOwner()->GetWorldTimerManager().ClearTimer(GuardCheckTimerHandle);
	GetOwner()->GetWorldTimerManager().ClearTimer(GuardMaxDurationTimerHandle);
	GetOwner()->GetWorldTimerManager().ClearTimer(HaloBreakReformTimerHandle);
	GetOwner()->GetWorldTimerManager().ClearTimer(HaloBreakFadeTimerHandle);
	GetOwner()->GetWorldTimerManager().ClearTimer(HaloBreakScatterTimerHandle);
	GetOwner()->GetWorldTimerManager().ClearTimer(HaloThrowRegenTimerHandle);
	bHaloBreakFading = false;
	StartHaloTouchFlicker(false);
}

void UHaloComponent::EndGuard(bool bRestartAI)
{
	AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(GetOwner());
	if (!Enemy || !Enemy->HasStateTag(TAG_State_Enemy_Guard)) return;

	// 直書きせずSetDeployedを通す (接触トリガー球のコリジョンを閉じるため)
	SetDeployed(false);
	Enemy->RemoveStateTag(TAG_State_Enemy_Guard);
	StartHaloDither();

	GetOwner()->GetWorldTimerManager().ClearTimer(GuardCheckTimerHandle);
	GetOwner()->GetWorldTimerManager().ClearTimer(GuardMaxDurationTimerHandle);

	if (!bRestartAI) return;

	// ガード成立時に立てた行動抑止を解除する
	Enemy->SetReacting(false);
}

void UHaloComponent::ClearGuardReaction()
{
	if (AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(GetOwner()))
	{
		Enemy->SetReacting(false);
	}
}

void UHaloComponent::CheckGuardRelease()
{
	AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(GetOwner());
	if (!Enemy || !BattleComponent) return;

	const float Dist  = Enemy->GetDistToTarget();
	const float Angle = Enemy->GetAngleToTarget();
	if (Dist < 0.0f) return;

	if (BattleComponent->HasReadyAttack(Dist, Angle))
		EndGuard();
}

void UHaloComponent::StartHaloDither()
{
	bHaloDithering     = true;
	// 再生フェードイン (bHaloDitherOut=false)
	// がAttachHaloForAttackで中止された後だと方向がinのまま残り、付け替え
	// (Restore) を素通りして光輪が腰に取り残される
	bHaloDitherOut     = true;
	HaloDitherElapsed = 0.0f;
	SetComponentTickEnabled(true);

	if (HasHaloDMI() && Data)
	{
		SetHaloScalar(Data->HaloDitherParamName, 0.0f);
	}
}

void UHaloComponent::OnHaloOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	bool bFromSweep, const FHitResult& SweepResult)
{
	if (bHaloDamageCooling) return;
	if (bHaloTouchWindowActive) return;
	if (GetOwner()->GetWorldTimerManager().IsTimerActive(HaloTouchTimerHandle)) return;
	if (!bHaloDeployed) return;

	AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(GetOwner());
	if (!Enemy || !OtherActor || !TideCombatUtil::IsHostileTo(GetOwner(), OtherActor)) return;
	if (!Data) return;

	// 誰が触れたかは覚えない。発火時にApplyHaloTouchDamageが範囲内を取り直して当てる
	GetOwner()->GetWorldTimerManager().SetTimer(HaloTouchTimerHandle, this,
		&UHaloComponent::OnHaloTouchFired, Data->HaloTouchDamageDelay, false);

	const float VfxDelay = FMath::Max(Data->HaloTouchDamageDelay - Data->HaloTouchVfxOffset, 0.0f);
	GetOwner()->GetWorldTimerManager().SetTimer(HaloVfxTimerHandle, this,
		&UHaloComponent::OnHaloVfxFired, VfxDelay, false);

	StartHaloTouchFlicker(true);
}

void UHaloComponent::OnHaloVfxFired()
{
	AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(GetOwner());
	if (!Enemy) return;
	// ガード中またはふっとび復帰中に発火する
	const bool bRecovery = Enemy->HasStateTag(TAG_State_Common_HitReaction_Recovering);
	if (!Enemy->HasStateTag(TAG_State_Enemy_Guard) && !bRecovery) return;
	if (!HaloMeshComp || !Data || !Data->HaloTouchEffect) return;

	if (UNiagaraComponent* NiagaraComp = UNiagaraFunctionLibrary::SpawnSystemAttached(
		Data->HaloTouchEffect,
		HaloMeshComp,
		NAME_None,
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		EAttachLocation::KeepRelativeOffset,
		true))
	{
		NiagaraComp->SetWorldScale3D(FVector(Data->HaloTouchEffectScale));

		// 起き上がり中は背中ソケットが激しく回転しVFXが暴れるため、
		// 位置は光輪に追従させたまま回転だけワールド0へ固定する (ガード中は従来通り光輪に追従)
		if (bRecovery)
		{
			NiagaraComp->SetAbsolute(false, true, false);
			NiagaraComp->SetWorldRotation(FRotator::ZeroRotator);
		}
	}
}

void UHaloComponent::OnHaloTouchFired()
{
	StartHaloTouchFlicker(false);

	AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(GetOwner());
	if (!Enemy) return;
	const bool bGuard    = Enemy->HasStateTag(TAG_State_Enemy_Guard);
	const bool bRecovery = Enemy->HasStateTag(TAG_State_Common_HitReaction_Recovering);
	if (!bGuard && !bRecovery) return;
	if (!HaloMeshComp || !Data) return;

	// ダメージウィンドウを開く
	// 実際の判定・ダメージはApplyHaloTouchDamageが
	// HaloTouchDamageDuration秒の間、Tickから毎フレーム行う
	// (Duration=0なら次の1Fで閉じる)
	bHaloTouchWindowActive   = true;
	HaloTouchWindowRemaining = Data->HaloTouchDamageDuration;
	HaloTouchDamagedActors.Reset();
	SetComponentTickEnabled(true);
}

void UHaloComponent::CloseHaloTouchWindow()
{
	bHaloTouchWindowActive   = false;
	HaloTouchWindowRemaining = 0.0f;
	HaloTouchDamagedActors.Reset();

	if (!Data) return;

	bHaloDamageCooling = true;
	GetOwner()->GetWorldTimerManager().SetTimer(HaloCooldownTimerHandle, this,
		&UHaloComponent::ResetHaloCooldown, Data->HaloTouchCooldown, false);
}

void UHaloComponent::ApplyHaloTouchDamage()
{
	AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(GetOwner());
	if (!Enemy || !HaloMeshComp || !Data) return;

	// ウィンドウ中にガードを割られた/復帰が終わったら判定を畳む
	const bool bGuard    = Enemy->HasStateTag(TAG_State_Enemy_Guard);
	const bool bRecovery = Enemy->HasStateTag(TAG_State_Common_HitReaction_Recovering);
	if (!bGuard && !bRecovery)
	{
		CloseHaloTouchWindow();
		return;
	}

	const FVector Center = HaloMeshComp->GetComponentLocation();

#if !UE_BUILD_SHIPPING
	// ダメージが発生している範囲を可視化する
	// ウィンドウが開いている間だけ毎フレーム描くので、表示期間が実際のダメージ発生期間と一致する
	// 実判定は水平距離のみ (DistSquared2D) なので高さは見ていない
	if (UTideGameSettings::Get()->bDebugDrawAttackHitbox)
	{
		DrawDebugSphere(GetWorld(), Center, Data->HaloTouchAoeRadius, 16, FColor::Red, false, -1.0f, 0, 1.5f);
	}
#endif

	UWorld* World = GetWorld();
	if (!World) return;

	// トリガー球 (HaloTouchTriggerRadius) ではなくAOE半径で取り直す
	// トリガーをAOEより小さくしても巻き込み範囲が痩せないようにするため
	FCollisionObjectQueryParams ObjQuery(ECC_Pawn);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(HaloTouchAoe), false, GetOwner());
	Params.AddIgnoredActor(GetOwner());

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, Center, FQuat::Identity, ObjQuery,
		FCollisionShape::MakeSphere(Data->HaloTouchAoeRadius), Params);

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Actor = Overlap.GetActor();
		if (!Actor || HaloTouchDamagedActors.Contains(Actor)) continue;
		if (!TideCombatUtil::IsHostileTo(GetOwner(), Actor)) continue;
		if (FVector::DistSquared2D(Center, Actor->GetActorLocation())
			> FMath::Square(Data->HaloTouchAoeRadius)) continue;

		IDamageable* Target = Cast<IDamageable>(Actor);
		if (!Target) continue;

		HaloTouchDamagedActors.Add(Actor);

		FDamageInfo DamageInfo;
		// 復帰中は実HPダメージ、ガード中は従来通りノックバックのみ(0)
		DamageInfo.BaseDamage     = bRecovery ? Data->HaloTouchDamage : 0.0f;
		DamageInfo.HitReactionTag = TAG_HitReaction_Blowoff_L;
		DamageInfo.Instigator     = GetOwner();
		Target->ReceiveDamage(DamageInfo);
	}
}

void UHaloComponent::StartHaloTouchFlicker(bool bStart)
{
	if (bStart)
	{
		bHaloTouchFlickerState = false;
		GetOwner()->GetWorldTimerManager().SetTimer(HaloFlickerTimerHandle, this,
			&UHaloComponent::OnHaloFlickerTick, 0.3f, false);
	}
	else
	{
		GetOwner()->GetWorldTimerManager().ClearTimer(HaloFlickerTimerHandle);
		if (HasHaloDMI() && Data)
		{
			SetHaloScalar(Data->HaloTouchGlowParamName, 0.0f);
		}
	}
}

void UHaloComponent::OnHaloFlickerTick()
{
	if (!HasHaloDMI() || !Data) return;

	bHaloTouchFlickerState = !bHaloTouchFlickerState;
	SetHaloScalar(Data->HaloTouchGlowParamName, bHaloTouchFlickerState ? 1.0f : 0.0f);

	const float Remaining = GetOwner()->GetWorldTimerManager().GetTimerRemaining(HaloTouchTimerHandle);
	const float Progress  = 1.0f - FMath::Clamp(Remaining / Data->HaloTouchDamageDelay, 0.0f, 1.0f);
	const float Interval  = FMath::Lerp(0.3f, 0.05f, Progress);
	GetOwner()->GetWorldTimerManager().SetTimer(HaloFlickerTimerHandle, this,
		&UHaloComponent::OnHaloFlickerTick, Interval, false);
}

void UHaloComponent::ResetHaloCooldown()
{
	bHaloDamageCooling = false;
}

void UHaloComponent::StartHaloBreak(const FVector& ImpulseDir, float InstigatorDistance)
{
	if (!HaloMeshComp) return;

	// 直書きせずSetDeployedを通す (接触トリガー球のコリジョンを閉じるため)
	SetDeployed(false);
	bHaloBreakFading     = false;
	HaloBreakFadeElapsed = 0.0f;
	HaloBreakFieldComp   = nullptr;
	ActiveHaloBreakGCA.Reset();

	// 破壊された瞬間から再生完了まで無防備。投擲中に壊された場合もこちらが優先して残る
	bHaloBroken = true;
	UpdateHaloAwayTag();

	HaloMeshComp->SetVisibility(false);
	HaloMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	if (!Data) return;

	// 破壊クール明けに光輪を戻す。破片の寿命 (HaloBreakReformDelay) とは独立させ、
	// 破片が消えたあとも無防備を継続できるようにする。破片アセットが無い敵でも必ず再生させる
	const float RegenDelay = Data->HaloBreakRegenCooldown > 0.0f
		? Data->HaloBreakRegenCooldown
		: Data->HaloBreakReformDelay;
	GetOwner()->GetWorldTimerManager().SetTimer(
		HaloBreakReformTimerHandle,
		this, &UHaloComponent::OnHaloBreakReformed,
		RegenDelay, false);

	if (!Data->HaloBreakGeometryCollection) return;

	CachedHaloBreakImpulseDir = ImpulseDir.GetSafeNormal();
	CachedHaloBreakCenter     = HaloMeshComp->GetComponentLocation();
	CachedHaloBreakInstigatorDistance = InstigatorDistance;

	const FTransform SpawnTM = HaloMeshComp->GetComponentTransform();
	AGeometryCollectionActor* GCA = GetWorld()->SpawnActorDeferred<AGeometryCollectionActor>(
		AGeometryCollectionActor::StaticClass(), SpawnTM, GetOwner());

	if (GCA)
	{
		if (UGeometryCollectionComponent* GCC = GCA->GetGeometryCollectionComponent())
		{
			GCC->SetRestCollection(Data->HaloBreakGeometryCollection);
			GCC->ComponentTags.AddUnique(HaloCameraSweepIgnoreTag);

			// 破片は背景(WorldStatic)のみブロックする
			// 可動物やPawnとの接触で追加インパルスが入り、近距離時に不自然な加速が起きるのを防ぐ
			// 全レベルへ同一プロファイルを適用する。配列要素が1つだとエンジン側でFMath::Min
			// (Num-1, Level) によりルートからリーフまで全レベルがHaloFragmentになる
			// SetSimulatePhysicsより前に設定しておけばOnPostCreateParticles
			// 時に各破片ボディのシェイプフィルタへ焼き込まれる
			GCC->SetPerLevelCollisionProfileNames({HaloBreakFragmentProfileName});

			GCC->SetSimulatePhysics(true);
			GCC->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

			// 破片マテリアルを破壊時の明るさに合わせる (暗め:ガード・通常 / 明るめ:攻撃)
			// 破片が飛ぶ瞬間から正しい色を出すためスポーン時にMIDを生成しておき、
			// フェード時 (OnHaloBreakFadeStart) はこのMIDをそのまま使う
			for (int32 i = 0; i < GCC->GetNumMaterials(); ++i)
			{
				if (UMaterialInstanceDynamic* MID = GCC->CreateAndSetMaterialInstanceDynamic(i))
				{
					MID->SetScalarParameterValue(Data->HaloAttackGlowParamName, bHaloAttackGlow ? 1.0f : 0.0f);
				}
			}
		}

		HaloBreakFieldComp = NewObject<UFieldSystemComponent>(GCA, TEXT("HaloBreakFieldComp"));
		HaloBreakFieldComp->RegisterComponent();
		HaloBreakFieldComp->AttachToComponent(GCA->GetRootComponent(),
			FAttachmentTransformRules::KeepWorldTransform);
		GCA->Tags.AddUnique(HaloCameraSweepIgnoreTag);

		GCA->SetLifeSpan(Data->HaloBreakReformDelay);
		UGameplayStatics::FinishSpawningActor(GCA, SpawnTM);
		ActiveHaloBreakGCA = GCA;

		// 破壊破片(別アクター)に敵カプセルが引っかかってmove stuckになり吹き飛ばない/横へ
		// 押し出される不具合を防ぐため、敵カプセルの移動スイープで破片アクターを無視する
		if (ACharacter* OwnerChar = Cast<ACharacter>(GetOwner()))
		{
			if (UCapsuleComponent* Capsule = OwnerChar->GetCapsuleComponent())
			{
				Capsule->IgnoreActorWhenMoving(GCA, true);
			}
		}

		// Strainフィールドでクラスターを砕く
		URadialFalloff* StrainField = NewObject<URadialFalloff>(HaloBreakFieldComp);
		StrainField->Magnitude = Data->HaloBreakStrainMagnitude;
		StrainField->Radius    = Data->HaloBreakFieldRadius;
		StrainField->Position  = CachedHaloBreakCenter;
		StrainField->Falloff   = EFieldFalloffType::Field_FallOff_None;
		StrainField->MinRange  = 0.0f;
		StrainField->MaxRange  = 1.0f;
		StrainField->Default   = 0.0f;
		HaloBreakFieldComp->ApplyPhysicsField(true,
			EFieldPhysicsType::Field_ExternalClusterStrain, nullptr, StrainField);

		// 破片が独立ボディになった次ステップで散布力を加える
		GetOwner()->GetWorldTimerManager().SetTimer(
			HaloBreakScatterTimerHandle,
			this, &UHaloComponent::ApplyHaloBreakScatterForce,
			0.05f, false);
	}

	// フェード開始タイマー
	// (破片が消えるHaloBreakReformDelayより
	// HaloBreakFadeDuration秒早く起動)
	if (GCA && Data->HaloBreakFadeDuration > 0.0f)
	{
		const float FadeDelay = FMath::Max(Data->HaloBreakReformDelay - Data->HaloBreakFadeDuration, 0.0f);
		GetOwner()->GetWorldTimerManager().SetTimer(
			HaloBreakFadeTimerHandle,
			this, &UHaloComponent::OnHaloBreakFadeStart,
			FadeDelay, false);
	}
}

void UHaloComponent::ApplyHaloBreakScatterForce()
{
	if (!HaloBreakFieldComp || !Data) return;

	// 攻撃者側へオフセットした原点からラジアル散布 → 攻撃方向の先へ飛ぶ
	const FVector RadialOrigin = CachedHaloBreakCenter - CachedHaloBreakImpulseDir * Data->HaloBreakFieldRadius;

	float ScatterForce = Data->HaloBreakScatterForce;
	if (CachedHaloBreakInstigatorDistance >= 0.0f)
	{
		// 密着時は散布力を弱め、接触時の不自然な加速を抑える
		const float DistanceAlpha = FMath::GetMappedRangeValueClamped(
			FVector2D(80.0f, 350.0f),
			FVector2D(0.2f, 1.0f),
			CachedHaloBreakInstigatorDistance);
		ScatterForce *= DistanceAlpha;
	}

	URadialVector* ForceField = NewObject<URadialVector>(HaloBreakFieldComp);
	ForceField->Magnitude = ScatterForce;
	ForceField->Position  = RadialOrigin;

	HaloBreakFieldComp->ApplyPhysicsField(true,
		EFieldPhysicsType::Field_LinearForce, nullptr, ForceField);
}

void UHaloComponent::OnHaloBreakFadeStart()
{
	if (!ActiveHaloBreakGCA.IsValid()) return;

	// MIDはStartHaloBreakのスポーン時に生成済み (明るさ反映のため)
	// ここで作り直すとAttackGlowが失われるので、ディザはフェードTickで既存MIDに適用する
	bHaloBreakFading     = true;
	HaloBreakFadeElapsed = 0.0f;
	SetComponentTickEnabled(true);
}

void UHaloComponent::OnHaloBreakReformed()
{
	if (!HaloMeshComp) return;

	// 破壊クール明け。ここで初めて無防備が解ける
	bHaloBroken = false;
	UpdateHaloAwayTag();

	HaloMeshComp->SetVisibility(true);
	HaloMeshComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	RestoreHaloToStateSocket();

	// 常時ヒビの背中光輪は再生後もヒビ状態で戻す (無傷にはしない)
	if (ShouldBackHaloStayCracked())
	{
		SetHaloCracked(true);
	}

	// 復元後にディザパラメータを1.0 (不透明) にしてからフェードインさせる
	if (HasHaloDMI() && Data && Data->HaloDitherTime > 0.0f)
	{
		SetHaloScalar(Data->HaloDitherParamName, 1.0f);
		bHaloDithering     = true;
		bHaloDitherOut    = false;
		HaloDitherElapsed = 0.0f;
		SetComponentTickEnabled(true);
	}
}

void UHaloComponent::SetFresnelActive(bool bOn, bool bImmediate)
{
	if (!SkelMesh || !Data) return;

	// 光輪を持つ敵だけ胴体フレネルを出す。光輪を持たない敵 (BackHalo運用等) は常に無効
	const bool bWantOn = bOn && (HaloMeshComp != nullptr);

	// 色 (RGB) は先に入れておく。以降のフェードはIntensity(Index0) だけ動かす
	if (bWantOn)
	{
		SkelMesh->SetCustomPrimitiveDataFloat(1, Data->HaloBreakFresnelColor.R);
		SkelMesh->SetCustomPrimitiveDataFloat(2, Data->HaloBreakFresnelColor.G);
		SkelMesh->SetCustomPrimitiveDataFloat(3, Data->HaloBreakFresnelColor.B);
	}

	FresnelTarget = bWantOn ? Data->HaloBreakFresnelIntensity : 0.0f;

	if (bImmediate || Data->HaloBreakFresnelFadeOutTime <= 0.0f)
	{
		FresnelCurrent = FresnelTarget;
		SkelMesh->SetCustomPrimitiveDataFloat(0, FresnelCurrent);
		bBodyFresnelFading = false;
	}
	else
	{
		bBodyFresnelFading = true;
		SetComponentTickEnabled(true);
	}
}

bool UHaloComponent::ShouldBackHaloStayCracked() const
{
	return Data && Data->bHaloBackAlwaysCracked;
}

void UHaloComponent::SetHaloCracked(bool bCracked)
{
	// 常時ヒビの敵はヒビ解除を無視して常にヒビを維持する
	const bool bTarget = bCracked || ShouldBackHaloStayCracked();

	bHaloCracked = bTarget;
	if (HasHaloDMI() && Data)
	{
		SetHaloScalar(Data->HaloCrackParamName, bTarget ? 1.0f : 0.0f);
	}
}

bool UHaloComponent::IsBackHaloAttackFromBehind(const FDamageInfo& DamageInfo) const
{
	if (!Data || !DamageInfo.Instigator.IsValid()) return false;

	const FVector OwnerLoc      = GetOwner()->GetActorLocation();
	const FVector InstigatorLoc = DamageInfo.Instigator->GetActorLocation();

	const FVector ToInstigator2D = (InstigatorLoc - OwnerLoc).GetSafeNormal2D();
	const FVector Forward2D      = ForwardSnapshot.GetSafeNormal2D();
	const float   Dot            = FVector::DotProduct(ToInstigator2D, Forward2D);
	const float   Threshold      = FMath::Cos(FMath::DegreesToRadians(Data->HaloBackDetectHalfAngleDeg));
	const float   HeightDiff     = FMath::Abs(InstigatorLoc.Z - OwnerLoc.Z);

	return HeightDiff <= Data->HaloBackDetectMaxHeightDiff && Dot <= -Threshold;
}

EHaloBackHitResult UHaloComponent::HandleBackHaloHit(const FDamageInfo& DamageInfo)
{
	// 手元に無い光輪 (投擲中・破壊〜再生待ち) と、光輪攻撃中 (発光中) は被弾・破壊しない
	if (!bBackHaloActive || IsHaloAway() || bHaloAttackSuppressed) return EHaloBackHitResult::None;

	// 神技 (UGodActionPlayerModule) とスライドパッシブ
	// (突風・竜巻)は背後条件・ギア・ヒビ状態に関係なく一撃で破壊する
	// パッシブは敵を囲むAOEなので、Instigatorの方向で判定する背後条件は意味を成さない
	const bool bGodAction   = TideCombatUtil::IsHaloBreakerDamage(DamageInfo);
	if (!bGodAction && !IsBackHaloAttackFromBehind(DamageInfo)) return EHaloBackHitResult::None;

	const FGameplayTag Gear = DamageInfo.ChargeGearTag;
	const bool bHeavyGear   = (Gear == TAG_Charge_Gear3 || Gear == TAG_Charge_Gear4);

	// 弱攻撃(ギア無し/壱/弍) かつ ヒビなし → ヒビのみ
	// 光輪は残し背面待機を継続する (神技は除く)
	if (!bGodAction && !bHeavyGear && !bHaloCracked)
	{
		SetHaloCracked(true);
		return EHaloBackHitResult::Crack;
	}

	// 即破壊 (ギア参・極 もしくは ヒビ中の再ヒット)
	bBackHaloActive = false;
	SetHaloCracked(false);

	FVector ImpulseDir = FVector::ZeroVector;
	if (DamageInfo.Instigator.IsValid() && HaloMeshComp)
	{
		ImpulseDir = (HaloMeshComp->GetComponentLocation()
			- DamageInfo.Instigator->GetActorLocation()).GetSafeNormal();
	}
	float InstigatorDistance = -1.0f;
	if (DamageInfo.Instigator.IsValid() && HaloMeshComp)
	{
		InstigatorDistance = FVector::Distance(HaloMeshComp->GetComponentLocation(), DamageInfo.Instigator->GetActorLocation());
	}
	StartHaloBreak(ImpulseDir, InstigatorDistance);
	return EHaloBackHitResult::Break;
}

#if !UE_BUILD_SHIPPING
void UHaloComponent::DrawHaloDebug() const
{
	if (!HaloMeshComp || !Data) return;

	const UWorld* World = GetWorld();

	// 背中待機中は背後判定コーンのみを描く (バリア球/タッチAOEは背中判定に無関係なので出さない)
	// ForwardSnapshotの逆方向を中心に、
	// 半角HaloBackDetectHalfAngleDegの水平ウェッジ
	// PCがこの範囲内かつ高低差HaloBackDetectMaxHeightDiff以内で
	// 本体被弾すると破壊
	// /ヒビになる
	if (bBackHaloActive)
	{
		// 実際の判定は被弾直前のForwardSnapshotを基準にするが、常時デバッグでは現在の前方で描く
		const FVector Forward2D = GetOwner()->GetActorForwardVector().GetSafeNormal2D();
		if (Forward2D.IsNearlyZero()) return;

		const FVector Origin   = GetOwner()->GetActorLocation();
		const FVector Back2D    = -Forward2D;
		const FVector RightPerp(-Forward2D.Y, Forward2D.X, 0.0f);
		const float   HalfRad   = FMath::DegreesToRadians(Data->HaloBackDetectHalfAngleDeg);
		const float   Len       = 250.0f;

		// 基準の前方 = 黄
		DrawDebugLine(World, Origin, Origin + Forward2D * Len, FColor::Yellow, false, 0.0f, 0, 2.0f);

		// ウェッジ内を薄いシアンで塗り、境界を濃いシアンで示す
		const int32 Segments = 10;
		for (int32 s = 0; s <= Segments; ++s)
		{
			const float A     = FMath::Lerp(-HalfRad, HalfRad, static_cast<float>(s) / Segments);
			const FVector Dir = Back2D * FMath::Cos(A) + RightPerp * FMath::Sin(A);
			const bool bEdge  = (s == 0 || s == Segments);
			DrawDebugLine(World, Origin, Origin + Dir * Len,
				bEdge ? FColor::Cyan : FColor(0, 110, 110), false, 0.0f, 0, bEdge ? 2.0f : 0.5f);
		}

		// 高低差ゲート (±HaloBackDetectMaxHeightDiff) の目安線
		const float HeightBand = Data->HaloBackDetectMaxHeightDiff;
		DrawDebugLine(World, Origin + FVector(0, 0, HeightBand), Origin + Back2D * Len + FVector(0, 0, HeightBand),
			FColor(0, 60, 60), false, 0.0f, 0, 1.5f);
		DrawDebugLine(World, Origin - FVector(0, 0, HeightBand), Origin + Back2D * Len - FVector(0, 0, HeightBand),
			FColor(0, 60, 60), false, 0.0f, 0, 1.5f);

		// 角度・高低差の数値
		DrawDebugString(World, Origin + Back2D * (Len + 40.0f),
			FString::Printf(TEXT("BackCone half=%.0fdeg  H<=%.0f"),
				Data->HaloBackDetectHalfAngleDeg, HeightBand),
			nullptr, FColor::Cyan, 0.0f);
		return;
	}

	// 背中待機以外 (ガード/展開): バリア球・タッチAOEを描く
	const FVector Center = HaloMeshComp->GetComponentLocation();

	// バリア球: 有効=青 / 無効=暗青
	if (HaloBarrierComp)
	{
		const bool bActive = HaloBarrierComp->GetCollisionEnabled() != ECollisionEnabled::NoCollision;
		DrawDebugSphere(World, HaloBarrierComp->GetComponentLocation(),
			Data->HaloBarrierRadius, 16,
			bActive ? FColor::Blue : FColor(0, 0, 60), false, 0.0f, 0, 1.5f);
	}

	// タッチAOE: 待機=橙 / カウント中=赤
	const bool bTouchActive = GetOwner()->GetWorldTimerManager().IsTimerActive(HaloTouchTimerHandle);
	DrawDebugSphere(World, Center, Data->HaloTouchAoeRadius, 24,
		bTouchActive ? FColor::Red : FColor::Orange, false, 0.0f, 0, 1.5f);
}
#endif
