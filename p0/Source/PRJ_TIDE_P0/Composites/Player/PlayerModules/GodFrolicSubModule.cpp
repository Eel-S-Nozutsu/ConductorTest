// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#include "GodFrolicSubModule.h"

#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
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
#include "PRJ_TIDE_P0/Data/Niagara/NiagaraSystemListDataAsset.h"
#include "PRJ_TIDE_P0/Data/Player/PlayerNiagaraTags.h"

namespace
{
	// 戯れはチャージギア概念が無いためタグ一致の行を使う（行は 1 つ想定）
	const FPlayerAttackParameterRow* FindGodFrolicRow( const ATidePlayerCharacter* Player )
	{
		if ( !Player ) return nullptr;
		const UTideCharacterDataAsset* DataAsset = Player->GetCharacterData();
		if ( !DataAsset || !DataAsset->AttackParameterTable ) return nullptr;

		static const FString Context = TEXT( "GodFrolicAttackParamLookup" );
		TArray<FPlayerAttackParameterRow*> Rows;
		DataAsset->AttackParameterTable->GetAllRows<FPlayerAttackParameterRow>( Context, Rows );
		for ( const FPlayerAttackParameterRow* Row : Rows )
		{
			if ( Row && Row->AttackTypeTag == TAG_AttackType_Player_GodActionFrolic )
			{
				return Row;
			}
		}
		return nullptr;
	}
}

void UGodFrolicSubModule::Execute( const TArray<AActor*>& InLockedTargets )
{
	if ( bActive ) return;	// 二重発動防止
	if ( !OwnerCharacter ) return;

	const UTidePlayerParamDataAsset* Params = GetParams();
	if ( !Params ) return;

	// 専用クラス、未設定なら常駐お供のクラスを流用する。どちらも無ければ何もしない（ゲージも消費しない）
	TSubclassOf<ATideGodBird> BirdClass = Params->GodFrolicBirdClass;
	if ( !BirdClass ) BirdClass = Params->GodBirdActorClass;
	if ( !BirdClass ) return;

	UWorld* World = OwnerCharacter->GetWorld();
	if ( !World ) return;

	// プレイヤー座標そのままではなく、右肩上などのお供位置で ST を再生する
	const FVector SpawnLoc = GetHomeLocation();

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = OwnerCharacter;
	SpawnParams.Instigator = OwnerCharacter;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ATideGodBird* Bird = World->SpawnActor<ATideGodBird>(
		BirdClass, SpawnLoc, OwnerCharacter->GetActorRotation(), SpawnParams );
	if ( !Bird ) return;

	BirdBaseScale = FMath::Max( 0.01f, Bird->GetActorScale3D().X );	// 帰るときにここへ縮めて戻す

	// 外部駆動＋拡大で自律攻撃モードに（位置・向きは本サブモジュールが動かす）。
	// ST モーション中は開始スケールから最終スケールまで徐々に拡大する
	const float StartScale = Params->GodFrolicBirdScale * Params->GodFrolicBirdStartScaleRatio;
	Bird->ActivateFrolicAttack( Params->GodFrolicBirdScale, Params->GodBirdRelativeRotation, StartScale );

	// 初期向きはカメラ正面（Yaw のみ）。メッシュ補正は ActivateFrolicAttack で設定済み
	FRotator CamRot = OwnerCharacter->GetActorRotation();
	if ( const APlayerController* PC = Cast<APlayerController>( OwnerCharacter->GetController() ) )
	{
		CamRot = PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraRotation() : PC->GetControlRotation();
	}
	Bird->SetActorFacingWithModelOffset( FRotator( 0.0f, CamRot.Yaw, 0.0f ) );

	// 旋回の起点もカメラ正面。ここから GodFrolicTurnRate ぶんずつ狙う方向へ寄せていく
	FlyDirection = FRotator( 0.0f, CamRot.Yaw, 0.0f ).Vector();

	SpawnFrolicBirdVFX( PlayerNiagaraTags::GODBIRD_APPEAR, Bird->GetActorLocation() );

	// 専用トレイルを鳥メッシュへアタッチする（bAutoDestroy=false でも鳥の破棄で一緒に消える）
	if ( OwnerCharacter->NiagaraSystemDataAsset )
	{
		if ( UNiagaraSystem* TrailSys = OwnerCharacter->NiagaraSystemDataAsset->GetNiagaraSystem( PlayerNiagaraTags::GODBIRD_FROLIC_TRAIL ) )
		{
			USceneComponent* AttachTarget = Bird->GetBirdMesh() ? (USceneComponent*)Bird->GetBirdMesh() : Bird->GetRootComponent();
			// 回転オフセットで縦向きのトレイルを倒す
			const FRotator TrailRotation = Params->GodFrolicTrailRotation;
			if ( UNiagaraComponent* Trail = UNiagaraFunctionLibrary::SpawnSystemAttached(
				TrailSys, AttachTarget, NAME_None, FVector::ZeroVector, TrailRotation,
				EAttachLocation::SnapToTarget, false, true ) )
			{
				Trail->SetFloatParameter( TEXT( "LifeTime" ), 0.4f );	// 高速飛行の軌跡を見せるリボン寿命
				Trail->SetColorParameter( TEXT( "Color" ), FLinearColor( 0.4f, 1.0f, 0.1f, 1.0f ) );
			}
		}
	}

	// プレイヤーは専用モーションを行わず直前のアクションを継続する。
	// キャストは即完了扱いにして滞空ホールド等を発生させない
	MontageTimer = 0.0f;
	bCastInEnd = true;
	bCastEnded = true;

	FrolicBird = Bird;

	// 巡回対象＝構え中のロックオン敵。空なら OnModuleUpdate で最寄りを狙う
	LockedTargets.Reset();
	for ( AActor* A : InLockedTargets )
	{
		if ( A ) LockedTargets.Add( A );
	}
	LockedCursor = -1;	// 最初の巡回で index 0 から始める（PickNextTarget が +1 する）

	CurrentTarget = nullptr;
	RetargetTimer = 0.0f;
	LastDamageTimes.Reset();
	Elapsed = 0.0f;
	BirdPhase = EBirdPhase::Dive;
	NoTargetElapsed = 0.0f;
	PulloutTarget = nullptr;
	PulloutElapsed = 0.0f;
	bActive = true;

	// 無限ゲージ時のスキップは傘 ConsumeGauge が担う
	if ( OwnerModule )
	{
		OwnerModule->ConsumeGauge( 1.0f );
	}
}

