// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#pragma once

#include "imgui.h"

// ImGui HUD 共通カラーパレット。色味を調整したいときはこのファイルだけ触ればよい。
// ・「基準カラー」を起点に用途別 namespace が参照する。色を増やすときはまず基準カラーを定義する
// ・0..1 正規化の ImVec4 が単一の真実。ImU32 が要る描画では ToU32() で変換する
// ・点滅や段階で変わる色は、両端の色を基準に Lerp() / WithAlpha() で作る
namespace TideHudColors
{
	// GetColorU32 と違いスタイル Alpha 非依存
	inline ImU32 ToU32( const ImVec4& C )
	{
		return IM_COL32(
			(int)( C.x * 255.0f + 0.5f ),
			(int)( C.y * 255.0f + 0.5f ),
			(int)( C.z * 255.0f + 0.5f ),
			(int)( C.w * 255.0f + 0.5f ) );
	}
	inline ImVec4 WithAlpha( ImVec4 C, float A ) { C.w = A; return C; }
	inline ImVec4 Lerp( const ImVec4& A, const ImVec4& B, float T )
	{
		return ImVec4( A.x + ( B.x - A.x ) * T, A.y + ( B.y - A.y ) * T, A.z + ( B.z - A.z ) * T, A.w + ( B.w - A.w ) * T );
	}

	// --- 基準カラー（Tideカラー。ここを変えると参照元すべての色味が動く）---
	inline const ImVec4 White        = ImVec4( 1.0f, 1.0f, 1.0f, 1.0f );							// テキスト・最大HP
	inline const ImVec4 DangerRed     = ImVec4( 0.80f, 0.10f, 0.10f, 1.0f );						// 低HP・チャージ極
	inline const ImVec4 Gold          = ImVec4( 230.0f / 255, 180.0f / 255, 50.0f / 255, 1.0f );	// 神技ゲージ
	inline const ImVec4 DarkPanel     = ImVec4( 40.0f / 255, 40.0f / 255, 40.0f / 255, 200.0f / 255 );	// ゲージ下地
	inline const ImVec4 NeutralLight  = ImVec4( 180.0f / 255, 180.0f / 255, 180.0f / 255, 1.0f );	// 追従残像（白寄りグレー）
	inline const ImVec4 Crimson       = ImVec4( 220.0f / 255, 30.0f / 255, 30.0f / 255, 1.0f );		// 敵HP
	inline const ImVec4 YellowGreen   = ImVec4( 160.0f / 255, 230.0f / 255, 70.0f / 255, 1.0f );	// 回復（黄緑）

	// --- モチーフ基準カラー。bUseTideMotifHudColors が ON のとき Motif 系が使われる ---
	inline const ImVec4 MotifCream     = ImVec4( 236.0f / 255, 230.0f / 255, 212.0f / 255, 1.0f );	// 生成り（明・テキスト）
	inline const ImVec4 MotifSage      = ImVec4( 146.0f / 255, 168.0f / 255, 150.0f / 255, 1.0f );	// セージ緑（追従残像）
	inline const ImVec4 MotifTeal      = ImVec4( 74.0f / 255, 120.0f / 255, 128.0f / 255, 1.0f );	// くすんだティール（敵HP）
	inline const ImVec4 MotifTealLight = ImVec4( 150.0f / 255, 186.0f / 255, 188.0f / 255, 1.0f );	// 明るいティール（先端線）
	inline const ImVec4 MotifSlate     = ImVec4( 60.0f / 255, 96.0f / 255, 112.0f / 255, 1.0f );		// 青みティール（構造体）
	inline const ImVec4 MotifDeepTeal  = ImVec4( 24.0f / 255, 42.0f / 255, 52.0f / 255, 1.0f );		// 深い藍緑（下地・輪郭）
	inline const ImVec4 MotifRust      = ImVec4( 190.0f / 255, 102.0f / 255, 62.0f / 255, 1.0f );	// ラスト（補色＝低HPの警告）
	inline const ImVec4 MotifMint      = ImVec4( 118.0f / 255, 236.0f / 255, 206.0f / 255, 1.0f );	// 明るいミント緑（神技レティクル等）
	inline const ImVec4 MotifLeaf      = ImVec4( 168.0f / 255, 214.0f / 255, 118.0f / 255, 1.0f );	// 若葉（回復）

