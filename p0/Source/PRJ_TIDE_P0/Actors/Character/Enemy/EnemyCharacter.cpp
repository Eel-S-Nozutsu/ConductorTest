// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/Actors/Character/Enemy/EnemyCharacter.h"
#include "PRJ_TIDE_P0/Components/Combat/HaloSystemComponent.h"
#include "PRJ_TIDE_P0/Actors/Patrol/EnemyRoutePath.h"
#include "PRJ_TIDE_P0/Components/AI/EnemyBattleComponent.h"
#include "PRJ_TIDE_P0/Components/Combat/DamageSystemComponent.h"
#include "PRJ_TIDE_P0/Components/Combat/HitReactionComponent.h"
#include "PRJ_TIDE_P0/Components/Targeting/LockOnTargetComponent.h"
#include "PRJ_TIDE_P0/Components/Combat/StatusComponent.h"
#include "PRJ_TIDE_P0/Components/State/StateTagComponent.h"
#include "PRJ_TIDE_P0/Actors/Character/Player/TidePlayerCharacter.h"
#include "PRJ_TIDE_P0/Data/Enemy/EnemyDataAsset.h"
#include "PRJ_TIDE_P0/Data/Enemy/DamageReactionTables.h"
#include "PRJ_TIDE_P0/Data/Combat/AttackParameterRow.h"
#include "PRJ_TIDE_P0/Data/Tags/TideGameplayTags.h"
#include "PRJ_TIDE_P0/Core/TideGameSettings.h"
#include "PRJ_TIDE_P0/Utilities/TideCombatUtils.h"
#if !UE_BUILD_SHIPPING
#include "PRJ_TIDE_P0/Integration/ImGui/Gameplay/EnemyHudPlaceholder.h"
#include "PRJ_TIDE_P0/Integration/ImGui/TideHudAnim.h"
#endif

#include "AIController.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Sight.h"
#include "DrawDebugHelpers.h"
#include "Animation/AnimInstance.h"
#include "BrainComponent.h"
#include "PRJ_TIDE_P0/AI/Components/Enemy/EnemyBrainComponent.h"
#include "PRJ_TIDE_P0/AI/Controllers/Enemy/EnemyAIController.h"
#include "PRJ_TIDE_P0/AI/Components/Enemy/EnemyThreatComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "Kismet/GameplayStatics.h"
#include "MotionWarpingComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NavigationSystem.h"
#include "DrawDebugHelpers.h"
#include "imgui.h"


AEnemyCharacter::AEnemyCharacter()
{
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	BattleComponent    = CreateDefaultSubobject<UEnemyBattleComponent>(TEXT("BattleComponent"));
	HaloComponent      = CreateDefaultSubobject<UHaloComponent>(TEXT("HaloComponent"));
	HaloSystemComponent = CreateDefaultSubobject<UHaloSystemComponent>(TEXT("HaloSystemComponent"));

	RagdollDamageCollider = CreateDefaultSubobject<USphereComponent>(TEXT("RagdollDamageCollider"));
	RagdollDamageCollider->SetupAttachment(GetMesh());
	RagdollDamageCollider->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RagdollDamageCollider->SetGenerateOverlapEvents(false);

	LockOnTarget = CreateDefaultSubobject<ULockOnTargetComponent>(TEXT("LockOnTarget"));
	LockOnTarget->SetupAttachment(GetMesh());

	MotionWarping = CreateDefaultSubobject<UMotionWarpingComponent>(TEXT("MotionWarping"));

	// 光輪メッシュ実体。メッシュ/ソケット/スケールはOnConstructionがDAから流し込む
	// 見た目専用なので当たりは持たず、カメラを含む全チャンネルを無視する
	// (背後被弾は向き判定・ガードや接触はHaloComponentが別球で処理する)
	HaloMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HaloMesh"));
	HaloMeshComponent->SetupAttachment(GetMesh());
	HaloMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HaloMeshComponent->SetCollisionObjectType(ECC_WorldDynamic);
	HaloMeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	HaloMeshComponent->SetGenerateOverlapEvents(false);

	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	//GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_WorldDynamic , ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	// 画面外の敵のアニメコストを抑える
	// OnlyTickPoseWhenRenderedではなく
	// OnlyTickMontagesWhenNotRenderedを使う
	// 前者は非描画時にモンタージュも止まり、攻撃Executionの
	// OnMontageEnded/BlendingOutが飛ばず攻撃ステートが固まる
	// 後者はモンタージュ(攻撃/RM/Notify)は進めつつ
	// 重いポーズ更新だけを非描画時にスキップする
	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickMontagesWhenNotRendered;
	// 距離に応じてアニメ更新頻度を落とす(遠い敵ほど間引く)
	GetMesh()->bEnableUpdateRateOptimizations = true;

	bUseControllerRotationYaw = false;

	if (auto* Movement = GetCharacterMovement())
	{
		Movement->MaxAcceleration = 800.0f;
		Movement->MaxWalkSpeed = 80.0f;
		Movement->MinAnalogWalkSpeed = 20.0f;
		Movement->BrakingDecelerationWalking = 1000.0f;
		Movement->GroundFriction = 6.0f;
		Movement->bUseSeparateBrakingFriction = true;
		Movement->BrakingFrictionFactor = 1.0f;
		Movement->BrakingFriction = 1.0f;
		Movement->RotationRate = FRotator(0.0f, 360.0f, 0.0f);
		Movement->bUseControllerDesiredRotation = true;

		Movement->GetNavMovementProperties()->bUseAccelerationForPaths = true;
	}
}

void AEnemyCharacter::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// 光輪メッシュをDAから構成する。エディタでも走るのでビューポートに実物が出る
	// メッシュ/ソケット/スケールの唯一の真実はDA側で、BPへダミーを置く必要はない
	if (!HaloMeshComponent) return;

	const UEnemyDataAsset* EnemyData = Cast<UEnemyDataAsset>(GetCharacterData());
	UStaticMesh* HaloMesh = EnemyData ? EnemyData->HaloMesh : nullptr;

	HaloMeshComponent->SetStaticMesh(HaloMesh);
	if (!HaloMesh) return;

	// 待機は背中ソケット・通常スケール(ガード時の腰ソケット
	// /スケールはHaloComponentが実行時に切り替える)
	HaloMeshComponent->AttachToComponent(GetMesh(),
		FAttachmentTransformRules::SnapToTargetNotIncludingScale, EnemyData->HaloBackSocketName);
	HaloMeshComponent->SetRelativeScale3D(FVector(EnemyData->HaloNormalScale));
}

void AEnemyCharacter::BeginPlay()
{
	Super::BeginPlay();

	SpawnTransform = GetActorTransform();
	PlayerCameraVisibilitySampleRemaining = FMath::FRandRange(
		0.0f,
		FMath::Max(PlayerCameraVisibilitySampleInterval, 0.0f));

	// カプセルの半高さ(キャラクター全体を覆う最大寸法)を球半径として流用する
	if (RagdollDamageCollider)
	{
		RagdollDamageCollider->SetSphereRadius(GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	}

	// ストリーミング再入で残留束縛と衝突して二重束縛のensureが出るのを防ぐため
	// AddUniqueDynamicで束縛する(EndPlay側で対称に解除もする)
	if (StatusComponent)
	{
		StatusComponent->OnDeath.AddUniqueDynamic(this, &AEnemyCharacter::OnDeath);
	}

	if (DamageSystem)
	{
		DamageSystem->OnDamageReceived.AddUniqueDynamic(this, &AEnemyCharacter::OnDamageReceived);
	}

	if (UAnimInstance* AnimInst = GetMesh()->GetAnimInstance())
	{
		AnimInst->OnMontageEnded.AddUniqueDynamic(this, &AEnemyCharacter::OnReactionMontageEnded);
	}

	if (HitReactionComponent)
	{
		HitReactionComponent->OnBlowbackPhaseChanged.AddUniqueDynamic(this, &AEnemyCharacter::OnBlowbackPhaseChanged);
		HitReactionComponent->OnBlowbackEnded.AddUniqueDynamic(this, &AEnemyCharacter::OnBlowbackEnded);
		// 巻き上げの完全終了でAIを再開する(吹き飛びのOnBlowbackEndedと同じ役割)
		HitReactionComponent->OnWindReactionEnded.AddUniqueDynamic(this, &AEnemyCharacter::RestartAIAfterReaction);
	}

	if (const UEnemyDataAsset* Data = Cast<UEnemyDataAsset>(GetCharacterData()))
	{
		// 攻撃テーブルはタイプ別DA (Data) 由来
		// 旧AttackTableOverrideがあれば移行期間中のフォールバックとして優先
		BattleComponent->Initialize(Data, AttackTableOverride);

		if (HitReactionComponent)
		{
			// 敵のリアクション定義はすべてテーブル由来(単発・多段・巻き上げ)
			HitReactionComponent->InitializeReactionTable(Data->HitReactionTable);
			HitReactionComponent->InitializeWindReaction();
		}

		HaloComponent->Initialize(Data, GetMesh(), BattleComponent);
		HaloComponent->InitializeDMI();
		HaloComponent->BindHaloOverlap();
		HaloComponent->RestoreHaloToStateSocket();
		HaloComponent->OnGuardPenetrated = [this](const FDamageInfo& Info)
		{
			Super::ReceiveDamage(Info);
		};
	}

	// ボディDMI初期化 (スーパーアーマー発光用・全スロット)
	if (USkeletalMeshComponent* SkelMesh = GetMesh())
	{
		const int32 MatCount = SkelMesh->GetNumMaterials();
		BodyDMIs.SetNum(MatCount);
		for (int32 i = 0; i < MatCount; ++i)
		{
			if (UMaterialInterface* Mat = SkelMesh->GetMaterial(i))
			{
				BodyDMIs[i] = UMaterialInstanceDynamic::Create(Mat, this);
				SkelMesh->SetMaterial(i, BodyDMIs[i]);
			}
		}
	}

	// DAの色はBodyDMI生成後に流し込む (BodyColor既定=白ならマテリアル既定のまま)
	ApplyBodyColor();

	if (const UEnemyDataAsset* Data = BattleComponent ? BattleComponent->GetDataAsset() : nullptr)
	{
		if (Data->bUseArmament)
		{
			SetArmamentActive(Data->bStartWithArmamentActive);
		}
	}

}

void AEnemyCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// BeginPlayで張った束縛を対称に解除する。GC任せだと束縛が残留し、
	// ストリーミング再入時に二重束縛のensure(ScriptDelegates.h)を招く
	if (StatusComponent)
	{
		StatusComponent->OnDeath.RemoveDynamic(this, &AEnemyCharacter::OnDeath);
	}

	if (DamageSystem)
	{
		DamageSystem->OnDamageReceived.RemoveDynamic(this, &AEnemyCharacter::OnDamageReceived);
	}

	if (GetMesh())
	{
		if (UAnimInstance* AnimInst = GetMesh()->GetAnimInstance())
		{
			AnimInst->OnMontageEnded.RemoveDynamic(this, &AEnemyCharacter::OnReactionMontageEnded);
		}
	}

	if (HitReactionComponent)
	{
		HitReactionComponent->OnBlowbackPhaseChanged.RemoveDynamic(this, &AEnemyCharacter::OnBlowbackPhaseChanged);
		HitReactionComponent->OnBlowbackEnded.RemoveDynamic(this, &AEnemyCharacter::OnBlowbackEnded);
		HitReactionComponent->OnWindReactionEnded.RemoveDynamic(this, &AEnemyCharacter::RestartAIAfterReaction);
	}

	Super::EndPlay(EndPlayReason);
}

void AEnemyCharacter::FellOutOfWorld(const UDamageType& DmgType)
{
	// AbyssVolumeをすり抜けた場合の最終手段
	// OnDeath()
	// 直呼びではStatusComponent::OnDeathが発火せずSpawnerに通知が届かないた
	// めModifyHP経由で死亡させる
	if (!HasStateTag(TAG_State_Common_Dead))
	{
		if (StatusComponent)
			StatusComponent->ModifyHP(-TNumericLimits<float>::Max());
		else
			OnDeath();
	}
	Destroy();
}

void AEnemyCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdatePlayerCameraVisibility(DeltaSeconds);

	// 竜巻のけぞり(アンカー)の終了監視。のけぞりはモンタージュを内部ループさせているので、
	// 最後のスリップからAnchoredFlinchHoldTime途切れたらここで一元的に終了する
	if (bAnchoredWindFlinching)
	{
		const UEnemyDataAsset* WFData = BattleComponent ? BattleComponent->GetDataAsset() : nullptr;
		const float HoldTime = WFData ? WFData->WindReaction.AnchoredFlinchHoldTime : 0.0f;
		if (GetWorld()->GetTimeSeconds() - LastAnchoredWindHitTime > HoldTime)
		{
			EndAnchoredWindFlinch();
		}
	}

	// 竜巻リアクション中の空中アニメ更新は
	// HitReactionComponent::TickComponentが行う

	AAIController* AIC = Cast<AAIController>(GetController());

	// 被弾硬直
	// (stagger)のCanTransitionウィンドウに入ったら行動抑止を解いて通常抽選へ戻す
	// 被弾ごとに固められて判断機会が無い問題への対処
	// 再開後に近距離攻撃が抽選されればstaggerを割り込みキャンセルし、
	// スーパーアーマー攻撃でコンボを断ち切ってカウンターになる
	if (bHitReactionCounterArmed && CanTransitionToNextAction())
	{
		bHitReactionCounterArmed = false;

		const bool bRestartable = StatusComponent && !StatusComponent->IsDead()
			&& !HasStateTag(TAG_State_Enemy_Guard)
			&& !(HitReactionComponent && HitReactionComponent->IsInBlowback())
			&& !(HitReactionComponent && HitReactionComponent->IsInWindReaction())
			&& !IsDodging();
		if (bRestartable)
		{
			SetReacting(false);
		}
	}

#if !UE_BUILD_SHIPPING
	// 暫定HUD(ImGui)。正式UI実装時はEnemyHudPlaceholderごと削除し、
	// この呼び出しを外す。プレイヤーを発見済み
	// (Targetあり)かつ距離HpGaugeShowDistance以内のとき「表示したい」状態
	// 条件を外れてもin/outアニメ(HpGaugeShowAlpha)が0に戻るまでは描画を続ける
	if (StatusComponent)
	{
		const UTideGameSettings* HudSettings = UTideGameSettings::Get();
		// カットシーン中/UI非表示中は「表示したくない」扱いにして out アニメで引っ込める
		const bool bHideUI = TideHudAnim::IsHudSuppressed(GetWorld());

		const AActor* HudTarget = GetTargetActor();

		const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
		const float DistToPlayer = PlayerPawn ? FVector::Dist(GetActorLocation(), PlayerPawn->GetActorLocation()) : TNumericLimits<float>::Max();

		// 通常は戦闘対象を持つ間だけ表示するが、帰還中はTargetActorを消しているので
		// 距離内なら (灰色ゲージを) 見せる扱いにする
		const bool bWantVisible = !bHideUI &&
			!StatusComponent->IsDead() &&
			PlayerPawn != nullptr &&
			(HudTarget != nullptr || IsReturningHome());

		// 表示したい間、またはoutアニメ再生中(アルファが残っている間)だけ描画する
		if (bWantVisible || HpGaugeShowAlpha > 0.0f)
		{
			const bool bShowHpText = HudSettings && HudSettings->bDebugEnemyShowHpText;
			const float CurHP = StatusComponent->GetCurrentHP();
			const float MaxHP = StatusComponent->GetMaxHP();

			if (UsesBossHpGauge())
			{
				// ボス(EM0010 / BossCharacter派生)は画面上部固定のボス用ゲージ＋名前
				EnemyHudPlaceholder::DrawBoss(GetWorld(),
					TCHAR_TO_UTF8(*HpGaugeBossName),
					CurHP, MaxHP, HpGaugeTrailingHP, bShowHpText, bWantVisible, HpGaugeShowAlpha);
			}
			else
			{
				// 通常敵は頭上追従ゲージ。アンカーは頭(カプセル上端)のワールド点
				// 画面上のオフセットはDraw側でピクセル固定するので距離でズレない
				const float CapsuleHalfHeight = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 0.0f;
				const FVector HeadAnchor = GetActorLocation() + FVector(0.0f, 0.0f, CapsuleHalfHeight);

				EnemyHudPlaceholder::Draw(GetWorld(),
					HeadAnchor, HpGaugeScreenOffsetY,
					CurHP, MaxHP, HpGaugeTrailingHP, bShowHpText, bWantVisible, HpGaugeShowAlpha,
					IsReturningHome());
			}
		}
	}
#endif

#if !UE_BUILD_SHIPPING
	// 検知ゲージ。交戦中(満タン固定)も出して未交戦との区別をつける
	// DrawDebugStringのフォントは日本語グリフを持たないためASCIIで描く
	if (StatusComponent && !StatusComponent->IsDead())
	{
		const UTideGameSettings* GaugeSettings = UTideGameSettings::Get();
		if (GaugeSettings && GaugeSettings->bDebugDrawDetectionGauge)
		{
			const AEnemyAIController* GaugeAIC = Cast<AEnemyAIController>(GetController());
			if (const UEnemyThreatComponent* Threat = GaugeAIC ? GaugeAIC->ThreatComponent : nullptr)
			{
				const float Gauge = Threat->GetDetectionGauge();
				const int32 Filled = FMath::RoundToInt(Gauge * 10.0f);

				FString Bar;
				for (int32 i = 0; i < 10; ++i) Bar += (i < Filled) ? TEXT("|") : TEXT(".");

				const bool bEngaged = Threat->IsEngaged();
				const FColor Color = bEngaged ? FColor::Red
					: (Gauge > 0.0f ? FColor::Orange : FColor(120, 120, 120));

				const float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
				DrawDebugString(GetWorld(), GetActorLocation() + FVector(0.0f, 0.0f, HalfHeight + 60.0f),
					FString::Printf(TEXT("[%s] %3.0f%%%s"), *Bar, Gauge * 100.0f,
						bEngaged ? TEXT(" ENGAGED") : TEXT("")),
					nullptr, Color, 0.0f, true);
			}
		}
	}
#endif

#if !UE_BUILD_SHIPPING
	// 近中遠レンジ境界の表示 (Dist2D基準なので円柱で水平距離を可視化)
	if (BattleComponent && StatusComponent && !StatusComponent->IsDead())
	{
		const UTideGameSettings* DebugSettings = UTideGameSettings::Get();
		if (DebugSettings && DebugSettings->bDebugDrawAttackRange)
		{
			const float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
			const FVector Feet = GetActorLocation() - FVector(0.0f, 0.0f, HalfHeight);
			const FVector Top  = Feet + FVector(0.0f, 0.0f, 250.0f);
			const float NearMid = BattleComponent->GetNearMidBoundary();
			const float MidFar  = BattleComponent->GetMidFarBoundary();

			// 近/中 境界 (緑) と 中/遠 境界 (橙) の2本で3ゾーンを示す
			DrawDebugCylinder(GetWorld(), Feet, Top, NearMid, 32, FColor::Green,  false, 0.0f, 0, 1.5f);
			DrawDebugCylinder(GetWorld(), Feet, Top, MidFar,  32, FColor::Orange, false, 0.0f, 0, 1.5f);
			DrawDebugString(GetWorld(), Feet + FVector(NearMid, 0.0f, 20.0f),
				FString::Printf(TEXT("近|中 %.0f"), NearMid), nullptr, FColor::Green, 0.0f, true);
			DrawDebugString(GetWorld(), Feet + FVector(MidFar, 0.0f, 20.0f),
				FString::Printf(TEXT("中|遠 %.0f"), MidFar), nullptr, FColor::Orange, 0.0f, true);
		}
	}
#endif

#if !UE_BUILD_SHIPPING
	// 攻撃の上下角
	// (ピッチ)除外コーン表示
	// MaxPitchToTargetを超える上下のターゲットは攻撃除外
	// 赤コーン内(真下/真上側)にターゲットの線が入ると、その攻撃は抽選から外れる
	if (BattleComponent && StatusComponent && !StatusComponent->IsDead())
	{
		const UTideGameSettings* DebugSettings = UTideGameSettings::Get();
		if (DebugSettings && DebugSettings->bDebugDrawAttackAngle)
		{
			const FVector Origin = GetActorLocation();

			// 現在ターゲットへの線と、水平角/上下角の数値
			if (const AActor* Target = GetTargetActor())
			{
				DrawDebugLine(GetWorld(), Origin, Target->GetActorLocation(), FColor::White, false, 0.0f, 0, 1.5f);
				DrawDebugString(GetWorld(), Origin + FVector(0.0f, 0.0f, 140.0f),
					FString::Printf(TEXT("Yaw %.0f / Pitch %.0f"), GetAngleToTarget(), GetPitchAngleToTarget()),
					nullptr, FColor::White, 0.0f, true);
			}

			// MaxPitchToTargetを設定した攻撃ごとに、除外される上下領域を赤コーンで描画する
			const float ConeLen = 700.0f;
			float LabelOffsetZ = 60.0f;
			for (const FAttackEntry& Attack : BattleComponent->GetCachedAttacks())
			{
				if (Attack.MaxPitchToTarget <= 0.0f) continue;

				// 許容は水平から ±MaxPitch。除外コーンは真下/真上の軸から半頂角 (90 -
				// MaxPitch)
				const float ExclHalfRad = FMath::DegreesToRadians(FMath::Clamp(90.0f - Attack.MaxPitchToTarget, 0.0f, 90.0f));
				DrawDebugCone(GetWorld(), Origin, FVector(0.0f, 0.0f, -1.0f), ConeLen, ExclHalfRad, ExclHalfRad, 24, FColor::Red, false, 0.0f, 0, 1.0f);
				DrawDebugCone(GetWorld(), Origin, FVector(0.0f, 0.0f,  1.0f), ConeLen, ExclHalfRad, ExclHalfRad, 24, FColor(150, 0, 0), false, 0.0f, 0, 1.0f);
				DrawDebugString(GetWorld(), Origin + FVector(0.0f, 0.0f, -100.0f - LabelOffsetZ),
					FString::Printf(TEXT("%s: 上下<=%.0f"), *Attack.RowName.ToString(), Attack.MaxPitchToTarget),
					nullptr, FColor::Red, 0.0f, true);
				LabelOffsetZ += 30.0f;
			}
		}
	}
#endif
}

void AEnemyCharacter::ApplyWaitSettings(const FEnemyWaitSettings& InSettings)
{
	// 保持するだけ。ルート巡回の進行管理はUEnemyAIState_Patrolが持つ
	WaitSettings = InSettings;
}


