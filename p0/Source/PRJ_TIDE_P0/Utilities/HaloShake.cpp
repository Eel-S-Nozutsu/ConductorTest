// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/Utilities/HaloShake.h"

#include "Components/SceneComponent.h"

void FHaloShakeState::Trigger(USceneComponent* Mesh, const FHaloHitShakeParams& Params)
{
	if (!Mesh || Params.Duration <= 0.0f) return;

	// 揺れ中の再トリガーでは基準姿勢を保つ
	// (現在のオフセット込み回転を新しい基準にしてしまうのを防ぐ)
	if (!bActive)
	{
		BaseRelRot = Mesh->GetRelativeRotation();
	}

	bActive      = true;
	Elapsed      = 0.0f;
	Duration     = Params.Duration;
	AmplitudeDeg = Params.AmplitudeDeg;
	FrequencyHz  = Params.FrequencyHz;
}

void FHaloShakeState::Tick(float DeltaTime, USceneComponent* Mesh)
{
	if (!bActive) return;
	if (!Mesh) { bActive = false; return; }

	Elapsed += DeltaTime;
	if (Elapsed >= Duration)
	{
		Mesh->SetRelativeRotation(BaseRelRot);
		bActive = false;
		return;
	}

	// 振幅を線形に減衰させる
	const float Decay = 1.0f - (Elapsed / Duration);
	const float W     = 2.0f * PI * FrequencyHz;
	const float A     = AmplitudeDeg * Decay;

	// 軸ごとに周波数と位相をずらして非周期的な3軸ブレにする
	const FRotator Offset(
		A * FMath::Sin(W * Elapsed * 1.00f),
		A * FMath::Sin(W * Elapsed * 1.27f + 1.7f),
		A * FMath::Sin(W * Elapsed * 0.83f + 3.4f));

	Mesh->SetRelativeRotation(BaseRelRot + Offset);
}
