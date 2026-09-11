// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#include "TidePlayerController.h"

#include "EnhancedInputComponent.h"
#include "Engine/World.h"

#include "PRJ_TIDE_P0/Components/Input/InputRouterComponent.h"
#include "PRJ_TIDE_P0/Components/Input/DPadFromHatComponent.h"
#include "PRJ_TIDE_P0/Actors/Character/Player/TidePlayerCharacter.h"
#include "PRJ_TIDE_P0/Actors/Camera/ExCameraActor.h"
#include "PRJ_TIDE_P0/Components/Camera/ExCameraModeComponent.h"
#include "PRJ_TIDE_P0/Subsystems/Camera/ExCameraSubsystem.h"
#include "PRJ_TIDE_P0/Composites/Camera/CameraMode/ThirdPersonExCameraMode.h"
#include "PRJ_TIDE_P0/Components/Input/InputBufferComponent.h"
#include "PRJ_TIDE_P0/Data/Tags/TideGameplayTags.h"
// Debug
#include "PRJ_TIDE_P0/Integration/ImGui/ImGuiDebugSubsystem.h"
#include "PRJ_TIDE_P0/Composites/Debug/TideCheatManager.h"

ATidePlayerController::ATidePlayerController()
{
	InputRouter = CreateDefaultSubobject<UInputRouterComponent>( TEXT( "InputRouter" ) );
	DPadFromHat = CreateDefaultSubobject<UDPadFromHatComponent>( TEXT( "DPadFromHat" ) );

	ThirdPersonCameraModeClass = UThirdPersonExCameraMode::StaticClass();
	CheatClass = UTideCheatManager::StaticClass();
}

void ATidePlayerController::BeginPlay()
{
	Super::BeginPlay();

	InitializeCamera();
	InitializeInput();
}

void ATidePlayerController::EndPlay( const EEndPlayReason::Type EndPlayReason )
{
	// InitializeInput で張った入力デリゲートを解除する
	if ( InputRouter )
	{
		InputRouter->OnActionStartedDelegate.RemoveDynamic( this, &ATidePlayerController::HandleActionStarted );
		InputRouter->OnActionTriggeredDelegate.RemoveDynamic( this, &ATidePlayerController::HandleActionTriggered );
		InputRouter->OnActionCompletedDelegate.RemoveDynamic( this, &ATidePlayerController::HandleActionCompleted );
	}

	if ( SpawnedCamera )
	{
		if ( UExCameraModeComponent* ModeComp = SpawnedCamera->GetCameraModeComponent() )
		{
			// CameraActor / Component より先に Controller が片付くケースに備える
			ModeComp->SetInputProvider( nullptr );
		}
	}

	Super::EndPlay( EndPlayReason );
}

void ATidePlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if ( !InputComponent ) return;

	InputComponent->BindKey(
		EKeys::F3,
		IE_Pressed,
		this,
		&ATidePlayerController::ToggleImGui
	);
}

void ATidePlayerController::Tick( float DeltaTime )
{
	Super::Tick( DeltaTime );

	const bool bCurrentImGuiInputOn = FImGuiModule::Get().GetProperties().IsInputEnabled();
	if ( bCurrentImGuiInputOn != bLastImGuiInputState )
	{
		UpdateInputModeByImGui();
		bLastImGuiInputState = bCurrentImGuiInputOn;
	}

	UpdateDebugCameraEnterInput();
	UpdateGodArtSelectInput();
}

