// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "NativeGameplayTags.h"

// -----------------------------------------
// Audio
// -----------------------------------------

// グループ
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Sound_Music);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Sound_Ambience);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Sound_GameSE);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Sound_UI);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Sound_Voice);

// Music
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Sound_Music_Test);

// GameSE
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Sound_GameSE_TrainCrossingSignal);

// -----------------------------------------
// Option
// -----------------------------------------

UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Settings_Camera_CameraSensitivityLR);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Settings_Camera_CameraSensitivityUD);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Settings_Camera_bCameraInvertX);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Settings_Camera_bCameraInvertY);

UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Settings_Audio_MasterVolume);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Settings_Audio_BGMVolume);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Settings_Audio_AmbienceVolume);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Settings_Audio_GameSEVolume);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Settings_Audio_MenuSEVolume);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Settings_Audio_VoiceVolume);

UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Settings_Graphics_bMotionBlur);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Settings_Graphics_Brightness);

UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Settings_Language_LanguageVoice);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Settings_Language_LanguageText);

// -----------------------------------------
// HitReaction
// -----------------------------------------
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_HitReaction_Knockback_S);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_HitReaction_Knockback_M);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_HitReaction_Knockback_L);
// のけぞり後ろ方向。ReactionConversionTableでBack方向の被弾時に上記から変換
// する
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_HitReaction_Knockback_S_Back);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_HitReaction_Knockback_M_Back);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_HitReaction_Knockback_L_Back);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_HitReaction_Blowoff);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_HitReaction_Blowoff_S);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_HitReaction_Blowoff_M);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_HitReaction_Blowoff_L);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_HitReaction_SmashDown);
// 打ち上げ: ヒットで開始→着地までループ→着地の3構成(Blowbackステートマシンで駆動)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_HitReaction_Blowup);
// 神技リアクション群(GodSkill.*)。今後の神技追加を見越して階層化
// Iai = 居合切り: ヒット後に時を止め、n秒後に斬撃リアクションを遅延再生する
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_HitReaction_GodSkill_Iai);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_HitReaction_NoReaction);
// 竜巻の巻き上げ。被弾ではなく環境由来だが、多段リアクションとして同じテーブル行型で定義する
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_HitReaction_Wind);

// -----------------------------------------
// Attack Event
// -----------------------------------------
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_AttackEvent_ChargeStart);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_AttackEvent_ChargeEnd);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_AttackEvent_MissileStart);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_AttackEvent_MissileShot);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_AttackEvent_ProjectileFire);	// 弾発射攻撃の1発目の発射タイミング
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_AttackEvent_StrafeStart);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_AttackEvent_ShockwallStart);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_AttackEvent_ShockwallLand);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_AttackEvent_ShockwallMineBurst);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_AttackEvent_HaloAttackStart);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_AttackEvent_HaloThrowStart);		// 光輪投擲開始(光輪を非表示にして弾をスポーン)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_AttackEvent_HaloReturn);			// 光輪を背中ソケットへ戻して再表示
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_AttackEvent_HaloStepStart);		// 光輪ステップ移動開始
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_AttackEvent_HaloStepEnd);		// 光輪ステップ移動終了
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_AttackEvent_GuardOn);			// ガード開始(光輪を腰ソケットへ固定、Guardタグ付与)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_AttackEvent_HaloReady);			// 展開アニメーション完了(接触反応を有効化)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_AttackEvent_HaloPlace);			// 光輪配置発射(腕の輪を上へ発射し、2ラインに落下配置する)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_AttackEvent_StompConnect);		// 踏みつけ連結点(回数が残っていれば反対足モンタージュへ接続する)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_AttackEvent_StompImpact);		// 踏み下ろし着地点(光輪が残っていれば足元から衝撃波を発生させる)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_AttackEvent_PlatformJumpLaunch);	// 足場ジャンプ離陸(踏ん張り終わり。放物線移動を開始する)

