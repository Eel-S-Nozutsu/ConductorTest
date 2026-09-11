// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.

#pragma once

namespace PlayerAnimTags
{
	// Montage
	const FName CHARGE_01_ST			= TEXT( "ChargeStart_01" );
	const FName CHARGE_01_LP_V1			= TEXT( "ChargeLoop_01" );
	const FName CHARGE_01_ATK			= TEXT( "ChargeAttack_01" );

	const FName CHARGE_02_ST			= TEXT( "ChargeStart_02" );
	const FName CHARGE_02_LP_V1			= TEXT( "ChargeLoop_02" );
	const FName CHARGE_02_ATK			= TEXT( "ChargeAttack_02" );

	const FName CHARGE_03_ST			= TEXT( "ChargeStart_03" );
	const FName CHARGE_03_LP_V1			= TEXT( "ChargeLoop_03" );
	const FName CHARGE_03_ATK			= TEXT( "ChargeAttack_03" );

	const FName CHARGE_04_ST			= TEXT( "ChargeStart_04" );
	const FName CHARGE_04_LP_V1			= TEXT( "ChargeLoop_04" );
	const FName CHARGE_04_ATK			= TEXT( "ChargeAttack_04" );

	const FName CHARGE_01_DASH_ST		= TEXT( "ChargeDashStart_01" );
	const FName CHARGE_02_DASH_ST		= TEXT( "ChargeDashStart_02" );
	const FName CHARGE_03_DASH_ST		= TEXT( "ChargeDashStart_03" );
	const FName CHARGE_04_DASH_ST		= TEXT( "ChargeDashStart_04" );
	const FName CHARGE_DASH_ED			= TEXT( "ChargeDashEnd" );

	const FName AIRCHARGE_ST			= TEXT( "AirChargeStart" );	// 空中チャージ溜め ST
	const FName AIRCHARGE_LP			= TEXT( "AirChargeLoop" );	// 空中チャージ溜め LP

	// 空中通常攻撃（振り下ろし）。非チャージ時の空中攻撃ボタンで発動する縦ダイブ。
	// （ST=ホバー→LP=真下プランジ→ED=着地）。旧「空中チャージ攻撃(AirChargeAttack*)」の振り下ろしアセットをこちらへ移す
	const FName AIR_ATK_ST				= TEXT( "AirAttackStart" );
	const FName AIR_ATK_LP				= TEXT( "AirAttackLoop" );
	const FName AIR_ATK_ED				= TEXT( "AirAttackEnd" );

	// 空中チャージ攻撃。チャージ中の空中攻撃ボタンで発動する横ダイブ。
	// （ST=開始→LP=滞空ダイブ→ED=着地）。旧「空中チャージダッシュ」の空中チャージ攻撃アセットをこちらへ移す
	const FName AIRCHARGE_ATK_ST		= TEXT( "AirChargeAttackStart" );
	const FName AIRCHARGE_ATK_LP		= TEXT( "AirChargeAttackLoop" );
	const FName AIRCHARGE_ATK_ED		= TEXT( "AirChargeAttackEnd" );

	// 空中チャージダッシュ（ダッシュ本体）。ST 終了でそのまま落下へ。ED は持たない（着地は JUMP_ED）
	const FName AIRCHARGE_DASH_ST		= TEXT( "AirChargeDashStart" );
	// 滑空開始（空中でジャンプ長押し）。再生し切ってから AirGlideLoop へ
	const FName AIR_GLIDE_ST			= TEXT( "AirGlideStart" );
	// 滑空ループ。ジャンプボタンを離す／GlideMaxDuration／着地で終了
	const FName AIR_GLIDE_LP			= TEXT( "AirGlideLoop" );

	// 地上の弱攻撃コンボ。何段まで繋がるかは PlayerParamData の MaxLightComboCount
	const FName LIGHT_ATK_01			= TEXT( "LightAttack01" );
	const FName LIGHT_ATK_02			= TEXT( "LightAttack02" );
	const FName LIGHT_ATK_03			= TEXT( "LightAttack03" );
	const FName LIGHT_ATK_04			= TEXT( "LightAttack04" );