void ATidePlayerController::UpdateDebugCameraEnterInput()
{
	// デバッグカメラ起動中は SwitchController で Player が外れて false になる。その間は prev を更新せず
	// 早期 return し、ゲームプレイへ戻った際の誤発火（即再起動）を防ぐ
	if ( !IsLocalPlayerController() ) return;

	// R1 + Select 同時押しの立ち上がりで起動する
	const bool bEnterChord = IsInputKeyDown( EKeys::Gamepad_RightShoulder ) && IsInputKeyDown( EKeys::Gamepad_Special_Left );

	if ( bEnterChord && !bPrevDebugCamEnterChord )
	{
		if ( !CheatManager )
		{
			EnableCheats();
		}
		if ( UTideCheatManager* TideCheat = Cast<UTideCheatManager>( CheatManager ) )
		{
			TideCheat->EnterDebugCamera();
		}
	}

	bPrevDebugCamEnterChord = bEnterChord;
}

void ATidePlayerController::UpdateGodArtSelectInput()
{
	if ( !IsLocalPlayerController() ) return;

	// 押下状態は構えに関わらず毎フレーム更新する（突入時に押しっぱなしでも立ち上がり誤検知しないよう
	// prev を常に追従させる）。実際のカーソル移動は構え中だけ行う
	const bool bL1Down = IsInputKeyDown( EKeys::Gamepad_LeftShoulder );
	const bool bR1Down = IsInputKeyDown( EKeys::Gamepad_RightShoulder );
	const float StickX = GetInputAnalogKeyState( EKeys::Gamepad_LeftX );

	if ( auto PlayerCharacter = Cast<ATidePlayerCharacter>( GetPawn() ) )
	{
		// 直接選択モード（X/Y/B）は専用 IA のハンドラが担うので、ここはカーソルモードだけを見る
		if ( PlayerCharacter->IsGodArtSelecting() && !PlayerCharacter->IsGodArtFaceButtonSelectEnabled() )
		{
			if ( PlayerCharacter->IsGodArtStanceMovementAllowed() )
			{
				// L スティックは移動に取られるので L1/R1 で選択（立ち上がりで 1 段）
				if ( bL1Down && !bPrevGodArtSelectL1 ) PlayerCharacter->RequestGodArtSelect( -1 );
				if ( bR1Down && !bPrevGodArtSelectR1 ) PlayerCharacter->RequestGodArtSelect( +1 );
			}
			else
			{
				// 移動禁止モードは L スティック水平で選択（フリック 1 回＝1 段）
				PlayerCharacter->RequestGodArtStickSelect( StickX );
			}
		}
	}

	bPrevGodArtSelectL1 = bL1Down;
	bPrevGodArtSelectR1 = bR1Down;
}

void ATidePlayerController::ToggleImGui()
{
	if ( GetGameInstance() )
	{
		if ( UImGuiDebugSubsystem* ImGuiSubsystem = GetGameInstance()->GetSubsystem<UImGuiDebugSubsystem>() )
		{
			ImGuiSubsystem->ToggleImGui();
		}
	}
}

void ATidePlayerController::UpdateInputModeByImGui()
{
	const bool bImGuiInputOn = FImGuiModule::Get().GetProperties().IsInputEnabled();

	// 移動・視点だけ無視するのではなく入力自体を止める（Enhanced Input のイベント発火ごとブロックする）
	if ( bImGuiInputOn )
	{
		DisableInput( this );
	}
	else
	{
		EnableInput( this );
	}
}

// ToggleDebugCamera 等のコンソールコマンドをフックする口として残してある（現在は素通し）
bool ATidePlayerController::ProcessConsoleExec( const TCHAR* Cmd, FOutputDevice& Ar, UObject* Executor )
{
	return Super::ProcessConsoleExec( Cmd, Ar, Executor );
}