EDamageResult AEnemyCharacter::ReceiveDamage(const FDamageInfo& InDamageInfo)
{
	// 早期returnが多いためスコープガードで確実に戻す
	TGuardValue<bool> ProcessingDamageGuard(bProcessingDamage, true);

	if (HasStateTag(TAG_State_Common_HitReaction_Immune)) return EDamageResult::Immune;

	// HitReactionComponentがSetActorRotationで向きを変える前にフォワー
	// ドを保存
	{
		const FVector Fwd = GetActorForwardVector();
		if (HaloComponent) HaloComponent->CaptureForwardSnapshot(Fwd);
	}

	if (InDamageInfo.Instigator.IsValid())
	{
		LastDamageInstigator = InDamageInfo.Instigator;
	}

	// 神技 (UGodActionPlayerModule) 由来か
	// 神技は通常SA(ポイズ)を貫通してリアクション・光輪破壊まで通す
	// 完全無敵 (SuperArmor_Invincible) は貫通しない
	// (意図的な無敵フレームを潰さないため)
	const bool bGodAction    = TideCombatUtil::IsGodActionDamage(InDamageInfo);
	const bool bGodPenetrate = bGodAction && !HasStateTag(TAG_State_Common_SuperArmor_Invincible);

	// 光輪を無条件で割る攻撃 (神技 / スライドパッシブ)。完全無敵中だけは割らない
	const bool bHaloBreaker = TideCombatUtil::IsHaloBreakerDamage(InDamageInfo)
		&& !HasStateTag(TAG_State_Common_SuperArmor_Invincible);

	// ガード中の被弾をHaloComponentに委譲(変換テーブルをバイパス)
	//   通常: チャージ攻撃のみガード判定に乗せ、その一撃はガードで止める(Blocked)
	//     SA中は割れない(展開モーション中に潰されないための猶予)。ただし展開完了後は
	//     モーション保護だけ残したいので、HaloOpen付きSAの間は光輪判定を通す
	//   神技・パッシブ: ギア無し・SA中でも条件無視でガードを一撃で割り、
	//     かつ攻撃はそのまま通す(Blockedにせず後段の通常ダメージ処理へ続行)
	if (HaloComponent && HasStateTag(TAG_State_Enemy_Guard))
	{
		if (bHaloBreaker)
		{
			// リアクションは後段の通常処理で1回通すので、ここでは割るだけ (二重リアクション防止)
			HaloComponent->HandleGuardHit(InDamageInfo, /*bSuppressReaction=*/true);
		}
		else if (InDamageInfo.ChargeGearTag.IsValid()
			&& (HasStateTag(TAG_State_Common_SuperArmor_HaloOpen)
				|| !HasStateTag(TAG_State_Common_SuperArmor)))
		{
			HaloComponent->HandleGuardHit(InDamageInfo);
			return EDamageResult::Blocked;
		}
	}

	FDamageInfo ModifiedInfo = InDamageInfo;
	const bool bSuperArmorActive = HasStateTag(TAG_State_Common_SuperArmor)
		|| HasStateTag(TAG_State_Common_SuperArmor_Invincible);

	// 起き上がり中に攻撃を受けたらブロウバック状態を清算する
	if (HitReactionComponent && HitReactionComponent->GetBlowbackPhase() == EBlowbackPhase::Recovery)
	{
		bIsReacting = false;
		RemoveStateTag(TAG_State_Common_HitReaction_Recovering);
		RemoveStateTag(TAG_State_Common_HitReaction_Immune);
	}

	// --- リアクション変換 + ダメージ倍率 ---
	const UEnemyDataAsset* EnemyData = BattleComponent ? BattleComponent->GetDataAsset() : nullptr;

	// 竜巻スリップ由来の処理。浮かない敵 (ボス等, bAnchored) は大スリップで専用のけぞり
	// 常時SAでも見えるよう、後段のSAゲートを通さず専用モンタージュを直接再生する
	// 竜巻内にいる間はスリップが入り続けるのでループする
	// (TriggerAnchoredWindFlinch側で管理)
	//
	// アンカー・非アンカーともに、竜巻スリップは
	// 通常のヒットリアクションには乗せない(HitReactionTagをクリア)
	// 専用の風リアクション(浮き上がり/のけぞり)がmotionと
	// AI停止/再開を一元管理しているため
	// 通常リアクションに乗せるとOnDamageReceivedがStopLogic("Hit")を
	// 呼ぶが、風リアクション中はOnReactionMontageEndedが
	// 再開を風チェーンへ委譲してearly returnする(WindPhase!=Noneの経路)
	// その間に被弾モンタージュが風モンタージュをbInterruptedで中断すると、
	// 風チェーンもRestartAIAfterReactionに到達せず、
	// StopLogicが解除されないまま棒立ちになる
	if (EnemyData && ModifiedInfo.WindTier != EWindDamageTier::None)
	{
		if (EnemyData->WindReaction.bAnchored && ModifiedInfo.WindTier == EWindDamageTier::Large)
		{
			TriggerAnchoredWindFlinch();
		}
		ModifiedInfo.HitReactionTag = FGameplayTag();
	}

	if (EnemyData && ModifiedInfo.HitReactionTag.IsValid())
	{
		const bool bIsBack = IsBackAttack(ModifiedInfo);
		const EHitDirection Dir = bIsBack ? EHitDirection::Back : EHitDirection::Front;

		// 部位リーフ名
		FName PartLeaf = NAME_None;
		if (ModifiedInfo.HitPartTag.IsValid())
		{
			const FString PartFull = ModifiedInfo.HitPartTag.GetTagName().ToString();
			TArray<FString> PartParts;
			PartFull.ParseIntoArray(PartParts, TEXT("."));
			PartLeaf = FName(*PartParts.Last());
		}

		// リアクション変換テーブルを引く優先度: 方向一致(+2) + 部位一致(+1)
		// Any/NAME_Noneはワイルドカード扱い
		if (EnemyData->ReactionConversionTable)
		{
			TArray<FReactionConversionRow*> Rows;
			EnemyData->ReactionConversionTable->GetAllRows<FReactionConversionRow>(TEXT(""), Rows);

			const FReactionConversionRow* BestRow = nullptr;
			int32 BestScore = -1;
			for (const FReactionConversionRow* Row : Rows)
			{
				if (!Row) continue;
				if (Row->InputReactionTag != ModifiedInfo.HitReactionTag) continue;

				const bool bDirMatch = (Row->HitDirection == Dir);
				const bool bDirAny   = (Row->HitDirection == EHitDirection::Any);
				if (!bDirMatch && !bDirAny) continue;

				const bool bPartMatch = (Row->Part == PartLeaf && PartLeaf != NAME_None);
				const bool bPartAny   = (Row->Part == NAME_None);
				if (!bPartMatch && !bPartAny) continue;

				const int32 Score = (bDirMatch ? 2 : 0) + (bPartMatch ? 1 : 0);
				if (Score > BestScore)
				{
					BestScore = Score;
					BestRow = Row;
					if (Score == 3) break;
				}
			}

			if (BestRow)
			{
				ModifiedInfo.HitReactionTag = BestRow->OutputReactionTag;
			}
		}

		// 方向ダメージ倍率
		if (EnemyData->DirectionMultiplierTable)
		{
			const FName DirKey = bIsBack ? FName("Back") : FName("Front");
			if (const FDirectionMultiplierRow* Row =
				EnemyData->DirectionMultiplierTable->FindRow<FDirectionMultiplierRow>(DirKey, TEXT("")))
			{
				ModifiedInfo.BaseDamage *= Row->Multiplier;
			}
		}

		// 部位ダメージ倍率
		if (EnemyData->PartMultiplierTable && ModifiedInfo.HitPartTag.IsValid())
		{
			const FString PartFull = ModifiedInfo.HitPartTag.GetTagName().ToString();
			TArray<FString> PartParts;
			PartFull.ParseIntoArray(PartParts, TEXT("."));
			const FName PartKey(*PartParts.Last());
			if (const FPartMultiplierRow* Row =
				EnemyData->PartMultiplierTable->FindRow<FPartMultiplierRow>(PartKey, TEXT("")))
			{
				ModifiedInfo.BaseDamage *= Row->Multiplier;
			}
		}

		// 光輪状態による肉質倍率。ボス: ヒットコンポーネントのタグで部位ユニットを引く
		// (PartDestructionと同じ流儀)
		// ザコ(単体): 部位ユニットが無いので、無防備
		// (光輪を飛ばして手元に無い)の間だけ本体倍率を掛ける
		if (HaloSystemComponent)
		{
			const FName UnitTag = HaloSystemComponent->GetUnitPartTagForComponent(ModifiedInfo.HitResult.GetComponent());
			if (UnitTag != NAME_None)
			{
				ModifiedInfo.BaseDamage *= HaloSystemComponent->GetPartDamageMultiplier(UnitTag);
			}
			else if (HasStateTag(TAG_State_Enemy_HaloAway))
			{
				ModifiedInfo.BaseDamage *= EnemyData->HaloAwayBodyDamageMultiplier;
			}
		}
	}

	// 神技 (UGodActionPlayerModule) 由来の被弾は、
	// 光輪が割れても神技側のリアクションを優先し上書きしない(bGodActionは上部で算出済み)
	// 光輪の破壊自体は行う

	// 背中光輪が後ろから攻撃された → ギアに応じてヒビ/破壊
	// 破壊時のみリアクションを専用タグで上書きする
	// 神技・パッシブは通常SAを貫通して光輪破壊まで通す (bHaloBreaker)
	if ((!bSuperArmorActive || bHaloBreaker) && HaloComponent)
	{
		const EHaloBackHitResult BackResult = HaloComponent->HandleBackHaloHit(ModifiedInfo);
		// 神技・パッシブは自前のリアクション (竜巻なら無し) を尊重するので上書きしない
		if (BackResult == EHaloBackHitResult::Break && !bHaloBreaker)
		{
			if (EnemyData && EnemyData->HaloBackBreakReactionTag.IsValid())
			{
				ModifiedInfo.HitReactionTag = EnemyData->HaloBackBreakReactionTag;
			}

			// 吹き飛び方向を「攻撃者から離れる方向(=攻撃方向)」に固定する
			// 攻撃ごとに異なるKnockbackDirectionOverrideに依存すると方向・飛距離がバラつ
			// くため、ここで明示的に上書きして安定させる
			if (ModifiedInfo.Instigator.IsValid())
			{
				const FVector Away = (GetActorLocation() - ModifiedInfo.Instigator->GetActorLocation()).GetSafeNormal2D();
				if (!Away.IsNearlyZero())
				{
					ModifiedInfo.KnockbackDirectionOverride = Away;
				}
			}
		}
		// Crack: その攻撃のリアクション・ダメージをそのまま通す (光輪は割れず残る)
	}

#if !UE_BUILD_SHIPPING
	if (UTideGameSettings::Get()->bDebugEnemyNoDamage)
	{
		ModifiedInfo.BaseDamage = 0.0f;
	}
#endif

	if (EnemyData && EnemyData->bUseArmament && HasStateTag(TAG_State_Enemy_Armament))
	{
		ModifiedInfo.BaseDamage *= FMath::Clamp(EnemyData->ArmamentDamageMultiplier, 0.0f, 1.0f);
	}

	if (HasStateTag(TAG_State_Common_SuperArmor_Invincible))
	{
		return EDamageResult::Immune;
	}
	// 神技は通常SAを貫通する → SAブロックを通さず、後段の通常処理
	// (リアクション込みのSuper::ReceiveDamage) へ流す
	if (!bGodPenetrate && HasStateTag(TAG_State_Common_SuperArmor))
	{
		// SA中でもHPは減る(ProcessDamage→ModifyHP)
		// この一撃で倒れるならとどめ演出を起動する
		// ProcessDamageでHPが減る前に判定する(GetCurrentHPは被弾前HPのため)
		TryNotifyFinishingBlow(ModifiedInfo);

		// ProcessDamageを通してOnDamageReceivedをブロードキャストする
		// (PartDestructionComponentが部位HPを減算するために必要)
		if (DamageSystem)
			DamageSystem->ProcessDamage(ModifiedInfo);
		return EDamageResult::Hit;
	}

	// 死亡吹き飛び方向・回転モードの指定を退避する
	// OnDeath /
	// ApplyRagdollLaunchはDamageInfoを受け取らないためここで拾う
	// 大きさは死亡時にDeathLaunchForce由来で一律付与する
	if (!ModifiedInfo.DeathLaunchDirectionOverride.IsNearlyZero())
	{
		DeathLaunchDirOverride = ModifiedInfo.DeathLaunchDirectionOverride.GetSafeNormal2D();
	}
	if (ModifiedInfo.bOverrideDeathSpinMode)
	{
		bDeathSpinModeOverridden = true;
		DeathSpinModeOverride = ModifiedInfo.DeathSpinModeOverride;
	}
	if (ModifiedInfo.DeathSpinSpeedOverride >= 0.0f)
	{
		DeathSpinSpeedOverride = ModifiedInfo.DeathSpinSpeedOverride;
	}
	if (ModifiedInfo.DeathLaunchForceOverride >= 0.0f)
	{
		DeathLaunchForceOverride = ModifiedInfo.DeathLaunchForceOverride;
	}
	if (ModifiedInfo.DeathLaunchUpForceOverride >= 0.0f)
	{
		DeathLaunchUpForceOverride = ModifiedInfo.DeathLaunchUpForceOverride;
	}

	// とどめ(致死ヒット)判定: この一撃でHPが0以下になるなら、加害プレイヤーへ通知して
	// ヒットバック抑制＋スロー演出を起動する。ここは倍率込みの最終ダメージが確定済み
	TryNotifyFinishingBlow(ModifiedInfo);

	return Super::ReceiveDamage(ModifiedInfo);
}