void UGodFrolicSubModule::OnModuleUpdate( float DeltaTime )
{
	if ( !bActive ) return;

	ATideGodBird* Bird = FrolicBird.Get();
	if ( !Bird || !OwnerCharacter )
	{
		EndFrolic();
		return;
	}

	// ST モーション完了は待たず、スポーン直後から飛び回り・攻撃を開始する。
	// ST アニメとスケール拡大は TideGodBird::Tick 側で独立に再生されるので干渉しない
	Elapsed += DeltaTime;

	const UTidePlayerParamDataAsset* Params = GetParams();

	// ST が終わったら ED を再生し、ED が終わったらキャスト完了（プレイヤー解放）。
	// ED 中は憑依と同じく、CanMove 付与後の移動入力で締めをキャンセルできる
	if ( !bCastEnded )
	{
		if ( bCastInEnd
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
					bCastEnded = true;
				}
			}
		}
	}

	// 帰り道は攻撃しない（縮みながら戻るだけ）
	if ( BirdPhase == EBirdPhase::Return )
	{
		UpdateReturn( DeltaTime );
		return;
	}

	if ( BirdPhase == EBirdPhase::Pullout )
	{
		UpdatePullout( DeltaTime );
	}
	else
	{
		UpdateDive( DeltaTime );
	}

	// 鳥の現在地中心に範囲ダメージ。離脱中も出して抜け際の別の敵を巻き込む
	ApplyAttackDamage();

	// 帰還中はここまで来ないので、戻りきるまで打ち切られない
	const float Duration = Params ? Params->GodFrolicDuration : 3.0f;
	if ( Duration > 0.0f && Elapsed >= Duration )
	{
		EndFrolic();
	}
}

