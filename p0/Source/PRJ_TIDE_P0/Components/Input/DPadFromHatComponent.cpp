// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.

#include "DPadFromHatComponent.h"

#include "InputKeyEventArgs.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "GameFramework/Pawn.h"
#include "Engine/Engine.h"

UDPadFromHatComponent::UDPadFromHatComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// 入力処理より前に軸を読んで注入したいので PrePhysics で回す
	PrimaryComponentTick.TickGroup = TG_PrePhysics;

	// 暫定デフォルト。実機の ShowDebug Input で正しい軸を確認して差し替えること。
	HatAxisKey = FKey( TEXT( "GenericUSBController_Axis7" ) );
}

void UDPadFromHatComponent::BeginPlay()
{
	Super::BeginPlay();

	// ローカルコントローラでなければTickも不要
	if ( const APlayerController* PC = ResolvePlayerController() )
	{
		if ( !PC->IsLocalController() )
		{
			PrimaryComponentTick.SetTickFunctionEnable( false );
		}
	}
}

void UDPadFromHatComponent::EndPlay( const EEndPlayReason::Type EndPlayReason )
{
	ReleaseAll();
	Super::EndPlay( EndPlayReason );
}

APlayerController* UDPadFromHatComponent::ResolvePlayerController()
{
	if ( CachedPC.IsValid() )
	{
		return CachedPC.Get();
	}

	APlayerController* PC = Cast<APlayerController>( GetOwner() );
	if ( !PC )
	{
		if ( const APawn* Pawn = Cast<APawn>( GetOwner() ) )
		{
			PC = Cast<APlayerController>( Pawn->GetController() );
		}
	}

	CachedPC = PC;
	return PC;
}

void UDPadFromHatComponent::TickComponent( float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction )
{
	Super::TickComponent( DeltaTime, TickType, ThisTickFunction );

	APlayerController* PC = ResolvePlayerController();
	if ( !PC || !PC->PlayerInput )
	{
		return;
	}

	const float RawValue = PC->PlayerInput->GetKeyValue( HatAxisKey );
	const int32 PovIndex = DecodePovIndex( RawValue );

	// POVインデックス → 4方向（斜めは2方向同時押し）
	// 0=上 1=右上 2=右 3=右下 4=下 5=左下 6=左 7=左上
	const bool bUp    = ( PovIndex == 7 || PovIndex == 0 || PovIndex == 1 );
	const bool bRight = ( PovIndex == 1 || PovIndex == 2 || PovIndex == 3 );
	const bool bDown  = ( PovIndex == 3 || PovIndex == 4 || PovIndex == 5 );
	const bool bLeft  = ( PovIndex == 5 || PovIndex == 6 || PovIndex == 7 );

	ApplyDirections( bUp, bRight, bDown, bLeft );

	const EDPadDirection NewDir = PovIndexToDirection( PovIndex );
	if ( NewDir != CurrentDirection )
	{
		CurrentDirection = NewDir;
		OnDirectionChanged.Broadcast( CurrentDirection );
	}

	if ( bDrawDebug && GEngine )
	{
		GEngine->AddOnScreenDebugMessage(
			static_cast<uint64>( GetUniqueID() ), 0.0f, FColor::Cyan,
			FString::Printf( TEXT( "[DPadHat] %s raw=%.4f pov=%d dir=%d" ),
				*HatAxisKey.ToString(), RawValue, PovIndex, static_cast<int32>( CurrentDirection ) ) );

		// 軸ファインダー：どの生Axisにスティック/D-Padが来るか特定する用。
		// 動かした操作に対応する AxisN の値が変化する。
		FString AxisDump = TEXT( "[Axes] " );
		for ( int32 i = 1; i <= 12; ++i )
		{
			const FKey AxisKey( *FString::Printf( TEXT( "GenericUSBController_Axis%d" ), i ) );
			const float V = PC->PlayerInput->GetKeyValue( AxisKey );
			if ( FMath::Abs( V ) > KINDA_SMALL_NUMBER )
			{
				AxisDump += FString::Printf( TEXT( "A%d=%.3f  " ), i, V );
			}
		}
		GEngine->AddOnScreenDebugMessage(
			static_cast<uint64>( GetUniqueID() ) + 1, 0.0f, FColor::Yellow, AxisDump );
	}
}

