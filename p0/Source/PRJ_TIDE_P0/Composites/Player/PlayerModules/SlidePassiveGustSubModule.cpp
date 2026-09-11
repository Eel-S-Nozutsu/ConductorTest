// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#include "SlidePassiveGustSubModule.h"

#include "Engine/World.h"
#include "Components/CapsuleComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"

#include "PRJ_TIDE_P0/Actors/Character/Player/TidePlayerCharacter.h"
#include "PRJ_TIDE_P0/Composites/Player/PlayerModules/SlidePassivePlayerModule.h"
#include "PRJ_TIDE_P0/Data/Player/TidePlayerParamDataAsset.h"
#include "PRJ_TIDE_P0/Data/Player/PlayerNiagaraTags.h"
#include "PRJ_TIDE_P0/Data/Niagara/NiagaraSystemListDataAsset.h"

#if !UE_BUILD_SHIPPING
#include "imgui.h"
#endif

namespace
{
	constexpr float GustTurnEpsilonDeg = 1.0f;	// これ未満の delta は方向なしとみなす

	// 足元印（PASSIVE_MARK）の正規化年齢レイアウト：0.0〜0.15 が IN、0.85〜1.0 が OUT、中間がホールド帯。
	// IN/OUT 区間中に LifeTime を変えると正規化年齢が飛ぶため、操作はホールド帯でのみ行う
	constexpr float GustMarkOutStartNorm = 0.85f;

	// バフ arm 中は HOLD 帯に留め続けて OUT に到達させない（竜巻の蓄積印と同じ規約）
	constexpr float GustMarkHoldKeepNorm  = 0.70f;
	constexpr float GustMarkHoldResetNorm = 0.50f;
}

bool USlidePassiveGustSubModule::IsEnabled() const
{
	const UTidePlayerParamDataAsset* Params = GetParams();
	return Params && Params->bEnableSlidePassiveGust;
}

void USlidePassiveGustSubModule::OnTrackingStarted()
{
	ResetDetection();
}

void USlidePassiveGustSubModule::OnTrajectoryReset()
{
	ResetDetection();
}

void USlidePassiveGustSubModule::ResetDetection()
{
	bHasLastFoot = false;
	CurrentSegSign = 0;
	SegTurnDeg = 0.0f;
	SegDistance = 0.0f;
	ArcChainCount = 0;
	LastArcSign = 0;
	SamplesSinceLastArc = 0;
	ResetStickFlick();
}

void USlidePassiveGustSubModule::OnSampleAdded( const FVector& Foot, float SignedTurnDeltaDeg )
{
	const UTidePlayerParamDataAsset* Params = GetParams();
	if ( !Params ) return;

	// フラグ ON の間は入力（↓→↑）で成立させるため、軌跡検出は行わない
	if ( Params->bGustUseStickFlickTrigger ) return;

	// 弧ごとのジッタ除けに移動距離を積算する
	if ( bHasLastFoot )
	{
		SegDistance += FVector::Dist2D( Foot, LastFoot );
	}
	LastFoot = Foot;
	bHasLastFoot = true;

	if ( ArcChainCount > 0 )
	{
		// 次の弧が来るまでに窓を超えたらリセットする（古い切り返しを引きずらない）
		if ( ++SamplesSinceLastArc > Params->GustMaxArcSamples )
		{
			ResetDetection();
			return;
		}
	}

	const float Threshold = Params->GustArcAngleThreshold;
	const float MinDist = Params->GustMinArcDistance;

	const int32 DeltaSign = ( SignedTurnDeltaDeg > GustTurnEpsilonDeg ) ? 1
		: ( SignedTurnDeltaDeg < -GustTurnEpsilonDeg ? -1 : 0 );

	if ( DeltaSign == 0 )
	{
		// ほぼ直進。弧は途切れないが積算もしない
		return;
	}

	if ( CurrentSegSign == 0 )
	{
		// 弧の開始
		CurrentSegSign = DeltaSign;
		SegTurnDeg = SignedTurnDeltaDeg;
		SegDistance = 0.0f;
		return;
	}

	const int32 RequiredArcs = FMath::Max( 2, Params->GustRequiredArcCount );

	// 前の弧と逆向き（交互）ならチェーンを伸ばし、そうでなければこの弧から数え直す。
	// 規定本数に達したら true（＝発動）を返す
	auto CountQualifiedArc = [&]( int32 ArcSign ) -> bool
	{
		if ( ArcChainCount == 0 || ArcSign != -LastArcSign )
		{
			ArcChainCount = 1;
		}
		else
		{
			++ArcChainCount;
		}
		LastArcSign = ArcSign;
		SamplesSinceLastArc = 0;
		return ArcChainCount >= RequiredArcs;
	};

	if ( DeltaSign == CurrentSegSign )
	{
		SegTurnDeg += SignedTurnDeltaDeg;	// 同方向：弧を継続して積算
	}
	else
	{
		// 逆方向への切り返し。直前までの弧が成立条件を満たしていたら 1 本として数える
		const bool bSegQualifies = ( FMath::Abs( SegTurnDeg ) >= Threshold ) && ( SegDistance >= MinDist );
		if ( bSegQualifies && CountQualifiedArc( CurrentSegSign ) )
		{
			TriggerGust();
			ResetDetection();
			return;
		}

		// 新しい弧を、切り返した方向で開始
		CurrentSegSign = DeltaSign;
		SegTurnDeg = SignedTurnDeltaDeg;
		SegDistance = 0.0f;
	}

	// ヘッディングが滑らかに反転して DeltaSign の切り返しが出ないケースの保険。
	// 現在の弧が最終弧の条件を満たした瞬間に成立させる（カウントはせず、完成時のみ発動）
	if ( ArcChainCount > 0 && CurrentSegSign == -LastArcSign
		&& ArcChainCount + 1 >= RequiredArcs
		&& FMath::Abs( SegTurnDeg ) >= Threshold && SegDistance >= MinDist )
	{
		TriggerGust();
		ResetDetection();
	}
}