void UGodFrolicSubModule::UpdateDive( float DeltaTime )
{
	ATideGodBird* Bird = FrolicBird.Get();
	if ( !Bird ) return;

	const UTidePlayerParamDataAsset* Params = GetParams();

	// ダメージ半径内へ到達（＝当てた）するまで同じ対象へ突入し続ける。途中で切り替えると
	// 到達前に折り返して見えるため。GodFrolicChaseGiveUpTime は到達できない対象の見限り用
	{
		const float ReachR = Params ? Params->GodFrolicDamageRadius : 300.0f;
		const FVector BirdLoc = Bird->GetActorLocation();
		const AActor* Cur = CurrentTarget.Get();
		const bool bReachedCurrent = Cur
			&& FVector::DistSquared( Cur->GetActorLocation(), BirdLoc ) <= ReachR * ReachR;

		// 離脱距離 0 は離脱なし＝当てたら即次の対象へ（敵の周りを旋回し続ける）
		const float PulloutDist = Params ? Params->GodFrolicPulloutDistance : 800.0f;
		if ( bReachedCurrent && PulloutDist > 0.0f )
		{
			BeginPullout();
			return;
		}

		RetargetTimer -= DeltaTime;
		if ( !Cur || bReachedCurrent || RetargetTimer <= 0.0f )
		{
			CurrentTarget = PickNextTarget();
			RetargetTimer = Params ? FMath::Max( 0.1f, Params->GodFrolicChaseGiveUpTime ) : 2.0f;
		}
	}

	// 直接向き直すのではなく旋回上限で寄せるため、対象を通り過ぎたら弧を描いて旋回し直す
	if ( AActor* Target = CurrentTarget.Get() )
	{
		BirdPhase = EBirdPhase::Dive;
		NoTargetElapsed = 0.0f;

		const FVector BirdLoc = Bird->GetActorLocation();
		const FVector Desired = ( Target->GetActorLocation() - BirdLoc ).GetSafeNormal();
		const float Step = ( Params ? Params->GodFrolicMoveSpeed : 2500.0f ) * DeltaTime;

		MoveAlongFlyDirection( Bird, Desired, Step, DeltaTime );
		return;
	}

	// 対象なし：その場で待機する（この間も上の狙い直しで探し続ける）。
	// 待機が尽きたら持続時間の残りは使わず帰還へ移る
	BirdPhase = EBirdPhase::Wait;
	NoTargetElapsed += DeltaTime;

	// ST 中はスケールを鳥側の拡大演出が握っているので帰還へ入らない（縮小と取り合う）
	const float WaitTime = Params ? Params->GodFrolicNoTargetWaitTime : 2.0f;
	if ( NoTargetElapsed >= WaitTime && !Bird->IsStartMotionActive() )
	{
		BeginReturn();
	}
}

void UGodFrolicSubModule::BeginPullout()
{
	BirdPhase = EBirdPhase::Pullout;
	PulloutTarget = CurrentTarget;
	PulloutElapsed = 0.0f;

	// 突入してきた勢いのまま抜ける向き＋上方成分。真後ろへ引き返すと U ターンに見えるので、
	// 通り抜けて上へ逃がしてから突っ込み直す
	const UTidePlayerParamDataAsset* Params = GetParams();
	const float Rise = Params ? Params->GodFrolicPulloutRiseRatio : 0.5f;
	PulloutDirection = ( FlyDirection + FVector::UpVector * Rise ).GetSafeNormal();
	if ( PulloutDirection.IsNearlyZero() )
	{
		PulloutDirection = FlyDirection;
	}
}

