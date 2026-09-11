// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LockOnComponent.generated.h"

class ULockOnTargetComponent;

UCLASS( ClassGroup = ( LockOn ), meta = ( BlueprintSpawnableComponent ) )
class ULockOnComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	ULockOnComponent();

	void ToggleLockOn();
	void SetTarget( ULockOnTargetComponent* NewTarget );
	void ClearLockOn();

	bool HasTarget() const;
	ULockOnTargetComponent* GetTarget() const;
	ULockOnTargetComponent* FindBestTarget() const;

#if !UE_BUILD_SHIPPING
public:
	// ImGui用のデバッグ描画関数
	void DrawImGuiDebug();
#endif

public:
	// ロックオン対象を検索する半径
	UPROPERTY( EditAnywhere )
	float SearchRadius = 2000.0f;

	// 検索対象のオブジェクトタイプ（デフォルトでPawnなどを指定）
	UPROPERTY( EditAnywhere )
	TArray<TEnumAsByte<EObjectTypeQuery>> TargetObjectTypes;

	// 画面のど真ん中（カメラの正面）にいる対象をどれだけ優先するか。
	// 値を大きくすると「遠くにいても、画面の中央に捉えている対象」をロックオンしやすくなります。
	UPROPERTY( EditAnywhere )
	float AngleWeight = 1.0f;

	// 距離が近い対象をどれだけ優先するか（距離が遠いことによる減点の強さ）。
	// 値を大きくすると「画面の端にいても、目の前にいる対象」をロックオンしやすくなります。
	UPROPERTY( EditAnywhere )
	float DistanceWeight = 0.5f;

	// カメラの背後にいる対象を検索対象から外すための閾値（内積）。
	// 0.0で真横まで、0.5で前方60度のみを対象とする
	UPROPERTY( EditAnywhere )
	float MinDotProductThreshold = 0.0f;

private:
	TWeakObjectPtr<ULockOnTargetComponent> CurrentTarget;
};
