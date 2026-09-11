// Copyright (c) 2025, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#include "AutomaticTimer.h"

FAutomaticTimer::FAutomaticTimer()
{
	Clear();
}

FAutomaticTimer::FAutomaticTimer( float Seconds, bool bSetFinished )
{
	Clear();
	Set( Seconds );
	if ( bSetFinished )	SetFinished();
}

void FAutomaticTimer::Set( float Seconds, bool bSetFinished )
{
	CurrentSeconds = Seconds;
	StartSeconds = Seconds;
	if ( bSetFinished ) SetFinished();
}

void FAutomaticTimer::SetCurrent( float Seconds )
{
	CurrentSeconds = Seconds;
}

void FAutomaticTimer::SetCurrentByRate( float Rate )
{
	Rate = std::clamp( Rate, 0.0f, 1.0f );
	SetCurrent( StartSeconds * ( 1.0f - Rate ) );
}

float FAutomaticTimer::Get() const
{
	return CurrentSeconds;
}

float FAutomaticTimer::GetElapsed() const
{
	return GetStart() - Get();
}

float FAutomaticTimer::GetStart() const
{
	return StartSeconds;
}

float FAutomaticTimer::GetRate() const
{
	if ( !IsValid() ) return 1.0f;
	return 1.0f - Get() / GetStart();
}

float FAutomaticTimer::GetInvRate() const
{
	return 1.0f - GetRate();
}

float FAutomaticTimer::Update( float DeltaTime, bool bIsLimit )
{
	CurrentSeconds -= DeltaTime;

	if ( bIsLimit )
	{
		if ( CurrentSeconds < 0.0f )
		{
			CurrentSeconds = 0.0f;
		}
	}
	else
	{
		// 限界を超えて更新
		// 念のため値の最大値チェック
		if ( CurrentSeconds < -FLT_MAX )
		{
			CurrentSeconds = -FLT_MAX;
		}
	}
	return CurrentSeconds;
}

void FAutomaticTimer::Clear()
{
	CurrentSeconds = 0.0f;
	StartSeconds = 0.0f;
}

bool FAutomaticTimer::IsFinish() const
{
	return Get() <= 0.0f;
}

void FAutomaticTimer::SetFinished()
{
	CurrentSeconds = 0.0f;
}

void FAutomaticTimer::Reset()
{
	CurrentSeconds = StartSeconds;
}

bool FAutomaticTimer::IsValid() const
{
	return StartSeconds > 0.0f;
}




