// Copyright (c) 2025, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.

#include "InputRouterComponent.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"

#include "PRJ_TIDE_P0/Data/Input/MappingContextData.h"
#include "PRJ_TIDE_P0/Data/Input/InputActionListDataAsset.h"

// Sets default values for this component's properties
UInputRouterComponent::UInputRouterComponent()
{
//	PrimaryComponentTick.bCanEverTick = true;

	// ...
}

// Called when the game starts
void UInputRouterComponent::BeginPlay()
{
	Super::BeginPlay();

	OwnerPlayer = Cast<APlayerController>( GetOwner() );
	if ( !OwnerPlayer.IsValid() || !OwnerPlayer->IsLocalController() ) { return; }

	if ( auto* EIC = Cast<UEnhancedInputComponent>( OwnerPlayer->InputComponent ) )
	{
		BoundEIC = EIC;
	}

	// 一度だけ全コンテキストを走査してActionを重複除外
	CollectUniqueActions();
	BindAllUniqueActionsOnce();

	// AutoActivate のレイヤを起動
	for ( const auto& Context : MappingContextList )
	{
		if ( Context.bAutoActivate )
		{
			PushLayer( Context );
		}
	}
}

void UInputRouterComponent::EndPlay( const EEndPlayReason::Type EndPlayReason )
{
	ClearAllLayers();
	UnbindAllActions();
	Super::EndPlay( EndPlayReason );
}


// 入力アクションリストの精査
void UInputRouterComponent::CollectUniqueActions()
{
	UniqueActions.Reset();
	for ( const auto& Context : MappingContextList )
	{
		if ( Context.ActionList )
		{
			for ( auto& IA : Context.ActionList->Actions )
			{
				if ( IA ) { UniqueActions.Add( IA ); }
			}
		}
	}
}

// 入力アクションのバインド
void UInputRouterComponent::BindAllUniqueActionsOnce()
{
	auto* EIC = BoundEIC.Get();
	if ( !EIC ) return;

	for ( auto& IA : UniqueActions )
	{
		FBindTriple Triple;
		Triple.Started = EIC->BindAction(
			IA,
			ETriggerEvent::Started,
			this,
			&UInputRouterComponent::OnActionStarted
		).GetHandle();

		Triple.Triggered = EIC->BindAction(
			IA,
			ETriggerEvent::Triggered,
			this,
			&UInputRouterComponent::OnActionTriggered
		).GetHandle();

		Triple.Completed = EIC->BindAction(
			IA,
			ETriggerEvent::Completed,
			this,
			&UInputRouterComponent::OnActionCompleted
		).GetHandle();

		// Completed に加えて Canceled も「解放」として同じ経路へ流す。トリガー構成次第で、Triggered まで到達せずに
		// 離されると Canceled が飛ぶ（例：マウス右ボタンは Canceled、ゲームパッド右トリガーは Completed という非対称）。
		// 拾わないとチャージ等の「押しっぱなし→離しで終了」系が解除されず固まる
		Triple.Canceled = EIC->BindAction(
			IA,
			ETriggerEvent::Canceled,
			this,
			&UInputRouterComponent::OnActionCompleted
		).GetHandle();

		ActionBindingMap.Add( IA, Triple );
	}
}

// バインド解除
void UInputRouterComponent::UnbindAllActions()
{
	if ( auto* EIC = BoundEIC.Get() )
	{
		for ( auto& Pair : ActionBindingMap )
		{
			EIC->RemoveBindingByHandle( Pair.Value.Started );
			EIC->RemoveBindingByHandle( Pair.Value.Triggered );
			EIC->RemoveBindingByHandle( Pair.Value.Completed );
			EIC->RemoveBindingByHandle( Pair.Value.Canceled );
		}
	}
	ActionBindingMap.Reset();
}

// 入力アクション開始時
void UInputRouterComponent::OnActionStarted( const FInputActionInstance& Inst )
{
	const UInputAction* Action = Inst.GetSourceAction();
	const FName Name = Action ? Action->GetFName() : NAME_None;

	OnActionStartedDelegate.Broadcast( Name, Inst.GetValue() );
}

// 入力アクション押下中
void UInputRouterComponent::OnActionTriggered( const FInputActionInstance& Inst )
{
	const UInputAction* Action = Inst.GetSourceAction();
	OnActionTriggeredDelegate.Broadcast( Action ? Action->GetFName() : NAME_None, Inst.GetValue() );
}

// 入力アクション終了時
void UInputRouterComponent::OnActionCompleted( const FInputActionInstance& Inst )
{
	const UInputAction* Action = Inst.GetSourceAction();
	OnActionCompletedDelegate.Broadcast( Action ? Action->GetFName() : NAME_None, Inst.GetValue() );
}

// タグでのレイヤー追加
void UInputRouterComponent::PushLayerByTag( FGameplayTag Tag )
{
	for ( const auto& Context : MappingContextList )
	{
		if ( Context.LayerTag == Tag ) { PushLayer( Context ); }
	}
}
// タグでのレイヤー削除
void UInputRouterComponent::PopLayerByTag( FGameplayTag Tag )
{
	for ( const auto& Context : MappingContextList )
	{
		if ( Context.LayerTag == Tag ) { PopLayer( Context ); }
	}
}
// レイヤー全削除
void UInputRouterComponent::ClearAllLayers()
{
	ActiveLayerTags.Reset();
	UpdateMappingContexts();
}

// レイヤーの追加
void UInputRouterComponent::PushLayer( const FMappingContextData& Data )
{
	if ( !Data.LayerTag.IsValid() ) { return; }
	if ( ActiveLayerTags.Contains( Data.LayerTag ) ) { return; }

	ActiveLayerTags.Add( Data.LayerTag );
	UpdateMappingContexts();
}

// レイヤーの削除
void UInputRouterComponent::PopLayer( const FMappingContextData& Data )
{
	if ( !Data.LayerTag.IsValid() ) { return; }

	ActiveLayerTags.Remove( Data.LayerTag );
	UpdateMappingContexts();
}

// 入力リスト更新
void UInputRouterComponent::UpdateMappingContexts() const
{
	auto* PC = OwnerPlayer.Get();
	if ( !PC ) { return; }
	auto* LP = PC->GetLocalPlayer();
	if ( !LP ) { return; }

	auto* EnhancedInput = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>( LP );
	if ( !EnhancedInput ) { return; }

	// まず全部Remove
	for ( const auto& C : MappingContextList )
	{
		if ( C.MappingContext ) { EnhancedInput->RemoveMappingContext( C.MappingContext ); }
	}

	// 有効タグに合致するものをPriorityが低い順でAdd
	TArray<const FMappingContextData*> ActiveContexts;
	for ( const auto& Context : MappingContextList )
	{
		if ( !Context.MappingContext ) { continue; }
		if ( !Context.LayerTag.IsValid() ) { continue; }
		if ( ActiveLayerTags.Contains( Context.LayerTag ) )
		{
			ActiveContexts.Add( &Context );
		}
	}
	ActiveContexts.Sort( []( const FMappingContextData& L, const FMappingContextData& R ) {
		return L.Priority < R.Priority;
		} );
	for ( const auto* Context : ActiveContexts )
	{
		EnhancedInput->AddMappingContext( Context->MappingContext, Context->Priority );
	}
}
