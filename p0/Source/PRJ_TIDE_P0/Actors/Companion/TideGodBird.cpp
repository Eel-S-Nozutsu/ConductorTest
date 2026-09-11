// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.

#include "TideGodBird.h"

#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"

#include "PRJ_TIDE_P0/Data/Animation/AnimMontageListDataAsset.h"
#include "PRJ_TIDE_P0/Data/Animation/GodBirdAnimTags.h"

ATideGodBird::ATideGodBird()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>( TEXT( "Root" ) );
	RootComponent = Root;

	BirdMesh = CreateDefaultSubobject<USkeletalMeshComponent>( TEXT( "BirdMesh" ) );
	BirdMesh->SetupAttachment( Root );
	BirdMesh->SetCollisionEnabled( ECollisionEnabled::NoCollision );
	BirdMesh->SetCollisionProfileName( TEXT( "NoCollision" ) );
	BirdMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
}

void ATideGodBird::SetVisible( bool bVisible )
{
	// 子へは伝播させない。メッシュへアタッチした出現/消滅エフェクトを、
	// メッシュを消した直後でも描画し続けさせるため
	if ( BirdMesh )
	{
		BirdMesh->SetVisibility( bVisible, /*bPropagateToChildren=*/false );
	}
}

UAnimMontage* ATideGodBird::GetAnimMontage( const FName& MontageName ) const
{
	if ( !BirdAnimMontageDataAsset ) return nullptr;
	return BirdAnimMontageDataAsset->GetAnimMontage( MontageName );
}

float ATideGodBird::PlayAnimMontage( const FName& MontageName, float InPlayRate, FName StartSectionName )
{
	if ( UAnimMontage* Montage = GetAnimMontage( MontageName ) )
	{
		float PlayRate = 1.0f;
		if ( Montage->RateScale > 0.0f )	PlayRate = Montage->RateScale;
		return PlayAnimMontage( Montage, InPlayRate, StartSectionName ) / PlayRate;
	}
	return 0.0f;
}

float ATideGodBird::PlayAnimMontage( UAnimMontage* AnimMontage, float InPlayRate, FName StartSectionName )
{
	// ACharacter ではないため、メッシュの AnimInstance へ直接再生する（待機の上にスロットで差し込む）
	if ( !AnimMontage || !BirdMesh ) return 0.0f;
	UAnimInstance* AnimInstance = BirdMesh->GetAnimInstance();
	if ( !AnimInstance ) return 0.0f;

	const float Duration = AnimInstance->Montage_Play( AnimMontage, InPlayRate );
	if ( Duration > 0.0f && StartSectionName != NAME_None )
	{
		AnimInstance->Montage_JumpToSection( StartSectionName, AnimMontage );
	}
	return Duration;
}

void ATideGodBird::StopAnimMontage( UAnimMontage* AnimMontage )
{
	if ( !BirdMesh ) return;
	if ( UAnimInstance* AnimInstance = BirdMesh->GetAnimInstance() )
	{
		const float BlendOutTime = AnimMontage ? AnimMontage->BlendOut.GetBlendTime() : 0.0f;
		AnimInstance->Montage_Stop( BlendOutTime, AnimMontage );
	}
}

void ATideGodBird::PlayMontageSequence( const FName& StartLabel, const FName& LoopLabel )
{
	// 先に解決しておき、繋ぎ判定（UpdateMontageSequence）で現在モンタージュと比較できるようにする
	SeqStartMontage = GetAnimMontage( StartLabel );
	SeqLoopMontage = GetAnimMontage( LoopLabel );

	if ( SeqStartMontage )
	{
		// 流れ終わったら Tick 側で LP へ繋ぐ。ST 再生中は神技挙動を保留させる
		PlayAnimMontage( SeqStartMontage );
		bStartMotionActive = true;
	}
	else
	{
		// ST 未登録：LP があれば即再生。保留も無し（＝即・神技挙動開始）
		if ( SeqLoopMontage )
		{
			PlayAnimMontage( SeqLoopMontage );
			SeqLoopMontage = nullptr;
		}
		bStartMotionActive = false;
	}
}