void AEnemyCharacter::TryNotifyFinishingBlow(const FDamageInfo& Info)
{
	if (!StatusComponent) return;
	if (StatusComponent->GetCurrentHP() - Info.BaseDamage > 0.0f) return;

	if (ATidePlayerCharacter* PC = Cast<ATidePlayerCharacter>(Info.Instigator.Get()))
	{
		PC->OnDeliveredFinishingBlow(this, Info);
	}
}

float AEnemyCharacter::GetDistToTarget() const
{
	const AActor* Target = GetTargetActor();
	if (!Target) return -1.0f;

	return FVector::Dist2D(GetActorLocation(), Target->GetActorLocation());
}

AActor* AEnemyCharacter::GetTargetActor() const
{
	// ターゲットの実体はUEnemyThreatComponentが持つ
	const AEnemyAIController* AIC = Cast<AEnemyAIController>(GetController());
	return AIC && AIC->ThreatComponent ? AIC->ThreatComponent->GetTargetActor() : nullptr;
}

FVector AEnemyCharacter::GetPatrolOrigin() const
{
	const AEnemyAIController* AIC = Cast<AEnemyAIController>(GetController());
	return AIC && AIC->ThreatComponent ? AIC->ThreatComponent->GetPatrolOrigin() : FVector::ZeroVector;
}

float AEnemyCharacter::GetCombatRadius() const
{
	const AEnemyAIController* AIC = Cast<AEnemyAIController>(GetController());
	return AIC && AIC->ThreatComponent ? AIC->ThreatComponent->GetCombatRadius() : 0.0f;
}

float AEnemyCharacter::GetPatrolRadius() const
{
	const AEnemyAIController* AIC = Cast<AEnemyAIController>(GetController());
	return AIC && AIC->ThreatComponent ? AIC->ThreatComponent->GetPatrolRadius() : 0.0f;
}

float AEnemyCharacter::GetAngleToTarget() const
{
	const AActor* Target = GetTargetActor();
	if (!Target) return -1.0f;

	const FVector ToTarget = (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
	const float Dot = FVector::DotProduct(GetActorForwardVector().GetSafeNormal2D(), ToTarget);
	return FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Dot, -1.0f, 1.0f)));
}

float AEnemyCharacter::GetPitchAngleToTarget() const
{
	const AActor* Target = GetTargetActor();
	if (!Target) return -1.0f;

	const FVector To = Target->GetActorLocation() - GetActorLocation();
	const float HorizDist = FVector(To.X, To.Y, 0.0f).Size();
	// 水平=0, 真上/真下=90の絶対上下角
	return FMath::RadiansToDegrees(FMath::Abs(FMath::Atan2(To.Z, HorizDist)));
}

void AEnemyCharacter::UpdatePlayerCameraVisibility(float DeltaSeconds)
{
	if (StatusComponent && StatusComponent->IsDead())
	{
		bVisibleToPlayerCamera = false;
		ContinuousVisibleToPlayerCameraTime = 0.0f;
		PlayerCameraVisibilitySampleRemaining = 0.0f;
		return;
	}

	const float SampleInterval = FMath::Max(PlayerCameraVisibilitySampleInterval, 0.0f);
	if (SampleInterval <= 0.0f)
	{
		bVisibleToPlayerCamera = SampleVisibleToPlayerCamera();
	}
	else
	{
		PlayerCameraVisibilitySampleRemaining -= DeltaSeconds;
		if (PlayerCameraVisibilitySampleRemaining <= 0.0f)
		{
			bVisibleToPlayerCamera = SampleVisibleToPlayerCamera();
			PlayerCameraVisibilitySampleRemaining += SampleInterval;
		}
	}

	if (bVisibleToPlayerCamera)
	{
		ContinuousVisibleToPlayerCameraTime += DeltaSeconds;
	}
	else
	{
		ContinuousVisibleToPlayerCameraTime = 0.0f;
	}
}

bool AEnemyCharacter::SampleVisibleToPlayerCamera() const
{
	UWorld* World = GetWorld();
	if (!World) return false;

	APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
	if (!PC || !PC->PlayerCameraManager) return false;

	const float CapsuleHalfHeight = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 0.0f;
	const FVector TargetLocation = GetActorLocation() + FVector(0.0f, 0.0f, CapsuleHalfHeight);

	FVector2D ScreenPos = FVector2D::ZeroVector;
	if (!UGameplayStatics::ProjectWorldToScreen(PC, TargetLocation, ScreenPos))
	{
		return false;
	}

	int32 ViewportX = 0;
	int32 ViewportY = 0;
	PC->GetViewportSize(ViewportX, ViewportY);
	if (ViewportX <= 0 || ViewportY <= 0)
	{
		return false;
	}

	if (ScreenPos.X < 0.0f || ScreenPos.X > static_cast<float>(ViewportX)
		|| ScreenPos.Y < 0.0f || ScreenPos.Y > static_cast<float>(ViewportY))
	{
		return false;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(EnemyCameraVisibility), false, this);
	if (APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(World, 0))
	{
		QueryParams.AddIgnoredActor(PlayerPawn);

		TArray<AActor*> AttachedActors;
		PlayerPawn->GetAttachedActors(AttachedActors, true, true);
		for (AActor* AttachedActor : AttachedActors)
		{
			if (AttachedActor)
			{
				QueryParams.AddIgnoredActor(AttachedActor);
			}
		}
	}

	FHitResult HitResult;
	const bool bHit = World->LineTraceSingleByChannel(
		HitResult,
		PC->PlayerCameraManager->GetCameraLocation(),
		TargetLocation,
		ECC_Visibility,
		QueryParams);

	return !bHit || HitResult.GetActor() == this;
}

bool AEnemyCharacter::CanTransitionToNextAction() const
{
	return HasStateTag(TAG_State_Enemy_CanTransition);
}

void AEnemyCharacter::ClearCanTransition()
{
	// ForceRemoveはカウントに関係なくタグを0にする
	// 前モーションのブレンドアウト中に残っている
	// CanTransitionを確実に落とすためRemoveStateTag
	// (1減算) ではなくこちらを使う
	if (StateTagComponent)
		StateTagComponent->ForceRemoveStateTagsByParent(TAG_State_Enemy_CanTransition);
}

void AEnemyCharacter::DrawImGuiInspector()
{
	Super::DrawImGuiInspector();

	// --- AI State (ステートマシン運用時のみ) ---
	if (AEnemyAIController* EnemyAIC = Cast<AEnemyAIController>(GetController()))
	{
		if (UEnemyBrainComponent* Brain = EnemyAIC->BrainStateMachine; Brain && Brain->IsRunning())
		{
			if (ImGui::CollapsingHeader("AI State", ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::Text("State:");
				ImGui::SameLine(130.0f);
				ImGui::TextColored(ImVec4(0.4f, 0.9f, 1.0f, 1.0f), "%s",
					TCHAR_TO_UTF8(*Brain->GetCurrentStateDebugText()));

				// 検知が成立しているかの切り分け用
			if (const UEnemyThreatComponent* Threat = EnemyAIC->ThreatComponent)
			{
				ImGui::Text("Target:");
				ImGui::SameLine(130.0f);
				if (const AActor* T = Threat->GetTargetActor())
					ImGui::Text("%s", TCHAR_TO_UTF8(*T->GetName()));
				else
					ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "none");

				ImGui::Text("Engaged:");
				ImGui::SameLine(130.0f);
				if (Threat->IsEngaged()) ImGui::TextDisabled("yes");
				else                     ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "NO (未エンカウント)");

				// 検知ゲージ。交戦中は満タン固定なので、動くのは未交戦のときだけ
				ImGui::Text("Detect:");
				ImGui::SameLine(130.0f);
				ImGui::PushStyleColor(ImGuiCol_PlotHistogram, Threat->IsEngaged()
					? ImVec4(0.9f, 0.2f, 0.2f, 1.0f) : ImVec4(1.0f, 0.6f, 0.1f, 1.0f));
				ImGui::ProgressBar(Threat->GetDetectionGauge(), ImVec2(120.0f, 0.0f));
				ImGui::PopStyleColor();

				ImGui::Text("Perceived:");
				ImGui::SameLine(130.0f);
				const int32 PerceivedCount = Threat->GetPerceivedActorCount();
				if (PerceivedCount < 0)      ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "知覚Comp無し");
				else if (PerceivedCount == 0) ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "0 (視認していない)");
				else                          ImGui::Text("%d", PerceivedCount);
			}

			ImGui::Text("Reacting:");
				ImGui::SameLine(130.0f);
				if (IsReacting())
				{
					const FName Reason = GetReactionReason();
					const FString Label = Reason.IsNone() ? TEXT("DebugAIStop") : Reason.ToString();
					ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.2f, 1.0f), "YES (%s)", TCHAR_TO_UTF8(*Label));
				}
				else
				{
					ImGui::TextDisabled("no");
				}

				// 徘徊先が遠すぎる件の切り分け用: テリトリー半径が探索半径になる
				// 帰還が早すぎる/遅すぎる件の切り分け用に交戦圏(Outer)も並べる
				const float PatrolRadius = Brain->GetPatrolRadius();
				ImGui::Text("Patrol R:");
				ImGui::SameLine(130.0f);
				if (PatrolRadius > 0.0f)
				{
					const float DistFromOrigin =
						FVector::Dist2D(GetActorLocation(), Brain->GetPatrolOrigin());
					ImGui::Text("%.0f cm (self %.0f / combat %.0f)",
						PatrolRadius, DistFromOrigin, Brain->GetCombatRadius());
				}
				else
				{
					ImGui::TextDisabled("none (探索は自身周囲 500cm)");
				}

				// 「歩くはずが走っている」系の切り分け用。
				// 実効値 (MaxWalkSpeed) とDAの設定値を並べる
				ImGui::Text("Speed:");
				ImGui::SameLine(130.0f);
				const float CurrentMax = GetCharacterMovement() ? GetCharacterMovement()->MaxWalkSpeed : -1.0f;
				if (const UEnemyDataAsset* SpeedData = Cast<UEnemyDataAsset>(GetCharacterData()))
				{
					const FEnemyAISettings& AI = SpeedData->AISettings;
					ImGui::Text("%.0f  (walk %.0f / strafe %.0f / run %.0f)",
						CurrentMax, AI.WalkSpeed, AI.StrafeSpeed, AI.RunSpeed);
				}
				else
				{
					ImGui::Text("%.0f", CurrentMax);
				}

				ImGui::Text("Actual:");
				ImGui::SameLine(130.0f);
				ImGui::Text("%.0f cm/s", GetVelocity().Size2D());

				ImGui::Text("Move Goal:");
				ImGui::SameLine(130.0f);
				if (EnemyAIC->GetMoveStatus() != EPathFollowingStatus::Idle)
				{
					const FVector Goal = EnemyAIC->GetPathFollowingComponent()
						? EnemyAIC->GetPathFollowingComponent()->GetCurrentTargetLocation()
						: FVector::ZeroVector;
					ImGui::Text("%.0f cm", FVector::Dist2D(GetActorLocation(), Goal));
				}
				else
				{
					ImGui::TextDisabled("idle");
				}
			}
		}
	}

	// --- Attacks ---
	if (ImGui::CollapsingHeader("Attacks"))
	{
		// グローバル状態
		ImGui::Text("Dist to Target:");
		ImGui::SameLine(130.0f);
		const float DistToTarget = GetDistToTarget();
		if (DistToTarget >= 0.0f)
			ImGui::Text("%.0f cm", DistToTarget);
		else
			ImGui::TextDisabled("no target");

		const float GlobalTotal     = BattleComponent->GetGlobalCooldownTotal();
		const float GlobalRemaining = BattleComponent->GetGlobalCooldownRemaining();

		ImGui::Text("Global CD:");
		ImGui::SameLine(130.0f);
		if (GlobalTotal <= 0.0f)
		{
			ImGui::TextDisabled("disabled");
		}
		else if (GlobalRemaining <= 0.0f)
		{
			ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "READY");
		}
		else
		{
			const float Progress = 1.0f - GlobalRemaining / GlobalTotal;
			const FString Label = FString::Printf(TEXT("%.1fs / %.1fs##gcd"), GlobalRemaining, GlobalTotal);
			ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.2f, 0.55f, 0.9f, 1.0f));
			ImGui::ProgressBar(Progress, ImVec2(160.0f, 0.0f), TCHAR_TO_UTF8(*Label));
			ImGui::PopStyleColor();
		}

		ImGui::Separator();

		AEnemyAIController* AIC = Cast<AEnemyAIController>(GetController());
		UEnemyBrainComponent* Brain = AIC ? AIC->BrainStateMachine : nullptr;

		if (Brain)
		{
			ImGui::Text("ActiveAttackIndex: %d", Brain->GetActiveAttackIndex());
			ImGui::SameLine();
			if (ImGui::Button("Reset Force"))
				Brain->ClearAllAttackRequests();
		}
		else
		{
			ImGui::TextDisabled("Brain unavailable (Force disabled)");
		}

		ImGui::Separator();

		// 列ヘッダ
		ImGui::TextDisabled("En  [#] %-16s  %-12s  %-22s  Force", "Name", "Range(cm)", "Cooldown");
		ImGui::Separator();

		// 攻撃一覧
		for (int32 i = 0; i < BattleComponent->GetAttackCount(); ++i)
		{
			// チェックボックス (抽選有効/無効)
			bool bEnabled = BattleComponent->IsAttackEnabled(i);
			const FString CheckID = FString::Printf(TEXT("##en%d"), i);
			if (ImGui::Checkbox(TCHAR_TO_UTF8(*CheckID), &bEnabled))
				BattleComponent->SetAttackEnabled(i, bEnabled);

			ImGui::SameLine();

			// 無効行はグレーアウト
			if (!bEnabled) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));

			const FString Name  = BattleComponent->GetAttackName(i);
			const FString Group = BattleComponent->GetAttackRangeGroupLabel(i);
			ImGui::Text("[%d] %-16s  %s", i, TCHAR_TO_UTF8(*Name), TCHAR_TO_UTF8(*Group));

			ImGui::SameLine(265.0f);

			const float Total     = BattleComponent->GetCooldownTotal(i);
			const float Remaining = BattleComponent->GetCooldownRemaining(i);
			if (Remaining <= 0.0f)
			{
				ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "READY  ");
			}
			else
			{
				const float Progress = (Total > 0.0f) ? (1.0f - Remaining / Total) : 1.0f;
				const FString Label = FString::Printf(TEXT("%.1fs/%.1fs##cd%d"), Remaining, Total, i);
				ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.8f, 0.4f, 0.1f, 1.0f));
				ImGui::ProgressBar(Progress, ImVec2(150.0f, 0.0f), TCHAR_TO_UTF8(*Label));
				ImGui::PopStyleColor();
			}

			if (!bEnabled) ImGui::PopStyleColor();

			ImGui::SameLine(430.0f);

			const bool bCanForce = Brain != nullptr;
			if (!bCanForce) ImGui::BeginDisabled();
			const FString BtnLabel = FString::Printf(TEXT("Force##f%d"), i);
			if (ImGui::Button(TCHAR_TO_UTF8(*BtnLabel)))
				RequestForceAttack(i);
			if (!bCanForce) ImGui::EndDisabled();
		}
	}

}

