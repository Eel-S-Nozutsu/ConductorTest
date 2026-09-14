// Copyright (c) 2025, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.

#include "ImGui/ImGuiDebugSubsystem.h"
#include "ImGui/ImGuiCommon.h"
#include "ImGui/System/ConductorWindow.h"
#include "Core/ConductorTestGameSettings.h"

#include "ImGuiModule.h"
#include "Kismet/GameplayStatics.h"

#if WITH_EDITOR
// FEditorDelegates。共有PCH経由で通ってしまうので明示する
#include "Editor.h"
#endif

void UImGuiDebugSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

#if !UE_BUILD_SHIPPING
	RegisterWindow(MakeShared<ConductorWindow>());
#endif

	LoadImGuiState();

#if WITH_EDITOR
	FEditorDelegates::EndPIE.AddUObject(this, &UImGuiDebugSubsystem::OnEndPIE);
#endif
}

void UImGuiDebugSubsystem::Deinitialize()
{
#if WITH_EDITOR
	FEditorDelegates::EndPIE.RemoveAll(this);
#endif
	SaveImGuiState();

	Super::Deinitialize();
}

void UImGuiDebugSubsystem::Tick(float DeltaTime)
{
	// CDO(デフォルトオブジェクト)は無視
	if (HasAnyFlags(RF_ClassDefaultObject)) return;

	bool bImGuiInputOn = FImGuiModule::Get().GetProperties().IsInputEnabled();
	if (bImGuiInputOn)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(0, 0.1f, FColor::Red, TEXT("ImGui入力優先中[Key:I]"), true, FVector2D(2.0f));
		}
	}

	if (!bShowImGui)
	{
		return;
	}

#if UE_WITH_IMGUI
	if (ImGui::BeginMainMenuBar())
	{
		// ウィンドウの選択検知
		for (int i = 0; i < static_cast<int>(EImGuiMenuCategory::Max); ++i)
		{
			EImGuiMenuCategory Category = static_cast<EImGuiMenuCategory>(i);

			FString CategoryName = UEnum::GetValueAsString(Category);
			CategoryName.RemoveFromStart("EImGuiMenuCategory::"); // 短くする

			if (ImGui::BeginMenu(TCHAR_TO_ANSI(*CategoryName)))
			{
				for (const TSharedPtr<ImGuiWindowBase>& Window : Windows)
				{
					if (Window && Window->GetCategory() == Category)
					{
						if (ImGui::MenuItem(Window->GetWindowName(), nullptr, Window->bIsOpen))
						{
							Window->bIsOpen = !Window->bIsOpen;
						}
					}
				}

				// Systemメニューには例外で、ImGuiDemoWindowを追加する
				if (Category == EImGuiMenuCategory::System)
				{
					ImGui::MenuItem("ImGuiDemo", nullptr, &bShowDemoWindow);
				}

				ImGui::EndMenu();
			}
		}

		// FPSの表示と時間の加速機能
		{
			float Fps	   = 1.0f / FMath::Max(DeltaTime, KINDA_SMALL_NUMBER);
			FString FpsStr = FString::Printf(TEXT("FPS : %.2f"), Fps);

			ImGuiStyle& Style = ImGui::GetStyle();

			float Margin  = 10.0f;
			float BtnPad  = Style.FramePadding.x * 2;
			float ItemPad = Style.ItemSpacing.x;

			float Width = 0.0f;
			Width += ImGui::CalcTextSize("FPS : 999.99").x + Margin;
			Width += ImGui::CalcTextSize("0.2倍速").x + BtnPad + ItemPad; // 追加分
			Width += ImGui::CalcTextSize("1.5倍速").x + BtnPad + ItemPad;
			Width += ImGui::CalcTextSize("3.0倍速").x + BtnPad;

			// PIEの開始直後や終了時、専用サーバーではビューポートが無い
			if (GEngine->GameViewport)
			{
				FVector2D ViewportSize;
				GEngine->GameViewport->GetViewportSize(ViewportSize);

				ImGui::SetCursorPosX(ViewportSize.X - Width);
			}

			const ImU32 COLOR_NORMAL = IM_COL32(255, 255, 255, 255);
			const ImU32 COLOR_ACTIVE = IM_COL32(255, 0, 0, 255);

			const bool active0 = (SelectedButton == 0); // 0.2倍速用
			const bool active1 = (SelectedButton == 1); // 1.5倍速用
			const bool active2 = (SelectedButton == 2); // 3.0倍速用

			bool bSpeedChanged = false;

			// 0.2倍速ボタン
			if (active0) ImGui::PushStyleColor(ImGuiCol_Text, COLOR_ACTIVE);
			if (ImGui::Button("0.2倍速"))
			{
				SelectedButton = (active0 ? -1 : 0);
				bSpeedChanged  = true;
			}
			if (active0) ImGui::PopStyleColor();

			ImGui::SameLine();

			// 1.5倍速ボタン
			if (active1) ImGui::PushStyleColor(ImGuiCol_Text, COLOR_ACTIVE);
			if (ImGui::Button("1.5倍速"))
			{
				SelectedButton = (active1 ? -1 : 1);
				bSpeedChanged  = true;
			}
			if (active1) ImGui::PopStyleColor();

			ImGui::SameLine();

			// 3.0倍速ボタン
			if (active2) ImGui::PushStyleColor(ImGuiCol_Text, COLOR_ACTIVE);
			if (ImGui::Button("3.0倍速"))
			{
				SelectedButton = (active2 ? -1 : 2);
				bSpeedChanged  = true;
			}
			if (active2) ImGui::PopStyleColor();

			if (bSpeedChanged && GetWorld())
			{
				float NewDilation = 1.0f;
				if (SelectedButton == 0) NewDilation = 0.2f;
				else if (SelectedButton == 1) NewDilation = 1.5f;
				else if (SelectedButton == 2) NewDilation = 3.0f;

				GetWorld()->GetWorldSettings()->SetTimeDilation(NewDilation);
			}

			ImGui::Text("%s", TCHAR_TO_UTF8(*FpsStr));
		}

		ImGui::EndMainMenuBar();
	}

	// ウィンドウはメニューバーの外で描く。内側で ImGui::Begin するとウィンドウが
	// メニューバーのフレームに入れ子になり、フォーカスと前後関係が意図どおりにならない
	for (const TSharedPtr<ImGuiWindowBase>& Window : Windows)
	{
		if (Window.IsValid())
		{
			Window->Tick();
		}
	}

	if (bShowDemoWindow)
	{
		ImGui::ShowDemoWindow(&bShowDemoWindow);
	}
