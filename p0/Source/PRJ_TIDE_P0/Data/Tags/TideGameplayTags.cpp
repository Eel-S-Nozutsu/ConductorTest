// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#include "PRJ_TIDE_P0/Data/Tags/TideGameplayTags.h"

// -----------------------------------------
// Audio
// -----------------------------------------

// グループ
UE_DEFINE_GAMEPLAY_TAG(TAG_Sound_Music, "Sound.Music");
UE_DEFINE_GAMEPLAY_TAG(TAG_Sound_Ambience, "Sound.Ambience");
UE_DEFINE_GAMEPLAY_TAG(TAG_Sound_GameSE, "Sound.GameSE");
UE_DEFINE_GAMEPLAY_TAG(TAG_Sound_UI, "Sound.UI");
UE_DEFINE_GAMEPLAY_TAG(TAG_Sound_Voice, "Sound.Voice");

// Music
UE_DEFINE_GAMEPLAY_TAG(TAG_Sound_Music_Test, "Sound.Music.Test");

// GameSE
UE_DEFINE_GAMEPLAY_TAG(TAG_Sound_GameSE_TrainCrossingSignal, "Sound.GameSE.TrainCrossingSignal");

// -----------------------------------------
// Option
// -----------------------------------------

UE_DEFINE_GAMEPLAY_TAG(TAG_Settings_Camera_CameraSensitivityLR, "Settings.Camera.CameraSensitivityLR");
UE_DEFINE_GAMEPLAY_TAG(TAG_Settings_Camera_CameraSensitivityUD, "Settings.Camera.CameraSensitivityUD");
UE_DEFINE_GAMEPLAY_TAG(TAG_Settings_Camera_bCameraInvertX, "Settings.Camera.bCameraInvertX");
UE_DEFINE_GAMEPLAY_TAG(TAG_Settings_Camera_bCameraInvertY, "Settings.Camera.bCameraInvertY");

UE_DEFINE_GAMEPLAY_TAG(TAG_Settings_Audio_MasterVolume, "Settings.Audio.MasterVolume");
UE_DEFINE_GAMEPLAY_TAG(TAG_Settings_Audio_BGMVolume, "Settings.Audio.BGMVolume");
UE_DEFINE_GAMEPLAY_TAG(TAG_Settings_Audio_AmbienceVolume, "Settings.Audio.AmbienceVolume");
UE_DEFINE_GAMEPLAY_TAG(TAG_Settings_Audio_GameSEVolume, "Settings.Audio.GameSEVolume");
UE_DEFINE_GAMEPLAY_TAG(TAG_Settings_Audio_MenuSEVolume, "Settings.Audio.MenuSEVolume");
UE_DEFINE_GAMEPLAY_TAG(TAG_Settings_Audio_VoiceVolume, "Settings.Audio.VoiceVolume");

UE_DEFINE_GAMEPLAY_TAG(TAG_Settings_Graphics_bMotionBlur, "Settings.Graphics.bMotionBlur");
UE_DEFINE_GAMEPLAY_TAG(TAG_Settings_Graphics_Brightness, "Settings.Graphics.Brightness");

UE_DEFINE_GAMEPLAY_TAG(TAG_Settings_Language_LanguageVoice, "Settings.Language.LanguageVoice");
UE_DEFINE_GAMEPLAY_TAG(TAG_Settings_Language_LanguageText, "Settings.Language.LanguageText");

// -----------------------------------------
// HitReaction
// -----------------------------------------