void AEnemyCharacter::OnDamageReceived(const FDamageInfo& DamageInfo)
{
	if (StatusComponent && StatusComponent->IsDead()) return;

	// タグ未設定またはNoReaction: ダメージは通るがモーション・AI停止はスキップ
	if (!DamageInfo.HitReactionTag.IsValid() || DamageInfo.HitReactionTag == TAG_HitReaction_NoReaction) return;

	if (AAIController* AIC = Cast<AAIController>(GetController()))
	{
		// StopLogicは「実際にリアクションが走りRestartLogicが保証される」場合だけに限定す
		// る。montageや吹き飛びが発生しない被弾で止めると終了コールバックが来ずAIが再開しなくなる
		if (!HasStateTag(TAG_State_Common_SuperArmor)
			&& HitReactionComponent && HitReactionComponent->WillReact(DamageInfo.HitReactionTag))
		{
			AIC->StopMovement();
			AIC->ClearFocus(EAIFocusPriority::Gameplay);

			// 行動抑止 (= React状態への入場条件)。解除はOnReactionMontageEnded
			SetReacting(true, TEXT("Hit"));

			// staggerのCanTransition窓で行動を再開してコンボ割り込み反撃を狙う
			// (Tickで消費)
			bHitReactionCounterArmed = true;
		}

		const int32 CounterIndex = BattleComponent->FindCounterAttackIndex(DamageInfo.HitReactionTag);
		if (CounterIndex >= 0)
		{
			RequestCounterAttack(CounterIndex);
		}
	}

}

bool AEnemyCharacter::CanDodge() const
{
	if (HasStateTag(TAG_State_Common_Dead)) return false;
	if (HasStateTag(TAG_State_Enemy_Guard)) return false;
	if (IsDodging()) return false;
	if (HitReactionComponent && HitReactionComponent->IsInBlowback()) return false;

	const UEnemyDataAsset* Data = BattleComponent ? BattleComponent->GetDataAsset() : nullptr;
	if (!Data) return false;

	if (GetWorld()->GetTimeSeconds() - LastDodgeTime < Data->StepAnim.DodgeCooldown) return false;

	return true;
}

bool AEnemyCharacter::TryPlayDodgeMontage(FVector DodgeDir, UAnimMontage* Montage)
{
	if (!Montage) return false;

	const UEnemyDataAsset* Data = BattleComponent ? BattleComponent->GetDataAsset() : nullptr;
	if (!Data) return false;

	if (Data->StepAnim.NavCheckDistance > 0.0f)
	{
		UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());
		if (NavSys)
		{
			const FVector FootPos = GetActorLocation()
				- FVector(0.0f, 0.0f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
			const FVector CheckPos = FootPos + DodgeDir * Data->StepAnim.NavCheckDistance;
			FNavLocation NavLoc;
			const bool bNavOk  = NavSys->ProjectPointToNavigation(CheckPos, NavLoc, FVector(50.0f, 50.0f, 200.0f));
			const bool bDropOk = bNavOk && (FootPos.Z - NavLoc.Location.Z <= GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
#if !UE_BUILD_SHIPPING
			if (UTideGameSettings::Get()->bDebugEnemyDodgeNavCheck)
			{
				DrawDebugLine(GetWorld(), FootPos, CheckPos, FColor::Yellow, false, 1.5f, 0, 2.0f);
				if (bNavOk)
				{
					DrawDebugSphere(GetWorld(), NavLoc.Location, 25.0f, 8, bDropOk ? FColor::Green : FColor::Orange, false, 1.5f);
				}
				else
				{
					DrawDebugSphere(GetWorld(), CheckPos, 25.0f, 8, FColor::Red, false, 1.5f);
				}
			}
#endif
			if (!bNavOk || !bDropOk) return false;
		}
	}

	UAnimInstance* AnimInst = GetMesh()->GetAnimInstance();
	if (!AnimInst) return false;

	AnimInst->Montage_Play(Montage);
	ActiveDodgeMontage = Montage;
	return true;
}

void AEnemyCharacter::TriggerPredictiveDodge(FVector AttackDirection)
{
	if (UTideGameSettings::Get()->bDebugEnemyNoPredictiveDodge) return;
	// 自分の攻撃を実行中は予知回避で割り込まない(攻撃を中断して回避してしまうのを防ぐ)
	if (IsExecutingAttack()) return;
	if (!CanDodge()) return;

	const UEnemyDataAsset* Data = BattleComponent ? BattleComponent->GetDataAsset() : nullptr;
	if (!Data) return;

	if (FMath::FRand() > Data->StepAnim.PredictiveDodgeSuccessRate) return;

	struct FDodgeCandidate { FVector Dir; UAnimMontage* Montage; };
	TArray<FDodgeCandidate> Candidates = {
		{  GetActorRightVector(),    Data->StepAnim.Right },
		{ -GetActorRightVector(),    Data->StepAnim.Left },
		{ -GetActorForwardVector(),  Data->StepAnim.Backward },
	};
	for (int32 i = Candidates.Num() - 1; i > 0; --i)
		Candidates.Swap(i, FMath::RandRange(0, i));

	for (const FDodgeCandidate& C : Candidates)
	{
		if (TryPlayDodgeMontage(C.Dir, C.Montage))
		{
			PendingDodgeCounterAttackIndex = -1;

			int32 CounterAttackIndex = Data->StepAnim.PredictiveDodgeCounterAttackIndex;

			if (CounterAttackIndex >= 0 && BattleComponent && BattleComponent->GetAttackEntry(CounterAttackIndex))
			{
				PendingDodgeCounterAttackIndex = CounterAttackIndex;
			}

			LastDodgeTime = GetWorld()->GetTimeSeconds();
			return;
		}
	}
}

void AEnemyCharacter::OnReactionMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (Montage == ActiveDodgeMontage)
	{
		ActiveDodgeMontage = nullptr;

		if (!bInterrupted && PendingDodgeCounterAttackIndex >= 0)
		{
			RequestCounterAttack(PendingDodgeCounterAttackIndex);
		}

		PendingDodgeCounterAttackIndex = -1;
	}

	// 竜巻リアクション中はモンタージュ連結をHitReactionComponent側
	// (隙間なし)で行う。ここでは空中での汎用AI再開を抑止するだけ
	// 吹っ飛び/死亡に奪われたら破棄して通常処理へ明け渡す
	if (HitReactionComponent && HitReactionComponent->IsInWindReaction())
	{
		if ((HitReactionComponent->IsInBlowback())
			|| (StatusComponent && StatusComponent->IsDead()))
		{
			HitReactionComponent->AbortWindReaction();
			// フォールスルー: 下のblowback/deadチェックが拾う
		}
		else
		{
			return;
		}
	}

	// Blowback中はフェーズ管理をHitReactionComponentに委譲
	if (HitReactionComponent && HitReactionComponent->IsInBlowback()) return;

	// 中断されたモンタージュでも行動抑止は必ず下ろす。ここで残すと解除点が来ず棒立ちになる
	// 吹き飛び・巻き上げ中ならIsReacting
	// ()がHitReactionComponentのフェーズを見てtrueを返し続けるので、
	// 早く下ろしても取りこぼさない (上の2つの早期returnが既にその区間を守っている)
	bIsReacting = false;

	// 被弾リアクションはガード(浮き輪)中の棒立ち抑止を握っていたbIsReactingを落とす
	// まだガード中ならリアクションが消費したぶんを張り直し、浮き輪のまま構え続けさせる
	// ただし攻撃実行中(ガード開始モンタージュ再生中)はガードの生存を攻撃ステートが握る。
	// 直前リアクションのブレンドアウトがここでGuardを張り直すと、Reactが最優先で
	// まだ再生中のガード攻撃を割り込み中断し、中断経路がガードを畳んでしまう
	// (張り直しはガードhold中=攻撃終了後の被弾リアクションだけを対象にする)
	if (HasStateTag(TAG_State_Enemy_Guard) && !IsExecutingAttack())
	{
		SetReacting(true, TEXT("Guard"));
		return;
	}

	if (bInterrupted) return;
	if (StatusComponent && StatusComponent->IsDead()) return;
	// CanTransitionで既に再開させたぶんは消費済みにする
	bHitReactionCounterArmed = false;

}