	// --- 複数の HUD で共有する役割 ---
	namespace Common
	{
		inline const ImVec4 Text     = White;
		inline const ImVec4 GaugeBg  = DarkPanel;
		inline const ImVec4 Trailing = NeutralLight;	// 追従ゲージ（遅れて減る残像）
	}

	// --- プレイヤーHP ゲージ ---
	namespace Player
	{
		// クラシック
		inline const ImVec4 WindowBg = ImVec4( 0.6f, 0.6f, 0.6f, 0.2f );	// HUD ウィンドウ背景
		inline const ImVec4 HpHigh   = White;								// 高HP（白）→
		inline const ImVec4 HpLow    = DangerRed;							// 低HP（赤）へグラデーション
		inline const ImVec4 Heal     = YellowGreen;							// 回復帯（増えた分）

		// モチーフ。デザイン調整として輪郭線(Border)も足す
		inline const ImVec4 WindowBgMotif = ImVec4( 24.0f / 255, 42.0f / 255, 52.0f / 255, 0.30f );	// 深い藍緑の薄掛け
		inline const ImVec4 GaugeBgMotif  = ImVec4( 24.0f / 255, 42.0f / 255, 52.0f / 255, 210.0f / 255 );	// ゲージ下地
		inline const ImVec4 HpHighMotif   = MotifCream;								// 高HP（生成り）→
		inline const ImVec4 HpLowMotif    = MotifRust;								// 低HP（ラスト）へ
		inline const ImVec4 TrailingMotif = MotifSage;								// 追従残像（セージ）
		inline const ImVec4 HealMotif     = MotifLeaf;								// 回復帯（若葉）
		inline const ImVec4 TextMotif     = MotifCream;								// テキスト
		inline const ImVec4 BorderMotif   = ImVec4( 16.0f / 255, 30.0f / 255, 38.0f / 255, 1.0f );	// ゲージ輪郭線（ほぼ黒のティール）
	}

	// --- 敵 / ボス HP ゲージ（bGrayedOut＝戦闘外で灰色へ差し替え）---
	namespace Enemy
	{
		// クラシック
		inline const ImVec4 Hp         = Crimson;										// HP バー（赤）
		inline const ImVec4 Border     = ImVec4( 90.0f / 255, 5.0f / 255, 25.0f / 255, 1.0f );	// 暗い紅の枠
		inline const ImVec4 Tip        = ImVec4( 255.0f / 255, 90.0f / 255, 90.0f / 255, 1.0f );	// 先端の強調縦線

		// 戦闘外（帰還中など）のグレーアウト。モチーフ時も共用
		inline const ImVec4 HpGray     = ImVec4( 120.0f / 255, 120.0f / 255, 120.0f / 255, 1.0f );
		inline const ImVec4 BorderGray = ImVec4( 60.0f / 255, 60.0f / 255, 60.0f / 255, 1.0f );
		inline const ImVec4 TipGray    = ImVec4( 160.0f / 255, 160.0f / 255, 160.0f / 255, 1.0f );

		// モチーフ。プレイヤー＝暖色寄り（生成り/ラスト）に対し敵＝寒色グリーンで対比させ、
		// 明ティールを基準に残像・先端線をそのトーンで段階付ける
		inline const ImVec4 HpMotif       = MotifTealLight;								// HP バー（明ティールグリーン）
		inline const ImVec4 TipMotif      = ImVec4( 210.0f / 255, 232.0f / 255, 230.0f / 255, 1.0f );	// 先端の強調縦線（明るい白緑）
		inline const ImVec4 TrailingMotif = ImVec4( 92.0f / 255, 138.0f / 255, 140.0f / 255, 1.0f );		// 追従残像（HPより暗い緑）
		inline const ImVec4 BorderMotif   = MotifDeepTeal;								// 枠（深い藍緑）
		inline const ImVec4 GaugeBgMotif  = ImVec4( 24.0f / 255, 42.0f / 255, 52.0f / 255, 200.0f / 255 );	// 下地
		inline const ImVec4 TextMotif     = MotifCream;									// 名前/HP数値
	}