UE_DEFINE_GAMEPLAY_TAG(TAG_HitReaction_Knockback_S, "HitReaction.Knockback.S");
UE_DEFINE_GAMEPLAY_TAG(TAG_HitReaction_Knockback_M, "HitReaction.Knockback.M");
UE_DEFINE_GAMEPLAY_TAG(TAG_HitReaction_Knockback_L, "HitReaction.Knockback.L");
UE_DEFINE_GAMEPLAY_TAG(TAG_HitReaction_Knockback_S_Back, "HitReaction.Knockback.S.Back");
UE_DEFINE_GAMEPLAY_TAG(TAG_HitReaction_Knockback_M_Back, "HitReaction.Knockback.M.Back");
UE_DEFINE_GAMEPLAY_TAG(TAG_HitReaction_Knockback_L_Back, "HitReaction.Knockback.L.Back");
UE_DEFINE_GAMEPLAY_TAG(TAG_HitReaction_Blowoff, "HitReaction.Blowoff");
UE_DEFINE_GAMEPLAY_TAG(TAG_HitReaction_Blowoff_S, "HitReaction.Blowoff.S");
UE_DEFINE_GAMEPLAY_TAG(TAG_HitReaction_Blowoff_M, "HitReaction.Blowoff.M");
UE_DEFINE_GAMEPLAY_TAG(TAG_HitReaction_Blowoff_L, "HitReaction.Blowoff.L");
UE_DEFINE_GAMEPLAY_TAG(TAG_HitReaction_SmashDown, "HitReaction.SmashDown");
UE_DEFINE_GAMEPLAY_TAG(TAG_HitReaction_Blowup, "HitReaction.Blowup");
UE_DEFINE_GAMEPLAY_TAG(TAG_HitReaction_GodSkill_Iai, "HitReaction.GodSkill.Iai");
UE_DEFINE_GAMEPLAY_TAG(TAG_HitReaction_NoReaction, "HitReaction.NoReaction");
UE_DEFINE_GAMEPLAY_TAG(TAG_HitReaction_Wind, "HitReaction.Wind");

// -----------------------------------------
// Attack Event
// -----------------------------------------

UE_DEFINE_GAMEPLAY_TAG(TAG_AttackEvent_ChargeStart, "AttackEvent.ChargeStart");
UE_DEFINE_GAMEPLAY_TAG(TAG_AttackEvent_ChargeEnd, "AttackEvent.ChargeEnd");
UE_DEFINE_GAMEPLAY_TAG(TAG_AttackEvent_MissileStart, "AttackEvent.MissileStart");
UE_DEFINE_GAMEPLAY_TAG(TAG_AttackEvent_MissileShot, "AttackEvent.MissileShot");
UE_DEFINE_GAMEPLAY_TAG(TAG_AttackEvent_ProjectileFire, "AttackEvent.ProjectileFire");
UE_DEFINE_GAMEPLAY_TAG(TAG_AttackEvent_StrafeStart, "AttackEvent.StrafeStart");
UE_DEFINE_GAMEPLAY_TAG(TAG_AttackEvent_ShockwallStart, "AttackEvent.ShockwallStart");
UE_DEFINE_GAMEPLAY_TAG(TAG_AttackEvent_ShockwallLand, "AttackEvent.ShockwallLand");
UE_DEFINE_GAMEPLAY_TAG(TAG_AttackEvent_ShockwallMineBurst, "AttackEvent.ShockwallMineBurst");
UE_DEFINE_GAMEPLAY_TAG(TAG_AttackEvent_HaloAttackStart, "AttackEvent.HaloAttackStart");
UE_DEFINE_GAMEPLAY_TAG(TAG_AttackEvent_HaloThrowStart,  "AttackEvent.HaloThrowStart");
UE_DEFINE_GAMEPLAY_TAG(TAG_AttackEvent_HaloReturn,      "AttackEvent.HaloReturn");
UE_DEFINE_GAMEPLAY_TAG(TAG_AttackEvent_HaloStepStart,   "AttackEvent.HaloStepStart");
UE_DEFINE_GAMEPLAY_TAG(TAG_AttackEvent_HaloStepEnd,     "AttackEvent.HaloStepEnd");
UE_DEFINE_GAMEPLAY_TAG(TAG_AttackEvent_GuardOn,         "AttackEvent.GuardOn");
UE_DEFINE_GAMEPLAY_TAG(TAG_AttackEvent_HaloReady,       "AttackEvent.HaloReady");
UE_DEFINE_GAMEPLAY_TAG(TAG_AttackEvent_HaloPlace,       "AttackEvent.HaloPlace");
UE_DEFINE_GAMEPLAY_TAG(TAG_AttackEvent_StompConnect,    "AttackEvent.StompConnect");
UE_DEFINE_GAMEPLAY_TAG(TAG_AttackEvent_StompImpact,     "AttackEvent.StompImpact");
UE_DEFINE_GAMEPLAY_TAG(TAG_AttackEvent_PlatformJumpLaunch, "AttackEvent.PlatformJumpLaunch");