	// 神技・一閃（AS_god_slash_*）。実モンタージュは AnimMontageListDataAsset 側で割り当てる
	const FName GOD_SLASH_ST			= TEXT( "GodSlashStart" );		// ロックオン開始
	const FName GOD_SLASH_LP			= TEXT( "GodSlashLoop" );		// ロックオンループ
	const FName GOD_SLASH_ATK_ST		= TEXT( "GodSlashAttackStart" );	// 攻撃開始
	const FName GOD_SLASH_ATK_LP		= TEXT( "GodSlashAttackLoop" );	// 攻撃ループ
	const FName GOD_SLASH_ATK_ED		= TEXT( "GodSlashAttackEnd" );	// 攻撃終了

	const FName STEP_F					= TEXT( "StepForward" );
	const FName STEP_B					= TEXT( "StepBack" );
	const FName STEP_L					= TEXT( "StepLeft" );
	const FName STEP_R					= TEXT( "StepRight" );

	const FName DASH					= TEXT( "DashLoop" );

	// のけぞり（前から）。ダメージ強度に応じて 小／中／大 を使い分ける（AS_stagger_s/m/l_f）
	const FName STAGGER_S_F				= TEXT( "StaggerSmallForward" );
	const FName STAGGER_M_F				= TEXT( "StaggerMediumForward" );
	const FName STAGGER_L_F				= TEXT( "StaggerLargeForward" );
	const FName BLOWDAMAGE_F_ST			= TEXT( "BlowDamageForwardStart" );
	const FName BLOWDAMAGE_F_LP			= TEXT( "BlowDamageForwardLoop" );
	const FName BLOWDAMAGE_F_ED			= TEXT( "BlowDamageForwardEnd" );
	const FName RECOVERY_DOWN_U			= TEXT( "RecoveryDownUp" );

	const FName JUMP_ST					= TEXT( "JumpStart" );
	const FName JUMP_LP					= TEXT( "JumpLoop" );
	const FName JUMP_ED					= TEXT( "JumpEnd" );

	const FName CHARGE_JUMP_ST			= TEXT( "ChargeJumpStart" );
	const FName CHARGE_JUMP_LP			= TEXT( "ChargeJumpLoop" );
	const FName CHARGE_JUMP_ED			= TEXT( "ChargeJumpEnd" );

	// チャージホップ（チャージ幅跳び）専用モーション。チャージダッシュ中のジャンプ割り込みで
	// ギア壱固定で出るジャンプ（bEnableChargeHop）専用。従来は CHARGE_JUMP_ST/LP を流用していた
	const FName CHARGE_HOP_ST			= TEXT( "ChargeHopStart" );
	const FName CHARGE_HOP_LP			= TEXT( "ChargeHopLoop" );
	// 着地時に移動入力が無く、ダッシュ再開（ResumeChargeDashAfterLanding）を行わない場合の着地専用モーション
	const FName CHARGE_HOP_ED			= TEXT( "ChargeHopEnd" );

	const FName RUN_TURN				= TEXT( "RunTurn" );
	const FName DASH_TURN				= TEXT( "DashTurn" );
	// チャージダッシュ中のターン。発火は ChargeActionPlayerModule_V2、Turn フラグ管理は走り／ダッシュと共通
	const FName CHARGE_DASH_TURN		= TEXT( "ChargeDashTurn" );

	const FName CHARGE_BRAKE_ST			= TEXT( "ChargeBrakeStart" );
	const FName CHARGE_BRAKE_LP			= TEXT( "ChargeBrakeLoop" );
	const FName CHARGE_BRAKE_ED			= TEXT( "ChargeBrakeEnd" );

	const FName FALL_DAMAGE				= TEXT( "FallDamage" );

	const FName DIE						= TEXT( "Die" );
}
