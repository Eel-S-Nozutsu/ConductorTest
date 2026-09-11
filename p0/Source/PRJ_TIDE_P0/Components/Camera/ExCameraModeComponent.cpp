// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#include "ExCameraModeComponent.h"

#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Misc/App.h"

#include "PRJ_TIDE_P0/Components/Input/InputRouterComponent.h"
#include "PRJ_TIDE_P0/Composites/Camera/CameraMode/ExCameraMode.h"
#include "PRJ_TIDE_P0/Actors/Camera/ExCameraActor.h"
#include "PRJ_TIDE_P0/Subsystems/Camera/ExCameraSubsystem.h"
#include "PRJ_TIDE_P0/Core/TidePlayerController.h"

UExCameraModeComponent::UExCameraModeComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UExCameraModeComponent::BeginPlay()
{
	Super::BeginPlay();

	// 本体は CameraActor に付いているが、外部からの操作窓口は Subsystem に集約したいので登録する
	if ( const AExCameraActor* CameraActor = Cast<AExCameraActor>( GetOwner() ) )
	{
		if ( const APlayerController* OwningController = Cast<APlayerController>( CameraActor->GetOwner() ) )
		{
			if ( ULocalPlayer* LocalPlayer = OwningController->GetLocalPlayer() )
			{
				if ( UExCameraSubsystem* CameraSubsystem = LocalPlayer->GetSubsystem<UExCameraSubsystem>() )
				{
					CameraSubsystem->RegisterCameraModeComponent( this );
				}
			}
		}
	}
}

void UExCameraModeComponent::EndPlay( const EEndPlayReason::Type EndPlayReason )
{
	InputProvider.Reset();	// 供給元の PlayerController とは寿命が一致しない

	// Subsystem は LocalPlayer と同寿命で残るため、破棄時に必ず登録解除する
	if ( const AExCameraActor* CameraActor = Cast<AExCameraActor>( GetOwner() ) )
	{
		if ( const APlayerController* OwningController = Cast<APlayerController>( CameraActor->GetOwner() ) )
		{
			if ( ULocalPlayer* LocalPlayer = OwningController->GetLocalPlayer() )
			{
				if ( UExCameraSubsystem* CameraSubsystem = LocalPlayer->GetSubsystem<UExCameraSubsystem>() )
				{
					CameraSubsystem->UnregisterCameraModeComponent( this );
				}
			}
		}
	}

	Super::EndPlay( EndPlayReason );
}

void UExCameraModeComponent::SetInputProvider( ATidePlayerController* InProvider )
{
	InputProvider = InProvider;
}

int32 UExCameraModeComponent::GetActiveModeIndex() const
{
	// 最高 Priority を採用。同値は後積み優先にしたいので、昇順走査＋「>=」で最後尾寄りを勝たせる
	int32 BestIndex = INDEX_NONE;
	int32 BestPriority = 0;
	for ( int32 i = 0; i < ModeStack.Num(); ++i )
	{
		if ( !ModeStack[i] ) continue;

		const int32 ModePriority = ModeStack[i]->GetPriority();
		if ( BestIndex == INDEX_NONE || ModePriority >= BestPriority )
		{
			BestPriority = ModePriority;
			BestIndex = i;
		}
	}
	return BestIndex;
}

UExCameraMode* UExCameraModeComponent::GetActiveMode() const
{
	const int32 ActiveIndex = GetActiveModeIndex();
	return ModeStack.IsValidIndex( ActiveIndex ) ? ModeStack[ActiveIndex] : nullptr;
}

