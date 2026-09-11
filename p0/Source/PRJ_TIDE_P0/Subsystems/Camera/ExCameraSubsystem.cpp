// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#include "ExCameraSubsystem.h"

#include "Engine/DataTable.h"

#include "PRJ_TIDE_P0/Components/Camera/ExCameraModeComponent.h"
#include "PRJ_TIDE_P0/Data/Camera/CameraModeParam/ExCameraModeParam.h"
#include "PRJ_TIDE_P0/Data/Camera/CameraModeParam/ThirdPersonExCameraModeParamRow.h"
#include "PRJ_TIDE_P0/Composites/Camera/CameraMode/ThirdPersonExCameraMode.h"

namespace ExCameraBlendRowNames
{
	static const FName DefaultBlend = TEXT( "Default" );
}

void UExCameraSubsystem::RegisterCameraModeComponent( UExCameraModeComponent* InComponent )
{
	if ( !InComponent )
	{
		return;
	}

	// 1 LocalPlayer に 1 Component を前提とするため、
	// 既存登録が別インスタンスなら警告を出したうえで差し替える
	if ( RegisteredCameraModeComponent.IsValid() && RegisteredCameraModeComponent.Get() != InComponent )
	{
		UE_LOG( LogTemp, Warning, TEXT( "[ExCameraSubsystem] カメラモードコンポーネントが既に登録されています。前のインスタンスを置き換えます。" ) );
	}

	RegisteredCameraModeComponent = InComponent;
}

void UExCameraSubsystem::UnregisterCameraModeComponent( UExCameraModeComponent* InComponent )
{
	// 自分自身が登録されている場合だけ解除します。
	// 別インスタンスの登録を誤って消さないためのガード
	if ( RegisteredCameraModeComponent.Get() == InComponent )
	{
		RegisteredCameraModeComponent.Reset();
	}
}

FCameraModeHandle UExCameraSubsystem::PushCameraMode( UExCameraModeParam* Param, FName BlendRowName )
{
	FCameraModeHandle ResultHandle;
	if ( !Param || !Param->ModeClass )
	{
		UE_LOG( LogTemp, Warning, TEXT( "[ExCameraSubsystem] 追加するカメラモードのパラメータまたは ModeClass が無効です。" ) );
		return ResultHandle;
	}
	UExCameraModeComponent* ModeComponent = RegisteredCameraModeComponent.Get();
	if ( !IsValid( ModeComponent ) )
	{
		UE_LOG( LogTemp, Warning, TEXT( "[ExCameraSubsystem] 有効なカメラモードコンポーネントが登録されていません。" ) );
		return ResultHandle;
	}
	UExCameraMode* NewMode = NewObject<UExCameraMode>( ModeComponent, Param->ModeClass );
	// 共通パラメータ（Priority 等）をモードへ反映する
	NewMode->InitializeMode( Param->CommonParams );

	const FExCameraBlendTableRow BlendSettings = GetBlendSettings( BlendRowName );
	return ModeComponent->PushCameraMode( NewMode, BlendSettings );
}

FCameraModeHandle UExCameraSubsystem::PushThirdPersonCameraByKey( FName RowName, FName BlendRowName )
{
	FCameraModeHandle ResultHandle;
	if ( RowName.IsNone() )
	{
		UE_LOG( LogTemp, Warning, TEXT( "[ExCameraSubsystem] 無効な行名が指定されました。" ) );
		return ResultHandle;
	}
	if ( !ThirdPersonCameraTable )
	{
		UE_LOG( LogTemp, Error, TEXT( "[ExCameraSubsystem] ThirdPersonCameraTable が設定されていません。" ) );
		return ResultHandle;
	}
	if ( !ThirdPersonModeClass )
	{
		UE_LOG( LogTemp, Error, TEXT( "[ExCameraSubsystem] ThirdPersonModeClass が設定されていません。" ) );
		return ResultHandle;
	}
	UExCameraModeComponent* ModeComponent = RegisteredCameraModeComponent.Get();
	if ( !IsValid( ModeComponent ) )
	{
		UE_LOG( LogTemp, Warning, TEXT( "[ExCameraSubsystem] 有効なカメラモードコンポーネントが登録されていません。" ) );
		return ResultHandle;
	}
	static const FString ContextString( TEXT( "PushThirdPersonCameraByKey" ) );
	const FThirdPersonExCameraModeParamRow* RowData = ThirdPersonCameraTable->FindRow<FThirdPersonExCameraModeParamRow>( RowName, ContextString );
	if ( !RowData )
	{
		UE_LOG( LogTemp, Warning, TEXT( "[ExCameraSubsystem] 三人称カメラの行が見つかりません: %s" ), *RowName.ToString() );
		return ResultHandle;
	}

	UThirdPersonExCameraMode* NewMode = Cast<UThirdPersonExCameraMode>( NewObject<UExCameraMode>( ModeComponent, ThirdPersonModeClass ) );
	if ( NewMode )
	{
		NewMode->InitializeFromRow( *RowData );
	}
	else
	{
		UE_LOG( LogTemp, Error, TEXT( "[ExCameraSubsystem] UThirdPersonExCameraMode へのキャストに失敗しました。" ) );
	}

	const FExCameraBlendTableRow BlendSettings = GetBlendSettings( BlendRowName );
	return ModeComponent->PushCameraMode( NewMode, BlendSettings );
}

