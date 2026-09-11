// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#include "GodSlashExCameraMode.h"

#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Engine/EngineTypes.h"
#include "CollisionShape.h"

#include "PRJ_TIDE_P0/Actors/Character/Player/TidePlayerCharacter.h"
#include "PRJ_TIDE_P0/Data/Player/TidePlayerParamDataAsset.h"

void UGodSlashExCameraMode::OnActivated( const FMinimalViewInfo& LastViewInfo )
{
	// 直前のカメラ状態を始点にして、最初のカットへ滑らかに入る
	CurrentLocation = LastViewInfo.Location;
	CurrentRotation = LastViewInfo.Rotation;
	CurrentFOV = LastViewInfo.FOV > 0.0f ? LastViewInfo.FOV : FOV;
	bInitialized = true;

	FramingCutIndex = -1;	// 発動ごとに最初のカットから選び直す

	// ワイドカット演出は Push 直前（＝発動時点）の TPS カメラ姿勢を基準にし、
	// そこからの引き距離・高さだけでポーズを決める（以後変化しない）
	WideCutBaseLoc = CurrentLocation;
	WideCutBaseRot = CurrentRotation;
	WideCutBaseFOV = CurrentFOV;
}

void UGodSlashExCameraMode::UpdateCamera( const FCameraControlData& ControlData, FMinimalViewInfo& OutViewInfo )
{
	ATidePlayerCharacter* TidePlayer = Cast<ATidePlayerCharacter>( UGameplayStatics::GetPlayerCharacter( GetWorld(), 0 ) );
	const UTidePlayerParamDataAsset* Params = TidePlayer ? TidePlayer->PlayerParamData : nullptr;

	// 情報が取れない場合は現在値を維持する
	FVector DesiredLoc = CurrentLocation;
	FRotator DesiredRot = CurrentRotation;
	float DesiredFOV = FOV;

	FVector FocusLoc = FVector::ZeroVector;
	FVector TargetCenter = FVector::ZeroVector;
	float TargetRadius = 0.0f;
	int32 CutIndex = 0;
	bool bCutLanded = false;
	bool bWideCut = false;
	bool bWideCutFinishing = false;

	if ( TidePlayer && Params && TidePlayer->GetGodSlashCameraFraming( FocusLoc, TargetCenter, TargetRadius, CutIndex, bCutLanded, bWideCut, bWideCutFinishing ) )
	{
		const FVector Up = FVector::UpVector;

		// プレイヤーの注視基準（胸〜頭あたり）
		const float PlayerLookHeight = Params->GodSlashCamTwoShotPlayerLookHeight;
		const FVector PlayerPoint = TidePlayer->GetActorLocation() + Up * PlayerLookHeight;

		if ( bWideCut )
		{
			// 向きはそのまま、位置だけ後方＋上方へオフセットした固定ショットにする（切り抜け中は引いて上げた位置、
			// 戻り〜仕上げは基準へ近づけた位置）。プレイヤー・対象群の位置を参照しないので完全に静止する
			const float PullBack = bWideCutFinishing ? Params->GodSlashWideCutFinishDistance : Params->GodSlashWideCutPullBackDistance;
			const float PullUp   = bWideCutFinishing ? Params->GodSlashWideCutFinishHeight    : Params->GodSlashWideCutPullUpHeight;

			FVector ComputedLoc = WideCutBaseLoc - WideCutBaseRot.Vector() * PullBack + Up * PullUp;

			// 基準位置→引いた位置の間に地形があれば手前へ寄せる
			FCollisionQueryParams TraceParams( SCENE_QUERY_STAT( GodSlashWideCutCamCollision ), false );
			TraceParams.AddIgnoredActor( TidePlayer );
			FCollisionObjectQueryParams CamObjectParams;
			CamObjectParams.AddObjectTypesToQuery( ECC_WorldStatic );
			CamObjectParams.AddObjectTypesToQuery( ECC_WorldDynamic );
			const float ProbeRadius = FMath::Max( Params->GodSlashCamCollisionProbeRadius, 0.0f );

			{
				const FVector PivotToCam = ComputedLoc - WideCutBaseLoc;
				const float DesiredDist = PivotToCam.Size();
				if ( DesiredDist > KINDA_SMALL_NUMBER )
				{
					const FVector CamDir = PivotToCam / DesiredDist;
					FHitResult Hit;
					if ( GetWorld()->SweepSingleByObjectType(
						Hit, WideCutBaseLoc, ComputedLoc, FQuat::Identity,
						CamObjectParams,
						FCollisionShape::MakeSphere( ProbeRadius ),
						TraceParams ) )
					{
						const float HitDist = ( Hit.Location - WideCutBaseLoc ).Size();
						ComputedLoc = WideCutBaseLoc + CamDir * HitDist;
					}
				}
			}

			DesiredLoc = ComputedLoc;
			DesiredRot = WideCutBaseRot;
			DesiredFOV = WideCutBaseFOV;

			// カット単位の固定機構を使わないため、クラシック側へ戻った際に選び直しが起きるようリセットする
			FramingCutIndex = -1;
		}
		else
		{
			// プレイヤーと敵の 2 ショット。注視点は両者の中間
			const FVector Mid = ( PlayerPoint + FocusLoc ) * 0.5f;

			// プレイヤー→敵の水平方向と、それに直交する水平ベクトル（横から 2 体を捉える）
			FVector Horiz = FocusLoc - PlayerPoint;
			Horiz.Z = 0.0f;
			const float PlanarSep = Horiz.Size();		// 水平方向の 2 点間距離
			Horiz = Horiz.GetSafeNormal();
			if ( Horiz.IsNearlyZero() )
			{
				Horiz = TidePlayer->GetActorForwardVector().GetSafeNormal2D();
			}
			const FVector Perp = FVector::CrossProduct( Horiz, Up ).GetSafeNormal();

			const float VertSep = FMath::Abs( FocusLoc.Z - PlayerPoint.Z );	// 高さ方向の 2 点間距離

			// FOV と 2 点間隔から、両者が必ず画面に収まる引き距離を算出する
			// （横は水平 FOV、縦はアスペクトから求めた垂直 FOV で、厳しい方を採用）
			const float FitFraction = FMath::Clamp( Params->GodSlashCamTwoShotFitFraction, 0.1f, 1.0f );

			float Aspect = 16.0f / 9.0f;	// 取得失敗時のフォールバック
			if ( const APlayerController* PC = Cast<APlayerController>( TidePlayer->GetController() ) )
			{
				int32 SizeX = 0;
				int32 SizeY = 0;
				PC->GetViewportSize( SizeX, SizeY );
				if ( SizeX > 0 && SizeY > 0 )
				{
					Aspect = (float)SizeX / (float)SizeY;
				}
			}

			// 0 以下なら共通 FOV、それも無効なら 75。フィット計算と出力の両方に使う
			const float UseFovDeg = ( Params->GodSlashCamTwoShotFOV > 0.0f )
				? Params->GodSlashCamTwoShotFOV
				: ( FOV > 0.0f ? FOV : 75.0f );
			const float HalfFovH = FMath::DegreesToRadians( UseFovDeg * 0.5f );
			const float HalfFovV = FMath::Atan( FMath::Tan( HalfFovH ) / FMath::Max( Aspect, 0.01f ) );

			// 余白を確保するため有効画角を FitFraction 倍に絞り、その画角に 2 点の半幅が収まる距離を求める
			const float TanH = FMath::Tan( FMath::Max( HalfFovH * FitFraction, 0.01f ) );
			const float TanV = FMath::Tan( FMath::Max( HalfFovV * FitFraction, 0.01f ) );

			// 巨大敵対応：対象の水平半径ぶん半幅を広げて引き距離を増やす
			// （点フレーミングだと巨大な体が画面からはみ出す／カメラが近すぎる）
			const float SizePad = TargetRadius * FMath::Clamp( Params->GodSlashCamTargetSizeInfluence, 0.0f, 1.0f );
			const float FitDistH = ( PlanarSep * 0.5f + SizePad ) / TanH;
			const float FitDistV = ( VertSep * 0.5f + SizePad ) / TanV;

			// --- 毎カットで画を変える（左右交互・前後変化＋ゆらぎ）---
			// カット番号で決定論的に散らすので再現性がある
			auto Hash01 = []( int32 N, int32 Salt ) -> float
			{
				const float V = FMath::Sin( (float)N * 12.9898f + (float)Salt * 78.233f ) * 43758.5453f;
				return V - FMath::FloorToFloat( V );	// 小数部 = [0,1)
			};

			float Side = 1.0f;
			if ( Params->bGodSlashCamAlternateSide )
				Side = ( CutIndex % 2 == 0 ) ? 1.0f : -1.0f;

			// +1 = 敵の奥側から（敵が手前）、-1 = プレイヤーの後ろ側から（プレイヤーが手前）
			const float HorizSign = ( Hash01( CutIndex, 5 ) > 0.5f ) ? 1.0f : -1.0f;

			// ゆらぎ（ヨー＝回り込み／高さ／距離は引き方向のみ）
			const float YawJitter    = ( Hash01( CutIndex, 1 ) * 2.0f - 1.0f ) * Params->GodSlashCamYawVariation;
			const float HeightJitter = ( Hash01( CutIndex, 2 ) * 2.0f - 1.0f ) * Params->GodSlashCamHeightVariation;
			const float DistJitter   = Hash01( CutIndex, 3 ) * Params->GodSlashCamDistanceVariation;	// 外側のみ

			// 注視点を Mid からプレイヤーか敵寄りへずらし（主体を画面の端寄りに置く）、全体を上下に振る
			const float LookBias = ( Hash01( CutIndex, 4 ) * 2.0f - 1.0f ) * Params->GodSlashCamLookAtBias;
			const FVector LookAt = FMath::Lerp( PlayerPoint, FocusLoc, 0.5f + LookBias )
				+ Up * Params->GodSlashCamTwoShotLookAtZOffset;

			// 側面（Perp）と斬り込み方向（Horiz）をブレンドして斜めショットにする。
			// 0 → 純側面、0.5 → 45度斜め、1 → 純前後
			const float FB   = FMath::Clamp( Params->GodSlashCamForwardBlend, 0.0f, 1.0f );
			const float Dist = FMath::Max3( Params->GodSlashCamTwoShotDistance, FitDistH, FitDistV ) + DistJitter;
			const float Height = Params->GodSlashCamTwoShotHeight + HeightJitter;

			// 巨大敵の体内から撮らないための「敵バウンズ球」の押し出し半径
			const float KeepOut = TargetRadius * FMath::Max( Params->GodSlashCamTargetClearanceScale, 0.0f );

			// --- 衝突トレース設定（地面クランプ・壁遮蔽チェックで共用）---
			FCollisionQueryParams TraceParams( SCENE_QUERY_STAT( GodSlashCamCollision ), false );
			if ( TidePlayer ) TraceParams.AddIgnoredActor( TidePlayer );

			// 対象は「本物の壁（地形）」だけ。ByChannel だと敵カプセルを壁と誤認し、引き寄せがカメラを
			// 敵表面へ引き込む。敵による隠れは IsPlayerHiddenByTarget が扱う
			FCollisionObjectQueryParams CamObjectParams;
			CamObjectParams.AddObjectTypesToQuery( ECC_WorldStatic );
			CamObjectParams.AddObjectTypesToQuery( ECC_WorldDynamic );

			const float ProbeRadius = FMath::Max( Params->GodSlashCamCollisionProbeRadius, 0.0f );

			// 起点は注視点（足元寄りで低い）ではなくカメラ高さのピボット。LookAt 起点だと地面へ向かうレイが
			// 凹凸の凸を拾ってカメラを地面際へ引き込むが、高い起点なら高さを保ったまま手前へ寄るだけで済む
			const FVector CollisionPivot = Mid + Up * Height;

			// 巨大敵では敵バウンズ球の外へ「同じ視線方向のまま距離を伸ばして」出す
			// （半径方向へ押し出すと視線・注視点が崩れて何も映らない絵になる）
			auto MakeCamLoc = [&]( float S, float H ) -> FVector
			{
				FVector CDir = ( Perp * S * ( 1.0f - FB ) + Horiz * H * FB ).GetSafeNormal();
				CDir = CDir.RotateAngleAxis( YawJitter, Up );

				const FVector RayOrigin = Mid + Up * Height;	// カメラはこの点から CDir 方向へ引く
				float UseDist = Dist;

				// 敵バウンズ球に入るなら、同じ CDir 方向のまま球との far 交点まで距離を伸ばして外へ出す
				if ( KeepOut > 0.0f )
				{
					const FVector OC = TargetCenter - RayOrigin;
					const float Tca = FVector::DotProduct( OC, CDir );	// 視線上の敵中心射影
					const float D2 = OC.SizeSquared() - Tca * Tca;		// 視線から敵中心までの距離^2
					const float R2 = KeepOut * KeepOut;
					if ( D2 < R2 )
					{
						const float Tfar = Tca + FMath::Sqrt( R2 - D2 );
						UseDist = FMath::Max( UseDist, Tfar );
					}
				}
				return RayOrigin + CDir * UseDist;
			};

			// 候補は [元, Side反転, HorizSign反転, 両反転] の順で試す
			struct FCamCandidate { float S; float H; };
			const FCamCandidate Candidates[] = {
				{  Side,  HorizSign },
				{ -Side,  HorizSign },
				{  Side, -HorizSign },
				{ -Side, -HorizSign },
			};

			// ピボットから見て地形に遮られている／潜っているか
			auto IsCandidateBlocked = [&]( const FVector& CandLoc ) -> bool
			{
				FHitResult Hit;
				return GetWorld()->SweepSingleByObjectType(
					Hit, CollisionPivot, CandLoc, FQuat::Identity,
					CamObjectParams,
					FCollisionShape::MakeSphere( ProbeRadius ),
					TraceParams );
			};

			// プレイヤーが地形で隠れないか（敵による隠れは IsPlayerHiddenByTarget が担当）
			auto IsPlayerOccluded = [&]( const FVector& CandLoc ) -> bool
			{
				FHitResult Hit;
				return GetWorld()->LineTraceSingleByObjectType(
					Hit, CandLoc, PlayerPoint, CamObjectParams, TraceParams );
			};

			// 巨大敵はコリジョンが見た目より小さくトレースでは取りこぼすため、
			// camera→player 線分が視覚バウンズ球の中ほどを横切るかで判定する
			auto IsPlayerHiddenByTarget = [&]( const FVector& CandLoc ) -> bool
			{
				if ( TargetRadius <= 0.0f ) return false;
				const FVector AB = PlayerPoint - CandLoc;
				const float L2 = AB.SizeSquared();
				if ( L2 < KINDA_SMALL_NUMBER ) return false;
				const float T = FVector::DotProduct( TargetCenter - CandLoc, AB ) / L2;
				if ( T >= 0.85f ) return false;	// プレイヤーのすぐ手前で球際＝隠れではない
				const FVector Closest = CandLoc + AB * FMath::Clamp( T, 0.0f, 1.0f );
				return FVector::DistSquared( Closest, TargetCenter ) < TargetRadius * TargetRadius;
			};

			// 敵の体で隠れない(＋3、最優先) ＞ 地形遮蔽なし(＋2) ＞ 注視先が遮られない(＋1)。
			// さらにプレイヤー側へ回り込んでいるほど僅かに加点して敵の裏を避ける
			auto ScoreCandidate = [&]( const FVector& CandLoc ) -> float
			{
				float Score = 0.0f;
				if ( !IsPlayerHiddenByTarget( CandLoc ) ) Score += 3.0f;
				if ( !IsPlayerOccluded( CandLoc ) )       Score += 2.0f;
				if ( !IsCandidateBlocked( CandLoc ) )     Score += 1.0f;
				if ( TargetRadius > 0.0f )
				{
					// 敵中心から見て、カメラがプレイヤーと同じ側にいるほど高い（敵の裏に回り込まない）
					const FVector ToCam = ( CandLoc - TargetCenter ).GetSafeNormal();
					const FVector ToPlayer = ( PlayerPoint - TargetCenter ).GetSafeNormal();
					Score += 0.5f * FVector::DotProduct( ToCam, ToPlayer );	// [-0.5, +0.5]
				}
				return Score;
			};

			// 同点なら初回カットだけ開始位置に近い順（急なスイング回避）、それ以外は候補配列の優先順
			float ChosenS = Side;
			float ChosenH = HorizSign;
			{
				float BestScore = -1.0f;
				float BestTie = TNumericLimits<float>::Max();
				for ( int32 i = 0; i < (int32)UE_ARRAY_COUNT( Candidates ); ++i )
				{
					const FVector CandidateLoc = MakeCamLoc( Candidates[i].S, Candidates[i].H );
					const float Score = ScoreCandidate( CandidateLoc );
					const float Tie = ( CutIndex == 0 )
						? FVector::DistSquared( CandidateLoc, CurrentLocation )
						: (float)i;	// 配列順＝左右交互の優先順
					if ( Score > BestScore + KINDA_SMALL_NUMBER
						|| ( FMath::IsNearlyEqual( Score, BestScore ) && Tie < BestTie ) )
					{
						BestScore = Score;
						BestTie = Tie;
						ChosenS = Candidates[i].S;
						ChosenH = Candidates[i].H;
					}
				}
			}

			FVector ChosenLoc = MakeCamLoc( ChosenS, ChosenH );

			// 選ばれたアングルでもピボット→カメラの間に地形があれば、めり込み回避のためヒット点まで引き寄せる。
			// 寄せ距離には下限を設けて俯角が立ちすぎる“真上化”を防ぎ、下限を割るほど近い場合はめり込みを優先する
			{
				const FVector PivotToCam = ChosenLoc - CollisionPivot;
				const float DesiredDist = PivotToCam.Size();
				if ( DesiredDist > KINDA_SMALL_NUMBER )
				{
					const FVector CamDir = PivotToCam / DesiredDist;
					FHitResult Hit;
					if ( GetWorld()->SweepSingleByObjectType(
						Hit, CollisionPivot, ChosenLoc, FQuat::Identity,
						CamObjectParams,
						FCollisionShape::MakeSphere( ProbeRadius ),
						TraceParams ) )
					{
						const float HitDist = ( Hit.Location - CollisionPivot ).Size();
						const float MinDist = FMath::Min( FMath::Max( Params->GodSlashCamMinCollisionDistance, 0.0f ), DesiredDist );
						const float UseDist = FMath::Max( HitDist, MinDist );	// 下限より近くへは寄せない（真上化防止）
						ChosenLoc = CollisionPivot + CamDir * UseDist;
					}
				}
			}

			// 出力はカット単位で固定し、カット中はプレイヤーがワープ移動してもカメラを動かさない
			// （固定ショットの中を斬り抜ける絵）
			const FVector ComputedLoc = ChosenLoc;
			const FRotator ComputedRot = ( LookAt - ChosenLoc ).Rotation();
			const float ComputedFOV = UseFovDeg;

			// 切り替えのタイミング：true なら次の対象へ飛ぶ瞬間、
			// false なら貫通した瞬間（貫通前は直前のポーズを保持する）
			const bool bSwitchOnLaunch = Params->bGodSlashCamSwitchOnLaunch;
			const bool bDoLock = ( FramingCutIndex != CutIndex ) && ( bSwitchOnLaunch || bCutLanded );
			if ( bDoLock )
			{
				FramedLoc = ComputedLoc;
				FramedRot = ComputedRot;
				FramedFOV = ComputedFOV;
				FramingCutIndex = CutIndex;
			}

			if ( FramingCutIndex >= 0 )
			{
				// 直近で固定したポーズを次の貫通まで保持する
				DesiredLoc = FramedLoc;
				DesiredRot = FramedRot;
				DesiredFOV = FramedFOV;
			}
			else
			{
				// まだ一度も貫通していない（最初の斬りかかり中）＝寄っていくライブ構図
				DesiredLoc = ComputedLoc;
				DesiredRot = ComputedRot;
				DesiredFOV = ComputedFOV;
			}
		}
	}

	// 切り替えを少しだけ補間してハードカットを避ける
	const float InterpSpeed = Params ? Params->GodSlashCamInterpSpeed : 12.0f;
	const float Dt = ControlData.DeltaTime;
	if ( bInitialized && InterpSpeed > 0.0f && Dt > 0.0f )
	{
		CurrentLocation = FMath::VInterpTo( CurrentLocation, DesiredLoc, Dt, InterpSpeed );
		CurrentRotation = FMath::RInterpTo( CurrentRotation, DesiredRot, Dt, InterpSpeed );
		CurrentFOV = FMath::FInterpTo( CurrentFOV, DesiredFOV, Dt, InterpSpeed );
	}
	else
	{
		CurrentLocation = DesiredLoc;
		CurrentRotation = DesiredRot;
		CurrentFOV = DesiredFOV;
		bInitialized = true;
	}

	OutViewInfo.Location = CurrentLocation;
	OutViewInfo.Rotation = CurrentRotation;
	OutViewInfo.FOV = CurrentFOV;
}