// -----------------------------------------
// Attack Type
// -----------------------------------------
// Player
UE_DEFINE_GAMEPLAY_TAG(TAG_AttackType_Player_LightAttack01, "AttackType.Player.LightAttack01");
UE_DEFINE_GAMEPLAY_TAG(TAG_AttackType_Player_LightAttack02, "AttackType.Player.LightAttack02");
UE_DEFINE_GAMEPLAY_TAG(TAG_AttackType_Player_LightAttack03, "AttackType.Player.LightAttack03");
UE_DEFINE_GAMEPLAY_TAG(TAG_AttackType_Player_LightAttack04, "AttackType.Player.LightAttack04");
UE_DEFINE_GAMEPLAY_TAG(TAG_AttackType_Player_ComboChargeAttack01, "AttackType.Player.ComboChargeAttack01");
UE_DEFINE_GAMEPLAY_TAG(TAG_AttackType_Player_ComboChargeAttack02, "AttackType.Player.ComboChargeAttack02");
UE_DEFINE_GAMEPLAY_TAG(TAG_AttackType_Player_ComboChargeAttack03, "AttackType.Player.ComboChargeAttack03");
UE_DEFINE_GAMEPLAY_TAG(TAG_AttackType_Player_ComboChargeAttack04, "AttackType.Player.ComboChargeAttack04");
UE_DEFINE_GAMEPLAY_TAG(TAG_AttackType_Player_AirChargeAttack, "AttackType.Player.AirChargeAttack");
UE_DEFINE_GAMEPLAY_TAG(TAG_AttackType_Player_ComboChargeDash01, "AttackType.Player.ComboChargeDash01");
UE_DEFINE_GAMEPLAY_TAG(TAG_AttackType_Player_ComboChargeDash02, "AttackType.Player.ComboChargeDash02");
UE_DEFINE_GAMEPLAY_TAG(TAG_AttackType_Player_ComboChargeDash03, "AttackType.Player.ComboChargeDash03");
UE_DEFINE_GAMEPLAY_TAG(TAG_AttackType_Player_ComboChargeDash04, "AttackType.Player.ComboChargeDash04");
UE_DEFINE_GAMEPLAY_TAG(TAG_AttackType_Player_AirChargeDash, "AttackType.Player.AirChargeDash");
UE_DEFINE_GAMEPLAY_TAG(TAG_AttackType_Player_ChargeDrift, "AttackType.Player.ChargeDrift");
UE_DEFINE_GAMEPLAY_TAG(TAG_AttackType_Player_GodActionSlash, "AttackType.Player.GodActionSlash");
UE_DEFINE_GAMEPLAY_TAG(TAG_AttackType_Player_GodActionFrolic, "AttackType.Player.GodActionFrolic");
UE_DEFINE_GAMEPLAY_TAG(TAG_AttackType_Player_GodActionGuidance, "AttackType.Player.GodActionGuidance");
UE_DEFINE_GAMEPLAY_TAG(TAG_AttackType_Player_SlidePassiveTornadoSmall, "AttackType.Player.SlidePassiveTornadoSmall");
UE_DEFINE_GAMEPLAY_TAG(TAG_AttackType_Player_SlidePassiveTornadoLarge, "AttackType.Player.SlidePassiveTornadoLarge");
UE_DEFINE_GAMEPLAY_TAG(TAG_AttackType_Player_SlidePassiveGust, "AttackType.Player.SlidePassiveGust");

// Enemy
UE_DEFINE_GAMEPLAY_TAG(TAG_AttackType_Enemy_DefaultAttack, "AttackType.Enemy.DefaultAttack");

// -----------------------------------------
// 部位
// -----------------------------------------

UE_DEFINE_GAMEPLAY_TAG(TAG_Part_LeftLeg, "Part.LeftLeg");
UE_DEFINE_GAMEPLAY_TAG(TAG_Part_RightLeg, "Part.RightLeg");

