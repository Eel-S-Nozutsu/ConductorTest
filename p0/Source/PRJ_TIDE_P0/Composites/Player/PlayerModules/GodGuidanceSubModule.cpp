// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#include "GodGuidanceSubModule.h"

#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "PRJ_TIDE_P0/Actors/Gimmick/BoostGimmick.h"
#if !UE_BUILD_SHIPPING
#include "DrawDebugHelpers.h"
#include "imgui.h"
#endif

#include "PRJ_TIDE_P0/Composites/Player/PlayerModules/GodActionPlayerModule.h"
#include "PRJ_TIDE_P0/Actors/Character/Player/TidePlayerCharacter.h"
#include "PRJ_TIDE_P0/Actors/Companion/TideGodBird.h"
#include "PRJ_TIDE_P0/Data/Player/TidePlayerParamDataAsset.h"
#include "PRJ_TIDE_P0/Data/Tags/TideGameplayTags.h"
#include "PRJ_TIDE_P0/Data/Player/PlayerAnimTags.h"
#include "PRJ_TIDE_P0/Data/Combat/DamageInfo.h"
#include "PRJ_TIDE_P0/Data/Combat/AttackParameterRow.h"
#include "PRJ_TIDE_P0/Data/Character/TideCharacterDataAsset.h"
#include "PRJ_TIDE_P0/Interfaces/Combat/IDamageable.h"
#include "PRJ_TIDE_P0/Utilities/TideCombatUtils.h"

namespace
{
	// 導きはチャージギア概念が無いためタグ一致の行を使う（行は 1 つ想定）
	const FPlayerAttackParameterRow* FindGodGuidanceRow( const ATidePlayerCharacter* Player )
	{
		if ( !Player ) return nullptr;
		const UTideCharacterDataAsset* DataAsset = Player->GetCharacterData();
		if ( !DataAsset || !DataAsset->AttackParameterTable ) return nullptr;

		static const FString Context = TEXT( "GodGuidanceAttackParamLookup" );
		TArray<FPlayerAttackParameterRow*> Rows;
		DataAsset->AttackParameterTable->GetAllRows<FPlayerAttackParameterRow>( Context, Rows );
		for ( const FPlayerAttackParameterRow* Row : Rows )
		{
			if ( Row && Row->AttackTypeTag == TAG_AttackType_Player_GodActionGuidance )
			{
				return Row;
			}
		}
		return nullptr;
	}
}

void UGodGuidanceSubModule::OnModuleUpdate( float DeltaTime )
{
	if ( !bActive ) return;

	ATideGodBird* Bird = GuidanceBird.Get();
	if ( !Bird )
	{
		EndGuidance();	// 鳥が自壊済み
		return;
	}

	// ST モーション中は範囲ダメージ・ギミック設置・寿命計測を保留する（持続時間は Loop 開始から計測）
	if ( Bird->IsStartMotionActive() ) return;

	Elapsed += DeltaTime;

	// ST が終わったら ED を再生し、ED が終わったらキャスト完了（プレイヤー解放）。
	// ED 中は憑依と同じく、CanMove 付与後の移動入力で締めをキャンセルできる
	if ( !bCastEnded )
	{
		if ( bCastInEnd && OwnerCharacter
			&& OwnerCharacter->HasStateTag( TAG_State_Player_CanMove )
			&& !OwnerCharacter->GetRawMovementInput().IsNearlyZero( 0.1f ) )
		{
			OwnerCharacter->StopAnimMontage();	// 締めを中断して即移動できるようにする
			bCastEnded = true;
		}
		else
		{
			MontageTimer -= DeltaTime;
			if ( MontageTimer <= 0.0f )
			{
				if ( !bCastInEnd )
				{
					MontageTimer = PlayAnimMontage( PlayerAnimTags::GOD_SLASH_ATK_ED );
					bCastInEnd = true;
				}
				else
				{
					bCastEnded = true;	// ED まで再生完了 → 傘が滞空解除して落下・自由操作へ戻す
				}
			}
		}
	}

	ApplyChargeDamage();			// 鳥の現在地を中心に範囲ダメージ
	DropBoostGimmicksAlongPath();	// 通った軌跡に沿って加速ギミックを等間隔で配置

	// 鳥自身も自壊するが、状態はこちらでも閉じる
	const UTidePlayerParamDataAsset* Params = GetParams();
	const float Duration = Params ? Params->GuidanceDuration : 0.6f;
	if ( Duration > 0.0f && Elapsed >= Duration )
	{
		EndGuidance();
	}
}