void ATidePlayerController::InitializeCamera()
{
	// 仮の黒フェード処理
	if ( APlayerCameraManager* CameraManager = PlayerCameraManager )
	{
		CameraManager->SetManualCameraFade( 1.0f, FLinearColor::Black, true );

		CameraManager->StartCameraFade(
			1.0f,
			0.0f,
			1.0f,
			FLinearColor::Black
		);
	}

	if ( CameraActorClass && GetWorld() )
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		SpawnedCamera = GetWorld()->SpawnActor<AExCameraActor>(
			CameraActorClass,
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			SpawnParams
		);
		if ( SpawnedCamera )
		{
			SetViewTargetWithBlend( SpawnedCamera );

			// 入力データは PlayerController が持っているので、供給元として自身を渡す
			UExCameraModeComponent* CameraModeComp = SpawnedCamera->GetCameraModeComponent();
			if ( CameraModeComp )
			{
				CameraModeComp->SetInputProvider( this );
			}

			// カメラ操作の公開窓口は Subsystem に集約し、初期テーブル設定もここで行う
			if ( ULocalPlayer* LocalPlayer = GetLocalPlayer() )
			{
				if ( UExCameraSubsystem* CameraSubsystem = LocalPlayer->GetSubsystem<UExCameraSubsystem>() )
				{
					CameraSubsystem->SetThirdPersonCameraTable( ThirdPersonCameraTable );
					CameraSubsystem->SetThirdPersonModeClass( ThirdPersonCameraModeClass );
					CameraSubsystem->SetBlendSettingsTable( BlendSettingsTable );

					if ( !DefaultCameraModeRowName.IsNone() )
					{
						CameraSubsystem->PushThirdPersonCameraByKey( DefaultCameraModeRowName, TEXT( "Init" ) );
					}
					else
					{
						UE_LOG( LogTemp, Error, TEXT( "[TidePlayerController] デフォルトのカメラモードが設定されていません。" ) );
					}
				}
			}
		}
	}
}

void ATidePlayerController::InitializeInput()
{
	InitializeInputHandlers();

	if ( InputRouter )
	{
		InputRouter->OnActionStartedDelegate.AddDynamic( this, &ATidePlayerController::HandleActionStarted );
		InputRouter->OnActionTriggeredDelegate.AddDynamic( this, &ATidePlayerController::HandleActionTriggered );
		InputRouter->OnActionCompletedDelegate.AddDynamic( this, &ATidePlayerController::HandleActionCompleted );
	}
}