// -----------------------------------------
// Input
// -----------------------------------------

UE_DEFINE_GAMEPLAY_TAG(TAG_Input_Default, "Input.Default");
UE_DEFINE_GAMEPLAY_TAG(TAG_Input_UI, "Input.UI");
UE_DEFINE_GAMEPLAY_TAG(TAG_Input_Debug, "Input.Debug");

// -----------------------------------------
// InputCommand
// -----------------------------------------
UE_DEFINE_GAMEPLAY_TAG(TAG_Input_Command_Dodge, "Input.Command.Dodge");
UE_DEFINE_GAMEPLAY_TAG(TAG_Input_Command_ChargeAction, "Input.Command.ChargeAction");
UE_DEFINE_GAMEPLAY_TAG(TAG_Input_Command_Attack, "Input.Command.Attack");
UE_DEFINE_GAMEPLAY_TAG(TAG_Input_Command_Jump, "Input.Command.Jump");

// -----------------------------------------
// State Tags
// -----------------------------------------

// =========================================
// [Common] 全キャラクター共通
// =========================================
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Common_Invincible, "State.Common.Invincible");
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Common_SuperArmor, "State.Common.SuperArmor");
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Common_SuperArmor_Invincible, "State.Common.SuperArmor.Invincible");
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Common_SuperArmor_HaloOpen, "State.Common.SuperArmor.HaloOpen");
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Common_Dead, "State.Common.Dead");

UE_DEFINE_GAMEPLAY_TAG(TAG_State_Common_Disable, "State.Common.Disable");
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Common_Disable_Event, "State.Common.Disable.Event");

UE_DEFINE_GAMEPLAY_TAG(TAG_State_Common_HitReaction, "State.Common.HitReaction");
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Common_HitReaction_Flinching, "State.Common.HitReaction.Flinching");
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Common_HitReaction_BlowBack, "State.Common.HitReaction.BlowBack");
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Common_HitReaction_Downed, "State.Common.HitReaction.Downed");
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Common_HitReaction_Recovering, "State.Common.HitReaction.Recovering");
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Common_HitReaction_Immune,    "State.Common.HitReaction.Immune");

// =========================================
// [Player] プレイヤー専用
// =========================================
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Player_CanAttack, "State.Player.CanAttack");
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Player_CanMove, "State.Player.CanMove");
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Player_CannotDodge, "State.Player.CannotDodge");
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Player_CanCharge, "State.Player.CanCharge");
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Player_CanDash, "State.Player.CanDash");
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Player_Invincible_Dodge, "State.Common.Invincible.PlayerDodge");
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Player_CanCombo, "State.Player.CanCombo");
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Player_CannotLockOn, "State.Player.CannotLockOn");
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Player_MoveCancelable, "State.Player.MoveCancelable");
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Player_CannotJump, "State.Player.CannotJump");
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Player_CanJump, "State.Player.CanJump");

// =========================================
// [Enemy] 敵専用
// =========================================
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Enemy_CanTransition,   "State.Enemy.CanTransition");
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Enemy_Guard,           "State.Enemy.Guard");
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Enemy_HaloAway,        "State.Enemy.HaloAway");
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Enemy_Armament,        "State.Enemy.Armament");
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Enemy_PlatformJumping, "State.Enemy.PlatformJumping");
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Enemy_PartComboDown,   "State.Enemy.PartComboDown");

// -----------------------------------------
// Charge Gear
// -----------------------------------------
UE_DEFINE_GAMEPLAY_TAG(TAG_Charge_Gear1, "Charge.Gear1");
UE_DEFINE_GAMEPLAY_TAG(TAG_Charge_Gear2, "Charge.Gear2");
UE_DEFINE_GAMEPLAY_TAG(TAG_Charge_Gear3, "Charge.Gear3");
UE_DEFINE_GAMEPLAY_TAG(TAG_Charge_Gear4, "Charge.Gear4");

// -----------------------------------------
// Hazard DoT
// -----------------------------------------
UE_DEFINE_GAMEPLAY_TAG(TAG_HazardDot_Lightning, "HazardDot.Lightning");