void UGodGuidanceSubModule::Execute()
{
	if ( bActive ) return;	// 二重発動防止
	if ( !OwnerCharacter ) return;

	const UTidePlayerParamDataAsset* Params = GetParams();
	if ( !Params ) return;

	// 専用クラスを優先し、無ければ常駐お供のクラスを流用する。どちらも無ければ何もしない（ゲージも消費しない）
	TSubclassOf<ATideGodBird> BirdClass = Params->GuidanceBirdClass;
	if ( !BirdClass ) BirdClass = Params->GodBirdActorClass;
	if ( !BirdClass ) return;

	UWorld* World = OwnerCharacter->GetWorld();
	if ( !World ) return;

	const FVector Destination = ComputeDestination();

	// プレイヤー座標そのままではなく、ずらした位置で ST を再生する。
	// 向きは ActivateGuidanceCharge が到達点方向＋メッシュ補正で設定する
	const FVector SpawnLoc = OwnerCharacter->GetActorTransform().TransformPositionNoScale( Params->GuidanceBirdSpawnOffset );

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = OwnerCharacter;
	SpawnParams.Instigator = OwnerCharacter;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ATideGodBird* Bird = World->SpawnActor<ATideGodBird>(
		BirdClass, SpawnLoc, OwnerCharacter->GetActorRotation(), SpawnParams );
	if ( !Bird ) return;

	// 巨大化＋到達点への突進を起動（移動・寿命は鳥側が持つ）。
	// ST モーション中は開始スケールから最終スケールまで徐々に拡大する
	const float StartScale = Params->GuidanceBirdScale * Params->GuidanceBirdStartScaleRatio;
	Bird->ActivateGuidanceCharge( Destination, Params->GuidanceChargeSpeed, Params->GuidanceDuration, Params->GuidanceBirdScale, Params->GodBirdRelativeRotation, StartScale );

	// プレイヤーは専用モーションを行わず直前のアクションを継続する。
	// キャストは即完了扱いにして滞空ホールド等を発生させない
	MontageTimer = 0.0f;
	bCastInEnd = true;
	bCastEnded = true;

	GuidanceBird = Bird;
	LastDamageTimes.Reset();
	BoostPathOrigin = SpawnLoc;	// ここからの距離で加速ギミックを刻む

	// 先頭ギミックまでの距離＝StartOffset。0 なら Interval まで空ける（プレイヤーへ当てないため）
	{
		const float StartOffset = FMath::Max( 0.0f, Params->GuidanceBoostGimmickStartOffset );
		NextBoostDropDistance = ( StartOffset > 0.0f ) ? StartOffset : Params->GuidanceBoostGimmickInterval;
	}
	Elapsed = 0.0f;
	bActive = true;

#if !UE_BUILD_SHIPPING
	if ( Params->bGuidanceDrawDestination )
	{
		DrawDebugBox( World, Destination, FVector( 50.0f ), FColor::Cyan, false, FMath::Max( 0.1f, Params->GuidanceDuration ) + 1.0f, 0, 3.0f );
	}
#endif

	// 無限ゲージ時のスキップは傘 ConsumeGauge が担う
	if ( OwnerModule )
	{
		OwnerModule->ConsumeGauge( 1.0f );
	}
}

FVector UGodGuidanceSubModule::GetAimForward2D() const
{
	// カメラ方向（Yaw の水平前方）。取れなければコントロール回転→アクター前方の順で代用する
	if ( OwnerCharacter )
	{
		if ( const APlayerController* PC = Cast<APlayerController>( OwnerCharacter->GetController() ) )
		{
			const FRotator CamRot = PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraRotation() : PC->GetControlRotation();
			const FVector Fwd = FRotator( 0.0f, CamRot.Yaw, 0.0f ).Vector();
			if ( !Fwd.IsNearlyZero() ) return Fwd.GetSafeNormal2D();
		}
		return OwnerCharacter->GetActorForwardVector().GetSafeNormal2D();
	}
	return FVector::ForwardVector;
}