void ATideGodBird::UpdateMontageSequence()
{
	if ( !bStartMotionActive || !BirdMesh ) return;

	const UAnimInstance* AnimInstance = BirdMesh->GetAnimInstance();
	if ( !AnimInstance ) return;

	// ST 以外（＝流れ終わって待機へブレンドアウトした）になったら LP へ繋ぐ
	const UAnimMontage* Current = AnimInstance->GetCurrentActiveMontage();
	if ( SeqStartMontage && Current == SeqStartMontage ) return;

	// LP はモンタージュ側のループ設定で流れ続ける。ここで開始モーションフェーズ終了＝神技挙動開始
	if ( SeqLoopMontage )
	{
		PlayAnimMontage( SeqLoopMontage );
		SeqLoopMontage = nullptr;
	}
	bStartMotionActive = false;
}

void ATideGodBird::SetFollowTarget( AActor* InLeader, const FVector& InLocalOffset, const FRotator& InRotationOffset, const FName& InSocketName, bool bInStickToTarget, bool bSnapToTargetNow )
{
	FollowLeader = InLeader;
	FollowLocalOffset = InLocalOffset;
	FollowRotationOffset = InRotationOffset;
	FollowSocketName = InSocketName;
	bStickToTarget = bInStickToTarget;

	// 残っているふわふわのローカルオフセットを消して完全に引っ付ける
	if ( bStickToTarget && BirdMesh )
	{
		BirdMesh->SetRelativeLocation( FVector::ZeroVector );
	}

	// 初回表示でワールド原点から飛んでこないよう、目標位置へ一度スナップする
	if ( bSnapToTargetNow )
	{
		FVector DesiredLoc;
		FRotator DesiredRot;
		if ( GetDesiredFollowTransform( DesiredLoc, DesiredRot ) )
		{
			SetActorLocationAndRotation( DesiredLoc, DesiredRot );
		}
	}
}

void ATideGodBird::SetFacingOverride( bool bEnabled, const FRotator& InBaseWorldFacing )
{
	bUseFacingOverride = bEnabled;
	FacingOverrideBase = InBaseWorldFacing;
}

void ATideGodBird::SetActorFacingWithModelOffset( const FRotator& InBaseWorldFacing )
{
	const FRotator Composed = ( InBaseWorldFacing.Quaternion() * FollowRotationOffset.Quaternion() ).Rotator();
	SetActorRotation( Composed );
}

void ATideGodBird::ActivateGuidanceCharge( const FVector& InTargetLocation, float InSpeed, float InMaxDuration, float InScale, const FRotator& InModelRotationOffset, float InStartScale )
{
	// 導きの鳥は SetFollowTarget を通らないため、ここで明示的にメッシュ補正を与える
	FollowRotationOffset = InModelRotationOffset;

	// 地面クリアランスの自動計算を「最終スケールのバウンズ」で行いたいので先に最終スケールへ。
	// 開始スケールへ縮めるのは後段（PlayMontageSequence 後）
	SetActorScale3D( FVector( FMath::Max( 0.01f, InScale ) ) );

	GuidanceTargetLocation = InTargetLocation;
	GuidanceSpeed = InSpeed;
	GuidanceMaxDuration = InMaxDuration;
	GuidanceElapsed = 0.0f;
	bGuidanceCharging = true;

	// 地面クリアランスを発動時に一度だけ確定する。待機アニメの羽ばたきや回頭でワールドバウンズが伸縮して
	// 跳ね／潜りが出るのを避けるため、回転だけ無効化した基準姿勢のバウンズから求める
	if ( GuidanceGroundClearance > 0.0f )
	{
		CachedGuidanceClearance = GuidanceGroundClearance;
	}
	else if ( BirdMesh )
	{
		// メッシュ最下点をアクター原点からの距離で測る。位置・スケールは実値のまま
		FTransform MeshXform = BirdMesh->GetComponentTransform();
		MeshXform.SetRotation( FQuat::Identity );
		const FBoxSphereBounds UprightBounds = BirdMesh->CalcBounds( MeshXform );
		const float MeshBottomZ = UprightBounds.Origin.Z - UprightBounds.BoxExtent.Z;
		CachedGuidanceClearance = FMath::Max( 0.0f, GetActorLocation().Z - MeshBottomZ );
	}
	else
	{
		CachedGuidanceClearance = 0.0f;
	}

	FVector Dir = InTargetLocation - GetActorLocation();
	if ( !Dir.GetSafeNormal().IsNearlyZero() )
	{
		SetActorFacingWithModelOffset( Dir.GetSafeNormal().Rotation() );
	}

	SetVisible( true );

	PlayMontageSequence( GodBirdAnimTags::GUIDANCE_ST, GodBirdAnimTags::GUIDANCE_LP );
	SetupStartMotionScale( InScale, InStartScale );
}

