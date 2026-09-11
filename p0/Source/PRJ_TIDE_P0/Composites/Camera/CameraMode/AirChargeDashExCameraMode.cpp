// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#include "AirChargeDashExCameraMode.h"

#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

#include "PRJ_TIDE_P0/Actors/Character/Player/TidePlayerCharacter.h"

void UAirChargeDashExCameraMode::InitializeMode( const FExCameraModeCommonParams& Params )
{
	Super::InitializeMode( Params );

	// DA（CommonParams）で設定した FOV を反映する。これを行わないとモード既定 FOV のまま出力され、寄せ演出中に FOV が変わる
	if ( Params.FOV > 0.0f )
	{
		FOV = Params.FOV;
	}
}

void UAirChargeDashExCameraMode::OnActivated( const FMinimalViewInfo& LastViewInfo )
{
	// 直前のカメラ状態を始点にして、目標構図へ滑らかに（グイっと）寄せていく
	CurrentLocation = LastViewInfo.Location;
	CurrentRotation = LastViewInfo.Rotation;
	CurrentFOV = LastViewInfo.FOV > 0.0f ? LastViewInfo.FOV : FOV;
	bInitialized = true;
}

void UAirChargeDashExCameraMode::UpdateCamera( const FCameraControlData& ControlData, FMinimalViewInfo& OutViewInfo )
{
	ATidePlayerCharacter* TidePlayer = Cast<ATidePlayerCharacter>( UGameplayStatics::GetPlayerCharacter( GetWorld(), 0 ) );

	// 既定は現在値維持（情報が取れない場合のフォールバック）
	FVector DesiredLoc = CurrentLocation;
	float DesiredFOV = FOV;

	bool bHasFocus = false;
	FVector Focus = FVector::ZeroVector;

	FRotator TargetRotation = FRotator::ZeroRotator;
	if ( TidePlayer && TidePlayer->GetAirChargeDashCameraFraming( TargetRotation ) )
	{
		const FVector Up = FVector::UpVector;

		// 注視点（プレイヤー位置＋高さ）。HeightOffset を負にするとプレイヤーが画面上寄りになり、
		// 下方向（落下先）に余白が生まれる
		Focus = TidePlayer->GetActorLocation() + Up * HeightOffset;
		bHasFocus = true;

		// カメラ位置の軌道方向：寄せ演出中（入力ロック中）はダイブ角固定、完了後は ControlRotation を使ってスティックで
		// 軌道を回せるようにする（解除時にモジュールが ControlRotation をダイブ角へ同期するので切り替わりは連続）
		const FRotator OrbitRotation = TidePlayer->IsAirChargeDashCameraSwinging()
			? TargetRotation
			: ControlData.ControlRotation;

		// 軌道方向の後ろ側へ引き距離ぶん離してカメラを置く。位置だけ補間して初回の「グイっと」を出す
		const FVector Forward = OrbitRotation.Vector();
		DesiredLoc = Focus - Forward * ArmLength;
	}

	// 位置・FOV を補間（初回の寄せをなめらかに）。※回転は下で注視点ベースに作り直す
	const float Dt = ControlData.DeltaTime;
	if ( bInitialized && InterpSpeed > 0.0f && Dt > 0.0f )
	{
		CurrentLocation = FMath::VInterpTo( CurrentLocation, DesiredLoc, Dt, InterpSpeed );
		CurrentFOV = FMath::FInterpTo( CurrentFOV, DesiredFOV, Dt, InterpSpeed );
	}
	else
	{
		CurrentLocation = DesiredLoc;
		CurrentFOV = DesiredFOV;
		bInitialized = true;
	}

	// 回転は常に注視点（プレイヤー）を向く。位置追従が加速落下に遅れても注視点が画面中心に居続けるので見切れない
	// （固定角だと追従遅れぶんフレーム下へ流れる）。上下の画面内位置は HeightOffset で調整する
	if ( bHasFocus )
	{
		CurrentRotation = ( Focus - CurrentLocation ).Rotation();
	}

	OutViewInfo.Location = CurrentLocation;
	OutViewInfo.Rotation = CurrentRotation;
	OutViewInfo.FOV = CurrentFOV;
}
