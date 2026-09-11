// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/Actors/Character/TideCharacter.h"
#include "PRJ_TIDE_P0/Components/Combat/StatusComponent.h"
#include "PRJ_TIDE_P0/Components/Combat/DamageSystemComponent.h"
#include "PRJ_TIDE_P0/Components/Combat/HitReactionComponent.h"
#include "PRJ_TIDE_P0/Components/State/StateTagComponent.h"
#include "PRJ_TIDE_P0/Components/Combat/HazardDotComponent.h"
#include "PRJ_TIDE_P0/Data/Character/TideCharacterDataAsset.h"
#include "PRJ_TIDE_P0/Data/Tags/TideGameplayTags.h"

#include "imgui.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PrimitiveComponent.h"

ATideCharacter::ATideCharacter()
{
	StatusComponent = CreateDefaultSubobject<UStatusComponent>(TEXT("StatusComponent"));
	DamageSystem = CreateDefaultSubobject<UDamageSystemComponent>(TEXT("DamageSystemComponent"));
	HitReactionComponent = CreateDefaultSubobject<UHitReactionComponent>(TEXT("HitReactionComponent"));
	StateTagComponent = CreateDefaultSubobject<UStateTagComponent>(TEXT("StateTagComponent"));
	HazardDotComponent = CreateDefaultSubobject<UHazardDotComponent>(TEXT("HazardDotComponent"));

	// デカール受け取りの一括OFFはPostInitializeComponentsで行う
	// コンストラクタ時点ではBP追加コンポーネント (装備品/HaloComponent等)
	// が未生成のため
}

void ATideCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// キャラに属する全メッシュへ「デカールを受け取らない」を一括適用する
	// 本体・ビジュアル確認用の子メッシュ・装備品・HaloComponent等を個別設定する手間と
	// 設定漏れを防ぐ。地面へ投影する視野デバッグ扇・着弾予告デカールが体/装備に滲むのを防ぐ
	SetReceivesDecalsOnAllMeshes(false);
}

void ATideCharacter::SetReceivesDecalsOnAllMeshes(bool bReceives)
{
	TArray<UPrimitiveComponent*> Primitives;
	GetComponents<UPrimitiveComponent>(Primitives);
	for (UPrimitiveComponent* Prim : Primitives)
	{
		if (Prim)
		{
			Prim->SetReceivesDecals(bReceives);
		}
	}
}

void ATideCharacter::BeginPlay()
{
	Super::BeginPlay();

	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	if (CharacterData)
	{
		StatusComponent->InitializeFromData(CharacterData->StatusSettings);
		HitReactionComponent->InitializeFromData(CharacterData->HitReactionSettings);
	}

	DamageSystem->Initialize(StatusComponent);
	HitReactionComponent->Initialize(DamageSystem, StatusComponent);
}

EDamageResult ATideCharacter::ReceiveDamage(const FDamageInfo& DamageInfo)
{
	if (!CanBeDamaged()) return EDamageResult::Immune;
	DamageSystem->ProcessDamage(DamageInfo);
	return EDamageResult::Hit;
}

bool ATideCharacter::CanBeDamaged() const
{
	if (!StatusComponent) return false;
	return !StatusComponent->IsDead();
}

void ATideCharacter::DrawImGuiInspector()
{
	DrawImGuiStatusSection();
	DrawImGuiStateTagsSection();
	DrawImGuiAnimationSection();
}

void ATideCharacter::DrawImGuiStatusSection()
{
	if (!ImGui::CollapsingHeader("Status")) return;
	if (!StatusComponent) return;

	const float CurrentHP = StatusComponent->GetCurrentHP();
	const float MaxHP = StatusComponent->GetMaxHP();
	const float HPRatio = MaxHP > 0.0f ? CurrentHP / MaxHP : 0.0f;

	ImGui::ProgressBar(HPRatio, ImVec2(-1.0f, 0.0f));
	ImGui::Text("HP: %.0f / %.0f", CurrentHP, MaxHP);

	// HPをMaxHPの割合で増減するデバッグボタン群
	auto HealButton = [this, MaxHP](const char* Label, float Ratio)
	{
		if (ImGui::Button(Label))
		{
			StatusComponent->ModifyHP(MaxHP * Ratio);
		}
	};
	auto DamageButton = [this, MaxHP](const char* Label, float Ratio)
	{
		if (ImGui::Button(Label))
		{
			StatusComponent->ModifyHP(-MaxHP * Ratio);
		}
	};

	ImGui::TextDisabled("Damage:");
	ImGui::SameLine();
	DamageButton("-10%##Damage", 0.1f);
	ImGui::SameLine();
	DamageButton("-50%##Damage", 0.5f);
	ImGui::SameLine();
	DamageButton("-100%##Damage", 1.0f);

	ImGui::TextDisabled("Heal:");
	ImGui::SameLine();
	HealButton("+10%##Heal", 0.1f);
	ImGui::SameLine();
	HealButton("+50%##Heal", 0.5f);
	ImGui::SameLine();
	HealButton("+100%##Heal", 1.0f);


	if (StatusComponent->IsDead())
	{
		ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "DEAD");

		ImGui::SameLine();

		// 死亡時のみ蘇生ボタンを表示
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.4f, 0.1f, 1.0f));

		if (ImGui::Button("Debug Revive"))
		{
			StatusComponent->ResetStatus();
		}

		ImGui::PopStyleColor(3);
	}
}