	// --- 神技ゲージ / ロックオンレティクル ---
	namespace GodAction
	{
		inline const ImVec4 GaugeBg        = ImVec4( 30.0f / 255, 30.0f / 255, 35.0f / 255, 200.0f / 255 );	// 下地
		inline const ImVec4 GaugeBorder    = ImVec4( 200.0f / 255, 200.0f / 255, 210.0f / 255, 180.0f / 255 );	// 枠（通常）
		inline const ImVec4 Trailing       = ImVec4( 120.0f / 255, 90.0f / 255, 30.0f / 255, 220.0f / 255 );	// 追従（くすんだ金）
		inline const ImVec4 Label          = White;															// "神技" ラベル
		inline const ImVec4 GaugeFill      = Gold;																// 蓄積中の塗り（金）
		// 満タン時の塗りパルス（Lo↔Hi を Pulse で補間）
		inline const ImVec4 FillPulseLo    = ImVec4( 255.0f / 255, 200.0f / 255, 60.0f / 255, 1.0f );
		inline const ImVec4 FillPulseHi    = ImVec4( 255.0f / 255, 255.0f / 255, 160.0f / 255, 1.0f );
		// 満タン時の枠パルス
		inline const ImVec4 BorderPulseLo  = ImVec4( 255.0f / 255, 255.0f / 255, 120.0f / 255, 1.0f );
		inline const ImVec4 BorderPulseHi  = White;
		inline const ImVec4 Ready          = ImVec4( 255.0f / 255, 240.0f / 255, 120.0f / 255, 1.0f );	// "発動可能"（alpha はパルス）
		inline const ImVec4 LockCount      = ImVec4( 120.0f / 255, 220.0f / 255, 255.0f / 255, 1.0f );	// LOCK ON カウント（水色）
		inline const ImVec4 Hint           = ImVec4( 220.0f / 255, 220.0f / 255, 220.0f / 255, 230.0f / 255 );	// 操作ヒント
		inline const ImVec4 ReticleGold    = ImVec4( 255.0f / 255, 230.0f / 255, 90.0f / 255, 1.0f );	// レティクル（金）
		inline const ImVec4 ReticleNeutral = ImVec4( 200.0f / 255, 200.0f / 255, 210.0f / 255, 200.0f / 255 );	// 蓄積中（灰）
		// 発動中の小レティクル（赤寄りパルス Lo↔Hi）
		inline const ImVec4 ReticleSlashLo = ImVec4( 255.0f / 255, 80.0f / 255, 80.0f / 255, 1.0f );
		inline const ImVec4 ReticleSlashHi = ImVec4( 255.0f / 255, 200.0f / 255, 80.0f / 255, 1.0f );

		// モチーフ。神技は赤・黄を使わず「緑（セージ）と白」中心。
		// 蓄積＝セージ緑、満タン＝白へ発光、周辺（枠・カウント）は明ティールで揃える
		inline const ImVec4 GaugeBgMotif      = ImVec4( 24.0f / 255, 42.0f / 255, 52.0f / 255, 200.0f / 255 );	// 下地（深い藍緑）
		inline const ImVec4 GaugeBorderMotif  = ImVec4( 150.0f / 255, 186.0f / 255, 188.0f / 255, 180.0f / 255 );	// 枠（明ティール）
		inline const ImVec4 TrailingMotif     = ImVec4( 74.0f / 255, 100.0f / 255, 80.0f / 255, 220.0f / 255 );	// 追従（くすんだ深緑）
		inline const ImVec4 LabelMotif        = MotifCream;														// "神技" ラベル
		inline const ImVec4 GaugeFillMotif    = MotifSage;														// 蓄積中の塗り（セージ緑）
		// 満タン時の塗りパルス（セージ緑→白で発光感）
		inline const ImVec4 FillPulseLoMotif  = MotifSage;
		inline const ImVec4 FillPulseHiMotif  = White;
		// 満タン時の枠パルス（クリーム→白）
		inline const ImVec4 BorderPulseLoMotif = MotifCream;
		inline const ImVec4 BorderPulseHiMotif = White;
		inline const ImVec4 ReadyMotif        = White;															// "発動可能"（alpha はパルス）
		inline const ImVec4 LockCountMotif    = MotifTealLight;													// LOCK ON カウント
		inline const ImVec4 HintMotif         = ImVec4( 210.0f / 255, 205.0f / 255, 188.0f / 255, 230.0f / 255 );	// 操作ヒント（くすんだクリーム）