FVector UGodGuidanceSubModule::GetAimForward3D() const
{
	// カメラのピッチ＋ヨー方向そのまま。取れなければコントロール回転→アクター前方の順で代用する
	if ( OwnerCharacter )
	{
		if ( const APlayerController* PC = Cast<APlayerController>( OwnerCharacter->GetController() ) )
		{
			const FRotator CamRot = PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraRotation() : PC->GetControlRotation();
			const FVector Fwd = CamRot.Vector();
			if ( !Fwd.IsNearlyZero() ) return Fwd.GetSafeNormal();
		}
		return OwnerCharacter->GetActorForwardVector().GetSafeNormal();
	}
	return FVector::ForwardVector;
}

FVector UGodGuidanceSubModule::ComputeDestination() const
{
	if ( !OwnerCharacter ) return FVector::ZeroVector;

	const UTidePlayerParamDataAsset* Params = GetParams();
	const float Distance = Params ? Params->GuidanceDistance : 1500.0f;

	const FVector PlayerLoc = OwnerCharacter->GetActorLocation();

	// 3D 発射は着地補正をしない（上下へも真っ直ぐ飛ばせる）
	if ( Params && Params->bGuidanceAim3D )
	{
		return PlayerLoc + GetAimForward3D() * Distance;
	}

	// 2D 発射は水平前方（Yaw のみ）＋到達点の真下トレースで着地補正する
	const FVector Forward2D = GetAimForward2D();
	FVector Dest = PlayerLoc + Forward2D * Distance;

	if ( UWorld* World = OwnerCharacter->GetWorld() )
	{
		const FVector TraceStart = Dest + FVector( 0.0f, 0.0f, 500.0f );
		const FVector TraceEnd = Dest - FVector( 0.0f, 0.0f, 5000.0f );
		FHitResult Hit;
		FCollisionQueryParams QueryParams( SCENE_QUERY_STAT( GodGuidanceGround ), false, OwnerCharacter );
		if ( World->LineTraceSingleByChannel( Hit, TraceStart, TraceEnd, ECC_Visibility, QueryParams ) )
		{
			Dest.Z = Hit.ImpactPoint.Z;
		}
		else
		{
			Dest.Z = PlayerLoc.Z;	// 地面が見つからなければプレイヤー高さのまま前方へ
		}
	}
	return Dest;
}

