// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"

#include "AnimNotifyState_WeaponTrail.generated.h"

// 武器トレイル（剣閃）を Notify 区間で制御する AnimNotifyState。「攻撃中」などのステート判定では区間が曖昧なので、
// モンタージュ上に置いたこの Notify の Begin/End で StartWeaponTrail / StopWeaponTrail を確実に呼ぶ。
// 通常攻撃は WeaponTrail、神技スラッシュは GodActionWeaponTrail と NiagaraTag を差し替えて使い分ける
// （実アセット・アタッチ先ソケット／オフセットは ATidePlayerCharacter 側で設定する）
UCLASS( meta = ( DisplayName = "Weapon Trail", Category = "Tide|FX" ) )
class UAnimNotifyState_WeaponTrail : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	UAnimNotifyState_WeaponTrail();

	virtual void NotifyBegin( USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference ) override;
	virtual void NotifyEnd( USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference ) override;

	virtual FString GetNotifyName_Implementation() const override;

public:
	// 起動するトレイルの NiagaraTag（NiagaraSystemDataAsset で実アセットを割り当てる）。
	// 通常攻撃なら "WeaponTrail"、神技スラッシュなら "GodActionWeaponTrail"
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Tide|WeaponTrail" )
	FName NiagaraTag;

	// Niagara の float パラメータ "LifeTime" に渡す値（通常 0.4 / 神技 0.15 など）
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Tide|WeaponTrail" )
	float LifeTime = 0.15f;

	// アタッチ先ソケット。剣の刃に合わせる（ヒットエフェクトと同じ "Weapon_Blade"）
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Tide|WeaponTrail" )
	FName SocketName = TEXT( "Weapon_Blade" );

	// ソケットに対するローカルオフセット（位置）。剣閃の発生位置を微調整する
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Tide|WeaponTrail" )
	FVector LocationOffset = FVector( 50.0f, 0.0f, 0.0f );

	// ソケットに対するローカルオフセット（回転）
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Tide|WeaponTrail" )
	FRotator RotationOffset = FRotator( -90.0f, 0.0f, 0.0f );
};
