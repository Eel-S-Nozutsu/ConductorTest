// Copyright (c) 2026, Syunsuke Nodutsu EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/Integration/ImGui/ImGuiLogWindow.h"

FImGuiLogWindow::FImGuiLogWindow()
{
	Clear();
}

void FImGuiLogWindow::Clear()
{
	Buffer.clear();
	LineOffsets.Empty();
	LineOffsets.Add(0);
}

void FImGuiLogWindow::AddLog(const FString& Message)
{
	int32 SizeOld = Buffer.size();

	// タイムスタンプを付与してUTF-8変換
	FString FinalLine = FString::Printf(TEXT("[%s] %s\n"),
		*FDateTime::Now().ToString(TEXT("%H:%M:%S")), *Message);

	Buffer.append(TCHAR_TO_UTF8(*FinalLine));

	for (int32 SizeNew = Buffer.size(); SizeOld < SizeNew; SizeOld++)
	{
		if (Buffer[SizeOld] == '\n')
		{
			LineOffsets.Add(SizeOld + 1);
		}
	}
}

void FImGuiLogWindow::Draw(const char* Title, bool* p_open)
{
	if (!ImGui::Begin(Title, p_open))
	{
		ImGui::End();
		return;
	}

	DrawContents();
	
	ImGui::End();
}

void FImGuiLogWindow::DrawContents()
{
	// オプションメニュー
	if (ImGui::BeginPopup("Options"))
	{
		ImGui::Checkbox("Auto-scroll", &AutoScroll);
		ImGui::EndPopup();
	}

	if (ImGui::Button("Options")) ImGui::OpenPopup("Options");
	ImGui::SameLine();
	if (ImGui::Button("Clear")) Clear();
	ImGui::SameLine();
	if (ImGui::Button("Copy")) ImGui::LogToClipboard();
	ImGui::SameLine();
	Filter.Draw("Filter", -100.0f);

	ImGui::Separator();
	ImGui::BeginChild("Scrolling", ImVec2(0, 0), false,
		ImGuiWindowFlags_HorizontalScrollbar);

	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

	const char* BufferStart = Buffer.begin();
	//const char* BufferEnd = Buffer.end();

	if (Filter.IsActive())
	{
		// フィルタリング有効時
		for (int32 i = 0; i < LineOffsets.Num() - 1; i++)
		{
			const char* Start = BufferStart + LineOffsets[i];
			const char* End = BufferStart + LineOffsets[i + 1] - 1;
			if (Filter.PassFilter(Start, End))
			{
				ImGui::TextUnformatted(Start, End);
			}
		}
	}
	else
	{
		// 標準表示
		ImGuiListClipper Clipper;
		Clipper.Begin(LineOffsets.Num() - 1);
		while (Clipper.Step())
		{
			for (int32 i = Clipper.DisplayStart; i < Clipper.DisplayEnd; i++)
			{
				const char* line_start = BufferStart + LineOffsets[i];
				const char* line_end = BufferStart + LineOffsets[i + 1] - 1;
				ImGui::TextUnformatted(line_start, line_end);
			}
		}
		Clipper.End();
	}
	ImGui::PopStyleVar();

	if (AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
	{
		ImGui::SetScrollHereY(1.0f);
	}

	ImGui::EndChild();
}