void UGodFrolicSubModule::UpdatePullout( float DeltaTime )
{
	ATideGodBird* Bird = FrolicBird.Get();
	if ( !Bird ) return;

	const UTidePlayerParamDataAsset* Params = GetParams();
	const float Step = ( Params ? Params->GodFrolicMoveSpeed : 2000.0f ) * DeltaTime;

	MoveAlongFlyDirection( Bird, PulloutDirection, Step, DeltaTime );

	PulloutElapsed += DeltaTime;

	// 離脱距離ぶん離れたら次の突入へ。倒した／消えた対象からは即戻り、
	// 壁際などで離れきれない場合は GodFrolicPulloutMaxTime で打ち切る
	bool bDone = PulloutElapsed >= ( Params ? Params->GodFrolicPulloutMaxTime : 1.0f );
	if ( !bDone )
	{
		const AActor* From = PulloutTarget.Get();
		if ( !From )
		{
			bDone = true;
		}
		else
		{
			const float Dist = Params ? Params->GodFrolicPulloutDistance : 800.0f;
			bDone = FVector::DistSquared( From->GetActorLocation(), Bird->GetActorLocation() ) >= Dist * Dist;
		}
	}
	if ( !bDone ) return;

	// ロックオン巡回のカーソルはここで進む。居なければ待機へ移し、以降の判断は UpdateDive に任せる
	CurrentTarget = PickNextTarget();
	RetargetTimer = Params ? FMath::Max( 0.1f, Params->GodFrolicChaseGiveUpTime ) : 2.0f;
	BirdPhase = CurrentTarget.IsValid() ? EBirdPhase::Dive : EBirdPhase::Wait;
	PulloutTarget = nullptr;
}

void UGodFrolicSubModule::BeginReturn()
{
	BirdPhase = EBirdPhase::Return;
	ReturnStartDistance = 0.0f;
	ReturnStartScale = BirdBaseScale;
	ReturnScaleAlpha = 0.0f;

	// 残り距離の比で縮めるための基準を採っておく
	if ( const ATideGodBird* Bird = FrolicBird.Get() )
	{
		ReturnStartDistance = FVector::Dist( GetHomeLocation(), Bird->GetActorLocation() );
		ReturnStartScale = FMath::Max( 0.01f, Bird->GetActorScale3D().X );
	}
}

void UGodFrolicSubModule::UpdateReturn( float DeltaTime )
{
	ATideGodBird* Bird = FrolicBird.Get();
	if ( !Bird || !OwnerCharacter )
	{
		EndFrolic( false );
		return;
	}

	// プレイヤーが動いても追えるよう毎フレーム引き直す
	const FVector Home = GetHomeLocation();
	const FVector BirdLoc = Bird->GetActorLocation();
	const FVector ToHome = Home - BirdLoc;

	const UTidePlayerParamDataAsset* Params = GetParams();
	const float Step = ( Params ? Params->GodFrolicReturnSpeed : 2000.0f ) * DeltaTime;

	// 残り距離の比で元サイズへ補間するので、着く頃にちょうど元サイズ＝入れ替わりの継ぎ目が見えない
	if ( ReturnStartDistance > KINDA_SMALL_NUMBER )
	{
		const float Alpha = FMath::Clamp( 1.0f - ToHome.Size() / ReturnStartDistance, 0.0f, 1.0f );
		ReturnScaleAlpha = FMath::Max( ReturnScaleAlpha, Alpha );
		Bird->SetActorScale3D( FVector( FMath::Lerp( ReturnStartScale, BirdBaseScale, ReturnScaleAlpha ) ) );
	}

	// 旋回上限があるため到達は緩く見る（位置ぴったりを狙うとその場を旋回し続ける）。
	// 常駐お供へそのまま入れ替わるなら消失エフェクトは出さない
	const float ArriveDist = FMath::Max( Step, 100.0f );
	if ( ToHome.SizeSquared() <= ArriveDist * ArriveDist )
	{
		Bird->SetActorLocation( Home, false );

		const bool bCompanionStays = OwnerCharacter->IsGodBirdShownByPlayerAction();
		EndFrolic( !bCompanionStays );
		return;
	}

	MoveAlongFlyDirection( Bird, ToHome.GetSafeNormal(), Step, DeltaTime );
}

void UGodFrolicSubModule::MoveAlongFlyDirection( ATideGodBird* Bird, const FVector& DesiredDir, float Step, float DeltaTime )
{
	if ( !Bird ) return;

	// 1 秒あたり GodFrolicTurnRate 度までしか寄せない（小さいほど大きく弧を描く）
	if ( !DesiredDir.IsNearlyZero() )
	{
		const UTidePlayerParamDataAsset* Params = GetParams();
		const float TurnRate = Params ? Params->GodFrolicTurnRate : 360.0f;
		FlyDirection = FMath::VInterpNormalRotationTo( FlyDirection, DesiredDir, DeltaTime, TurnRate );
	}

	if ( FlyDirection.IsNearlyZero() ) return;

	Bird->SetActorLocation( Bird->GetActorLocation() + FlyDirection * Step, false );
	Bird->SetActorFacingWithModelOffset( FlyDirection.Rotation() );
}