#endif // WITH_IMGUI
}

TStatId UImGuiDebugSubsystem::GetStatId() const /* override */
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UImGuiDebugSubsystem, STATGROUP_Tickables);
}

#if WITH_EDITOR
void UImGuiDebugSubsystem::OnEndPIE(const bool bIsSimulating)
{
	for (const TSharedPtr<ImGuiWindowBase>& Window : Windows)
	{
		if (Window.IsValid() && Window->bIsOpen)
		{
			Window->OnClose();
		}
	}
}
#endif

void UImGuiDebugSubsystem::RegisterWindow(TSharedPtr<ImGuiWindowBase> Window)
{
	if (!Windows.Contains(Window))
	{
		Window->GameInstance = GetGameInstance();
		Windows.Add(Window);
	}
}

void UImGuiDebugSubsystem::SaveImGuiState()
{
	UConductorTestGameSettings* Settings = UConductorTestGameSettings::Get();
	check(Settings);
	Settings->ImGuiWindowOpened.Reset();

	for (const TSharedPtr<ImGuiWindowBase>& Window : Windows)
	{
		if (Window.IsValid())
		{
			Settings->ImGuiWindowOpened.Add(
				Window->GetWindowName(), Window->bIsOpen);
		}
	}
}

void UImGuiDebugSubsystem::LoadImGuiState()
{
	UConductorTestGameSettings* Settings = UConductorTestGameSettings::Get();
	check(Settings);

	for (const TSharedPtr<ImGuiWindowBase>& Window : Windows)
	{
		if (!Window.IsValid()) continue;
		bool* bOpen = Settings->ImGuiWindowOpened.Find(Window->GetWindowName());
		if (bOpen)
		{
			Window->bIsOpen = *bOpen;
		}
	}
}