void USlidePassiveGustSubModule::UpdateStickFlickDetection( float DeltaTime )
{
	const UTidePlayerParamDataAsset* Params = GetParams();
	if ( !Params || !Params->bGustUseStickFlickTrigger || !OwnerCharacter ) return;

	// 受け付け条件は軌跡版と同じ（完成インターバル中は止める）。チャージダッシュ猶予中も成立させない
	const bool bAcceptInput = OwnerModule && OwnerModule->IsTracking() && !OwnerModule->IsTrailCompletionLocked()
		&& OwnerCharacter->IsCharging();
	if ( !bAcceptInput )
	{
		ResetStickFlick();
		return;
	}

	// カメラ基準。Y の +=↑ / -=↓、X が横
	const FVector2D Input = OwnerCharacter->GetRawMovementInput();
	const float Threshold = FMath::Clamp( Params->GustStickFlickThreshold, 0.05f, 1.0f );

	if ( StickFlickPhase == EStickFlickPhase::WaitDown )
	{
		if ( Input.Y <= -Threshold )
		{
			StickFlickPhase = EStickFlickPhase::WaitNeutral;
			StickFlickWindowTimer = FMath::Max( 0.0f, Params->GustStickFlickWindowTime );
			StickFlickMaxLateral = 0.0f;
		}
		return;
	}

	// スティック 1 回転（倒したまま円周を回す）で成立しないよう、「横へ大きく倒れたら破棄」と
	// 「ニュートラル通過を要求」の 2 条件で弾く（回転は入力の大きさが下がらないので通過できない）
	StickFlickMaxLateral = FMath::Max( StickFlickMaxLateral, FMath::Abs( Input.X ) );

	const float MaxLateral = Params->GustStickFlickMaxLateral;
	if ( MaxLateral > 0.0f && StickFlickMaxLateral > MaxLateral )
	{
		ResetStickFlick();
		return;
	}

	if ( StickFlickPhase == EStickFlickPhase::WaitNeutral )
	{
		if ( Input.Size() <= Params->GustStickFlickNeutralThreshold )
		{
			StickFlickPhase = EStickFlickPhase::WaitUp;
		}
	}
	else if ( Input.Y >= Threshold )
	{
		// ニュートラル通過後の↑ → 成立
		ResetStickFlick();
		TriggerGust();
		return;
	}

	StickFlickWindowTimer -= DeltaTime;
	if ( StickFlickWindowTimer <= 0.0f )
	{
		ResetStickFlick();
	}
}