void ATideGodBird::UpdateStartMotionScale()
{
	// ST の再生位置を進捗に変換して開始→最終スケールへ補間する。取れない場合は最終スケールへ
	float Progress = 1.0f;
	if ( BirdMesh && SeqStartMontage )
	{
		if ( const UAnimInstance* AnimInst = BirdMesh->GetAnimInstance() )
		{
			const float Len = SeqStartMontage->GetPlayLength();
			if ( Len > 0.0f )
			{
				const float Pos = AnimInst->Montage_GetPosition( SeqStartMontage );
				Progress = FMath::Clamp( Pos / Len, 0.0f, 1.0f );
			}
		}
	}
	const float S = FMath::Lerp( StartScaleFrom, StartScaleTo, Progress );
	SetActorScale3D( FVector( FMath::Max( 0.01f, S ) ) );
}

void ATideGodBird::SetupStartMotionScale( float InTargetScale, float InStartScale )
{
	// 開始モーション中かつ開始スケールが最終より小さいときだけ有効化する。
	// 以降 Tick の UpdateStartMotionScale が ST 進捗で最終スケールまで拡大する
	StartScaleTo = FMath::Max( 0.01f, InTargetScale );
	StartScaleFrom = ( InStartScale > 0.0f ) ? FMath::Max( 0.01f, InStartScale ) : StartScaleTo;
	bStartScaleGrow = ( bStartMotionActive && StartScaleFrom < StartScaleTo );
	if ( bStartScaleGrow )
	{
		SetActorScale3D( FVector( StartScaleFrom ) );
	}
}

void ATideGodBird::ActivateFrolicAttack( float InScale, const FRotator& InModelRotationOffset, float InStartScale )
{
	// 外部駆動にして追従・浮遊を止め、位置・向きはサブモジュールが直接動かす
	FollowRotationOffset = InModelRotationOffset;
	SetActorScale3D( FVector( FMath::Max( 0.01f, InScale ) ) );
	SetExternallyDriven( true );
	SetVisible( true );

	PlayMontageSequence( GodBirdAnimTags::FROLIC_ST, GodBirdAnimTags::FROLIC_LP );
	SetupStartMotionScale( InScale, InStartScale );
}

void ATideGodBird::ActivateTornadoEscort( const FVector& InCenter, float InRadius, float InHeight, float InStartHeight, float InSpeed, float InAngularSpeedDeg, float InTurns )
{
	EscortCenter = InCenter;
	EscortRadius = FMath::Max( 1.0f, InRadius );
	EscortHeight = InHeight;
	EscortStartHeight = InStartHeight;
	EscortSpeed = FMath::Max( 1.0f, InSpeed );
	EscortAngularSpeedDeg = FMath::Max( 1.0f, InAngularSpeedDeg );
	EscortTurnsTotalDeg = FMath::Max( 0.0f, InTurns ) * 360.0f;
	EscortTurnsRemainingDeg = EscortTurnsTotalDeg;

	// 現在地の方位から周回開始角度を決め、飛び込みが周回とつながるようにする
	const FVector ToBird = GetActorLocation() - EscortCenter;
	EscortAngleDeg = FMath::RadiansToDegrees( FMath::Atan2( ToBird.Y, ToBird.X ) );

	EscortPhase = EEscortPhase::FlyTo;
	SetVisible( true );
}

float ATideGodBird::CurrentEscortOrbitHeight() const
{
	// 周回の消化率で補間する。周回数 0 なら終了高さ固定
	const float Alpha = ( EscortTurnsTotalDeg > KINDA_SMALL_NUMBER )
		? FMath::Clamp( 1.0f - EscortTurnsRemainingDeg / EscortTurnsTotalDeg, 0.0f, 1.0f )
		: 1.0f;
	return FMath::Lerp( EscortStartHeight, EscortHeight, Alpha );
}