FVector UGodFrolicSubModule::GetHomeLocation() const
{
	if ( !OwnerCharacter ) return FVector::ZeroVector;

	const UTidePlayerParamDataAsset* Params = GetParams();
	return OwnerCharacter->GetActorTransform().TransformPositionNoScale(
		Params ? Params->GodBirdSideOffset : FVector::ZeroVector );
}

AActor* UGodFrolicSubModule::PickNextTarget()
{
	// ロックオン対象があれば round-robin で次の有効な敵へ
	const int32 Num = LockedTargets.Num();
	for ( int32 i = 0; i < Num; ++i )
	{
		LockedCursor = ( LockedCursor + 1 ) % Num;
		if ( AActor* A = LockedTargets[LockedCursor].Get() )
		{
			return A;
		}
	}

	// 全て無効なら鳥の現在地から最寄りの敵を狙う
	const UTidePlayerParamDataAsset* Params = GetParams();
	const float Range = Params ? Params->GodFrolicTargetRange : 2500.0f;
	const FVector From = FrolicBird.IsValid() ? FrolicBird->GetActorLocation()
		: ( OwnerCharacter ? OwnerCharacter->GetActorLocation() : FVector::ZeroVector );
	return FindNearestEnemy( From, Range );
}

AActor* UGodFrolicSubModule::FindNearestEnemy( const FVector& From, float Range ) const
{
	if ( !OwnerCharacter ) return nullptr;

	UWorld* World = OwnerCharacter->GetWorld();
	if ( !World ) return nullptr;

	// 球オーバーラップで候補を集め、敵対＋被ダメージ可能な中から最も近い相手を選ぶ
	FCollisionObjectQueryParams ObjQuery = FCollisionObjectQueryParams::AllDynamicObjects;
	FCollisionQueryParams QueryParams( FName( TEXT( "GodFrolicTarget" ) ), false, OwnerCharacter );
	if ( ATideGodBird* Bird = FrolicBird.Get() )
	{
		QueryParams.AddIgnoredActor( Bird );
	}

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType( Overlaps, From, FQuat::Identity, ObjQuery, FCollisionShape::MakeSphere( FMath::Max( 1.0f, Range ) ), QueryParams );

	AActor* Best = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();
	TSet<AActor*> Seen;
	for ( const FOverlapResult& O : Overlaps )
	{
		AActor* Other = O.GetActor();
		if ( !Other || Seen.Contains( Other ) ) continue;
		Seen.Add( Other );
		if ( !TideCombatUtil::IsHostileTo( OwnerCharacter, Other ) ) continue;
		if ( !Cast<IDamageable>( Other ) ) continue;

		const float DistSq = FVector::DistSquared( From, Other->GetActorLocation() );
		if ( DistSq < BestDistSq )
		{
			BestDistSq = DistSq;
			Best = Other;
		}
	}
	return Best;
}