void USlidePassiveGustSubModule::ResetStickFlick()
{
	StickFlickPhase = EStickFlickPhase::WaitDown;
	StickFlickWindowTimer = 0.0f;
	StickFlickMaxLateral = 0.0f;
}

void USlidePassiveGustSubModule::TriggerGust()
{
	if ( !OwnerCharacter ) return;

	// 重ね掛け防止。次のチャージアクションで消費されるまで 1 回限り
	if ( OwnerCharacter->IsGustChargeBuffArmed() ) return;

	// 風まとい VFX。arm 中はずっと追従させたいので寿命固定せずループ前提で生成し、
	// バフ消費時に OnUpdate で破棄する
	if ( OwnerCharacter->NiagaraSystemDataAsset )
	{
		if ( UNiagaraSystem* Sys = OwnerCharacter->NiagaraSystemDataAsset->GetNiagaraSystem( PlayerNiagaraTags::PASSIVE_GUST_CLOAK ) )
		{
			StopGustCloak();

			GustCloakVFX = UNiagaraFunctionLibrary::SpawnSystemAttached(
				Sys,
				OwnerCharacter->GetRootComponent(),
				NAME_None,
				FVector::ZeroVector,
				FRotator::ZeroRotator,
				EAttachLocation::SnapToTarget,
				false,	// bAutoDestroy — バフ消費まで持続させ、こちらで破棄する
				true	// bAutoActivate
			);
		}
	}

	// バフ中であることを示す小さな印を足元に追従生成（バフ消費まで持続）
	SpawnGustFootMark();

	const bool bGearRaised = OwnerCharacter->ForceMaxChargeGear();

	// 火花は「ギアが上がった」印なので、実際に段が上がったときだけ出す。
	// ドリフト中でないと火花ループが出ないため、成立の演出として強制バーストを叩く
	if ( bGearRaised )
	{
		const UTidePlayerParamDataAsset* Params = GetParams();
		if ( Params && Params->bEnableGustSparkBurst ) OwnerCharacter->NotifyChargeDriftSparkBurst();
	}

	// 次の 1 チャージアクションに範囲攻撃バフを arm（単発）
	OwnerCharacter->ArmGustChargeBuff();

	// 通常は即フラッシュ＋インターバル、解除時まとめモードでは保留（竜巻と同じく親が判断する）
	if ( OwnerModule ) OwnerModule->NotifyFigureCompleted();

#if !UE_BUILD_SHIPPING
	TriggerFlashTimer = 1.5f;
#endif
}

void USlidePassiveGustSubModule::OnUpdate( float DeltaTime )
{
	UpdateStickFlickDetection( DeltaTime );	// ↓→↑ 入力での成立判定（フラグ OFF なら何もしない）

	// 風まとい・足元印は arm されている間だけ表示し、消費されたら片付ける
	const bool bBuffArmed = OwnerCharacter && OwnerCharacter->IsGustChargeBuffArmed();
	const UTidePlayerParamDataAsset* Params = GetParams();
	const float WorldTime = ( OwnerCharacter && OwnerCharacter->GetWorld() ) ? OwnerCharacter->GetWorld()->GetTimeSeconds() : GustFootMarkSpawnTime;

	if ( bBuffArmed )
	{
		// IN は自然再生させ、OUT 手前まで来たら LifeTime を再延長して HOLD のど真ん中へ戻す
		// （破棄・再スポーンしないので点滅もしない）
		if ( GustFootMarkVFX && !bGustFootMarkFadingOut && GustFootMarkLifeTime > 0.0f )
		{
			const float MarkAge = FMath::Max( 0.0f, WorldTime - GustFootMarkSpawnTime );
			if ( MarkAge / GustFootMarkLifeTime >= GustMarkHoldKeepNorm )
			{
				GustFootMarkLifeTime = MarkAge / GustMarkHoldResetNorm;
				GustFootMarkVFX->SetFloatParameter( TEXT( "LifeTime" ), GustFootMarkLifeTime );
			}
		}
	}
	else
	{
		if ( GustCloakVFX )
		{
			StopGustCloak();	// 風まといは即破棄
		}

		// 足元印は OUT アニメへ移行させて消す。ホールド帯なら正規化年齢を OutStartNorm へ合わせて
		// OUT を頭から 1 回だけ仕込み、以後は触らない（区間中に LifeTime を変えるとアニメが乱れる）
		if ( GustFootMarkVFX )
		{
			if ( !bGustFootMarkFadingOut )
			{
				// 基準は今設定されている LifeTime（HOLD 保持で伸びているため生成時の値ではない）
				const float MarkAge = FMath::Max( 0.0f, WorldTime - GustFootMarkSpawnTime );
				const float CurNorm = ( GustFootMarkLifeTime > 0.0f ) ? ( MarkAge / GustFootMarkLifeTime ) : 1.0f;

				float OutDuration = 0.0f;
				if ( CurNorm < GustMarkOutStartNorm )
				{
					// ホールド帯 → 正規化年齢を OUT 開始点に合わせる
					const float OutLife = MarkAge / GustMarkOutStartNorm;
					GustFootMarkVFX->SetFloatParameter( TEXT( "LifeTime" ), OutLife );
					GustFootMarkLifeTime = OutLife;
					OutDuration = OutLife - MarkAge;	// OUT 区間の実時間
				}
				else
				{
					// すでに自然 OUT 中。触らず、今の LifeTime の残りで消えるのを待つ
					OutDuration = FMath::Max( 0.0f, GustFootMarkLifeTime - MarkAge );
				}

				bGustFootMarkFadingOut = true;
				const float Margin = Params ? Params->GustBuffFootMarkFadeOutTime : 0.2f;
				GustFootMarkFadeTimer = OutDuration + FMath::Max( 0.0f, Margin );
			}
			else
			{
				GustFootMarkFadeTimer -= DeltaTime;
				if ( GustFootMarkFadeTimer <= 0.0f )
				{
					StopGustFootMark();	// OUT 尺が尽きても残っていれば確実に破棄
				}
			}
		}
	}

#if !UE_BUILD_SHIPPING
	if ( TriggerFlashTimer > 0.0f )
	{
		TriggerFlashTimer -= DeltaTime;
	}
#endif
}