		// レティクル系は視認性重視でセージ→明るいミント緑へ
		inline const ImVec4 ReticleGoldMotif  = MotifMint;
		inline const ImVec4 ReticleNeutralMotif = ImVec4( 120.0f / 255, 172.0f / 255, 176.0f / 255, 205.0f / 255 );	// 蓄積中（半透明ティール）
		// 発動中の小レティクル（ミント緑→白 パルス）
		inline const ImVec4 ReticleSlashLoMotif = MotifMint;
		inline const ImVec4 ReticleSlashHiMotif = White;
		// レティクル背後のグロー
		inline const ImVec4 ReticleGlowMotif  = MotifMint;
		inline const ImVec4 ReticleGlow       = ImVec4( 255.0f / 255, 230.0f / 255, 90.0f / 255, 1.0f );	// 金グロー

		// 3択セレクタのボタン記号。パッドの配色に合わせる
		inline const ImVec4 FaceButtonX = ImVec4( 70.0f / 255, 140.0f / 255, 225.0f / 255, 1.0f );	// X（青）
		inline const ImVec4 FaceButtonY = ImVec4( 235.0f / 255, 200.0f / 255, 60.0f / 255, 1.0f );	// Y（黄）
		inline const ImVec4 FaceButtonB = ImVec4( 220.0f / 255, 75.0f / 255, 65.0f / 255, 1.0f );	// B（赤）

		// エネルギー玉：コア＝明るい中心、グロー＝ゲージ色に合わせた外周
		inline const ImVec4 OrbCore      = White;
		inline const ImVec4 OrbGlow      = Gold;
		inline const ImVec4 OrbCoreMotif = White;
		inline const ImVec4 OrbGlowMotif = MotifSage;	// 神技ゲージと統一
	}

	// --- ダメージ数値。文字は基調色で、被弾者・大ダメージは輪郭色で区別する（alpha は呼び出し側）---
	namespace DamageNumber
	{
		inline const ImVec4 Text          = White;
		inline const ImVec4 TextMotif     = MotifCream;
		inline const ImVec4 PlayerOutline = ImVec4( 120.0f / 255, 14.0f / 255, 20.0f / 255, 1.0f );	// 被弾者=PL（深い赤）
		inline const ImVec4 EnemyOutline  = MotifDeepTeal;								// 被弾者=敵（深い藍緑）
		inline const ImVec4 BigOutline    = ImVec4( 255.0f / 255, 170.0f / 255, 0.0f, 1.0f );	// 大ダメージ(30以上)
	}

	// --- チャージ漢字（alpha は出現フェードで呼び出し側が設定）---
	namespace Charge
	{
		inline const ImVec4 Gear1 = ImVec4( 0.95f, 0.95f, 0.85f, 1.0f );	// 壱（生成り）
		inline const ImVec4 Gear2 = ImVec4( 1.0f, 1.0f, 0.4f, 1.0f );		// 弐（黄）
		inline const ImVec4 Gear3 = ImVec4( 1.0f, 0.5f, 0.0f, 1.0f );		// 参（橙）
		inline const ImVec4 Gear4 = ImVec4( 1.0f, 0.1f, 0.1f, 1.0f );		// 極（赤）
	}
}