int32 UDPadFromHatComponent::DecodePovIndex( float RawValue )
{
	// ニュートラルが範囲外(負値等)で来るデバイス向けの早期判定
	if ( bTreatOutOfRangeAsNeutral && RawValue < 0.0f )
	{
		return INDEX_NONE;
	}

	float IndexF = RawValue;
	switch ( ValueMode )
	{
		case EDPadHatValueMode::Raw_0_8:
			IndexF = RawValue;
			break;

		case EDPadHatValueMode::Normalized:
			IndexF = RawValue * NormalizedScale;
			break;

		case EDPadHatValueMode::Auto:
		default:
			// 大きな値を一度でも観測したらRaw確定（ラッチ）。それまではNormalized扱い。
			if ( FMath::Abs( RawValue ) > AutoRawThreshold )
			{
				bAutoDetectedRaw = true;
			}
			IndexF = bAutoDetectedRaw ? RawValue : RawValue * NormalizedScale;
			break;
	}

	const int32 Index = FMath::RoundToInt( IndexF );

	// 0-7 が有効な方向。8以上（=無入力）や負値はニュートラル。
	if ( Index < 0 || Index >= 8 )
	{
		return INDEX_NONE;
	}
	return Index;
}

void UDPadFromHatComponent::ApplyDirections( bool bUp, bool bRight, bool bDown, bool bLeft )
{
	if ( !bInjectSyntheticDPadKeys )
	{
		// 注入しない場合も状態は更新しておく
		bPrevUp = bUp; bPrevRight = bRight; bPrevDown = bDown; bPrevLeft = bLeft;
		return;
	}

	if ( bUp != bPrevUp )       { InjectKey( EKeys::Gamepad_DPad_Up,    bUp ); }
	if ( bRight != bPrevRight ) { InjectKey( EKeys::Gamepad_DPad_Right, bRight ); }
	if ( bDown != bPrevDown )   { InjectKey( EKeys::Gamepad_DPad_Down,  bDown ); }
	if ( bLeft != bPrevLeft )   { InjectKey( EKeys::Gamepad_DPad_Left,  bLeft ); }

	bPrevUp = bUp;
	bPrevRight = bRight;
	bPrevDown = bDown;
	bPrevLeft = bLeft;
}

void UDPadFromHatComponent::InjectKey( const FKey& Key, bool bPressed )
{
	APlayerController* PC = ResolvePlayerController();
	if ( !PC )
	{
		return;
	}

	const FInputKeyEventArgs Args = FInputKeyEventArgs::CreateSimulated(
		Key,
		bPressed ? IE_Pressed : IE_Released,
		bPressed ? 1.0f : 0.0f );

	PC->InputKey( Args );
}

void UDPadFromHatComponent::ReleaseAll()
{
	if ( bInjectSyntheticDPadKeys )
	{
		if ( bPrevUp )    { InjectKey( EKeys::Gamepad_DPad_Up,    false ); }
		if ( bPrevRight ) { InjectKey( EKeys::Gamepad_DPad_Right, false ); }
		if ( bPrevDown )  { InjectKey( EKeys::Gamepad_DPad_Down,  false ); }
		if ( bPrevLeft )  { InjectKey( EKeys::Gamepad_DPad_Left,  false ); }
	}

	bPrevUp = bPrevRight = bPrevDown = bPrevLeft = false;

	if ( CurrentDirection != EDPadDirection::None )
	{
		CurrentDirection = EDPadDirection::None;
		OnDirectionChanged.Broadcast( CurrentDirection );
	}
}

EDPadDirection UDPadFromHatComponent::PovIndexToDirection( int32 PovIndex )
{
	switch ( PovIndex )
	{
		case 0: return EDPadDirection::Up;
		case 1: return EDPadDirection::UpRight;
		case 2: return EDPadDirection::Right;
		case 3: return EDPadDirection::DownRight;
		case 4: return EDPadDirection::Down;
		case 5: return EDPadDirection::DownLeft;
		case 6: return EDPadDirection::Left;
		case 7: return EDPadDirection::UpLeft;
		default: return EDPadDirection::None;
	}
}
