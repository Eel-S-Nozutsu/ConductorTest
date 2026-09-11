// Copyright (c) 2025, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#pragma once

// 秒数カウントタイマー
class PRJ_TIDE_P0_API FAutomaticTimer
{
public:
	FAutomaticTimer();
	FAutomaticTimer( float Seconds, bool bSetFinished = false );

public:
	void	Set( float Seconds, bool bSetFinished = false );		// タイマー秒数設定

	void	SetCurrent( float CurrentSeconds );						// 現在秒数の上書き
	void	SetCurrentByRate( float Rate );							// 現在秒数の上書き（割合値0～1設定）

	float	Get() const;											// 現在秒数の取得
	float	GetElapsed() const;										// 現在の経過した秒数の取得
	float	GetStart() const;										// 設定秒数の取得

	float	GetRate() const;										// 現在秒数の割合値取得
	float	GetInvRate() const;										// 現在秒数の割合値反転取得

	// タイマー更新。bIsLimit で現在秒数を 0 で止めるかを切り替え、更新後の現在秒数を返す
	float	Update( float DeltaTime, bool bIsLimit = true );

	void	Clear();												// 設定クリア

	bool	IsFinish() const;										// タイマーが終了しているか
	void	SetFinished();											// 現在秒数を終了させる

	void	Reset();												// タイマーのリセット
	bool	IsValid() const;										// タイマー設定が有効か

private:
	float	CurrentSeconds;
	float	StartSeconds;
};