void AEnemyCharacter::OnModifyDamageInfo(FDamageInfo& DamageInfo, FGameplayTag AttackTypeTag, AActor* Target)
{
	DamageInfo.AttackTypeTag = AttackTypeTag;

	// 実行中攻撃 (FAttackEntry::HitReactionTag)
	// を被弾側へ与えるリアクションとして流す。空 = 未設定なら従来どおり (リアクション無し)
	// 近接攻撃の攻撃ごとリアクション設定はここが唯一の注入点
	if (CurrentAttackHitReactionTag.IsValid())
	{
		DamageInfo.HitReactionTag = CurrentAttackHitReactionTag;
	}

	// プロト: 実行中の攻撃エントリ (FAttackEntry::Damage)
	// を唯一のダメージ真実とする。値がセットされていればテーブル参照をバイパスする
	// 攻撃非実行中 (負値) のみ従来のテーブル経路へフォールバックする
	if (CurrentAttackDamage >= 0.0f)
	{
		DamageInfo.BaseDamage = CurrentAttackDamage;
		return;
	}

	const UEnemyDataAsset* EnemyData = BattleComponent ? BattleComponent->GetDataAsset() : nullptr;
	if (!EnemyData || !EnemyData->AttackParameterTable) return;

	static const FString ContextString = TEXT("EnemyAttackParameterLookup");
	TArray<FEnemyAttackParameterRow*> Rows;
	EnemyData->AttackParameterTable->GetAllRows<FEnemyAttackParameterRow>(ContextString, Rows);

	for (const FEnemyAttackParameterRow* Row : Rows)
	{
		if (!Row || Row->AttackTypeTag != AttackTypeTag) continue;

		const float Base = StatusComponent ? StatusComponent->GetBaseAttackPower() : 1.0f;
		DamageInfo.BaseDamage      = Base * Row->DamageMultiplier;
		DamageInfo.bUseHitStop     = Row->bUseHitStop;
		DamageInfo.HitStopDuration = Row->HitStopDuration;
		DamageInfo.HitStopDilation = Row->HitStopDilation;
		break;
	}
}

void AEnemyCharacter::RequestCounterAttack(int32 AttackIndex)
{
	AEnemyAIController* AIC = Cast<AEnemyAIController>(GetController());
	if (AIC && AIC->BrainStateMachine)
	{
		AIC->BrainStateMachine->RequestCounterAttack(AttackIndex);
	}
}

bool AEnemyCharacter::RequestForceAttack(int32 AttackIndex)
{
	AEnemyAIController* AIC = Cast<AEnemyAIController>(GetController());
	if (!AIC || !AIC->BrainStateMachine) return false;

	AIC->BrainStateMachine->RequestForceAttack(AttackIndex);
	return true;
}

void AEnemyCharacter::SetReacting(bool bInReacting, FName Reason)
{
	bIsReacting = bInReacting;
	ReactionReason = bInReacting ? Reason : NAME_None;
}

FName AEnemyCharacter::GetReactionReason() const
{
	// IsReactingと同じ判定順で返す (表示と実挙動がズレないように)
	if (HitReactionComponent)
	{
		if (HitReactionComponent->IsInBlowback())     return TEXT("Blowback");
		if (HitReactionComponent->IsInWindReaction()) return TEXT("Wind");
	}
	if (IsPlayingPartReaction()) return TEXT("PartDown");

	return bIsReacting ? ReactionReason : NAME_None;
}

bool AEnemyCharacter::IsReacting() const
{
	// 吹き飛び・巻き上げはHitReactionComponentがフェーズを持っているので、
	// そちらを真実として使う。フラグで二重管理すると必ず片方が取り残される
	if (HitReactionComponent
		&& (HitReactionComponent->IsInBlowback() || HitReactionComponent->IsInWindReaction()))
	{
		return true;
	}

	// ボスの部位破壊/連動ダウンも専用の再生中フラグを持つ
	// bIsReactingはOnMontageEnded経由で他モンタージュの中断でも落ちるため、
	// ダウン時間 (= 連動モンタージュ長) の保持をフラグ側に任せると即復帰してしまう
	if (IsPlayingPartReaction()) return true;

	return bIsReacting;
}

void AEnemyCharacter::OnDeath()
{
	bIsReacting = false;
	HaloComponent->ClearAllTimers();
	RemoveStateTag(TAG_State_Enemy_Guard);
	RemoveStateTag(TAG_State_Common_SuperArmor_Invincible);

	AddStateTag(TAG_State_Common_Dead);

	// 死亡後はロックオン対象から除外する(BPで追加した部位コンポーネントも含む)
	TArray<ULockOnTargetComponent*> TargetComps;
	GetComponents<ULockOnTargetComponent>(TargetComps);
	for (ULockOnTargetComponent* Comp : TargetComps)
	{
		Comp->bIsTargetable = false;
	}

	// ラグドールに引き継ぐ吹き飛び速度を停止前に保存する
	// 死因 (Blowoff / 通常) を問わず大きさはDeathLaunchForceに一律で寄せ、
	// 回転はStartRagdoll→ApplyRagdollLaunchが付与する
	if (DeathVelocity.IsNearlyZero())
	{
		const UEnemyDataAsset* DeathData = Cast<UEnemyDataAsset>(GetCharacterData());
		// 攻撃が力を指定していれば敵既定より優先する
		const float Force = DeathLaunchForceOverride >= 0.0f
			? DeathLaunchForceOverride
			: (DeathData ? DeathData->DeathLaunchForce : 600.0f);
		if (Force > 0.0f)
		{
			// Instigatorが未設定の場合はプレイヤーポーンで代用する
			AActor* Source = LastDamageInstigator.IsValid()
				? LastDamageInstigator.Get()
				: UGameplayStatics::GetPlayerPawn(this, 0);

			// 上方向は指定があればそれを、無ければ水平力の0.5倍。
			// 水平と独立に打ち上げ高さを変えられる
			const float UpForce = DeathLaunchUpForceOverride >= 0.0f ? DeathLaunchUpForceOverride : Force * 0.5f;

			// 攻撃が横飛ばし等を指定していればそれを優先し、無ければ加害者から遠ざかる向き
			if (!DeathLaunchDirOverride.IsNearlyZero())
			{
				DeathVelocity = DeathLaunchDirOverride * Force + FVector(0.0f, 0.0f, UpForce);
			}
			else if (Source)
			{
				const FVector Away = (GetActorLocation() - Source->GetActorLocation()).GetSafeNormal2D();
				DeathVelocity = Away * Force + FVector(0.0f, 0.0f, UpForce);
			}
			else
			{
				DeathVelocity = GetVelocity();
			}
		}
	}
	GetCharacterMovement()->StopMovementImmediately();

	if (AAIController* AIC = Cast<AAIController>(GetController()))
	{
		AIC->StopMovement();
		AIC->ClearFocus(EAIFocusPriority::Gameplay);

		if (AEnemyAIController* EnemyAIC = Cast<AEnemyAIController>(AIC))
		{
			if (EnemyAIC->BrainStateMachine) EnemyAIC->BrainStateMachine->StopLogicForDeath();
		}
	}

	// URO (距離でアニメ更新を間引く) はスキップフレームでRefreshBoneTransformsごと飛ばすが、
	// ラグドールの姿勢を物理から取り込んでいるのがそのRefreshBoneTransforms。
	// 物理だけが毎フレーム進み表示は数フレームに1回になるため、
	// 溜まった移動量が一気に出て死体が横へワープして見える
	// 死亡ポーズの取りこぼしも防げるのでモンタージュ再生前に切る
	GetMesh()->bEnableUpdateRateOptimizations = false;

	// 死亡ポーズを一枚かぶせてからラグドールへ渡す。物理ボディは「最後に評価されたポーズ」から
	// 初期化されるため、Montage_Playとラグドール化の間に最低1回のアニメ評価を挟む必要がある
	// モンタージュ終了コールバックは使わない (ブレンドアウト完了後に来るのでポーズが戻ってしまう)
	const UEnemyDataAsset* Data = Cast<UEnemyDataAsset>(GetCharacterData());
	UAnimMontage* DeathMontage = Data ? Data->DeathMontage : nullptr;
	UAnimInstance* AnimInst = GetMesh()->GetAnimInstance();
	if (!DeathMontage || !AnimInst)
	{
		StartRagdoll();
		return;
	}

	AnimInst->Montage_Play(DeathMontage);

	// 1Fモンタージュは再生した次のフレームには終端に達し、
	// 自動ブレンドアウトでポーズが戻ってしまう
	// インスタンス側で切っておくと停止するまで最終ポーズを保持する
	// (アセットのチェックに依存しない)
	if (FAnimMontageInstance* MontageInst = AnimInst->GetActiveInstanceForMontage(DeathMontage))
	{
		MontageInst->bEnableAutoBlendOut = false;
	}

	TWeakObjectPtr<AEnemyCharacter> WeakThis = this;
	GetWorldTimerManager().SetTimer(DeathPoseTimerHandle, [WeakThis]()
	{
		if (WeakThis.IsValid()) WeakThis->StartRagdoll();
	}, FMath::Max(Data->DeathPoseHoldTime, KINDA_SMALL_NUMBER), false);
}

void AEnemyCharacter::StartRagdoll()
{
	// 物理ボディは「最後に評価されたポーズ」から初期化されるので、
	// 死亡ポーズを確実に反映させてから
	// 物理へ渡す。画面外でアニメ更新が間引かれていても取りこぼさない
	GetMesh()->TickAnimation(0.0f, false);
	GetMesh()->RefreshBoneTransforms();

	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetMesh()->SetCollisionProfileName(TEXT("Ragdoll"));
	GetMesh()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetMesh()->SetSimulatePhysics(true);

	// 物理へ渡した後なら止めてもポーズに影響しない
	// 逆にSetSimulatePhysicsより前で止めると直後の評価で死亡ポーズがベースへ戻ってしまう
	if (UAnimInstance* AnimInst = GetMesh()->GetAnimInstance())
	{
		AnimInst->StopAllMontages(0.0f);
	}

	// ChaosはSetSimulatePhysicsでボディを再構築するため、
	// その後に設定しないと各ボディに反映されない
	GetMesh()->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	for (FBodyInstance* BI : GetMesh()->Bodies)
	{
		if (BI) BI->SetResponseToChannel(ECC_WorldDynamic, ECR_Block);
	}

	const UEnemyDataAsset* RagdollData = Cast<UEnemyDataAsset>(GetCharacterData());
#if !UE_BUILD_SHIPPING
	const bool bRagdollDamageEnabled = UTideGameSettings::Get()->bDebugEnableBlowbackCollisionDamage;
#else
	constexpr bool bRagdollDamageEnabled = true;
#endif
	if (bRagdollDamageEnabled && HitReactionComponent && RagdollData && RagdollData->DeathRagdollCollisionDamage > 0.0f)
	{
		// ルートボーンにアタッチして物理シミュレーションに追従させる
		if (GetMesh()->GetNumBones() > 0)
		{
			RagdollDamageCollider->AttachToComponent(GetMesh(),
				FAttachmentTransformRules::SnapToTargetNotIncludingScale,
				GetMesh()->GetBoneName(0));
		}
		HitReactionComponent->BeginRagdollDamage(
			RagdollDamageCollider,
			RagdollData->DeathRagdollCollisionDamage,
			RagdollData->DeathRagdollCollisionHitReactionTag,
			LastDamageInstigator.Get());
	}

	if (!DeathVelocity.IsNearlyZero())
	{
		// SetSimulatePhysics直後に速度を設定すると物理初期化に上書きされるため1フレーム遅ら
		// せる
		const FVector VelocityToApply = DeathVelocity;
		DeathVelocity = FVector::ZeroVector;

		TWeakObjectPtr<AEnemyCharacter> WeakThis = this;
		GetWorld()->GetTimerManager().SetTimerForNextTick([WeakThis, VelocityToApply]()
		{
			if (WeakThis.IsValid()) WeakThis->ApplyRagdollLaunch(VelocityToApply);
		});
	}

	const UEnemyDataAsset* Data = Cast<UEnemyDataAsset>(GetCharacterData());

	if (RagdollDeathDelay >= 0.0f)
	{
		TWeakObjectPtr<AEnemyCharacter> WeakThis = this;
		GetWorldTimerManager().SetTimer(DeathFadeTimerHandle, [WeakThis]()
		{
			if (!WeakThis.IsValid()) return;
			WeakThis->PlayDeathExplosion();
			WeakThis->Destroy();
		}, RagdollDeathDelay, false);
	}
}