void UExCameraModeComponent::BeginActiveTransition( UExCameraMode* OldActive, UExCameraMode* NewActive, const FExCameraBlendTableRow& BlendSettings )
{
	// 低優先モードを積む／眠っているモードを抜く場合はここで終わる
	if ( NewActive == OldActive )
	{
		return;
	}

	// スタックが空になった等。表示対象が無いのでブレンドしない
	if ( !NewActive )
	{
		bIsBlending = false;
		BlendingOutMode = nullptr;
		return;
	}

	BlendingOutMode = OldActive;	// 初回アクティブ化では nullptr
	NewActive->OnActivated( LastFrameViewInfo );

	CurrentBlendSettings = BlendSettings;
	constexpr float ZeroBlendTime = 0.0f;

	if ( CurrentBlendSettings.BlendTime > ZeroBlendTime )
	{
		bIsBlending = true;
		BlendTimeElapsed = 0.0f;

		BlendStartViewInfo = LastFrameViewInfo;	// ブレンドの始点
	}
	else
	{
		bIsBlending = false;
		BlendingOutMode = nullptr;	// ブレンド時間 0 なら即破棄
	}
}

FCameraModeHandle UExCameraModeComponent::PushCameraMode( UExCameraMode* ModeInstance, const FExCameraBlendTableRow& BlendSettings )
{
	if ( !ModeInstance )
	{
		UE_LOG( LogTemp, Warning, TEXT( "[ExCameraModeComponent] カメラモードが不正な値です。" ) );
		return FCameraModeHandle();
	}

	// Priority 次第では積んでもアクティブが変わらない
	UExCameraMode* OldActive = GetActiveMode();

	FCameraModeHandle ResultHandle;
	ModeInstance->InstanceID = NextHandleID++;
	ResultHandle.ID = ModeInstance->InstanceID;
	ModeStack.Push( ModeInstance );

	BeginActiveTransition( OldActive, GetActiveMode(), BlendSettings );

	return ResultHandle;
}

bool UExCameraModeComponent::PopCameraMode( FCameraModeHandle Handle, const FExCameraBlendTableRow& BlendSettings )
{
	if ( !Handle.IsValid() ) return false;

	int32 FoundIndex = INDEX_NONE;
	for ( int32 i = ModeStack.Num() - 1; i >= 0; --i )
	{
		if ( ModeStack[i] && ModeStack[i]->InstanceID == Handle.ID )
		{
			FoundIndex = i;
			break;
		}
	}

	if ( FoundIndex == INDEX_NONE ) return false;

	// 抜くのがアクティブ本体でなければブレンドは起こらない
	UExCameraMode* OldActive = GetActiveMode();
	ModeStack.RemoveAt( FoundIndex );

	BeginActiveTransition( OldActive, GetActiveMode(), BlendSettings );

	return true;
}