// -----------------------------------------
// Attack Type
// -----------------------------------------
// Player
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_AttackType_Player_LightAttack01);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_AttackType_Player_LightAttack02);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_AttackType_Player_LightAttack03);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_AttackType_Player_LightAttack04);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_AttackType_Player_ComboChargeAttack01);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_AttackType_Player_ComboChargeAttack02);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_AttackType_Player_ComboChargeAttack03);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_AttackType_Player_ComboChargeAttack04);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_AttackType_Player_AirChargeAttack);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_AttackType_Player_ComboChargeDash01);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_AttackType_Player_ComboChargeDash02);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_AttackType_Player_ComboChargeDash03);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_AttackType_Player_ComboChargeDash04);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_AttackType_Player_AirChargeDash);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_AttackType_Player_ChargeDrift);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_AttackType_Player_GodActionSlash);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_AttackType_Player_GodActionFrolic);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_AttackType_Player_GodActionGuidance);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_AttackType_Player_SlidePassiveTornadoSmall);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_AttackType_Player_SlidePassiveTornadoLarge);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_AttackType_Player_SlidePassiveGust);

// Enemy
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_AttackType_Enemy_DefaultAttack);

// -----------------------------------------
// 部位
// -----------------------------------------
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Part_LeftLeg);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Part_RightLeg);

// -----------------------------------------
// Input
// -----------------------------------------
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Input_Default);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Input_UI);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Input_Debug);

// -----------------------------------------
// InputCommand
// -----------------------------------------
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Input_Command_Dodge);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Input_Command_ChargeAction);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Input_Command_Attack);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Input_Command_Jump);

// -----------------------------------------
// Charge Gear
// -----------------------------------------
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Charge_Gear1);	// チャージギア壱
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Charge_Gear2);	// チャージギア弍
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Charge_Gear3);	// チャージギア参
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Charge_Gear4);	// チャージギア極

// -----------------------------------------
// Hazard DoT
// -----------------------------------------
// エリアハザードの残留床(しびれ床など)の継続ダメージ型
// UHazardDotComponentのカデンツのキーに使う
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_HazardDot_Lightning);	// 落雷しびれ床

// -----------------------------------------
// State Tags
// -----------------------------------------

// =========================================
// [Common] 全キャラクター共通 (Player, Enemy, NPC共通)
// =========================================
// ダメージ処理や共通AIロジックで判定されるべき基本的な状態
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Common_Invincible);				// 無敵(親タグ)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Common_SuperArmor);				// 怯み無効
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Common_SuperArmor_Invincible);	// 怯み無効かつダメージ無効(帰還中など)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Common_SuperArmor_HaloOpen);		// 怯み無効だが光輪へのガード判定だけは通す
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Common_Dead);					// 死亡

UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Common_Disable);					// 行動不能(親タグ)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Common_Disable_Event);			// イベント中行動不能

UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Common_HitReaction);				// ヒットリアクション中(親タグ)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Common_HitReaction_Flinching);	// 怯みやられ
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Common_HitReaction_BlowBack);	// 吹っ飛びやられ
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Common_HitReaction_Downed);		// 吹っ飛びやられダウン中
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Common_HitReaction_Recovering);	// 起き上がり中
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Common_HitReaction_Immune);		// ヒットリアクション無敵

// =========================================
// [Player] プレイヤー専用
// =========================================
// プレイヤーの入力制御や、固有のシステムに関する状態
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Player_CanAttack);				// 攻撃可能
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Player_CanMove);					// 移動可能
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Player_CannotDodge);				// 回避不可
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Player_CanCharge);				// チャージ可能
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Player_CanDash);					// ダッシュ可能
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Player_Invincible_Dodge);		// プレイヤー固有の回避無敵
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Player_CanCombo);				// コンボ可能
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Player_CannotLockOn);			// ロックオン不可
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Player_MoveCancelable);			// 移動キャンセル可能(ブレンドでタグが残るための対応)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Player_CannotJump);				// ジャンプ不可(ジャンプパッド打ち上げ中など。他アクションは制限しない)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Player_CanJump);					// ジャンプだけ先行キャンセル可(CanMoveとは別枠。チャージ攻撃をジャンプで抜ける窓の調整用)

// =========================================
// [Enemy] 敵専用
// =========================================
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Enemy_CanTransition);	// 次アクションへの遷移を許可
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Enemy_Guard);			// ガード中(光輪ガード状態)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Enemy_HaloAway);		// 光輪を技で飛ばして手元に無い＝無防備(光輪攻撃のBlockedStateTagsに入れて再抽選を止める)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Enemy_Armament);		// 硬化装鋼中(被ダメージ軽減)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Enemy_PlatformJumping);	// 足場ジャンプ移動中(攻撃/ステップの割り込みを抑止)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Enemy_PartComboDown);		// 複数部位破壊リアクション中(ダウン。条件付き部位のロックオン解禁に使う)