void ATideCharacter::DrawImGuiStateTagsSection()
{
	if (!ImGui::CollapsingHeader("Active State Tags")) return;

	if (!StateTagComponent)
	{
		ImGui::TextDisabled("  StateTagComponent unavailable");
		return;
	}

	const FGameplayTagContainer& ActiveTags = StateTagComponent->GetActiveStateTags();

	if (ActiveTags.IsEmpty())
	{
		ImGui::TextDisabled("  None");
	}
	else
	{
		TArray<FGameplayTag> TagArray;
		ActiveTags.GetGameplayTagArray(TagArray);

		for (const FGameplayTag& Tag : TagArray)
		{
			ImGui::BulletText("%s", TCHAR_TO_UTF8(*Tag.ToString()));
		}
	}
}

void ATideCharacter::DrawImGuiAnimationSection()
{
	if (!ImGui::CollapsingHeader("Animation")) return;

	// 終了したモンタージュの履歴スタック(PlayerWindowと同じ方式)
	struct FMontageHistory { FString Name; float EndTime; };
	static TArray<FMontageHistory> FinishedHistory;
	static FString PrevPlayingName = TEXT("");

	const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	const float LifeTime = 3.0f; // 履歴の保持秒数

	const UAnimInstance* AnimInst = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;

	ImGui::SeparatorText("Current");
	if (!AnimInst)
	{
		ImGui::TextDisabled("  AnimInstance unavailable");
	}
	else if (const UAnimMontage* Montage = AnimInst->GetCurrentActiveMontage())
	{
		const FString Name   = Montage->GetName();
		const float Pos      = AnimInst->Montage_GetPosition(Montage);
		const float Length   = Montage->GetPlayLength();
		const float Progress = (Length > KINDA_SMALL_NUMBER) ? (Pos / Length) : 0.0f;

		const FString Label = FString::Printf(TEXT("%.2fs / %.2fs"), Pos, Length);
		ImGui::Text("  Montage: %s", TCHAR_TO_UTF8(*Name));
		ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.3f, 0.7f, 0.4f, 1.0f));
		ImGui::ProgressBar(Progress, ImVec2(-1.0f, 0.0f), TCHAR_TO_UTF8(*Label));
		ImGui::PopStyleColor();

		// 再生中のものが切り替わったら、古い方を履歴へ送る
		if (!PrevPlayingName.IsEmpty() && PrevPlayingName != Name)
		{
			FinishedHistory.Insert({ PrevPlayingName, CurrentTime }, 0);
		}
		PrevPlayingName = Name;
	}
	else
	{
		ImGui::TextDisabled("  No montage (locomotion)");
		// 再生中だったものが停止したなら履歴へ
		if (!PrevPlayingName.IsEmpty())
		{
			FinishedHistory.Insert({ PrevPlayingName, CurrentTime }, 0);
			PrevPlayingName = TEXT("");
		}
	}

	// 古い履歴を削除(LifeTime以上経過したもの)
	for (int32 i = FinishedHistory.Num() - 1; i >= 0; i--)
	{
		if (CurrentTime - FinishedHistory[i].EndTime > LifeTime)
		{
			FinishedHistory.RemoveAt(i);
		}
	}

	ImGui::SeparatorText("Finished History (3s)");
	if (FinishedHistory.IsEmpty())
	{
		ImGui::TextDisabled("  None");
	}
	else
	{
		for (const FMontageHistory& Item : FinishedHistory)
		{
			const float Elapsed = CurrentTime - Item.EndTime;
			ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "  %s (End: %.1fs ago)", TCHAR_TO_UTF8(*Item.Name), Elapsed);
		}
	}
}

void ATideCharacter::AddStateTag(const FGameplayTag& Tag)
{
	if (!StateTagComponent) return;
	StateTagComponent->AddStateTag(Tag);
}

void ATideCharacter::RemoveStateTag(const FGameplayTag& Tag)
{
	if (!StateTagComponent) return;
	StateTagComponent->RemoveStateTag(Tag);
}

bool ATideCharacter::HasStateTag(const FGameplayTag& Tag) const
{
	if (!StateTagComponent) return false;
	return StateTagComponent->HasStateTag(Tag);
}

bool ATideCharacter::HasAnyStateTag(const FGameplayTagContainer& GameplayTags) const
{
	if (!StateTagComponent || GameplayTags.IsEmpty()) return false;
	for (const FGameplayTag& MyTag : GameplayTags)
	{
		if (StateTagComponent->HasStateTag(MyTag)) return true;
	}
	return false;
}