void USlidePassiveGustSubModule::StopGustCloak()
{
	if ( GustCloakVFX )
	{
		GustCloakVFX->Deactivate();
		GustCloakVFX->DestroyComponent();
		GustCloakVFX = nullptr;
	}
}

void USlidePassiveGustSubModule::SpawnGustFootMark()
{
	if ( !OwnerCharacter || !OwnerCharacter->NiagaraSystemDataAsset ) return;

	const UTidePlayerParamDataAsset* Params = GetParams();
	if ( !Params ) return;

	// 印は竜巻と同じ単一アセットで、スケールを小さめにして出す
	UNiagaraSystem* Sys = OwnerCharacter->NiagaraSystemDataAsset->GetNiagaraSystem( PlayerNiagaraTags::PASSIVE_MARK );
	if ( !Sys ) return;

	StopGustFootMark();

	float FootOffsetZ = 0.0f;	// 足元（カプセル下端）へのオフセット
	if ( const UCapsuleComponent* Capsule = OwnerCharacter->GetCapsuleComponent() )
	{
		FootOffsetZ = -Capsule->GetScaledCapsuleHalfHeight();
	}

	// arm の瞬間に 1 回だけ生成（再スポーンしない＝点滅なし）。生成時 LifeTime で IN → ホールドを
	// 自然再生させ、以後は OnUpdate が HOLD 帯へ留め続ける
	GustFootMarkVFX = UNiagaraFunctionLibrary::SpawnSystemAttached(
		Sys,
		OwnerCharacter->GetRootComponent(),
		NAME_None,
		FVector( 0.0f, 0.0f, FootOffsetZ ),
		FRotator::ZeroRotator,
		EAttachLocation::KeepRelativeOffset,
		true,	// bAutoDestroy（寿命到達で自動破棄。消費時はその寿命を詰めて自然に消す）
		true	// bAutoActivate
	);

	bGustFootMarkFadingOut = false;
	GustFootMarkSpawnTime = OwnerCharacter->GetWorld() ? OwnerCharacter->GetWorld()->GetTimeSeconds() : 0.0f;
	GustFootMarkLifeTime = FMath::Max( 0.0f, Params->GustBuffFootMarkLifeTime );	// 以後 HOLD 保持で再延長される

	if ( GustFootMarkVFX )
	{
		GustFootMarkVFX->SetFloatParameter( TEXT( "LifeTime" ), Params->GustBuffFootMarkLifeTime );
		GustFootMarkVFX->SetFloatParameter( TEXT( "Scale" ), Params->GustBuffFootMarkScale );
	}
}