void ATidePlayerController::InitializeInputHandlers()
{
	auto& Move = InputHandlers.FindOrAdd( "IA_Move" );
	{
		Move.OnTriggered = [this]( const FInputActionValue& Value )
			{
				if ( auto Character = Cast<ATidePlayerCharacter>( GetPawn() ) )
				{
					const FVector2D MoveValue = Value.Get<FVector2D>();
					Character->RequestMove( MoveValue.X, MoveValue.Y );
				}
			};
		Move.OnCompleted = [this]( const FInputActionValue& Value )
			{
				if ( auto Character = Cast<ATidePlayerCharacter>( GetPawn() ) )
				{
					Character->RequestMoveEnd();
				}
			};
	}

	auto& Look = InputHandlers.FindOrAdd( "IA_Look" );
	{
		Look.OnStarted = [this]( const FInputActionValue& Value )
			{
				if ( auto Character = Cast<ATidePlayerCharacter>( GetPawn() ) )
				{
					Character->RequestLockOnTargetSwitch( Value.Get<FVector2D>().X );
				}
			};
		Look.OnTriggered = [this]( const FInputActionValue& Value )
			{
				if ( auto Character = Cast<ATidePlayerCharacter>( GetPawn() ) )
				{
					const FVector2D LookValue = Value.Get<FVector2D>();
					Character->RequestLook( LookValue.X, LookValue.Y );
				}
			};
	}

	auto& Jump = InputHandlers.FindOrAdd( "IA_Jump" );
	{
		Jump.OnStarted = [this]( const FInputActionValue& Value )
			{
				if ( auto Character = Cast<ATidePlayerCharacter>( GetPawn() ) )
				{
					// 長押しでの滑空移行判定に使うため、物理押下も記録する
					Character->SetJumpInputHeld( true );
					if ( UInputBufferComponent* Buffer = Character->GetInputBufferComponent() )
					{
						Buffer->PushCommand( TAG_Input_Command_Jump );
					}
				}
			};
		Jump.OnCompleted = [this]( const FInputActionValue& Value )
			{
				if ( auto Character = Cast<ATidePlayerCharacter>( GetPawn() ) )
				{
					Character->SetJumpInputHeld( false );
				}
			};
	}

	auto& ChargeAction = InputHandlers.FindOrAdd( "IA_ChargeAction" );
	{
		ChargeAction.OnStarted = [this]( const FInputActionValue& Value )
			{
				if ( auto Character = Cast<ATidePlayerCharacter>( GetPawn() ) )
				{
					// 神技構え中は R2 で選択神技を発動（チャージ入力は積まない）
					if ( Character->TryGodArtExecute() ) return;

					// 打ち上げ封印中の押下も拾い、解除時の自動チャージ判定に使う
					Character->SetChargeInputHeld( true );
					if ( UInputBufferComponent* Buffer = Character->GetInputBufferComponent() )
					{
						Buffer->PushCommand( TAG_Input_Command_ChargeAction );
					}
				}
			};
		ChargeAction.OnCompleted = [this]( const FInputActionValue& Value )
			{
				if ( auto Character = Cast<ATidePlayerCharacter>( GetPawn() ) )
				{
					Character->RequestChargeActionEnd();
				}
			};
	}

	auto& Attack = InputHandlers.FindOrAdd( "IA_Attack" );
	{
		Attack.OnStarted = [this]( const FInputActionValue& Value )
			{
				if ( auto Character = Cast<ATidePlayerCharacter>( GetPawn() ) )
				{
					// 直接選択モードの構え中は X を神技選択へ振り替えるため、攻撃はマスクする
					if ( Character->IsGodArtFaceButtonSelectActive() ) return;
					if ( UInputBufferComponent* Buffer = Character->GetInputBufferComponent() )
					{
						Buffer->PushCommand( TAG_Input_Command_Attack );
					}
				}
			};
	}

	auto& Dodge = InputHandlers.FindOrAdd( "IA_Dodge" );
	{
		Dodge.OnStarted = [this]( const FInputActionValue& Value )
			{
				if ( auto Character = Cast<ATidePlayerCharacter>( GetPawn() ) )
				{
					// 構え中は L1/R1 を神技選択へ振り替えるため、回避もマスクする
					if ( Character->IsGodArtSelecting() ) return;
					if ( UInputBufferComponent* Buffer = Character->GetInputBufferComponent() )
					{
						Buffer->PushCommand( TAG_Input_Command_Dodge );
					}
					Character->SetDodgeInputHeld( true );
				}
			};
		Dodge.OnTriggered = [this]( const FInputActionValue& Value )
			{
				if ( auto Character = Cast<ATidePlayerCharacter>( GetPawn() ) )
				{
					if ( Character->IsGodArtSelecting() ) return;	// ダッシュ化もマスクする

					// 回避ホールド→ダッシュ化（もう一方の経路は下の IA_Dash。両経路は意図的に併存）
					Character->RequestDash();
				}
			};
		Dodge.OnCompleted = [this]( const FInputActionValue& Value )
			{
				if ( auto Character = Cast<ATidePlayerCharacter>( GetPawn() ) )
				{
					Character->SetDodgeInputHeld( false );
				}
			};
	}

	// L1/R1 は IA_Dodge と共用のため左右を判別できず回避と競合もするので、手動ギア操作は専用 IA を立てている。
	// 有効／無効の判定は PlayerParamData 側のフラグで行う
	auto& ChargeGearDown = InputHandlers.FindOrAdd( "IA_ChargeGearDown" );
	{
		ChargeGearDown.OnStarted = [this]( const FInputActionValue& Value )
			{
				if ( auto Character = Cast<ATidePlayerCharacter>( GetPawn() ) )
				{
					Character->RequestChargeGearDown();
				}
			};
	}

	auto& ChargeGearUp = InputHandlers.FindOrAdd( "IA_ChargeGearUp" );
	{
		ChargeGearUp.OnStarted = [this]( const FInputActionValue& Value )
			{
				if ( auto Character = Cast<ATidePlayerCharacter>( GetPawn() ) )
				{
					Character->RequestChargeGearUp();
				}
			};
	}

	auto& LockOn = InputHandlers.FindOrAdd( "IA_LockOn" );
	{
		LockOn.OnStarted = [this]( const FInputActionValue& Value )
			{
				if ( auto Character = Cast<ATidePlayerCharacter>( GetPawn() ) )
				{
					Character->RequestLockOn();
				}
			};
	}

	// ダッシュ入力は意図的に 2 経路ある（統合しない）：IA_Dodge のホールドと、この IA_Dash（専用ボタン）。
	// 片方を消すと対応する入力デバイス割当が死ぬため両方維持する
	auto& Dash = InputHandlers.FindOrAdd( "IA_Dash" );
	{
		Dash.OnStarted = [this]( const FInputActionValue& Value )
			{
				if ( auto Character = Cast<ATidePlayerCharacter>( GetPawn() ) )
				{
					Character->RequestDash();
				}
			};
	}

	auto& ChargeCancel = InputHandlers.FindOrAdd( "IA_CancelAction" );
	{
		ChargeCancel.OnStarted = [this]( const FInputActionValue& Value )
			{
				if ( auto Character = Cast<ATidePlayerCharacter>( GetPawn() ) )
				{
					// 直接選択モードの構え中は B を神技選択へ振り替えるため、キャンセルも解除もさせない
					// （構えの解除は L2 を離す側が担う）
					if ( Character->IsGodArtFaceButtonSelectActive() ) return;
					Character->RequestChargeCancelStart();
					Character->RequestGodActionCancel();
				}
			};
		ChargeCancel.OnCompleted = [this]( const FInputActionValue& Value )
			{
				if ( auto Character = Cast<ATidePlayerCharacter>( GetPawn() ) )
				{
					Character->RequestChargeCancelEnd();
				}
			};
	}

	auto& CameraReset = InputHandlers.FindOrAdd( "IA_CameraReset" );
	{
		CameraReset.OnStarted = [this]( const FInputActionValue& Value )
			{
				if ( auto Character = Cast<ATidePlayerCharacter>( GetPawn() ) )
				{
					Character->RequestCameraReset();
				}
			};
	}

	auto& GodAction = InputHandlers.FindOrAdd( "IA_GodAction" );
	{
		// L2 長押しでロックオン開始、解除で一閃発動。L2 中の B（IA_ChargeCancel）でキャンセル
		GodAction.OnStarted = [this]( const FInputActionValue& Value )
			{
				if ( auto Character = Cast<ATidePlayerCharacter>( GetPawn() ) )
				{
					Character->RequestGodActionStart();
				}
			};
		GodAction.OnCompleted = [this]( const FInputActionValue& Value )
			{
				if ( auto Character = Cast<ATidePlayerCharacter>( GetPawn() ) )
				{
					Character->RequestGodActionRelease();
				}
			};
	}

	// 神技メニュー：十字キー左右でカーソル移動（専用 IA）
	auto& GodArtSelectLeft = InputHandlers.FindOrAdd( "IA_GodArtSelectLeft" );
	{
		GodArtSelectLeft.OnStarted = [this]( const FInputActionValue& Value )
			{
				if ( auto Character = Cast<ATidePlayerCharacter>( GetPawn() ) )
				{
					Character->RequestGodArtSelect( -1 );
				}
			};
	}

	auto& GodArtSelectRight = InputHandlers.FindOrAdd( "IA_GodArtSelectRight" );
	{
		GodArtSelectRight.OnStarted = [this]( const FInputActionValue& Value )
			{
				if ( auto Character = Cast<ATidePlayerCharacter>( GetPawn() ) )
				{
					// D-pad 右に加えマウスホイール軸もこの IA へ集約し、値の符号で右(+1)／左(-1) に振り分ける。
					// ※ホイール軸は Right にのみ割り当てること（Left にも割り当てると二重発火する）
					const float Axis = Value.Get<float>();
					Character->RequestGodArtSelect( Axis < 0.0f ? -1 : +1 );
				}
			};
	}

	// 直接選択（bGodArtSelectByFaceButtons=ON）。画面の並びに合わせて X＝戯／Y＝導／B＝依。
	// X は IA_Attack、B は IA_CancelAction と物理ボタンを共用するため構え中はそちら側をマスクする。
	// ※発火にはアセット側の設定が必要：IA を作成 → IMC_Default に X/Y/B を割り当て → DA_DefaultInputActions へ追加
	auto& GodArtSelectFrolic = InputHandlers.FindOrAdd( "IA_GodArtSelectFrolic" );
	{
		GodArtSelectFrolic.OnStarted = [this]( const FInputActionValue& Value )
			{
				if ( auto Character = Cast<ATidePlayerCharacter>( GetPawn() ) )
				{
					Character->RequestGodArtSelectAt( 0 );
				}
			};
	}

	auto& GodArtSelectGuidance = InputHandlers.FindOrAdd( "IA_GodArtSelectGuidance" );
	{
		GodArtSelectGuidance.OnStarted = [this]( const FInputActionValue& Value )
			{
				if ( auto Character = Cast<ATidePlayerCharacter>( GetPawn() ) )
				{
					Character->RequestGodArtSelectAt( 1 );
				}
			};
	}

	auto& GodArtSelectPossession = InputHandlers.FindOrAdd( "IA_GodArtSelectPossession" );
	{
		GodArtSelectPossession.OnStarted = [this]( const FInputActionValue& Value )
			{
				if ( auto Character = Cast<ATidePlayerCharacter>( GetPawn() ) )
				{
					Character->RequestGodArtSelectAt( 2 );
				}
			};
	}

	// デバッグ用 MappingContext の専用 IA へ割り当てて使う。
	// 実際の発火は bDebugFlagSpawnTornadoInFront が立っているときのみ（既定 false＝本番に影響なし）
	auto& DebugSpawnTornado = InputHandlers.FindOrAdd( "IA_DebugSpawnTornado" );
	{
		DebugSpawnTornado.OnStarted = [this]( const FInputActionValue& Value )
			{
				if ( auto Character = Cast<ATidePlayerCharacter>( GetPawn() ) )
				{
					Character->RequestDebugSpawnTornadoInFront();
				}
			};
	}
}

void ATidePlayerController::HandleActionStarted( const FName& ActionName, const FInputActionValue& Value )
{
	if ( auto* Handler = InputHandlers.Find( ActionName ) )
	{
		if ( Handler->OnStarted ) Handler->OnStarted( Value );
	}
}
void ATidePlayerController::HandleActionTriggered( const FName& ActionName, const FInputActionValue& Value )
{
	if ( auto* Handler = InputHandlers.Find( ActionName ) )
	{
		if ( Handler->OnTriggered ) Handler->OnTriggered( Value );
	}
}
void ATidePlayerController::HandleActionCompleted( const FName& ActionName, const FInputActionValue& Value )
{
	if ( auto* Handler = InputHandlers.Find( ActionName ) )
	{
		if ( Handler->OnCompleted ) Handler->OnCompleted( Value );
	}
}

FCameraControlData ATidePlayerController::GetCameraControlData() const
{
	FCameraControlData Data;

	Data.DeltaTime = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.0f;
	Data.ControlRotation = GetControlRotation();
	if ( APawn* ControlledPawn = GetPawn() )
	{
		Data.TargetLocation = ControlledPawn->GetActorLocation();
	}

	return Data;
}
