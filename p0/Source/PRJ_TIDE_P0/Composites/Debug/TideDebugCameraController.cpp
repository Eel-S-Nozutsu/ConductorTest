// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#include "TideDebugCameraController.h"

#include "Components/InputComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "Engine/Player.h"

ATideDebugCameraController::ATideDebugCameraController()
{
}

void ATideDebugCameraController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if ( !InputComponent ) return;

	// ADebugCameraController は Tick を持たないため入力はエンジン同様 BindKey で受ける。イベント駆動なので、
	// 起動時に押しっぱなしでも初回フレームで誤発火しない（Pressed は起動後の押し直しでのみ来る）
	InputComponent->BindKey( EKeys::Gamepad_RightShoulder, IE_Pressed,  this, &ATideDebugCameraController::OnR1Pressed );
	InputComponent->BindKey( EKeys::Gamepad_RightShoulder, IE_Released, this, &ATideDebugCameraController::OnR1Released );
	InputComponent->BindKey( EKeys::Gamepad_LeftShoulder,  IE_Pressed,  this, &ATideDebugCameraController::OnL1Pressed );
	InputComponent->BindKey( EKeys::Gamepad_LeftShoulder,  IE_Released, this, &ATideDebugCameraController::OnL1Released );
	InputComponent->BindKey( EKeys::Gamepad_Special_Left,  IE_Pressed,  this, &ATideDebugCameraController::OnSelectPressed );
	InputComponent->BindKey( EKeys::Gamepad_FaceButton_Left, IE_Pressed, this, &ATideDebugCameraController::OnTeleportPressed );

	// キーボードの T も同じテレポートへ繋ぐ。エンジン既定の T（DebugExecBindings → CheatManager::Teleport）は
	// デバッグカメラ側のコントローラが Pawn を持たないため機能しないので、独自実装へ直接バインドする
	InputComponent->BindKey( EKeys::T, IE_Pressed, this, &ATideDebugCameraController::OnTeleportPressed );
}

void ATideDebugCameraController::OnR1Pressed()
{
	bR1Held = true;
	ApplyHoldSpeedScale();

	// R1 を押した時点で Select も押されていれば終了
	if ( IsInputKeyDown( EKeys::Gamepad_Special_Left ) )
	{
		ExitDebugCameraNow();
	}
}

void ATideDebugCameraController::OnR1Released()
{
	bR1Held = false;
	ApplyHoldSpeedScale();
}

void ATideDebugCameraController::OnL1Pressed()
{
	bL1Held = true;
	ApplyHoldSpeedScale();
}

void ATideDebugCameraController::OnL1Released()
{
	bL1Held = false;
	ApplyHoldSpeedScale();
}

void ATideDebugCameraController::OnSelectPressed()
{
	// Select を押した時点で R1 も押されていれば終了（押す順はどちらでも可）
	if ( IsInputKeyDown( EKeys::Gamepad_RightShoulder ) )
	{
		ExitDebugCameraNow();
	}
}

void ATideDebugCameraController::OnTeleportPressed()
{
	TeleportOriginalPawn();
}

void ATideDebugCameraController::ApplyHoldSpeedScale()
{
	float DesiredScale = NormalSpeedScale;
	if ( bR1Held && !bL1Held )
	{
		DesiredScale = FastSpeedScale;
	}
	else if ( bL1Held && !bR1Held )
	{
		DesiredScale = SlowSpeedScale;
	}

	SetPawnMovementSpeedScale( DesiredScale );
}

void ATideDebugCameraController::ExitDebugCameraNow()
{
	// UCheatManager::DisableDebugCamera() はデバッグカメラ自身の CheatManager からしか動作せず、ゲームプレイ側から
	// 呼んでも無効なので、ここで同等の復帰処理（元プレイヤーへコントローラを戻す）を直接行う
	if ( OriginalPlayer && OriginalControllerRef )
	{
		OriginalPlayer->SwitchController( OriginalControllerRef );
		OnDeactivate( OriginalControllerRef );
	}
}

void ATideDebugCameraController::TeleportOriginalPawn()
{
	if ( !OriginalControllerRef ) return;

	APawn* TargetPawn = OriginalControllerRef->GetPawn();
	if ( !TargetPawn ) return;

	UWorld* World = GetWorld();
	if ( !World ) return;

	// デバッグカメラ（このコントローラ）の視点を基準に前方トレースし、当たった面へ着地させる
	FVector ViewLocation;
	FRotator ViewRotation;
	GetPlayerViewPoint( ViewLocation, ViewRotation );

	const FVector TraceStart = ViewLocation;
	const FVector TraceEnd   = ViewLocation + ViewRotation.Vector() * TeleportTraceDistance;

	FVector TargetLocation = TraceEnd;

	FHitResult Hit;
	FCollisionQueryParams Params( SCENE_QUERY_STAT( DebugCamTeleport ), false, TargetPawn );
	Params.AddIgnoredActor( this );
	if ( World->LineTraceSingleByChannel( Hit, TraceStart, TraceEnd, ECC_Visibility, Params ) )
	{
		// 床にめり込まないようカプセル半高ぶん持ち上げる
		TargetLocation = Hit.Location + FVector( 0.0f, 0.0f, TargetPawn->GetSimpleCollisionHalfHeight() );
	}

	TargetPawn->TeleportTo( TargetLocation, TargetPawn->GetActorRotation() );
}