void USlidePassiveGustSubModule::StopGustFootMark()
{
	if ( GustFootMarkVFX )
	{
		GustFootMarkVFX->Deactivate();
		GustFootMarkVFX->DestroyComponent();
		GustFootMarkVFX = nullptr;
	}
}

#if !UE_BUILD_SHIPPING
void USlidePassiveGustSubModule::DrawDebugImGui()
{
	ImGui::SeparatorText( "Gust" );
	ImGui::Indent();

	const UTidePlayerParamDataAsset* Params = GetParams();
	const int32 RequiredArcs = Params ? FMath::Max( 2, Params->GustRequiredArcCount ) : 2;

	if ( Params && Params->bGustUseStickFlickTrigger )
	{
		ImGui::TextColored( ImVec4( 1.0f, 0.8f, 0.2f, 1.0f ), "検証モード: 左スティック↓→↑ で発動（軌跡検出OFF）" );
		ImGui::Text( "しきい値: %.2f / 猶予: %.2f 秒 / ニュートラル: %.2f / 横入力上限: %.2f",
			Params->GustStickFlickThreshold, Params->GustStickFlickWindowTime,
			Params->GustStickFlickNeutralThreshold, Params->GustStickFlickMaxLateral );

		const FVector2D Input = OwnerCharacter ? OwnerCharacter->GetRawMovementInput() : FVector2D::ZeroVector;
		ImGui::Text( "入力: 前後 %.2f / 横 %.2f（横最大 %.2f）", Input.Y, Input.X, StickFlickMaxLateral );

		switch ( StickFlickPhase )
		{
		case EStickFlickPhase::WaitNeutral:
			ImGui::TextColored( ImVec4( 0.3f, 1.0f, 0.5f, 1.0f ), "↓検知 → ニュートラル待ち（残り %.2f 秒）", StickFlickWindowTimer );
			break;
		case EStickFlickPhase::WaitUp:
			ImGui::TextColored( ImVec4( 0.3f, 1.0f, 0.5f, 1.0f ), "ニュートラル通過 → ↑待ち（残り %.2f 秒）", StickFlickWindowTimer );
			break;
		default:
			ImGui::TextColored( ImVec4( 0.6f, 0.6f, 0.6f, 1.0f ), "↓待ち" );
			break;
		}
	}
	else
	{
		if ( Params )
		{
			ImGui::Text( "弧しきい値: %.0f 度 / 必要本数: %d", Params->GustArcAngleThreshold, RequiredArcs );
		}

		ImGui::Text( "弧チェーン: %d / %d 本", ArcChainCount, RequiredArcs );
		ImGui::Text( "直近弧: %s", LastArcSign == 0 ? "-" : ( LastArcSign > 0 ? "L" : "R" ) );
		ImGui::Text( "現弧旋回: %.0f / 距離 %.0f", SegTurnDeg, SegDistance );
	}

	// エフェクト未用意でも判別できるよう色付きで表示する
	const bool bBuffArmed = OwnerCharacter && OwnerCharacter->IsGustChargeBuffArmed();
	const bool bBuffActionActive = OwnerCharacter && OwnerCharacter->IsGustBuffedChargeActionActive();

	if ( bBuffArmed )
	{
		ImGui::TextColored( ImVec4( 0.3f, 1.0f, 0.5f, 1.0f ), "突風バフ: ON（次のチャージアクションで発動）" );
	}
	else
	{
		ImGui::TextColored( ImVec4( 0.6f, 0.6f, 0.6f, 1.0f ), "突風バフ: OFF" );
	}

	if ( bBuffActionActive )
	{
		ImGui::TextColored( ImVec4( 1.0f, 0.8f, 0.2f, 1.0f ), "突風バフチャージアクション中（範囲攻撃判定）" );
	}
	else
	{
		ImGui::TextColored( ImVec4( 0.6f, 0.6f, 0.6f, 1.0f ), "突風バフチャージアクション中: いいえ" );
	}

	if ( TriggerFlashTimer > 0.0f )
	{
		ImGui::TextColored( ImVec4( 0.4f, 0.8f, 1.0f, 1.0f ), "突風発動!" );
	}

	ImGui::Unindent();
}
#endif
