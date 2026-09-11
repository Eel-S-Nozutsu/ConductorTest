// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/Components/Equipment/GasCylinderComponent.h"
#include "PRJ_TIDE_P0/Components/Combat/DamageSystemComponent.h"
#include "PRJ_TIDE_P0/Data/Combat/DamageInfo.h"

#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

UGasCylinderComponent::UGasCylinderComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UGasCylinderComponent::BeginPlay()
{
	Super::BeginPlay();

	ACharacter* OwnerChar = Cast<ACharacter>(GetOwner());
	if (OwnerChar)
	{
		CylinderMesh = NewObject<UStaticMeshComponent>(GetOwner(), TEXT("GasCylinderMesh"));
		CylinderMesh->SetStaticMesh(CylinderMeshAsset);
		CylinderMesh->RegisterComponent();
		CylinderMesh->AttachToComponent(
			OwnerChar->GetMesh(),
			FAttachmentTransformRules::SnapToTargetNotIncludingScale,
			AttachSocketName);
		CylinderMesh->SetRelativeLocation(FVector(-30.0f, -24.0f, 0.0f));
		CylinderMesh->SetRelativeRotation(FQuat::MakeFromEuler(FVector(180.0f, 90.0f, -10.0f)));
		CylinderMesh->SetRelativeScale3D(FVector(0.3675f));
		CylinderMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	}

	if (UDamageSystemComponent* DamageSystem = GetOwner()->FindComponentByClass<UDamageSystemComponent>())
	{
		// ストリーミング再入で残留束縛と衝突して二重束縛のensureが出るのを防ぐ
		DamageSystem->OnDamageReceived.AddUniqueDynamic(this, &UGasCylinderComponent::OnDamageReceived);
	}
}

void UGasCylinderComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// BeginPlayで張った束縛を対称に解除する(残留による二重束縛ensureを防ぐ)
	if (AActor* Owner = GetOwner())
	{
		if (UDamageSystemComponent* DamageSystem = Owner->FindComponentByClass<UDamageSystemComponent>())
		{
			DamageSystem->OnDamageReceived.RemoveDynamic(this, &UGasCylinderComponent::OnDamageReceived);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void UGasCylinderComponent::OnDamageReceived(const FDamageInfo& DamageInfo)
{
	if (!IsBackAttack(DamageInfo)) return;
	TriggerExplosion();
}

void UGasCylinderComponent::TriggerExplosion()
{
	if (ExplosionEffect && CylinderMesh)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(), ExplosionEffect,
			CylinderMesh->GetComponentLocation(),
			CylinderMesh->GetComponentRotation());
	}
}

bool UGasCylinderComponent::IsBackAttack(const FDamageInfo& DamageInfo) const
{
	if (!DamageInfo.Instigator.IsValid()) return false;

	FVector ToAttacker = DamageInfo.Instigator->GetActorLocation() - GetOwner()->GetActorLocation();
	ToAttacker.Normalize();

	return FVector::DotProduct(GetOwner()->GetActorForwardVector(), ToAttacker) < 0.35f;
}