void UGodFrolicSubModule::ApplyAttackDamage()
{
	ATideGodBird* Bird = FrolicBird.Get();
	if ( !Bird || !OwnerCharacter ) return;

	UWorld* World = OwnerCharacter->GetWorld();
	if ( !World ) return;

	const UTidePlayerParamDataAsset* Params = GetParams();
	if ( !Params ) return;

	const float Radius = FMath::Max( 1.0f, Params->GodFrolicDamageRadius );
	// 行が無ければ DA 値へフォールバックする
	float Damage = Params->GodFrolicDamage;
	if ( const FPlayerAttackParameterRow* AttackRow = FindGodFrolicRow( OwnerCharacter ) )
	{
		Damage = OwnerCharacter->CalculateDamage( AttackRow->DamageMultiplier );
	}
	const float ReArm = FMath::Max( 0.0f, Params->GodFrolicDamageReArmInterval );

	FCollisionObjectQueryParams ObjQuery = FCollisionObjectQueryParams::AllDynamicObjects;
	FCollisionQueryParams QueryParams( FName( TEXT( "GodFrolicDamage" ) ), false, Bird );
	QueryParams.AddIgnoredActor( OwnerCharacter );

	const FCollisionShape Shape = FCollisionShape::MakeSphere( Radius );
	const FVector Center = Bird->GetActorLocation();
	const FVector SweepStart = Center - FVector( 0.0f, 0.0f, 1.0f );
	const FVector SweepEnd   = Center + FVector( 0.0f, 0.0f, 1.0f );

#if !UE_BUILD_SHIPPING
	if ( Params->bGodFrolicDrawDamageRange )
	{
		DrawDebugSphere( World, Center, Radius, 16, FColor::Orange, false, -1.0f, 0, 1.5f );
	}
#endif

	TArray<FHitResult> Hits;
	World->SweepMultiByObjectType( Hits, SweepStart, SweepEnd, FQuat::Identity, ObjQuery, Shape, QueryParams );

	const double Now = World->GetTimeSeconds();

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

void UGodFrolicSubModule::EndFrolic( bool bPlayDisappearVFX )
{
	bActive = false;
	Elapsed = 0.0f;
	LastDamageTimes.Reset();
	CurrentTarget = nullptr;
	BirdPhase = EBirdPhase::Dive;
	NoTargetElapsed = 0.0f;
	PulloutTarget = nullptr;
	PulloutElapsed = 0.0f;

	// 帰還完了時は消失エフェクトを出さない＝元サイズで戻ってそのまま常駐お供へ入れ替わる
	if ( ATideGodBird* Bird = FrolicBird.Get() )
	{
		if ( bPlayDisappearVFX )
		{
			SpawnFrolicBirdVFX( PlayerNiagaraTags::GODBIRD_DISAPPEAR, Bird->GetActorLocation() );
		}
		Bird->Destroy();
	}
	FrolicBird = nullptr;
}

void UGodFrolicSubModule::SpawnFrolicBirdVFX( const FName& NiagaraTag, const FVector& WorldLoc )
{
	if ( !OwnerCharacter || !OwnerCharacter->NiagaraSystemDataAsset ) return;

	UWorld* World = OwnerCharacter->GetWorld();
	if ( !World ) return;

	UNiagaraSystem* Sys = OwnerCharacter->NiagaraSystemDataAsset->GetNiagaraSystem( NiagaraTag );
	if ( !Sys ) return;	// 未割当なら何もしない

	// 鳥は破棄されるためアタッチせずワールド位置へ単発スポーン（bAutoDestroy 既定で再生後に自動破棄）
	if ( UNiagaraComponent* VFX = UNiagaraFunctionLibrary::SpawnSystemAtLocation( World, Sys, WorldLoc, FRotator::ZeroRotator ) )
	{
		// 神鳥の黄緑（常駐お供の出現/消失と同系統）
		VFX->SetColorParameter( TEXT( "Color" ), FLinearColor( 0.4f, 1.0f, 0.1f, 1.0f ) );
	}
}

#if !UE_BUILD_SHIPPING
void UGodFrolicSubModule::DrawDebugImGui()
{
	ImGui::Text( "戯れ: %s", bActive ? "発動中" : "待機" );
	if ( bActive )
	{
		const char* PhaseText = "突入";
		if ( BirdPhase == EBirdPhase::Pullout )		PhaseText = "離脱";
		else if ( BirdPhase == EBirdPhase::Wait )	PhaseText = "待機";
		else if ( BirdPhase == EBirdPhase::Return )	PhaseText = "帰還";

		ImGui::Text( "  経過: %.2f", Elapsed );
		ImGui::Text( "  鳥: %s", FrolicBird.IsValid() ? "有効" : "なし" );
		ImGui::Text( "  対象: %s", CurrentTarget.IsValid() ? "あり" : "なし" );
		ImGui::Text( "  段階: %s（対象なし %.2f 秒）", PhaseText, NoTargetElapsed );
	}
}
#endif