void AEnemyCharacter::ApplyRagdollLaunch(const FVector& LaunchVelocity)
{
	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp || !MeshComp->IsSimulatingPhysics()) return;

	// まず全ボディを同じ速度で運ぶ。ただしこれだけだとボディ間の相対速度がゼロ = 角運動量ゼロで、
	// 直立ポーズを保ったまま平行移動する (棒立ち吹き飛び)。回転は下で別途与える
	MeshComp->SetAllPhysicsLinearVelocity(LaunchVelocity);

	static const FDeathRagdollSpin DefaultSpin;
	const UEnemyDataAsset* SpinData = Cast<UEnemyDataAsset>(GetCharacterData());
	const FDeathRagdollSpin& Spin = SpinData ? SpinData->DeathSpin : DefaultSpin;

	// 重心から外れたボーンに上乗せすると、そこを起点に体が折れながら引っ張られる
	if (!Spin.ImpulseBone.IsNone() && Spin.ImpulseScale > 0.0f
		&& MeshComp->GetBoneIndex(Spin.ImpulseBone) != INDEX_NONE)
	{
		MeshComp->AddImpulse(LaunchVelocity * Spin.ImpulseScale, Spin.ImpulseBone, /*bVelChange=*/true);
	}

	// 攻撃がモード・速度を指定していれば敵既定より優先する (突風パッシブは常にきりもみ＋強回転)
	// ばらつき・追加インパルスの軸ボーン等は敵のDeathSpinをそのまま使う
	EDeathRagdollSpinMode Mode = bDeathSpinModeOverridden ? DeathSpinModeOverride : Spin.Mode;
	const float SpeedDeg = DeathSpinSpeedOverride >= 0.0f ? DeathSpinSpeedOverride : Spin.SpeedDegrees;
	if (Mode == EDeathRagdollSpinMode::None || SpeedDeg <= 0.0f) return;

	if (Mode == EDeathRagdollSpinMode::Random)
	{
		Mode = FMath::RandBool() ? EDeathRagdollSpinMode::Tumble : EDeathRagdollSpinMode::Corkscrew;
	}

	const FVector LaunchDir = LaunchVelocity.GetSafeNormal();
	FVector SpinAxis;
	if (Mode == EDeathRagdollSpinMode::Corkscrew)
	{
		// 進行方向そのものを軸にすると、飛ぶ向きを保ったままロールする = きりもみ
		SpinAxis = LaunchDir;
	}
	else
	{
		// 進行方向と上方向の外積 = 進行方向に直交する水平軸。これを軸にすると前転になる
		SpinAxis = FVector::CrossProduct(LaunchDir, FVector::UpVector).GetSafeNormal();
		if (SpinAxis.IsNearlyZero())
		{
			// ほぼ真上/真下へ飛ぶと外積が潰れるのでキャラの横軸で代用する
			SpinAxis = GetActorRightVector();
		}
	}

	// 毎回同じ回り方だと死体が揃って見えるので速度と向きにばらつきを入れる
	const float Variance = FMath::FRandRange(1.0f - Spin.SpeedVariance, 1.0f + Spin.SpeedVariance);
	const float Direction = (Spin.bRandomizeDirection && FMath::RandBool()) ? -1.0f : 1.0f;

	// Chaosは角速度だけをMaxAngularVelocity (既定3600度/秒) でクランプする。
	// 超えると下で入れる接線速度と辻褄が合わなくなるので、ばらつきを掛けた後の値で頭を押さえる
	const float MaxSpinDeg = UPhysicsSettings::Get()->MaxAngularVelocity * 0.9f;
	const float FinalSpeedDeg = FMath::Min(SpeedDeg * Variance, MaxSpinDeg);

	// SetAllPhysicsAngularVelocityは全ボディへ同じ角速度を入れるだけで、
	// 剛体回転に必要な接線速度 (ω×r) を与えない。各ボディが自分の重心でその場回転する形になり、
	// そのズレをジョイントが毎フレーム吸収し続けることになるので、接線速度も併せて与える
	// 重心を軸にすれば追加する運動量の総和がゼロになるので、吹き飛びの軌道は変わらない
	const FVector AngVelRad = FMath::DegreesToRadians(SpinAxis * FinalSpeedDeg * Direction);
	const FVector SpinCenter = MeshComp->GetSkeletalCenterOfMass();
	for (FBodyInstance* BI : MeshComp->Bodies)
	{
		if (!BI || !BI->IsValidBodyInstance()) continue;

		BI->SetAngularVelocityInRadians(AngVelRad, /*bAddToCurrent=*/true);
		BI->SetLinearVelocity(
			FVector::CrossProduct(AngVelRad, BI->GetCOMPosition() - SpinCenter), /*bAddToCurrent=*/true);
	}
}

void AEnemyCharacter::PlayDeathExplosion()
{
	const UEnemyDataAsset* Data = Cast<UEnemyDataAsset>(GetCharacterData());
	if (!Data || !Data->DeathExplosionEffect) return;

	UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		GetWorld(),
		Data->DeathExplosionEffect,
		GetMesh()->GetComponentLocation(),
		FRotator::ZeroRotator,
		FVector(Data->DeathExplosionScale));
}

void AEnemyCharacter::OnBlowbackPhaseChanged(EBlowbackPhase NewPhase)
{
	switch (NewPhase)
	{
	case EBlowbackPhase::Start:
		bIsReacting = true;
		AddStateTag(TAG_State_Common_HitReaction_BlowBack);
		AddStateTag(TAG_State_Common_HitReaction_Immune);
		break;
	case EBlowbackPhase::Land:
		RemoveStateTag(TAG_State_Common_HitReaction_BlowBack);
		AddStateTag(TAG_State_Common_HitReaction_Downed);
		break;
	case EBlowbackPhase::Recovery:
		RemoveStateTag(TAG_State_Common_HitReaction_Downed);
		RemoveStateTag(TAG_State_Common_HitReaction_Immune);
		AddStateTag(TAG_State_Common_HitReaction_Recovering);
		// 復帰中は光輪を発光させ接触ダメージ判定を有効化する (破壊不可はSAゲートが別途担当)
		if (HaloComponent) HaloComponent->SetRecoveryArmor(true);
		break;
	default:
		break;
	}
}

void AEnemyCharacter::OnBlowbackEnded()
{
	bIsReacting = false;
	DeathVelocity = FVector::ZeroVector;
	RemoveStateTag(TAG_State_Common_HitReaction_Recovering);
	// 復帰終了で発光＋接触ダメージを解除する
	if (HaloComponent) HaloComponent->SetRecoveryArmor(false);

	if (!HasStateTag(TAG_State_Enemy_Guard))
	{
		// 復帰はIsReacting() がfalseになった時点の優先度再評価が担う
	}
}

// =====================================================================
// IWindAffectable(竜巻などの巻き上げ・速度オーバーライド方式)
// =====================================================================

bool AEnemyCharacter::IsWindAnchored() const
{
	const UEnemyDataAsset* Data = BattleComponent ? BattleComponent->GetDataAsset() : nullptr;
	return Data && Data->WindReaction.bAnchored;
}