FVector ATideGodBird::EscortOrbitPoint( float AngleDeg ) const
{
	const float Rad = FMath::DegreesToRadians( AngleDeg );
	return EscortCenter + FVector( FMath::Cos( Rad ) * EscortRadius, FMath::Sin( Rad ) * EscortRadius, CurrentEscortOrbitHeight() );
}

void ATideGodBird::FaceTravelDirection( const FVector& Dir, float DeltaTime, float InterpSpeed )
{
	if ( Dir.IsNearlyZero() ) return;
	const FRotator DesiredBase = Dir.GetSafeNormal().Rotation();
	const FRotator TargetRot = ( DesiredBase.Quaternion() * FollowRotationOffset.Quaternion() ).Rotator();
	SetActorRotation( FMath::RInterpTo( GetActorRotation(), TargetRot, DeltaTime, InterpSpeed ) );
}

void ATideGodBird::UpdateTornadoEscort( float DeltaTime )
{
	const FVector CurLoc = GetActorLocation();
	const float Step = EscortSpeed * DeltaTime;

	switch ( EscortPhase )
	{
	case EEscortPhase::FlyTo:
	{
		// 周回開始点へ一定速度で飛ぶ
		const FVector Entry = EscortOrbitPoint( EscortAngleDeg );
		const FVector ToEntry = Entry - CurLoc;
		if ( ToEntry.SizeSquared() <= Step * Step )
		{
			SetActorLocation( Entry );
			EscortPhase = EEscortPhase::Orbit;
		}
		else
		{
			const FVector Dir = ToEntry.GetSafeNormal();
			SetActorLocation( CurLoc + Dir * Step );
			FaceTravelDirection( Dir, DeltaTime, GuidanceFacingInterpSpeed );
		}
		break;
	}
	case EEscortPhase::Orbit:
	{
		// 中心まわりを一定角速度で回りつつ、進行に合わせて高さを上げていく（螺旋）
		const float DeltaDeg = EscortAngularSpeedDeg * DeltaTime;
		EscortAngleDeg += DeltaDeg;
		EscortTurnsRemainingDeg -= DeltaDeg;

		const FVector NewLoc = EscortOrbitPoint( EscortAngleDeg );
		FaceTravelDirection( NewLoc - CurLoc, DeltaTime, GuidanceFacingInterpSpeed );
		SetActorLocation( NewLoc );

		if ( EscortTurnsRemainingDeg <= 0.0f )
		{
			EscortPhase = EEscortPhase::Return;
		}
		break;
	}
	case EEscortPhase::Return:
	{
		// プレイヤーの追従目標へ飛んで戻り、近づいたら通常追従へ復帰する
		FVector DesiredLoc;
		FRotator DesiredRot;
		if ( !GetDesiredFollowTransform( DesiredLoc, DesiredRot ) )
		{
			EscortPhase = EEscortPhase::None;	// 追従対象が無ければ即復帰
			break;
		}
		const FVector ToHome = DesiredLoc - CurLoc;
		if ( ToHome.SizeSquared() <= Step * Step )
		{
			EscortPhase = EEscortPhase::None;	// 復帰。以降は Tick の通常追従（VInterp）が引き継ぐ
		}
		else
		{
			const FVector Dir = ToHome.GetSafeNormal();
			SetActorLocation( CurLoc + Dir * Step );
			FaceTravelDirection( Dir, DeltaTime, GuidanceFacingInterpSpeed );
		}
		break;
	}
	default:
		break;
	}
}

