// Copyright (c) 2025, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InputActionValue.h"

#include "InputRouterComponent.generated.h"

class UInputMappingContext;
class UInputAction;
class UEnhancedInputComponent;
struct FGameplayTag;
struct FInputActionInstance;

class UInputActionListDataAsset;
struct FMappingContextData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams( FOnActionEvent, const FName&, ActionName, const FInputActionValue&, Value );

// 入力中継管理コンポーネント
UCLASS( ClassGroup = ( Custom ), meta = ( BlueprintSpawnableComponent ) )
class PRJ_TIDE_P0_API UInputRouterComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UInputRouterComponent();

public:
//	virtual void TickComponent( float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction ) override;

	// タグでのレイヤーの有効化/無効化
	UFUNCTION( BlueprintCallable, Category = "Input|Layers" ) void PushLayerByTag( FGameplayTag LayerTag );
	UFUNCTION( BlueprintCallable, Category = "Input|Layers" ) void PopLayerByTag( FGameplayTag LayerTag );
	UFUNCTION( BlueprintCallable, Category = "Input|Layers" ) void ClearAllLayers();

	// 今アクティブなレイヤー（デバッグ用）
	UFUNCTION( BlueprintPure, Category = "Input|Layers" )
	const TArray<FGameplayTag>& GetActiveLayerTags() const { return ActiveLayerTags; }

#if !UE_BUILD_SHIPPING
	// デバッグ用：BeginPlay で Bind 済みの固有 IA 一覧（各コンテキストの ActionList から収集したもの）。
	// ImGui の InputRouter デバッグウィンドウから参照する。
	const TSet<TObjectPtr<UInputAction>>& GetUniqueActionsForDebug() const { return UniqueActions; }
#endif

private:
	virtual void BeginPlay() override;
	virtual void EndPlay( const EEndPlayReason::Type EndPlayReason ) override;

	void CollectUniqueActions();
	void BindAllUniqueActionsOnce();
	void UnbindAllActions();

	void OnActionStarted( const FInputActionInstance& Inst );
	void OnActionTriggered( const FInputActionInstance& Inst );
	void OnActionCompleted( const FInputActionInstance& Inst );

	void UpdateMappingContexts() const;

	void PushLayer( const FMappingContextData& Data );
	void PopLayer( const FMappingContextData& Data );

public:
	UPROPERTY( EditAnywhere, Category = "Tide|Input" ) TArray<FMappingContextData> MappingContextList;

	// 一括イベント
	UPROPERTY( BlueprintAssignable, Category = "Tide|Input|Events" ) FOnActionEvent	OnActionStartedDelegate;
	UPROPERTY( BlueprintAssignable, Category = "Tide|Input|Events" ) FOnActionEvent	OnActionTriggeredDelegate;
	UPROPERTY( BlueprintAssignable, Category = "Tide|Input|Events" ) FOnActionEvent	OnActionCompletedDelegate;

private:
	TWeakObjectPtr<class APlayerController> OwnerPlayer;
	TWeakObjectPtr<UEnhancedInputComponent> BoundEIC;

	// 現在アクティブなレイヤー
	UPROPERTY() TArray<FGameplayTag> ActiveLayerTags;

	// Bind用固有Actionリスト
	UPROPERTY() TSet<TObjectPtr<UInputAction>> UniqueActions;

	struct FBindTriple
	{
		int32 Started = INDEX_NONE;
		int32 Triggered = INDEX_NONE;
		int32 Completed = INDEX_NONE;
		int32 Canceled = INDEX_NONE;
	};
	TMap<TObjectPtr<UInputAction>, FBindTriple> ActionBindingMap;
};
