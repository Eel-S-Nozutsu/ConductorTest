// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.

#pragma once

// 神鳥（ATideGodBird）のモンタージュラベル。PlayerAnimTags と同じ流儀で、ここで定義した FName を鍵に
// BirdAnimMontageDataAsset から実モンタージュを引く（スケルトンが別物なのでラベルも独立して定義する）。
// ※ラベル文字列は DataAsset の Entries キーと一致させること。行末のコメントは割り当てる想定の実モンタージュ
namespace GodBirdAnimTags
{
	// 神技「戯れ」（ActivateFrolicAttack 中に差し込む）
	const FName FROLIC_ST			= TEXT( "FrolicStart" );	// AS_bird_caprice_st : 戯れ 開始
	const FName FROLIC_LP			= TEXT( "FrolicLoop" );		// AS_bird_caprice_lp : 戯れ 飛行ループ

	// 神技「導き」（ActivateGuidanceCharge 中に差し込む）
	const FName GUIDANCE_ST			= TEXT( "GuidanceStart" );	// AS_bird_guidance_st : 導き 開始
	const FName GUIDANCE_LP			= TEXT( "GuidanceLoop" );	// AS_bird_guidance_lp : 導き 飛行ループ
}