void ATideGodBird::UpdateGuidanceCharge( float DeltaTime )
{
	GuidanceElapsed += DeltaTime;

	const FVector CurLoc = GetActorLocation();
	const FVector ToTarget = GuidanceTargetLocation - CurLoc;
	const float Step = GuidanceSpeed * DeltaTime;

	// すり抜けて突っ込むイメージで sweep なし
	bool bReached = false;
	FVector NextLoc;
	if ( ToTarget.SizeSquared() <= Step * Step )
	{
		NextLoc = GuidanceTargetLocation;
		bReached = true;
	}
	else
	{
		NextLoc = CurLoc + ToTarget.GetSafeNormal() * Step;
	}

	// 地面へめり込む場合は Z を持ち上げて水平スキム移動にする
	const FVector PreClampLoc = NextLoc;
	NextLoc = ClampGuidanceAboveGround( NextLoc );
	const bool bClampedUp = ( NextLoc.Z > PreClampLoc.Z + KINDA_SMALL_NUMBER );
	SetActorLocation( NextLoc, false );

	// スキム中は水平の「目標 XY 方向」を狙って上下のブレを拾わないようにし、通常時は 3D の実移動方向を狙う
	FVector FaceDir = NextLoc - CurLoc;
	if ( bClampedUp )
	{
		FaceDir = GuidanceTargetLocation - NextLoc;
		FaceDir.Z = 0.0f;	// 水平の進行方向だけを見る（地面沿いは下を向かない）
	}
	if ( !FaceDir.IsNearlyZero() )
	{
		const FRotator DesiredBase = FaceDir.GetSafeNormal().Rotation();
		const FRotator TargetRot = ( DesiredBase.Quaternion() * FollowRotationOffset.Quaternion() ).Rotator();
		SetActorRotation( FMath::RInterpTo( GetActorRotation(), TargetRot, DeltaTime, GuidanceFacingInterpSpeed ) );
	}

	// 地面クランプで Z が届かずスキム中の場合のみ、水平で十分寄ったら到達扱いにする
	// （上向きエイムでは誤発火しないようクランプ時に限定）
	const bool bReachedHorizontally = bClampedUp && ( FVector::DistSquared2D( NextLoc, GuidanceTargetLocation ) <= Step * Step );

	if ( bReached || bReachedHorizontally || ( GuidanceMaxDuration > 0.0f && GuidanceElapsed >= GuidanceMaxDuration ) )
	{
		Destroy();
	}
}

FVector ATideGodBird::ClampGuidanceAboveGround( const FVector& DesiredLocation )
{
	UWorld* World = GetWorld();
	if ( !World ) return DesiredLocation;

	// ActivateGuidanceCharge で一度だけ確定済み。毎フレーム再計算しないので跳ね／潜りが出ない
	const float Clearance = CachedGuidanceClearance;

	// 候補位置の少し上から真下へトレースして地面を探す
	const FVector TraceStart = DesiredLocation + FVector( 0.0f, 0.0f, 200.0f );
	const FVector TraceEnd   = DesiredLocation - FVector( 0.0f, 0.0f, 5000.0f );
	FHitResult Hit;
	FCollisionQueryParams QueryParams( FName( TEXT( "GodBirdGuidanceGround" ) ), false, this );
	if ( World->LineTraceSingleByChannel( Hit, TraceStart, TraceEnd, ECC_Visibility, QueryParams ) )
	{
		const float MinZ = Hit.ImpactPoint.Z + Clearance;
		if ( DesiredLocation.Z < MinZ )
		{
			FVector Clamped = DesiredLocation;
			Clamped.Z = MinZ;
			return Clamped;
		}
	}
	return DesiredLocation;
}

void ATideGodBird::Tick( float DeltaTime )
{
	Super::Tick( DeltaTime );

	// 以下 2 つは追従・浮遊とは独立して更新するため、下の early return より前に呼ぶ
	UpdateMontageSequence();

	// ST 中は進捗で拡大、ST 終了（Loop 開始＝挙動開始）で最終スケールへ確定して無効化
	if ( bStartScaleGrow )
	{
		if ( bStartMotionActive )
		{
			UpdateStartMotionScale();
		}
		else
		{
			SetActorScale3D( FVector( FMath::Max( 0.01f, StartScaleTo ) ) );
			bStartScaleGrow = false;
		}
	}

	// エスコート中は位置・向きを自前で駆動する。終わると None になり、下の通常追従が自動で引き継ぐ
	if ( EscortPhase != EEscortPhase::None )
	{
		UpdateTornadoEscort( DeltaTime );
		return;
	}

	// ST モーション中は前進を保留し、スポーン位置で ST を再生する（Loop 開始で突進スタート）
	if ( bGuidanceCharging )
	{
		if ( !bStartMotionActive )
		{
			UpdateGuidanceCharge( DeltaTime );
		}
		return;
	}

	if ( bExternallyDriven ) return;	// 位置・向きは外部が直接制御する

	UpdateFollow( DeltaTime );
	UpdateFloat( DeltaTime );
}