void UGodGuidanceSubModule::ApplyChargeDamage()
{
	ATideGodBird* Bird = GuidanceBird.Get();
	if ( !Bird || !OwnerCharacter ) return;

	UWorld* World = OwnerCharacter->GetWorld();
	if ( !World ) return;

	const UTidePlayerParamDataAsset* Params = GetParams();
	if ( !Params ) return;

	const float Radius = FMath::Max( 1.0f, Params->GuidanceDamageRadius );
	// 行が無ければ DA 値へフォールバックする
	float Damage = Params->GuidanceDamage;
	if ( const FPlayerAttackParameterRow* AttackRow = FindGodGuidanceRow( OwnerCharacter ) )
	{
		Damage = OwnerCharacter->CalculateDamage( AttackRow->DamageMultiplier );
	}
	const float ReArm = FMath::Max( 0.0f, Params->GuidanceDamageReArmInterval );

	// 短い縦スイープで範囲内アクターを集める（地形は拾わない）
	FCollisionObjectQueryParams ObjQuery = FCollisionObjectQueryParams::AllDynamicObjects;
	FCollisionQueryParams QueryParams( SCENE_QUERY_STAT( GodGuidanceDamage ), false, Bird );
	QueryParams.AddIgnoredActor( OwnerCharacter );

	const FCollisionShape Shape = FCollisionShape::MakeSphere( Radius );
	const FVector Center = Bird->GetActorLocation();
	const FVector SweepStart = Center - FVector( 0.0f, 0.0f, 1.0f );
	const FVector SweepEnd   = Center + FVector( 0.0f, 0.0f, 1.0f );

#if !UE_BUILD_SHIPPING
	if ( Params->bGuidanceDrawDamageRange )
	{
		DrawDebugSphere( World, Center, Radius, 16, FColor::Orange, false, -1.0f, 0, 1.5f );
	}
#endif

	TArray<FHitResult> Hits;
	World->SweepMultiByObjectType( Hits, SweepStart, SweepEnd, FQuat::Identity, ObjQuery, Shape, QueryParams );

	const double Now = World->GetTimeSeconds();

	// 同一アクターに複数コンポーネントがヒットしても 1 回に絞る
	TSet<AActor*> Processed;
	for ( const FHitResult& Hit : Hits )
	{
		AActor* Other = Hit.GetActor();
		if ( !Other || Other == Bird || Other == OwnerCharacter ) continue;
		if ( Processed.Contains( Other ) ) continue;
		if ( !TideCombatUtil::IsHostileTo( OwnerCharacter, Other ) ) continue;

		IDamageable* Damageable = Cast<IDamageable>( Other );
		if ( !Damageable ) continue;

		Processed.Add( Other );

		// 多段ヒット防止
		if ( const double* Last = LastDamageTimes.Find( Other ) )
		{
			if ( Now - *Last < ReArm ) continue;
		}
		LastDamageTimes.Add( Other, Now );

		FDamageInfo Info;
		Info.BaseDamage = Damage;
		Info.Instigator = OwnerCharacter;
		Info.HitReactionTag = TAG_HitReaction_Blowoff_M;
		Info.HitResult = Hit;

		Damageable->ReceiveDamage( Info );
	}
}

void UGodGuidanceSubModule::DropBoostGimmicksAlongPath()
{
	ATideGodBird* Bird = GuidanceBird.Get();
	if ( !Bird || !OwnerCharacter ) return;

	const UTidePlayerParamDataAsset* Params = GetParams();
	if ( !Params || !Params->GuidanceBoostGimmickClass ) return;	// クラス未設定なら配置しない

	const float Interval = Params->GuidanceBoostGimmickInterval;
	if ( Interval <= 0.0f ) return;

	UWorld* World = OwnerCharacter->GetWorld();
	if ( !World ) return;

	// まだ動いていなければ何もしない
	const FVector ToBird = Bird->GetActorLocation() - BoostPathOrigin;
	const float DistFromOrigin = ToBird.Size();
	const FVector Dir = ToBird.GetSafeNormal();
	if ( Dir.IsNearlyZero() ) return;

	// 中心＋左右の 3 列配置に使う。Dir が斜め（3D 突進）でも Up との外積で「水平な真横」になる
	const float ColumnSpacing = Params->GuidanceBoostGimmickColumnSpacing;
	const FVector SidePerp = FVector::CrossProduct( Dir, FVector::UpVector ).GetSafeNormal();
	const bool bUseSideColumns = ( ColumnSpacing > 0.0f ) && !SidePerp.IsNearlyZero();

	// 原点からの距離で直接刻み、鳥が到達した距離まで置き残しを埋める
	// （1 フレームで大きく進んでも等間隔になる）
	while ( NextBoostDropDistance <= DistFromOrigin )
	{
		const FVector CenterLoc = BoostPathOrigin + Dir * NextBoostDropDistance;
		const FRotator Rotation = Dir.Rotation();

		// 列間隔が 0 以下なら中心のみ。各列で個別に地面高さを確保する
		SpawnBoostGimmickAt( World, Params, CenterLoc, Rotation );
		if ( bUseSideColumns )
		{
			SpawnBoostGimmickAt( World, Params, CenterLoc + SidePerp * ColumnSpacing, Rotation );
			SpawnBoostGimmickAt( World, Params, CenterLoc - SidePerp * ColumnSpacing, Rotation );
		}

		NextBoostDropDistance += Interval;
	}
}