void UExCameraModeComponent::TickComponent( float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction )
{
	Super::TickComponent( DeltaTime, TickType, ThisTickFunction );

	if ( ModeStack.IsEmpty() ) return;

	TObjectPtr<UExCameraMode> ActiveMode = GetActiveMode();
	if ( !ActiveMode ) return;

	// カメラはワールドのスローの影響を受けず常に実時間で動かす。FApp::GetDeltaTime() は
	// TimeDilation 適用前の生フレーム時間なので、TickComponent の DeltaTime は使わない
	const float RealDeltaTime = FApp::GetDeltaTime();
	DeltaTime = RealDeltaTime;

	FCameraControlData InputData;
	if ( InputProvider.IsValid() )
	{
		InputData = InputProvider->GetCameraControlData();
	}
	// プロバイダ側の DeltaTime もワールド依存なので実時間で上書きする
	InputData.DeltaTime = RealDeltaTime;

	// アクティブモードの理想値（ブレンド先）
	FMinimalViewInfo DesiredView;
	ActiveMode->UpdateCamera( InputData, DesiredView );

	// 退場する古いモード（ブレンド元）。bUseFixedBlendStart か退避モード無しなら、
	// BlendStartViewInfo（固定座標）がそのまま使われる
	FMinimalViewInfo OutgoingView = BlendStartViewInfo;
	if ( bIsBlending )
	{
		if ( !CurrentBlendSettings.bUseFixedBlendStart && BlendingOutMode )
		{
			// 古いモードの現在の理想状態を求め、開始時点からの差分を始点へ足し込む
			FMinimalViewInfo CurrentOutgoingIdeal;
			BlendingOutMode->UpdateCamera( InputData, CurrentOutgoingIdeal );

			if ( BlendTimeElapsed == 0.0f )
			{
				BlendStartOutgoingIdealView = CurrentOutgoingIdeal;
			}

			OutgoingView.Location = BlendStartViewInfo.Location + ( CurrentOutgoingIdeal.Location - BlendStartOutgoingIdealView.Location );

			FQuat StartIdealInv = BlendStartOutgoingIdealView.Rotation.Quaternion().Inverse();
			FQuat CurrentIdeal = CurrentOutgoingIdeal.Rotation.Quaternion();
			FQuat DeltaRot = CurrentIdeal * StartIdealInv;
			OutgoingView.Rotation = ( DeltaRot * BlendStartViewInfo.Rotation.Quaternion() ).Rotator();

			OutgoingView.FOV = BlendStartViewInfo.FOV + ( CurrentOutgoingIdeal.FOV - BlendStartOutgoingIdealView.FOV );
		}
	}

	FMinimalViewInfo FinalView = DesiredView;

	constexpr float ZeroBlendTime = 0.0f;
	if ( bIsBlending && CurrentBlendSettings.BlendTime > ZeroBlendTime )
	{
		BlendTimeElapsed += DeltaTime;

		constexpr float MinAlpha = 0.0f;
		constexpr float MaxAlpha = 1.0f;

		const float Alpha = FMath::Clamp( BlendTimeElapsed / CurrentBlendSettings.BlendTime, MinAlpha, MaxAlpha );
		float SmoothedAlpha = Alpha;

		switch ( CurrentBlendSettings.BlendFunction )
		{
		case EExCameraBlendFunction::Linear:
			SmoothedAlpha = Alpha;
			break;
		case EExCameraBlendFunction::SmoothStep:
			SmoothedAlpha = FMath::SmoothStep( MinAlpha, MaxAlpha, Alpha );
			break;
		case EExCameraBlendFunction::EaseIn:
			SmoothedAlpha = FMath::InterpEaseIn( MinAlpha, MaxAlpha, Alpha, CurrentBlendSettings.BlendExp );
			break;
		case EExCameraBlendFunction::EaseOut:
			SmoothedAlpha = FMath::InterpEaseOut( MinAlpha, MaxAlpha, Alpha, CurrentBlendSettings.BlendExp );
			break;
		case EExCameraBlendFunction::EaseInOut:
			SmoothedAlpha = FMath::InterpEaseInOut( MinAlpha, MaxAlpha, Alpha, CurrentBlendSettings.BlendExp );
			break;
		case EExCameraBlendFunction::CustomCurve:
			if ( CurrentBlendSettings.BlendCurve )
			{
				SmoothedAlpha = CurrentBlendSettings.BlendCurve->GetFloatValue( Alpha );
			}
			break;
		}

		FinalView.Location = FMath::Lerp( OutgoingView.Location, DesiredView.Location, SmoothedAlpha );
		FinalView.Rotation = FQuat::Slerp( OutgoingView.Rotation.Quaternion(), DesiredView.Rotation.Quaternion(), SmoothedAlpha ).Rotator();
		FinalView.FOV = FMath::Lerp( OutgoingView.FOV, DesiredView.FOV, SmoothedAlpha );

		if ( Alpha >= MaxAlpha )
		{
			bIsBlending = false;
			BlendingOutMode = nullptr;
		}
	}

	LastFrameViewInfo = FinalView;	// 次フレームのブレンド始点になる

	if ( AExCameraActor* CameraActor = Cast<AExCameraActor>( GetOwner() ) )
	{
		CameraActor->ApplyCameraView( FinalView );
	}
}

FVector2D UExCameraModeComponent::GetActiveRotationSpeedRate() const
{
	if ( UExCameraMode* ActiveMode = GetActiveMode() )
	{
		return ActiveMode->GetRotationSpeedRate();
	}
	return FVector2D( 1.0f, 1.0f );
}