bool ATideGodBird::GetDesiredFollowTransform( FVector& OutLocation, FRotator& OutRotation ) const
{
	const AActor* Leader = FollowLeader.Get();
	if ( !Leader ) return false;

	const FTransform LeaderTransform = Leader->GetActorTransform();

	// ソケット指定があればその位置（滑空中の hand_l など）、無ければアクター原点。
	// いずれもオフセットは対象ローカル軸で加算する
	FVector BaseLoc = LeaderTransform.GetLocation();
	if ( FollowSocketName != NAME_None )
	{
		if ( const ACharacter* LeaderChar = Cast<ACharacter>( Leader ) )
		{
			if ( const USkeletalMeshComponent* LeaderMesh = LeaderChar->GetMesh() )
			{
				if ( LeaderMesh->DoesSocketExist( FollowSocketName ) )
				{
					BaseLoc = LeaderMesh->GetSocketLocation( FollowSocketName );
				}
			}
		}
	}
	OutLocation = BaseLoc + LeaderTransform.TransformVectorNoScale( FollowLocalOffset );

	// 上昇/下降時は垂直速度に比例したピッチを加えて機首を上下へ傾ける。
	// 実際の傾きは UpdateFollow 側の RInterpTo で補間されるため、ここでは目標値だけを与える
	FRotator BaseRot = LeaderTransform.Rotator();
	if ( bFollowVerticalPitch )
	{
		const float Vz = Leader->GetVelocity().Z;
		BaseRot.Pitch += FMath::Clamp( Vz * FollowVerticalPitchPerSpeed, -FollowVerticalPitchMax, FollowVerticalPitchMax );
	}
	OutRotation = ( BaseRot.Quaternion() * FollowRotationOffset.Quaternion() ).Rotator();
	return true;
}

void ATideGodBird::UpdateFollow( float DeltaTime )
{
	FVector DesiredLoc;
	FRotator DesiredRot;
	if ( !GetDesiredFollowTransform( DesiredLoc, DesiredRot ) ) return;

	// 構え中はカメラ方向へ向く（位置は従来の追従のまま）
	if ( bUseFacingOverride )
	{
		DesiredRot = ( FacingOverrideBase.Quaternion() * FollowRotationOffset.Quaternion() ).Rotator();
	}

	// 滑空中の手元固定は補間せず毎フレーム目標へスナップする
	if ( bStickToTarget )
	{
		SetActorLocationAndRotation( DesiredLoc, DesiredRot );
		return;
	}

	// 速度が小さいほど遅れて大きくずれてついてくる
	const FVector NewLoc = ( FollowLocationInterpSpeed > 0.0f )
		? FMath::VInterpTo( GetActorLocation(), DesiredLoc, DeltaTime, FollowLocationInterpSpeed )
		: DesiredLoc;
	const FRotator NewRot = ( FollowRotationInterpSpeed > 0.0f )
		? FMath::RInterpTo( GetActorRotation(), DesiredRot, DeltaTime, FollowRotationInterpSpeed )
		: DesiredRot;

	SetActorLocationAndRotation( NewLoc, NewRot );
}

void ATideGodBird::UpdateFloat( float DeltaTime )
{
	if ( !BirdMesh ) return;

	if ( bStickToTarget ) return;	// 手元固定中は浮遊させない

	FloatElapsed += DeltaTime;

	// 軸ごとに周期をずらした独立の sin。直線往復でなく自然にゆらぐ
	auto AxisOffset = []( float Elapsed, float Amplitude, float Period ) -> float
	{
		if ( Amplitude == 0.0f || Period <= 0.0f ) return 0.0f;
		const float Phase = ( Elapsed / Period ) * 2.0f * PI;
		return FMath::Sin( Phase ) * Amplitude;
	};

	const FVector Offset(
		AxisOffset( FloatElapsed, FloatAmplitudeX, FloatPeriodX ),	// 前後（X）
		AxisOffset( FloatElapsed, FloatAmplitudeY, FloatPeriodY ),	// 左右（Y）
		AxisOffset( FloatElapsed, FloatAmplitude,  FloatPeriod  )	// 上下（Z）
	);

	BirdMesh->SetRelativeLocation( Offset );
}
