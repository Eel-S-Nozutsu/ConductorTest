// Copyright (c) 2025, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#pragma once

namespace PlayerNiagaraTags
{
	const FName CHARGING				= TEXT( "Charging" );
	const FName CHARGE_COMPLETE			= TEXT( "ChargeComplete" );
	const FName CHARGED_DASH			= TEXT( "ChargedDash" );

	// チャージダッシュ中の風切りエフェクト（想定：NS_Charge_DashTrail。ループ型 / int32 "Gear" 1〜3）。
	// プレイヤーにアタッチし前方へオフセット、向きは Yaw をプレイヤー＋180 で出す。
	// 竜巻ジャンプ／打ち上げでも流用する（ギアは JumpWindTrailGear 固定）
	const FName CHARGE_DASH_WIND		= TEXT( "ChargeDashWind" );

	const FName DAMAGED					= TEXT( "Damaged" );

	const FName CHAEGE_FOOT_DEFAULT		= TEXT( "ChargeFootDefault" );
	const FName CHAEGE_FOOT_WATER		= TEXT( "ChargeFootWater" );

	const FName CHARGE_BOOST			= TEXT( "ChargeBoost" );
	const FName CHARGE_BOOST_ST			= TEXT( "ChargeBoostStart" );

	const FName CHARGE_RELEASE_01		= TEXT( "ChargeRelease01" );
	const FName CHARGE_RELEASE_02		= TEXT( "ChargeRelease02" );
	const FName CHARGE_RELEASE_03		= TEXT( "ChargeRelease03" );

	// スライドパッシブ（円を描いた際の印・竜巻）。印を先に出し一定時間後に竜巻を出す。印は単一アセットで大小を Scale で
	// 調整し、竜巻は大／小でアセットが分かれる（想定：NS_Passive_Mark / NS_Passive_Tornado{Large/Small}。float LifeTime・Scale）
	const FName PASSIVE_MARK			= TEXT( "PassiveMark" );
	const FName PASSIVE_TORNADO_LARGE	= TEXT( "PassiveTornadoLarge" );
	const FName PASSIVE_TORNADO_SMALL	= TEXT( "PassiveTornadoSmall" );

	// スライドパッシブ 軌跡リボン。チャージスライド中に常時表示し、円完成時に赤フラッシュ、スライド終了時にフェードアウトする
	// 想定: NS_Passive_Trail（float: LifeTime、LinearColor: Color を持つリボン型ループエフェクト）
	const FName PASSIVE_TRAIL			= TEXT( "PassiveTrail" );

	// スライドパッシブ：突風。CLOAK＝S 字を描いた際の風まとい（プレイヤーへアタッチするループ型。float LifeTime）、
	// BURST＝バフ消費時の範囲攻撃バースト（単発型。float LifeTime / Scale）
	const FName PASSIVE_GUST_CLOAK		= TEXT( "PassiveGustCloak" );
	const FName PASSIVE_GUST_BURST		= TEXT( "PassiveGustBurst" );

	// 神鳥（お供精霊）のエフェクト。APPEAR/DISAPPEAR＝チャージ開始/解除で神鳥へアタッチする単発型、
	// TRAIL＝表示中ずっとメッシュへアタッチするループ型トレイル。
	// いずれも UserParameter "Color" を SlidePassive のトレイル色（黄緑）に合わせて設定する
	const FName GODBIRD_APPEAR			= TEXT( "GodBirdAppear" );
	const FName GODBIRD_DISAPPEAR		= TEXT( "GodBirdDisappear" );
	const FName GODBIRD_TRAIL			= TEXT( "GodBirdTrail" );

	// 突風バフ中、正面へ回り込んだ神鳥が「自分が風を出している」ように見せるアタッチ型VFX（未割当なら出さない）。
	// 想定：鳥メッシュへアタッチするループ型。UserParameter "Color"（黄緑）を持つ
	const FName GODBIRD_GUST			= TEXT( "GodBirdGust" );

	// 神技「戯れ」で飛び回る鳥の軌跡トレイル。GODBIRD_TRAIL は高速飛行で軌跡を引けないためリボン型を想定して別枠化した
	// （想定パラメータ：float "LifeTime"、LinearColor "Color"）
	const FName GODBIRD_FROLIC_TRAIL	= TEXT( "GodBirdFrolicTrail" );

	const FName HIT_SMALL				= TEXT( "HitSmall" );
	const FName HIT_MEDIUM				= TEXT( "HitMedium" );
	const FName HIT_LARGE				= TEXT( "HitLarge" );

	// 武器トレイル。剣のソケット "Weapon_Blade" にループ Niagara をアタッチして剣閃を出す（float LifeTime を持つループ型）。
	// WEAPON_TRAIL＝通常・チャージ攻撃中、GOD_ACTION_WEAPON_TRAIL＝神技スラッシュ中に起動する
	const FName WEAPON_TRAIL			= TEXT( "WeaponTrail" );
	const FName GOD_ACTION_WEAPON_TRAIL	= TEXT( "GodActionWeaponTrail" );
}