void UGodGuidanceSubModule::SpawnBoostGimmickAt( UWorld* World, const UTidePlayerParamDataAsset* Params, FVector DropLoc, const FRotator& Rotation )
{
	if ( !World || !Params || !Params->GuidanceBoostGimmickClass || !OwnerCharacter ) return;

	// 地面ギリギリを飛ぶ軌跡でも埋まらないよう、真下トレースで地面＋最低高さまで持ち上げる
	// （0 以下なら軌跡位置のまま）
	const float MinGroundHeight = Params->GuidanceBoostGimmickMinGroundHeight;
	if ( MinGroundHeight > 0.0f )
	{
		const FVector TraceStart = DropLoc + FVector( 0.0f, 0.0f, 200.0f );
		const FVector TraceEnd   = DropLoc - FVector( 0.0f, 0.0f, 5000.0f );
		FHitResult GroundHit;
		FCollisionQueryParams GroundQuery( FName( TEXT( "GuidanceBoostGround" ) ), false, OwnerCharacter );
		if ( World->LineTraceSingleByChannel( GroundHit, TraceStart, TraceEnd, ECC_Visibility, GroundQuery ) )
		{
			const float MinZ = GroundHit.ImpactPoint.Z + MinGroundHeight;
			if ( DropLoc.Z < MinZ ) DropLoc.Z = MinZ;
		}
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = OwnerCharacter;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ABoostGimmick* Pad = World->SpawnActor<ABoostGimmick>( Params->GuidanceBoostGimmickClass, DropLoc, Rotation, SpawnParams );
	if ( Pad && Params->GuidanceBoostGimmickLifeTime > 0.0f )
	{
		Pad->SetLifeSpan( Params->GuidanceBoostGimmickLifeTime );	// 0以下なら消えない（BP の復活設定に従う）
	}
}

void UGodGuidanceSubModule::EndGuidance()
{
	// プレイヤーの発動モーション（ST→ED）は Execute/OnModuleUpdate 側で独立して流し終えるので触らない
	bActive = false;
	Elapsed = 0.0f;
	LastDamageTimes.Reset();

	// 鳥は寿命で自壊するが、途中終了などで残っていれば破棄する
	if ( ATideGodBird* Bird = GuidanceBird.Get() )
	{
		Bird->Destroy();
	}
	GuidanceBird = nullptr;
}

#if !UE_BUILD_SHIPPING
void UGodGuidanceSubModule::DrawDebugImGui()
{
	ImGui::Text( "導き: %s", bActive ? "発動中" : "待機" );
	if ( bActive )
	{
		ImGui::Text( "  経過: %.2f", Elapsed );
		ImGui::Text( "  鳥: %s", GuidanceBird.IsValid() ? "有効" : "なし" );
	}
}

void UGodGuidanceSubModule::DrawRangePreview() const
{
	if ( !OwnerCharacter ) return;

	UWorld* World = OwnerCharacter->GetWorld();
	if ( !World ) return;

	const UTidePlayerParamDataAsset* Params = GetParams();
	if ( !Params ) return;

	// 現在地→到達点を軸に、範囲ダメージ半径ぶんの太さを持つ帯（＝突進で通り抜ける範囲）。
	// 発動時と同じ ComputeDestination で到達点を求め、その線分を包む OBB を描く
	const FVector Start = OwnerCharacter->GetActorLocation();
	const FVector Dest  = ComputeDestination();
	const FVector Delta = Dest - Start;
	const float Length  = Delta.Size();
	const float Radius  = FMath::Max( 1.0f, Params->GuidanceDamageRadius );

	// X＝進行方向（半分＋半径ぶん端に膨らませる）、Y/Z＝半径
	const FVector Center = ( Start + Dest ) * 0.5f;
	const FRotator BoxRot = ( Length > KINDA_SMALL_NUMBER ) ? Delta.GetSafeNormal().Rotation() : FRotator::ZeroRotator;
	const FVector Extent( Length * 0.5f + Radius, Radius, Radius );

	// 神鳥の黄緑。毎フレーム呼ぶので単フレーム描画
	const FColor YellowGreen( 102, 255, 26 );
	DrawDebugBox( World, Center, Extent, BoxRot.Quaternion(), YellowGreen, false, -1.0f, 0, 3.0f );
}
#endif
