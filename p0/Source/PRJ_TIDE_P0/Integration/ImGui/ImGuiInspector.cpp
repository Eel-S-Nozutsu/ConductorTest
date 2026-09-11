// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/Integration/ImGui/ImGuiInspector.h"
#include "PRJ_TIDE_P0/Interfaces/Debug/IImGuiInspectable.h"

#include "imgui.h"
#include "ImGuiModule.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "HAL/PlatformApplicationMisc.h"

void FImGuiInspector::Tick(UWorld* World)
{
	if (!World) return;

	// ImGui入力優先モード中かつImGuiウィンドウ上でないときのみ右クリック判定
	const bool bCanPick = FImGuiModule::Get().GetProperties().IsInputEnabled()
		&& !ImGui::GetIO().WantCaptureMouse;

	APlayerController* PC = World->GetFirstPlayerController();
	const bool bRightMouseDown = PC && PC->IsInputKeyDown(EKeys::RightMouseButton);
	const bool bRightClickJustPressed = bCanPick && bRightMouseDown && !bPrevRightMouseDown;
	bPrevRightMouseDown = bRightMouseDown;

	if (bRightClickJustPressed)
	{
		AActor* HitActor = LineTraceUnderCursor(World);
		if (HitActor && HitActor->Implements<UImGuiInspectable>())
		{
			const ImVec2 MousePos = ImGui::GetMousePos();
			TryOpenInspector(HitActor, FVector2D(MousePos.x, MousePos.y));
		}
	}

	DrawInspectorWindows();
}

void FImGuiInspector::TryOpenInspector(AActor* Actor, FVector2D SpawnPos)
{
	// 既に開いていれば何もしない
	for (const FInspectorEntry& Entry : InspectorEntries)
	{
		if (Entry.Actor.Get() == Actor) return;
	}

	// 上限超えならFIFOで最古のウィンドウを削除
	while (InspectorEntries.Num() >= MaxInspectorWindows)
	{
		InspectorEntries.RemoveAt(0);
	}

	InspectorEntries.Add({ Actor, NextWindowId++, true, SpawnPos });
}

void FImGuiInspector::DrawInspectorWindows()
{
	for (int32 i = 0; i < InspectorEntries.Num(); ++i)
	{
		FInspectorEntry& Entry = InspectorEntries[i];
		AActor* Actor = Entry.Actor.Get();
		if (!Actor) continue;

		// ###でウィンドウIDを固定(タイトル変化でもウィンドウ位置を保持)
		FString Title = FString::Printf(
			TEXT("[Inspector] %s###Inspector%d"), *Actor->GetActorNameOrLabel(), Entry.WindowId);
		bool bOpen = Entry.bOpen;

		ImGui::SetNextWindowPos(ImVec2(Entry.SpawnPos.X, Entry.SpawnPos.Y), ImGuiCond_Appearing);

		const bool bExpanded = ImGui::Begin(TCHAR_TO_UTF8(*Title), &bOpen, ImGuiWindowFlags_MenuBar);

		if (ImGui::BeginMenuBar())
		{
			if (ImGui::BeginMenu("Option"))
			{
				ImGui::SetNextItemWidth(100.0f);
				ImGui::DragInt("最大同時数", &MaxInspectorWindows, 1.0f, 1, 20);
				MaxInspectorWindows = FMath::Max(1, MaxInspectorWindows);
				ImGui::EndMenu();
			}
			ImGui::EndMenuBar();
		}

		if (bExpanded)
		{
			const bool bDeleted = DrawDefaultInfo(Actor);
			if (bDeleted)
			{
				bOpen = false;
			}
			else if (IImGuiInspectable* Inspectable = Cast<IImGuiInspectable>(Actor))
			{
				Inspectable->DrawImGuiInspector();
			}
		}
		ImGui::End();

		Entry.bOpen = bOpen;
	}

	// 無効エントリを後ろから削除
	for (int32 i = InspectorEntries.Num() - 1; i >= 0; --i)
	{
		if (!InspectorEntries[i].Actor.IsValid() || !InspectorEntries[i].bOpen)
		{
			InspectorEntries.RemoveAt(i);
		}
	}
}

bool FImGuiInspector::DrawDefaultInfo(AActor* Actor)
{
	if (!ImGui::CollapsingHeader("基本情報", ImGuiTreeNodeFlags_DefaultOpen)) return false;

	ImGui::Text("名前: %s", TCHAR_TO_UTF8(*Actor->GetActorNameOrLabel()));
	ImGui::Text("クラス: %s", TCHAR_TO_UTF8(*Actor->GetClass()->GetName()));

	ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.7f, 0.1f, 0.1f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.2f, 0.2f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.5f, 0.0f, 0.0f, 1.0f));
	const bool bDeletePressed = ImGui::Button("Delete");
	ImGui::PopStyleColor(3);

	if (bDeletePressed)
	{
		Actor->Destroy();
		return true;
	}

	FVector Location = Actor->GetActorLocation();
	FRotator Rotation = Actor->GetActorRotation();
	FVector Scale = Actor->GetActorScale3D();

	FString TransformText = FString::Printf(
		TEXT("Location: X=%.1f Y=%.1f Z=%.1f\nRotation: Pitch=%.1f Yaw=%.1f Roll=%.1f\nScale: X=%.2f Y=%.2f Z=%.2f"),
		Location.X, Location.Y, Location.Z, Rotation.Pitch, Rotation.Yaw, Rotation.Roll, Scale.X, Scale.Y, Scale.Z);

	ImGui::SeparatorText("Transform");

	if (ImGui::Button("コピー"))
	{
		FPlatformApplicationMisc::ClipboardCopy(*TransformText);
	}

	float LocArr[3]   = { Location.X, Location.Y, Location.Z };
	float RotArr[3]   = { Rotation.Pitch, Rotation.Yaw, Rotation.Roll };
	float ScaleArr[3] = { Scale.X, Scale.Y, Scale.Z };

	ImGui::InputFloat3("位置", LocArr, "%.1f", ImGuiInputTextFlags_ReadOnly);
	ImGui::InputFloat3("回転 (P/Y/R)", RotArr, "%.1f", ImGuiInputTextFlags_ReadOnly);
	ImGui::InputFloat3("スケール", ScaleArr, "%.2f", ImGuiInputTextFlags_ReadOnly);

	return false;
}

AActor* FImGuiInspector::LineTraceUnderCursor(UWorld* World) const
{
	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC) return nullptr;

	float MouseX = 0.0f, MouseY = 0.0f;
	if (!PC->GetMousePosition(MouseX, MouseY)) return nullptr;

	FVector WorldLocation, WorldDirection;
	if (!PC->DeprojectScreenPositionToWorld(MouseX, MouseY, WorldLocation, WorldDirection)) return nullptr;

	FHitResult Hit;
	const FVector TraceEnd = WorldLocation + WorldDirection * 100000.0f;
	if (World->LineTraceSingleByChannel(Hit, WorldLocation, TraceEnd, ECC_Visibility, {}))
	{
		return Hit.GetActor();
	}
	return nullptr;
}