bool UExCameraSubsystem::PopCameraMode( FCameraModeHandle Handle, FName BlendRowName, bool bForceFixedBlendStart )
{
	UExCameraModeComponent* ModeComponent = RegisteredCameraModeComponent.Get();
	if ( !IsValid( ModeComponent ) )
	{
		return false;
	}

	FExCameraBlendTableRow BlendSettings = GetBlendSettings( BlendRowName );

	// 退場モードを毎フレーム再計算（差分追従）せず、開始時のスナップショットで固定してブレンドする。ロックオン解除では
	// 退場側のカメラが対象を失って注視点を一気に戻すため、固定しないと中央へ吸い込まれるように見える
	if ( bForceFixedBlendStart )
	{
		BlendSettings.bUseFixedBlendStart = true;
	}

	return ModeComponent->PopCameraMode( Handle, BlendSettings );
}

void UExCameraSubsystem::SetThirdPersonCameraTable( UDataTable* InTable )
{
	ThirdPersonCameraTable = InTable;
}
void UExCameraSubsystem::SetThirdPersonModeClass( TSubclassOf<UExCameraMode> InModeClass )
{
	ThirdPersonModeClass = InModeClass;
}
void UExCameraSubsystem::SetBlendSettingsTable( UDataTable* InTable )
{
	BlendSettingsTable = InTable;
}

FVector2D UExCameraSubsystem::GetActiveCameraRotationSpeedRate() const
{
	// コンポーネントが有効なら、そこから現在の倍率をもらう
	if ( RegisteredCameraModeComponent.IsValid() )
	{
		return RegisteredCameraModeComponent->GetActiveRotationSpeedRate();
	}
	return FVector2D( 1.0f, 1.0f );
}

UExCameraModeComponent* UExCameraSubsystem::GetCameraModeComponent() const
{
	return RegisteredCameraModeComponent.Get();
}

FExCameraBlendTableRow UExCameraSubsystem::GetBlendSettings( FName BlendRowName ) const
{
	FExCameraBlendTableRow ResultSettings;
	ResultSettings.BlendTime = 0.0f;

	if ( !BlendSettingsTable )
	{
		return ResultSettings;
	}

	static constexpr const TCHAR* ContextString = TEXT( "CameraSubsystemBlendSearch" );
	bool bFound = false;

	// 1. 指定された行を検索
	if ( BlendRowName != NAME_None )
	{
		if ( const FExCameraBlendTableRow* FoundRow = BlendSettingsTable->FindRow<FExCameraBlendTableRow>( BlendRowName, ContextString, false ) )
		{
			ResultSettings = *FoundRow;
			bFound = true;
		}
	}
	// 2. 見つからなければデフォルト行を検索
	if ( !bFound )
	{
		if ( const FExCameraBlendTableRow* DefaultRow = BlendSettingsTable->FindRow<FExCameraBlendTableRow>( ExCameraBlendRowNames::DefaultBlend, ContextString, false ) )
		{
			ResultSettings = *DefaultRow;
		}
	}

	return ResultSettings;
}

