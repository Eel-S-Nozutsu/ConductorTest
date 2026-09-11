/// Copyright (c) 2025, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.

#include "PlayerWindow.h"

#include <format>
#include "Kismet/GameplayStatics.h"
#include "GameFramework/CharacterMovementComponent.h"

#include "PRJ_TIDE_P0/Actors/Character/Player/TidePlayerCharacter.h"
#include "PRJ_TIDE_P0/Data/Player/TidePlayerParamDataAsset.h"
#include "PRJ_TIDE_P0/Data/Tags/TideGameplayTags.h"

#if !UE_BUILD_SHIPPING

FImGuiLogWindow PlayerWindow::AttackLog;

void PlayerWindow::DrawContents()
{
	if ( !GameInstance ) { return; }

	if ( ImGui::BeginTabBar( "PlayerWindowTabBar" ) )
	{
		if ( ImGui::BeginTabItem( "汎用" ) )
		{
			DrawTabGenerals();
			ImGui::EndTabItem();
		}
		if ( ImGui::BeginTabItem( "デバッグフラグ" ) )
		{
			DrawTabDebugFlags();
			ImGui::EndTabItem();
		}
		if ( ImGui::BeginTabItem( "攻撃ログ" ) )
		{
			DrawTabAttackLog();
			ImGui::EndTabItem();
		}
		if ( ImGui::BeginTabItem( "じゆうてすと" ) )
		{
			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
	}
}

void PlayerWindow::DrawTabGenerals()
{
	if ( ImGui::CollapsingHeader( "プレイヤー情報" ) )
	{
		DrawContentsPlayerInfo();
	}
	if ( ImGui::CollapsingHeader( "アニメモンタージュ情報" ) )
	{
		DrawContentsAnimInfo();
	}
	if ( ImGui::CollapsingHeader( "チャージ情報" ) )
	{
		DrawContentsChargeInfo();
	}
	if ( ImGui::CollapsingHeader( "通常攻撃情報" ) )
	{
		DrawContentsAttackInfo();
	}
	if( ImGui::CollapsingHeader( "回避情報" ) )
	{
		DrawContentsDodgeInfo();
	}
	if( ImGui::CollapsingHeader( "ロックオン情報" ) )
	{
		DrawContentsLockOnInfo();
	}
	if( ImGui::CollapsingHeader( "被弾情報" ) )
	{
		DrawContentsDamageInfo();
	}
	if( ImGui::CollapsingHeader( "落下情報" ) )
	{
		DrawContentsFallInfo();
	}
	if( ImGui::CollapsingHeader( "神技情報" ) )
	{
		DrawContentsGodActionInfo();
	}
	if( ImGui::CollapsingHeader( "スライドパッシブ情報" ) )
	{
		DrawContentsSlidePassiveInfo();
	}
	if( ImGui::CollapsingHeader( "面沿い移動情報" ) )
	{
		DrawContentsSurfaceRideInfo();
	}
}

void PlayerWindow::DrawTabDebugFlags()
{

}

void PlayerWindow::DrawContentsPlayerInfo()
{
	ACharacter* PlayerCharacter = Cast<ACharacter>( UGameplayStatics::GetPlayerCharacter( GameInstance->GetWorld(), 0 ) );
	if ( !PlayerCharacter ) { return; }

	UCharacterMovementComponent* MovementComp = PlayerCharacter->GetCharacterMovement();
	if ( !MovementComp ) { return; }

	ImGui::Spacing();
	ImGui::Text( "--- Movement Status ---" );

	ImGui::Text( "Current Velocity Size: %.1f", MovementComp->Velocity.Size() );
	float Vel[3] = { MovementComp->Velocity.X, MovementComp->Velocity.Y, MovementComp->Velocity.Z };
	ImGui::DragFloat3( "Velocity", Vel, 0.1f );

	bool bIsFalling = MovementComp->IsFalling();
	ImGui::Checkbox( "Is Falling", &bIsFalling );	// 読み取り専用

	ImGui::Spacing();
	ImGui::Text( "--- Movement Parameters ---" );

	// 以下は UI 上でドラッグして直接弄れる
	ImGui::DragFloat( "Max Walk Speed", &MovementComp->MaxWalkSpeed, 5.0f );
	ImGui::DragFloat( "Min Analog Walk Speed", &MovementComp->MinAnalogWalkSpeed, 1.0f );
	ImGui::DragFloat( "Ground Friction", &MovementComp->GroundFriction, 0.1f );
	ImGui::DragFloat( "Braking (Walking)", &MovementComp->BrakingDecelerationWalking, 10.0f );
	ImGui::DragFloat( "Braking (Falling)", &MovementComp->BrakingDecelerationFalling, 10.0f );

	ImGui::Spacing();
	ImGui::DragFloat( "Jump Z Velocity", &MovementComp->JumpZVelocity, 5.0f );
	ImGui::DragFloat( "Gravity Scale", &MovementComp->GravityScale, 0.05f );
	ImGui::DragFloat( "Air Control", &MovementComp->AirControl, 0.01f );

	// FRotator の中身なので一度変数に出してから書き戻す
	float YawRate = MovementComp->RotationRate.Yaw;
	if ( ImGui::DragFloat( "Rotation Rate Yaw", &YawRate, 5.0f ) )
	{
		MovementComp->RotationRate.Yaw = YawRate;
	}
}

void PlayerWindow::DrawContentsAnimInfo()
{
	ACharacter* PlayerCharacter = Cast<ACharacter>( UGameplayStatics::GetPlayerCharacter( GameInstance->GetWorld(), 0 ) );
	if ( !PlayerCharacter ) return;

	static TArray<FAnimHistoryInfo> FinishedHistory;	// 終了したアニメの履歴
	float CurrentTime = GameInstance->GetWorld()->GetTimeSeconds();
	const float LifeTime = 3.0f;

	UAnimInstance* AnimInst = PlayerCharacter->GetMesh() ? PlayerCharacter->GetMesh()->GetAnimInstance() : nullptr;
	UAnimMontage* CurrentMontage = AnimInst ? AnimInst->GetCurrentActiveMontage() : nullptr;
	static FString PrevPlayingName = TEXT( "" );

	ImGui::SeparatorText( "Current Animation" );
	if ( CurrentMontage )
	{
		FString Name = CurrentMontage->GetName();
		float Len = CurrentMontage->GetPlayLength();
		float Pos = AnimInst->Montage_GetPosition( CurrentMontage );
		float Ratio = ( Len > 0.0f ) ? ( Pos / Len ) : 0.0f;

		ImGui::Text( "Montage: %s", TCHAR_TO_UTF8( *Name ) );
		ImGui::Text( "Time   : %.2f / %.2f sec", Pos, Len );
		ImGui::ProgressBar( Ratio, ImVec2( -1, 8.0f ), "" );

		// 切り替わったら古い方を履歴へ送る
		if ( !PrevPlayingName.IsEmpty() && PrevPlayingName != Name )
		{
			FinishedHistory.Insert( { PrevPlayingName, 0.0f, 0.0f, CurrentTime }, 0 );
		}
		PrevPlayingName = Name;
	}
	else
	{
		ImGui::TextDisabled( "None" );
		if ( !PrevPlayingName.IsEmpty() )
		{
			FinishedHistory.Insert( { PrevPlayingName, 0.0f, 0.0f, CurrentTime }, 0 );
			PrevPlayingName = TEXT( "" );
		}
	}

	ImGui::SeparatorText( "Finished History (3s Life)" );

	for ( int32 i = FinishedHistory.Num() - 1; i >= 0; i-- )
	{
		if ( CurrentTime - FinishedHistory[i].EndTime > LifeTime )
		{
			FinishedHistory.RemoveAt( i );
		}
	}

	for ( const auto& Item : FinishedHistory )
	{
		float Elapsed = CurrentTime - Item.EndTime;
		ImGui::TextColored( ImVec4( 0.7f, 0.7f, 0.7f, 1.0f ), "%s (End: %.1fs ago)", TCHAR_TO_UTF8( *Item.Name ), Elapsed );
	}
}

void PlayerWindow::DrawContentsChargeInfo()
{
	auto* PlayerCharacter = Cast<ATidePlayerCharacter>( UGameplayStatics::GetPlayerCharacter( GameInstance->GetWorld(), 0 ) );
	if ( !PlayerCharacter ) { return; }
	PlayerCharacter->DrawChargeActionDebugImGui();
	PlayerCharacter->DrawGlideDebugImGui();	// 滑空も空中の挙動として同じタブで見る
}

void PlayerWindow::DrawContentsAttackInfo()
{
	auto* PlayerCharacter = Cast<ATidePlayerCharacter>( UGameplayStatics::GetPlayerCharacter( GameInstance->GetWorld(), 0 ) );
	if ( !PlayerCharacter ) { return; }
	PlayerCharacter->DrawAttackDebugImGui();
}

void PlayerWindow::DrawContentsDodgeInfo()
{
	auto* PlayerCharacter = Cast<ATidePlayerCharacter>( UGameplayStatics::GetPlayerCharacter( GameInstance->GetWorld(), 0 ) );
	if ( !PlayerCharacter ) { return; }
	PlayerCharacter->DrawDodgeDebugImGui();
}

void PlayerWindow::DrawContentsLockOnInfo()
{
	auto* PlayerCharacter = Cast<ATidePlayerCharacter>( UGameplayStatics::GetPlayerCharacter( GameInstance->GetWorld(), 0 ) );
	if ( !PlayerCharacter ) { return; }
	PlayerCharacter->DrawLockOnDebugImGui();
}

void PlayerWindow::DrawContentsDamageInfo()
{

}

void PlayerWindow::DrawContentsFallInfo()
{
	auto* PlayerCharacter = Cast<ATidePlayerCharacter>( UGameplayStatics::GetPlayerCharacter( GameInstance->GetWorld(), 0 ) );
	if ( !PlayerCharacter ) { return; }
	PlayerCharacter->DrawFallDebugImGui();
}

void PlayerWindow::DrawContentsGodActionInfo()
{
	auto* PlayerCharacter = Cast<ATidePlayerCharacter>( UGameplayStatics::GetPlayerCharacter( GameInstance->GetWorld(), 0 ) );
	if ( !PlayerCharacter ) { return; }
	PlayerCharacter->DrawGodActionDebugImGui();
}

void PlayerWindow::DrawContentsSlidePassiveInfo()
{
	auto* PlayerCharacter = Cast<ATidePlayerCharacter>( UGameplayStatics::GetPlayerCharacter( GameInstance->GetWorld(), 0 ) );
	if ( !PlayerCharacter ) { return; }
	PlayerCharacter->DrawSlidePassiveDebugImGui();
}

namespace
{
	// 満たしていれば緑、満たしていなければ赤
	void DrawConditionLine( bool bOk, const FString& Text )
	{
		const ImVec4 Color = bOk ? ImVec4( 0.4f, 1.0f, 0.4f, 1.0f ) : ImVec4( 1.0f, 0.5f, 0.4f, 1.0f );
		ImGui::TextColored( Color, "%s", TCHAR_TO_UTF8( *Text ) );
	}
}

void PlayerWindow::DrawContentsSurfaceRideInfo()
{
	auto* PlayerCharacter = Cast<ATidePlayerCharacter>( UGameplayStatics::GetPlayerCharacter( GameInstance->GetWorld(), 0 ) );
	if ( !PlayerCharacter ) { return; }

	const UTidePlayerParamDataAsset* Params = PlayerCharacter->PlayerParamData;
	const UCharacterMovementComponent* MovementComp = PlayerCharacter->GetCharacterMovement();
	if ( !Params || !MovementComp ) { ImGui::TextDisabled( "PlayerParamData / Movement が無い" ); return; }

	const bool bRiding = PlayerCharacter->IsSurfaceRiding();
	DrawConditionLine( bRiding, FString::Printf( TEXT( "Surface Riding : %s" ), bRiding ? TEXT( "YES" ) : TEXT( "no" ) ) );

	// 発動条件を 1 行ずつ出す。赤い行がそのまま「発動しない原因」になる
	ImGui::SeparatorText( "発動条件" );

	DrawConditionLine( Params->bEnableSurfaceRide,
		FString::Printf( TEXT( "機能有効   : %s" ), Params->bEnableSurfaceRide ? TEXT( "ON" ) : TEXT( "OFF (bEnableSurfaceRide)" ) ) );

	const bool bHopEntry = PlayerCharacter->IsSurfaceRideChargeHopEntryAllowed();
	const bool bCharging = PlayerCharacter->IsCharging() || PlayerCharacter->IsPlayingChargeDash() || bHopEntry;
	DrawConditionLine( bCharging,
		FString::Printf( TEXT( "チャージ中 : %s%s" ),
			bCharging ? TEXT( "YES" ) : TEXT( "no (チャージ／チャージダッシュ／幅跳び中のみ発動)" ),
			bHopEntry ? TEXT( " ※チャージ幅跳び" ) : TEXT( "" ) ) );

	const bool bInZone = PlayerCharacter->IsInSurfaceRideZone();
	const bool bZoneOk = !Params->bSurfaceRideRequireZone || bInZone;
	DrawConditionLine( bZoneOk, FString::Printf( TEXT( "エリア     : %s (重なり数 %d, 要求 %s)" ),
		bInZone ? TEXT( "IN" ) : TEXT( "OUT" ), PlayerCharacter->GetSurfaceRideZoneCount(),
		Params->bSurfaceRideRequireZone ? TEXT( "ON" ) : TEXT( "OFF" ) ) );

	const bool bOnGround = MovementComp->IsMovingOnGround();
	DrawConditionLine( bOnGround,
		FString::Printf( TEXT( "接地       : %s" ), bOnGround ? TEXT( "YES" ) : TEXT( "no (空中では開始できない)" ) ) );

	// 判定に使うのは生の傾斜。平滑化後の値も併記して、空間平均でどれだけ浅く出ているか見比べられるようにする
	const FVector FloorNormal = PlayerCharacter->GetSmoothedFloorNormal();
	const float RawAngle = PlayerCharacter->GetRawFloorAngleDeg();
	const float SmoothedAngle = FMath::RadiansToDegrees( FMath::Acos( FMath::Clamp( FloorNormal.Z, -1.0f, 1.0f ) ) );
	// しきい値はギア別配列で上書きできるため、解決後の値（＝実際に判定に使う値）を出す
	const float AngleThreshold = bRiding
		? PlayerCharacter->GetSlopeParamForGear( Params->SurfaceRideExitAngleForGear )
		: PlayerCharacter->GetSlopeParamForGear( Params->SurfaceRideEnterAngleForGear );
	DrawConditionLine( RawAngle > AngleThreshold,
		FString::Printf( TEXT( "床の傾斜   : %.1f / 必要 %.1f deg (平滑化後 %.1f)" ), RawAngle, AngleThreshold, SmoothedAngle ) );

	const float Speed = MovementComp->Velocity.Size();
	const float MinSpeed = PlayerCharacter->GetSlopeParamForGear( Params->SurfaceRideMinSpeedForGear );
	DrawConditionLine( Speed >= MinSpeed,
		FString::Printf( TEXT( "速度       : %.0f / 必要 %.0f" ), Speed, MinSpeed ) );

	ImGui::TextDisabled( "しきい値はギア %d 時点の解決値（***ForGear が空ならスカラー）", PlayerCharacter->GetCurrentChargeGearIndex() );

	ImGui::SeparatorText( "内部状態" );
	const FVector Up = PlayerCharacter->GetSurfaceRideUp();
	const FVector GravityDir = MovementComp->GetGravityDirection();
	ImGui::Text( "Smoothed Normal : %.2f, %.2f, %.2f", FloorNormal.X, FloorNormal.Y, FloorNormal.Z );
	ImGui::Text( "Gravity Up      : %.2f, %.2f, %.2f", Up.X, Up.Y, Up.Z );
	ImGui::Text( "Gravity Dir     : %.2f, %.2f, %.2f", GravityDir.X, GravityDir.Y, GravityDir.Z );
	ImGui::Text( "WalkableFloor   : %.1f deg", MovementComp->GetWalkableFloorAngle() );

	// 姿勢の戻し。重力が戻り切っても姿勢が残る（＝傾いたまま落下する）不具合の切り分け用。
	// 解除後は「傾き」が 0 へ落ちて「姿勢の戻し」が no になるのが正常
	const float PostureTiltDeg = FMath::RadiansToDegrees( FMath::Acos( FMath::Clamp(
		static_cast< float >( FVector::DotProduct( PlayerCharacter->GetActorUpVector(), FVector::UpVector ) ), -1.0f, 1.0f ) ) );
	ImGui::Text( "姿勢の傾き      : %.1f deg / 戻し中 %s", PostureTiltDeg,
		PlayerCharacter->IsSurfaceRidePostureApplied() && !bRiding ? "YES" : "no" );

	// 入力基準の潰れ具合。カメラ前方と床法線のなす角の sin で、しきい値を割ると前フレームの基準を持ち回す。
	// ここが小さいまま滞留する場所は「カメラが壁を正面から見ている」＝入力方向が暴れやすい地点
	const float DegenerateSin = PlayerCharacter->GetSurfaceRideInputDegenerateSin();
	DrawConditionLine( DegenerateSin >= Params->SurfaceRideInputDegenerateSin,
		FString::Printf( TEXT( "入力基準の潰れ : %.2f / しきい値 %.2f%s" ), DegenerateSin, Params->SurfaceRideInputDegenerateSin,
			DegenerateSin >= Params->SurfaceRideInputDegenerateSin ? TEXT( "" ) : TEXT( " (縮退＝前フレームの基準を維持中)" ) ) );
	// 上り坂アシスト（ゾーンの bUphillAssist）。効いていない理由が「エリア外」「登っていない」のどちらかを見分けられるよう、
	// エリア在籍・登り成分・解決後の適用値を並べて出す
	const bool bAssistZone = PlayerCharacter->IsInSurfaceRideUphillAssistZone();
	const bool bAssistActive = PlayerCharacter->IsSurfaceRideUphillAssistActive();
	if ( !bAssistZone )
	{
		ImGui::TextDisabled( "上り坂アシスト : 対象エリア外（SurfaceRideZone の bUphillAssist）" );
	}
	else
	{
		DrawConditionLine( bAssistActive, FString::Printf( TEXT( "上り坂アシスト : %s (機能 %s / 登り成分 %.2f)" ),
			bAssistActive ? TEXT( "適用中" ) : TEXT( "待機（面沿い未発動）" ),
			Params->bEnableSurfaceRideUphillAssist ? TEXT( "ON" ) : TEXT( "OFF" ),
			PlayerCharacter->GetSurfaceRideUphillRate() ) );
		ImGui::Text( "  戻る力 %.2f / 登り加速 %.0f / ダッシュ倍率 %.2f",
			PlayerCharacter->GetSlopeParamForGear( bAssistActive
				? Params->SurfaceRideAssistSlideBackScaleForGear : Params->SurfaceRideSlideBackScaleForGear ),
			PlayerCharacter->GetSlopeParamForGear( Params->SurfaceRideAssistUphillAccelForGear ),
			PlayerCharacter->GetSurfaceRideUphillAssistDashSpeedScale() );
	}

	// --- エリア退出の解除 ---
	// 解除（チャージ終了・速度削り・モーション）は急斜面の上で出たときだけ走る。見送られた退出も理由付きで残す
	const auto& ExitRecord = PlayerCharacter->GetSurfaceRideZoneExitRecord();
	if ( ExitRecord.Time < 0.0f )
	{
		ImGui::TextDisabled( "エリア退出 : 未発生" );
	}
	else
	{
		const float Now = PlayerCharacter->GetWorld() ? PlayerCharacter->GetWorld()->GetTimeSeconds() : 0.0f;
		DrawConditionLine( ExitRecord.bReleased, FString::Printf(
			TEXT( "エリア退出 : %.1f 秒前 / %s / 面 %.1f deg / %s" ),
			Now - ExitRecord.Time, ExitRecord.bWasAirborne ? TEXT( "空中" ) : TEXT( "接地" ),
			ExitRecord.SurfaceAngleDeg, ExitRecord.bReleased ? TEXT( "解除した" ) : TEXT( "解除は見送り" ) ) );
		if ( ExitRecord.bReleased )
		{
			ImGui::Text( "%s", TCHAR_TO_UTF8( *FString::Printf(
				TEXT( "  速度 %.0f → %.0f / 上向き %.0f → %.0f" ),
				ExitRecord.SpeedBefore, ExitRecord.SpeedAfter, ExitRecord.UpBefore, ExitRecord.UpAfter ) ) );
		}
		else
		{
			// 見送り＝速度もチャージも触らず、チャージジャンプモーションも出ない
			ImGui::Text( "%s", TCHAR_TO_UTF8( *FString::Printf(
				TEXT( "  理由 : %s（速度 %.0f のまま）" ), *ExitRecord.SkipReason, ExitRecord.SpeedBefore ) ) );
		}
	}

	ImGui::TextDisabled( "エリア形状の表示は SurfaceRideZone の bDebugDraw" );

	// --- ガクッ検出ログ ---
	// 速度が跳ねた（または実移動量が速度から外れた）瞬間をラッチしたもの。CMC / モジュール / 面沿いのどの区間が
	// 動かしたかで原因の層が分かる（実移動が速度より大きいフレームは CMC の接地移動側が原因）
	ImGui::SeparatorText( "ガクッ検出ログ" );
	if ( Params->MovementSpikeLogThresholdSpeed <= 0.0f )
	{
		ImGui::TextDisabled( "無効（MovementSpikeLogThresholdSpeed が 0）" );
	}
	else
	{
		ImGui::Text( "しきい値 %.0f cm/s, %.0f deg / frame",
			Params->MovementSpikeLogThresholdSpeed, Params->MovementSpikeLogThresholdTurnDeg );
		ImGui::TextDisabled( "CMC 欄は Tick 外の書き込み（ジャンプ・ブースト・被弾）も含む" );
		if ( ImGui::Button( "ログクリア" ) ) { PlayerCharacter->ClearMovementSpikeLog(); }

		const auto& Log = PlayerCharacter->GetMovementSpikeLog();
		if ( Log.Num() == 0 )
		{
			ImGui::TextDisabled( "検出なし" );
		}
		for ( const auto& Sample : Log )
		{
			// ラッチした理由を明示する（大きさ／向き／実移動のどれで引っかかったか）。
			// 「主因」を大きさだけで決めると、向きや実移動で引っかかった件を誤読してしまうため
			const float SpeedTh = Params->MovementSpikeLogThresholdSpeed;
			const float TurnTh = Params->MovementSpikeLogThresholdTurnDeg;
			FString Reasons;
			if ( FMath::Abs( Sample.DeltaCmc ) >= SpeedTh || FMath::Abs( Sample.DeltaModules ) >= SpeedTh
				|| FMath::Abs( Sample.DeltaRide ) >= SpeedTh || FMath::Abs( Sample.DeltaPushWind ) >= SpeedTh ) Reasons += TEXT( "大きさ " );
			if ( Sample.TurnCmc >= TurnTh || Sample.TurnModules >= TurnTh
				|| Sample.TurnRide >= TurnTh || Sample.TurnPushWind >= TurnTh ) Reasons += TEXT( "向き " );
			if ( Sample.bSeparated ) Reasons += TEXT( "★離脱 " );
			if ( Sample.bOverlappingGeometry ) Reasons += TEXT( "★めり込み " );
			if ( Reasons.IsEmpty() ) Reasons = TEXT( "実移動 " );

			ImGui::Text( "%s", TCHAR_TO_UTF8( *FString::Printf(
				TEXT( "[%.2fs] 検出:%s (%.1f ms)" ), Sample.Time, *Reasons, Sample.DeltaTimeMs ) ) );
			ImGui::Text( "%s", TCHAR_TO_UTF8( *FString::Printf(
				TEXT( "      大きさ CMC %+.0f / モジュール %+.0f / 面沿い %+.0f / 押し出し風 %+.0f" ),
				Sample.DeltaCmc, Sample.DeltaModules, Sample.DeltaRide, Sample.DeltaPushWind ) ) );
			ImGui::Text( "%s", TCHAR_TO_UTF8( *FString::Printf(
				TEXT( "      向き   CMC %.1f / モジュール %.1f / 面沿い %.1f / 押し出し風 %.1f deg" ),
				Sample.TurnCmc, Sample.TurnModules, Sample.TurnRide, Sample.TurnPushWind ) ) );
			ImGui::Text( "%s", TCHAR_TO_UTF8( *FString::Printf(
				TEXT( "      速度 %.0f / 実移動 %.0f  ライド:%s 接地:%s ゾーン:%d" ),
				Sample.SpeedIn, Sample.ActualSpeed, Sample.bRiding ? TEXT( "ON" ) : TEXT( "off" ),
				Sample.bOnGround ? TEXT( "ON" ) : TEXT( "空中" ), Sample.ZoneCount ) ) );
			ImGui::Text( "%s", TCHAR_TO_UTF8( *FString::Printf(
				TEXT( "      床 %.1f deg / 登坂 %.0f deg / 追従遅れ %.1f deg / 歩行上限 %.0f" ),
				Sample.FloorAngleDeg, Sample.WalkableAngle, Sample.GravityLagDeg, Sample.MaxWalkSpeed ) ) );
			// 実移動が速度から外れているとき、接線側が膨らむ＝斜面での移動量の伸び、
			// 法線側が膨らむ＝縦方向の位置補正（段差処理・床スナップ・押し出し）
			ImGui::Text( "%s", TCHAR_TO_UTF8( *FString::Printf(
				TEXT( "      内訳 実移動[法線 %+.0f / 接線 %.0f] 速度[法線 %+.0f / 接線 %.0f]" ),
				Sample.MoveUpSpeed, Sample.MoveTangentSpeed, Sample.VelUpSpeed, Sample.VelTangentSpeed ) ) );
			ImGui::Text( "%s", TCHAR_TO_UTF8( *FString::Printf(
				TEXT( "      床距離 %.1f / 段差上限 %.0f / 押し出し上限 %.0f / 重力倍率 %.2f" ),
				Sample.FloorDist, Sample.MaxStepHeight, Sample.MaxDepenetration, Sample.GravityScale ) ) );
			// 実移動の出どころ。モジュール＝モジュール更新＋面沿い、押し出し風＝敵の押し出し＋風＋地面の吸い込み、
			// Tick外＝CMC・ルートモーション・アニメ通知・他アクター。いずれも Velocity には出ない
			ImGui::Text( "%s", TCHAR_TO_UTF8( *FString::Printf(
				TEXT( "      出どころ モジュール %.0f / 押し出し風 %.0f / Tick外 %.0f  風 %.0f  RootMotion:%s %s" ),
				Sample.MoveModulesSpeed, Sample.MovePushWindSpeed, Sample.MoveOutsideTickSpeed, Sample.WindSpeed,
				Sample.bAnimRootMotion ? TEXT( "ON" ) : TEXT( "off" ), *Sample.MontageName ) ) );
			// 地形へのめり込み。ここが出ていれば「スタック → CMC の押し出しで打ち上げ」の並びを裏付ける
			DrawConditionLine( !Sample.bOverlappingGeometry, FString::Printf(
				TEXT( "      めり込み %s  深さ %.1f cm  %s" ),
				Sample.bOverlappingGeometry ? TEXT( "あり" ) : TEXT( "なし" ),
				Sample.PenetrationDepth, *Sample.PenetrationName ) );
			ImGui::Separator();
		}
	}
}

void PlayerWindow::DrawTabAttackLog()
{
	ImGui::Separator();
	AttackLog.DrawContents();
}

#endif // !UE_BUILD_SHIPPING