void AEnemyCharacter::TriggerAnchoredWindFlinch()
{
	if (StatusComponent && StatusComponent->IsDead()) return;

	const UEnemyDataAsset* Data = BattleComponent ? BattleComponent->GetDataAsset() : nullptr;
	UAnimMontage* Flinch = Data ? Data->WindReaction.LargeWindFlinch.Get() : nullptr;
	if (!Flinch) return;

	// 直近にスリップを受けた時刻を更新する。Tickの終了監視に使う
	LastAnchoredWindHitTime = GetWorld()->GetTimeSeconds();

	// 既にループ中なら再生済み。時刻だけ更新してループ継続に任せる
	if (bAnchoredWindFlinching) return;

	bAnchoredWindFlinching = true;

	// SAを貫通して行動を中断させる。竜巻内にいる間は行動不能にする
	// 解除はTickの終了監視 (bAnchoredWindFlinchingを下ろす箇所) と対になる
	SetReacting(true, TEXT("WindFlinch"));
	if (AAIController* AIC = Cast<AAIController>(GetController()))
	{
		AIC->StopMovement();
	}

	// アンカーのけぞりは巻き上げシーケンスとは別経路。ここで直接再生する
	if (UAnimInstance* AnimInst = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
	{
		AnimInst->Montage_Play(Flinch);
	}

	// のけぞりを内部ループさせる。ブレンドアウトでの再生し直しは同一モンタージュ再入で
	// 「フラグ終了・モンタージュ生存」の孤児状態を招くため使わない。終了はTick監視が行う
	if (UAnimInstance* AnimInst = GetMesh()->GetAnimInstance())
	{
		const FName Section = AnimInst->Montage_GetCurrentSection(Flinch);
		if (!Section.IsNone())
		{
			AnimInst->Montage_SetNextSection(Section, Section, Flinch);
		}
	}
}

void AEnemyCharacter::EndAnchoredWindFlinch()
{
	if (!bAnchoredWindFlinching) return;
	bAnchoredWindFlinching = false;

	// TriggerAnchoredWindFlinchで立てた行動抑止を解除する
	bIsReacting = false;

	// のけぞりモンタージュを明示停止する。AutoBlendOut無効だと
	// 終端でIsPlaying=falseでもポーズを保持し続けるため、
	// IsPlayingでガードせず無条件にStopする(未再生ならno-op)
	// (フラグは先に落としているのでStopのBlendingOut通知で再入しない)
	const UEnemyDataAsset* Data = BattleComponent ? BattleComponent->GetDataAsset() : nullptr;
	UAnimMontage* Flinch = Data ? Data->WindReaction.LargeWindFlinch.Get() : nullptr;
	if (Flinch)
	{
		if (UAnimInstance* AnimInst = GetMesh()->GetAnimInstance())
		{
			AnimInst->Montage_Stop(0.2f, Flinch);
		}
	}

	// 行動を再開する
	RestartAIAfterReaction();
}

void AEnemyCharacter::OnWindEnter(const FWindInfluence& Wind)
{
	// 浮かない敵 (ボス等) は巻き上げに参加しない。スリップダメージは別経路で届く
	if (IsWindAnchored()) return;

	// 風源の数を数える(複数の竜巻に同時に巻き込まれても破綻しないように参照カウント)
	++WindCaptureCount;
}

void AEnemyCharacter::OnWindTick(const FWindInfluence& Wind, float DeltaTime)
{
	// 浮かない敵 (ボス等) は巻き上げない
	if (IsWindAnchored()) return;

	if (StatusComponent && StatusComponent->IsDead()) return;

	// 吹き飛び中は竜巻より優先(速度オーバーライドで吹き飛びと競合させない)
	if (HitReactionComponent && HitReactionComponent->IsInBlowback()) return;

	// まだ捕捉していなければ(巻き込み開始時、または吹き飛び明けなど)、
	// 可能になり次第ここで捕捉する
	if (!bWindCaptured)
	{
		// ガード中の敵は、まず光輪を破壊し、少し間を置いてから巻き込む
		if (bWindGuardBreakPending) return;

		if (HasStateTag(TAG_State_Enemy_Guard))
		{
			HaloComponent->BreakGuardForWind(Wind.Center);

			if (WindGuardBreakDelay > 0.0f)
			{
				bWindGuardBreakPending = true;
				GetWorldTimerManager().SetTimer(WindGuardBreakTimerHandle,
					[this]() { bWindGuardBreakPending = false; },
					WindGuardBreakDelay, false);
				return;
			}
		}

		BeginWindCapture();
		if (!bWindCaptured) return;
	}

	UCharacterMovementComponent* Move = GetCharacterMovement();
	if (!Move) return;

	// 物理打ち上げ(初速ポップ)は上昇中だけ速度上書きを抑制して撃ち出しを活かす
	// 頂点を過ぎて落下に転じたら通常の竜巻挙動(持続揚力)へ復帰し、範囲内での再上昇/落下の
	// 繰り返しを継続させる(飛行モードへ戻して重力と競合させない)
	if (HitReactionComponent && HitReactionComponent->IsWindPhysicsLaunching())
	{
		if (Move->Velocity.Z > 0.0f)
		{
			HitReactionComponent->NotifyWindLaunchRising(); // 上昇を確認(撃ち出しが適用済み)
			return;
		}
		if (!HitReactionComponent->HasWindLaunchPeaked()) return; // 撃ち出し直後で速度未適用 → 抑制継続
		HitReactionComponent->EndWindPhysicsLaunch();
		Move->SetMovementMode(MOVE_Flying);
	}

	const FVector Loc = GetActorLocation();

	// 水平：中心へ引き込む
	FVector ToCenter = Wind.Center - Loc;
	ToCenter.Z = 0.0f;
	const FVector PullVel = ToCenter.GetSafeNormal() * Wind.PullSpeed;

	// 垂直：上限高さまで上昇し、超えたら上昇を止めて滞空させる
	float VerticalVel = Wind.LiftSpeed;
	if (Wind.MaxLiftHeight > 0.0f && Loc.Z >= Wind.Center.Z + Wind.MaxLiftHeight)
	{
		VerticalVel = 0.0f;
	}

	// 竜巻側で速度をオーバーライド(AI移動は停止済み)
	Move->Velocity = FVector(PullVel.X, PullVel.Y, VerticalVel);
}

void AEnemyCharacter::OnWindExit()
{
	// 浮かない敵 (ボス等) は捕捉していないので解放処理も不要
	if (IsWindAnchored()) return;

	if (WindCaptureCount <= 0) return;
	--WindCaptureCount;
	if (WindCaptureCount == 0)
	{
		// 光輪破壊後の捕捉待ちのまま風から外れた場合は待機を打ち切る
		bWindGuardBreakPending = false;
		GetWorldTimerManager().ClearTimer(WindGuardBreakTimerHandle);

		EndWindCapture();
	}
}

// =====================================================================
// IGroundPullAffectable(アリジゴクの渦などの
// 地面の吸い込み。IWindAffectableとは別現象)
// =====================================================================

void AEnemyCharacter::OnGroundPullTick(const FGroundPullInfluence& Pull, float DeltaTime)
{
	// 浮かない敵 (ボス等) と同じ基準で、地面に固定された敵は引き込まない
	if (IsWindAnchored()) return;
	if (StatusComponent && StatusComponent->IsDead()) return;
	// 吹き飛び中・竜巻捕捉中は優先させ、引き込みと競合させない
	if (HitReactionComponent && HitReactionComponent->IsInBlowback()) return;
	if (bWindCaptured) return;

	if (Pull.PullVelocity.IsNearlyZero() || DeltaTime <= 0.0f) return;

	// PL側 (ATidePlayerCharacter::OnGroundPullTick) と同じく、
	// Velocityには混ぜず位置オフセット(速度×DeltaTime)をスイープ込みで加える
	const FVector Offset = Pull.PullVelocity * DeltaTime;
	AddActorWorldOffset(Offset, /*bSweep=*/true);
}

void AEnemyCharacter::BeginWindCapture()
{
	// 死亡中・吹き飛び中は捕捉しない(AI・移動を奪わない)
	if (StatusComponent && StatusComponent->IsDead()) return;
	if (HitReactionComponent && HitReactionComponent->IsInBlowback()) return;

	bWindCaptured = true;

	// 移動を止めて、竜巻側の速度オーバーライドに明け渡す
	// 行動抑止はIsReacting() がIsInWindReaction() を見て自動的に成立する
	if (AAIController* AIC = Cast<AAIController>(GetController()))
	{
		AIC->StopMovement();
	}

	// 重力に縛られず巻き上げられるよう飛行モードへ
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->SetMovementMode(MOVE_Flying);
	}

	// アニメの進行はHitReactionComponent(吹き飛びと共通のドライバ)に任せる
	// ここは物理だけを担当する: 打ち上げ力が返ってきたら撃ち出す
	if (!HitReactionComponent) return;

	HitReactionComponent->SetWindCaptured(true);

	const float LaunchUp = HitReactionComponent->BeginWindReaction();
	if (LaunchUp > 0.0f)
	{
		// 物理打ち上げモード: 飛行モード (重力OFF) だと落ちてこないので落下モードへ戻して撃ち出す
		if (UCharacterMovementComponent* LaunchMove = GetCharacterMovement())
		{
			LaunchMove->SetMovementMode(MOVE_Falling);
		}
		LaunchCharacter(FVector(0.0f, 0.0f, LaunchUp), true, true);
	}
}

void AEnemyCharacter::EndWindCapture()
{
	if (!bWindCaptured) return;
	bWindCaptured = false;

	// 飛行を解除して落下に戻す(着地で自然にWalkingへ)
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		if (Move->MovementMode == MOVE_Flying)
		{
			Move->SetMovementMode(MOVE_Falling);
		}
	}

	// アニメの進行(着地待ち・死亡
	// /吹き飛び時の中断判定)はHitReactionComponent側が持つ
	if (HitReactionComponent)
	{
		HitReactionComponent->SetWindCaptured(false);
		HitReactionComponent->OnWindReleased();
	}
}

void AEnemyCharacter::RestartAIAfterReaction()
{
	if (StatusComponent && StatusComponent->IsDead()) return;
	if (HasStateTag(TAG_State_Enemy_Guard)) return;

	// 復帰そのものはIsReacting() がfalseになった時点の優先度再評価が担う
	// ここでは取り残された抑止フラグを確実に落とすだけ
	SetReacting(false);
}


USceneComponent* AEnemyCharacter::AttachHaloToSocket(FName SocketName, FName HaloComponentName, float Scale)
{
	return HaloComponent->AttachHaloToSocket(SocketName, HaloComponentName, Scale);
}

void AEnemyCharacter::AttachHaloForAttack(FName SocketName, FName HaloComponentName)
{
	HaloComponent->AttachHaloForAttack(SocketName, HaloComponentName);
}

void AEnemyCharacter::RestoreHaloToStateSocket(FName HaloComponentName)
{
	HaloComponent->RestoreHaloToStateSocket(HaloComponentName);
}

void AEnemyCharacter::SetHaloDeployed(bool bActive)
{
	HaloComponent->SetDeployed(bActive);
}

void AEnemyCharacter::SetHaloThrown(bool bThrown)
{
	HaloComponent->SetHaloThrown(bThrown);
}

bool AEnemyCharacter::IsHaloAway() const
{
	return HaloComponent && HaloComponent->IsHaloAway();
}

void AEnemyCharacter::StartHaloThrowRegenIfAway()
{
	HaloComponent->StartThrowRegenIfAway();
}

void AEnemyCharacter::SetHaloAttackGlow(bool bBright)
{
	HaloComponent->SetHaloAttackGlow(bBright);
}

void AEnemyCharacter::SetHaloAttackSuppressed(bool bSuppressed)
{
	if (HaloComponent) HaloComponent->SetHaloAttackSuppressed(bSuppressed);
}

void AEnemyCharacter::BeginGuard(FName HaloComponentName)
{
	HaloComponent->BeginGuard(HaloComponentName);
}

void AEnemyCharacter::StartGuardAutoRelease()
{
	HaloComponent->StartGuardAutoRelease();
}

void AEnemyCharacter::SetHaloBarrierActive(bool bEnable)
{
	HaloComponent->SetHaloBarrierActive(bEnable);
}

void AEnemyCharacter::SetArmamentActive(bool bActive)
{
	const UEnemyDataAsset* Data = BattleComponent ? BattleComponent->GetDataAsset() : nullptr;
	if (!Data || !Data->bUseArmament) return;

	const bool bCurrentlyActive = HasStateTag(TAG_State_Enemy_Armament);
	if (bActive != bCurrentlyActive)
	{
		if (bActive)
		{
			AddStateTag(TAG_State_Enemy_Armament);
		}
		else
		{
			RemoveStateTag(TAG_State_Enemy_Armament);
		}
	}

	ApplyArmamentVisual(bActive ? 0.0f : 1.0f);
}

bool AEnemyCharacter::IsArmamentActive() const
{
	return HasStateTag(TAG_State_Enemy_Armament);
}

void AEnemyCharacter::BeginArmamentSoftWindow()
{
	bArmamentSoftWindowOpen = true;
	bArmamentRestorePending = false;
	SetArmamentActive(false);
}

void AEnemyCharacter::EndArmamentSoftWindow()
{
	if (!bArmamentSoftWindowOpen) return;
	bArmamentSoftWindowOpen = false;

	// 部位破壊/連動リアクションが再生中なら、殴れる窓を途切れさせないため
	// 再硬化をリアクション終了まで遅延する
	if (IsPlayingPartReaction())
	{
		bArmamentRestorePending = true;
		return;
	}

	SetArmamentActive(true);
}

void AEnemyCharacter::NotifyPartReactionEnded()
{
	if (!bArmamentRestorePending) return;
	bArmamentRestorePending = false;
	SetArmamentActive(true);
}

void AEnemyCharacter::ApplyBodyColor()
{
	const UEnemyDataAsset* Data = BattleComponent ? BattleComponent->GetDataAsset() : nullptr;
	if (!Data) return;

	if (!Data->BodyColorParamName.IsNone())
	{
		for (TObjectPtr<UMaterialInstanceDynamic>& DMI : BodyDMIs)
		{
			if (DMI)
			{
				DMI->SetVectorParameterValue(Data->BodyColorParamName, Data->BodyColor);
			}
		}
	}

	// P0対処 常駐効果に注入
	for (UActorComponent* Comp : GetComponentsByTag(
		UNiagaraComponent::StaticClass(), TEXT("BodyColorVFX")))
	{
		if (UNiagaraComponent* VFX = Cast<UNiagaraComponent>(Comp))
		{
			VFX->SetVariableLinearColor(TEXT("Color"), Data->BodyColor);
		}
	}
}

void AEnemyCharacter::ApplyArmamentVisual(float Value)
{
	const UEnemyDataAsset* Data = BattleComponent ? BattleComponent->GetDataAsset() : nullptr;
	if (!Data || !Data->bUseArmament || Data->ArmamentVisualParamName.IsNone()) return;

	for (TObjectPtr<UMaterialInstanceDynamic>& DMI : BodyDMIs)
	{
		if (DMI)
		{
			DMI->SetScalarParameterValue(Data->ArmamentVisualParamName, Value);
		}
	}
}

void AEnemyCharacter::SetHaloGlowIntensity(float Intensity)
{
	if (HaloComponent) HaloComponent->SetHaloGlowIntensity(Intensity);
}

void AEnemyCharacter::EndGuardNow(bool bRestartAI)
{
	if (HaloComponent) HaloComponent->EndGuard(bRestartAI);
}

void AEnemyCharacter::ClearGuardMotionArmor()
{
	// NotifyのNotifyEndがこの後に来てもRemoveStateTagは無害
	// (カウントは既に消えている)
	if (StateTagComponent)
		StateTagComponent->ForceRemoveStateTagsByParent(TAG_State_Common_SuperArmor_HaloOpen);
}

bool AEnemyCharacter::IsBackAttack(const FDamageInfo& DamageInfo) const
{
	if (!DamageInfo.Instigator.IsValid()) return false;

	FVector ToAttacker = DamageInfo.Instigator->GetActorLocation() - GetActorLocation();
	ToAttacker.Normalize();

	return FVector::DotProduct(GetActorForwardVector(), ToAttacker) < 0.35f;
}
