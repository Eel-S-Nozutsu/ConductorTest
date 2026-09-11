// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.
#include "SlidePassiveTornadoSubModule.h"

#include "Engine/World.h"
#include "Components/CapsuleComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"

#include "PRJ_TIDE_P0/Actors/Character/Player/TidePlayerCharacter.h"
#include "PRJ_TIDE_P0/Actors/SlidePassive/SlidePassiveTornado.h"
#include "PRJ_TIDE_P0/Composites/Player/PlayerModules/SlidePassivePlayerModule.h"
#include "PRJ_TIDE_P0/Data/Player/TidePlayerParamDataAsset.h"
#include "PRJ_TIDE_P0/Data/Player/PlayerNiagaraTags.h"
#include "PRJ_TIDE_P0/Data/Niagara/NiagaraSystemListDataAsset.h"
#include "PRJ_TIDE_P0/Data/Character/TideCharacterDataAsset.h"
#include "PRJ_TIDE_P0/Data/Combat/AttackParameterRow.h"
#include "PRJ_TIDE_P0/Data/Tags/TideGameplayTags.h"

#if !UE_BUILD_SHIPPING
#include "DrawDebugHelpers.h"
#include "imgui.h"
#endif

namespace
{
	// 印の正規化年齢レイアウト（Gust の足元印と同一規約）：IN 0〜0.15 ／ HOLD 0.15〜0.85 ／ OUT 0.85〜1.0。
	// 解除待ちの間は HOLD 帯に留めたいが、LifeTime を巨大にすると IN で固まるため、OUT 手前まで来たら再延長する
	constexpr float MarkHoldKeepNorm  = 0.70f;	// ここまで来たら OUT 手前とみなす
	constexpr float MarkHoldResetNorm = 0.50f;	// 再延長後の正規化年齢（HOLD のど真ん中）

	// アセットが kill しない場合の保険破棄に足す猶予（秒）。
	// 正常時はアセット側の OUT / bAutoDestroy が先に片付けるため到達しない
	constexpr float VFXCleanupMargin = 0.5f;
	constexpr float FallbackTornadoDefaultLife = 8.0f;	// Params 欠落時の保険破棄に使う既定寿命

	// 小／大でタグを分ける（ギアは使わずタグ一致・各 1 行想定）。
	// 見つからなければ nullptr で、呼び出し側はアクター既定の Damage を使う
	const FPlayerAttackParameterRow* FindTornadoAttackRow( const ATidePlayerCharacter* Player, bool bIsLarge )
	{
		if ( !Player ) return nullptr;
		const UTideCharacterDataAsset* DataAsset = Player->GetCharacterData();
		if ( !DataAsset || !DataAsset->AttackParameterTable ) return nullptr;

		const FGameplayTag WantTag = bIsLarge
			? TAG_AttackType_Player_SlidePassiveTornadoLarge
			: TAG_AttackType_Player_SlidePassiveTornadoSmall;

		static const FString Context = TEXT( "SlidePassiveTornadoAttackParamLookup" );
		TArray<FPlayerAttackParameterRow*> Rows;
		DataAsset->AttackParameterTable->GetAllRows<FPlayerAttackParameterRow>( Context, Rows );
		for ( const FPlayerAttackParameterRow* Row : Rows )
		{
			if ( Row && Row->AttackTypeTag == WantTag )
			{
				return Row;
			}
		}
		return nullptr;
	}
}

bool USlidePassiveTornadoSubModule::IsEnabled() const
{
	const UTidePlayerParamDataAsset* Params = GetParams();
	return Params && Params->bEnableSlidePassive;
}

void USlidePassiveTornadoSubModule::UpdateDeferredMarkPersistence()
{
	if ( DeferredCircles.Num() == 0 || !OwnerCharacter ) return;
	UWorld* World = OwnerCharacter->GetWorld();
	if ( !World ) return;

	const float WorldTime = World->GetTimeSeconds();

	for ( FDeferredCircle& Circle : DeferredCircles )
	{
		UNiagaraComponent* Mark = Circle.MarkComp.Get();
		if ( !Mark || Circle.MarkLifeTime <= 0.0f ) continue;

		const float Age = FMath::Max( 0.0f, WorldTime - Circle.MarkSpawnTime );
		const float Norm = Age / Circle.MarkLifeTime;

		// IN〜HOLD は自然再生。OUT 手前まで来たら HOLD のど真ん中へ戻すよう LifeTime を延長し、
		// 正規化年齢が ResetNorm↔KeepNorm を行き来して OUT に到達しないようにする
		if ( Norm >= MarkHoldKeepNorm )
		{
			Circle.MarkLifeTime = Age / MarkHoldResetNorm;
			Mark->SetFloatParameter( TEXT( "LifeTime" ), Circle.MarkLifeTime );
		}
	}
}

void USlidePassiveTornadoSubModule::OnSampleAdded( const FVector& Foot, float SignedTurnDeltaDeg )
{
	const UTidePlayerParamDataAsset* Params = GetParams();
	if ( !Params ) return;

	AccumulatedTurnDeg += SignedTurnDeltaDeg;

	Trajectory.Add( Foot );
	CumulativeTurnDeg.Add( AccumulatedTurnDeg );

	// 古い点を間引いて上限を保つ（助走部分は捨ててよい）
	constexpr int32 MaxTrajectoryPoints = 256;
	if ( Trajectory.Num() > MaxTrajectoryPoints )
	{
		const int32 RemoveCount = Trajectory.Num() - MaxTrajectoryPoints;
		Trajectory.RemoveAt( 0, RemoveCount, EAllowShrinking::No );
		CumulativeTurnDeg.RemoveAt( 0, RemoveCount, EAllowShrinking::No );
	}

	// 直近で一周ぶん旋回した区間を探し、その区間だけで円の閉じを判定する（始点・助走に依存しない）
	const int32 LoopStartIndex = FindLoopStartIndex();
	if ( LoopStartIndex != INDEX_NONE )
	{
		FVector Center;
		float Radius;
		if ( DetectCircleClosure( LoopStartIndex, Center, Radius ) )
		{
			OnCircleCompleted( Center, Radius );

			// 連続で描けるよう軌跡をリセットし、現在地を起点に再開する
			ResetTrajectory();
			Trajectory.Add( Foot );
			CumulativeTurnDeg.Add( AccumulatedTurnDeg );
		}
	}
}

void USlidePassiveTornadoSubModule::OnTrackingStarted()
{
	// リセットして、次セッションの 1 個目の竜巻で再びエスコート演出を出す
	bTornadoEscortTriggered = false;
}

void USlidePassiveTornadoSubModule::OnTrajectoryReset()
{
	ResetTrajectory();
}

void USlidePassiveTornadoSubModule::OnTrackingStopped( bool bChargeReleased )
{
	// 溜めた円から順番に時間差で発生させる。
	// 解除時の完成演出は親が bPendingReleaseFlash を見てまとめて出すので、ここでは出さない
	ReleaseDeferredTornadoes();
	ResetTrajectory();
}

void USlidePassiveTornadoSubModule::OnUpdate( float DeltaTime )
{
	UpdateDeferredMarkPersistence();	// 蓄積印を竜巻が出るまで HOLD 帯に保持し続ける
	UpdatePendingTornadoes( DeltaTime );	// 予約済みはチャージ終了後でも遅延時間が来たら発生させる
	UpdateVFXCleanups( DeltaTime );		// 消滅をアセット任せにしている Niagara の保険破棄

	if ( CompletionFlashTimer > 0.0f )
	{
		CompletionFlashTimer -= DeltaTime;
	}

#if !UE_BUILD_SHIPPING
	if ( bDebugDrawTrajectory )
	{
		DrawDebugTrajectory();
	}
#endif
}

void USlidePassiveTornadoSubModule::ResetTrajectory()
{
	Trajectory.Reset();
	CumulativeTurnDeg.Reset();
	AccumulatedTurnDeg = 0.0f;
}

int32 USlidePassiveTornadoSubModule::FindLoopStartIndex() const
{
	const UTidePlayerParamDataAsset* Params = GetParams();
	if ( !Params ) return INDEX_NONE;
	if ( CumulativeTurnDeg.Num() < 2 ) return INDEX_NONE;

	const float Threshold = Params->SlidePassiveLoopAngleThreshold;
	const float CurrentTurn = CumulativeTurnDeg.Last();

	// 末尾から遡り、旋回量が一周ぶんに達する最も新しい開始点を返す（＝最小の閉ループ区間）
	for ( int32 j = CumulativeTurnDeg.Num() - 2; j >= 0; --j )
	{
		if ( FMath::Abs( CurrentTurn - CumulativeTurnDeg[j] ) >= Threshold )
		{
			return j;
		}
	}
	return INDEX_NONE;
}

bool USlidePassiveTornadoSubModule::EvaluateCircleFit( int32 StartIndex, FCircleFit& OutFit ) const
{
	if ( !Trajectory.IsValidIndex( StartIndex ) ) return false;

	// 一周区間（StartIndex〜末尾）の点だけで判定する。助走・始点側は除外
	const int32 PointCount = Trajectory.Num() - StartIndex;
	constexpr int32 MinPoints = 6;
	if ( PointCount < MinPoints ) return false;

	FVector Center = FVector::ZeroVector;	// 区間の重心を中心とみなす
	for ( int32 i = StartIndex; i < Trajectory.Num(); ++i )
	{
		Center += Trajectory[i];
	}
	Center /= (float)PointCount;

	float Radius = 0.0f;	// 中心からの平均半径
	for ( int32 i = StartIndex; i < Trajectory.Num(); ++i )
	{
		Radius += FVector::Dist2D( Trajectory[i], Center );
	}
	Radius /= (float)PointCount;

	if ( Radius <= KINDA_SMALL_NUMBER ) return false;

	// 円らしさ＝平均半径からの平均絶対偏差 ÷ 平均半径。弧＋小円のような
	// 「1 つの円ではない区間」は中心から遠い点と近い点が混ざるので大きくなる
	float Deviation = 0.0f;
	for ( int32 i = StartIndex; i < Trajectory.Num(); ++i )
	{
		Deviation += FMath::Abs( FVector::Dist2D( Trajectory[i], Center ) - Radius );
	}
	Deviation /= (float)PointCount;

	OutFit.Center = Center;
	OutFit.Radius = Radius;
	OutFit.RoundnessRatio = Deviation / Radius;
	// 閉じ具合＝始点と終点の距離 ÷ 平均半径。区間はほぼ一周ぶんなので、閉じていれば小さくなる
	OutFit.ClosureRatio = FVector::Dist2D( Trajectory[StartIndex], Trajectory.Last() ) / Radius;
	return true;
}

bool USlidePassiveTornadoSubModule::DetectCircleClosure( int32 StartIndex, FVector& OutCenter, float& OutRadius ) const
{
	const UTidePlayerParamDataAsset* Params = GetParams();
	if ( !Params ) return false;

	FCircleFit Fit;
	if ( !EvaluateCircleFit( StartIndex, Fit ) ) return false;

	// 小さすぎる旋回（その場ジッター）は無効。「小」マスク時は大の閾値が下限になる。
	// 成立しなかった区間では軌跡をリセットしないので、そのまま描き続ければ後から成立する
	if ( Fit.Radius < GetEffectiveMinCircleRadius() )
	{
		return false;
	}

	// 旋回量だけでは通ってしまう「閉じていない／円でない」区間を弾く（どちらも 0 で判定しない）
	if ( Params->SlidePassiveCircleClosureRatio > 0.0f && Fit.ClosureRatio > Params->SlidePassiveCircleClosureRatio )
	{
		return false;
	}
	if ( Params->SlidePassiveCircleRoundnessRatio > 0.0f && Fit.RoundnessRatio > Params->SlidePassiveCircleRoundnessRatio )
	{
		return false;
	}

	OutCenter = Fit.Center;
	OutRadius = Fit.Radius;
	return true;
}

float USlidePassiveTornadoSubModule::GetEffectiveMinCircleRadius() const
{
	const UTidePlayerParamDataAsset* Params = GetParams();
	if ( !Params ) return 0.0f;

	return Params->bEnableSlidePassiveSmallTornado
		? Params->SlidePassiveMinRadius
		: FMath::Max( Params->SlidePassiveMinRadius, Params->SlidePassiveLargeRadiusThreshold );
}

float USlidePassiveTornadoSubModule::ComputeCircleScale( float Radius, bool bIsLarge ) const
{
	const UTidePlayerParamDataAsset* Params = GetParams();
	if ( !Params ) return 1.0f;

	const float SmallScale = Params->SlidePassiveSmallScale;
	const float LargeScale = Params->SlidePassiveLargeScale;

	if ( !Params->bEnableSlidePassiveRadiusScale )
	{
		return bIsLarge ? LargeScale : SmallScale;
	}

	// 成立最小半径で SmallScale、上限半径以上で LargeScale、その間は線形補間。
	// 上限半径 0 は「大の閾値で頭打ち」の意味で、範囲が潰れている場合は上限側を採用する
	const float MinRadius = GetEffectiveMinCircleRadius();
	const float MaxRadius = ( Params->SlidePassiveScaleMaxRadius > 0.0f )
		? Params->SlidePassiveScaleMaxRadius
		: Params->SlidePassiveLargeRadiusThreshold;

	const float Range = MaxRadius - MinRadius;
	const float T = ( Range > KINDA_SMALL_NUMBER )
		? FMath::Clamp( ( Radius - MinRadius ) / Range, 0.0f, 1.0f )
		: 1.0f;

	return FMath::Lerp( SmallScale, LargeScale, T );
}

void USlidePassiveTornadoSubModule::OnCircleCompleted( const FVector& Center, float Radius )
{
	const UTidePlayerParamDataAsset* Params = GetParams();
	if ( !Params || !OwnerCharacter ) return;

	const bool bIsLarge = Radius >= Params->SlidePassiveLargeRadiusThreshold;

	// 見た目・判定の大きさは描いた円の半径に合わせる（アセット選択・寿命・ジャンプ倍率は大小で切り替え）
	const float Scale = ComputeCircleScale( Radius, bIsLarge );

	if ( Params->bSlidePassiveDeferOnRelease )
	{
		// 印だけ出してリボンは継続し、竜巻はチャージ解除時に発生させるため溜める。
		// 蓄積印は解除まで残し続け（長寿命）、竜巻発生時に残り寿命を竜巻へ合わせて延長する
		const float MarkLife = FMath::Max(
			( bIsLarge ? Params->SlidePassiveLargeLifeTime : Params->SlidePassiveSmallLifeTime ),
			Params->SlidePassiveDeferMarkLifeTime );

		FDeferredCircle Deferred;
		Deferred.Center = Center;
		Deferred.bIsLarge = bIsLarge;
		Deferred.Scale = Scale;
		Deferred.MarkComp = SpawnMarkEffect( Center, bIsLarge, MarkLife, Scale );
		Deferred.MarkSpawnTime = OwnerCharacter->GetWorld() ? OwnerCharacter->GetWorld()->GetTimeSeconds() : 0.0f;
		Deferred.MarkLifeTime = MarkLife;	// 以後 UpdateDeferredMarkPersistence で HOLD 保持のため再延長する
		DeferredCircles.Add( Deferred );
	}
	else
	{
		SpawnMarkEffect( Center, bIsLarge, -1.0f, Scale );

		// 神鳥は竜巻発生を待たず、印が出た時点で向かわせる（鳥が竜巻を生んだように見せる）。
		// 解除時まとめモードでは印だけ溜まっていくため、竜巻発生時（SpawnTornado）に起動する
		MaybeTriggerBirdEscort( Center );

		// 竜巻は遅延後に発生させるため予約する
		FPendingTornado Pending;
		Pending.Center = Center;
		Pending.bIsLarge = bIsLarge;
		Pending.Scale = Scale;
		Pending.TimeRemaining = FMath::Max( 0.0f, Params->SlidePassiveTornadoDelay );
		PendingTornadoes.Add( Pending );
	}

	// 通常は即・解除時まとめモードでは解除まで保留。判断は親が行う
	if ( OwnerModule ) OwnerModule->NotifyFigureCompleted();

	// デバッグ円の点滅表示用
	LastCompletedCenter = Center;
	LastCompletedRadius = Radius;
	LastCompletedScale = Scale;
	bLastCompletedIsLarge = bIsLarge;
	CompletionFlashTimer = 1.0f;
}

void USlidePassiveTornadoSubModule::ReleaseDeferredTornadoes()
{
	if ( DeferredCircles.Num() == 0 ) return;

	const UTidePlayerParamDataAsset* Params = GetParams();
	const float Interval = Params ? FMath::Max( 0.0f, Params->SlidePassiveDeferReleaseInterval ) : 0.0f;

	// 溜めた順に時間差で発生させる（i 番目を i*Interval 遅延）。
	// 各竜巻の発生まで印を残し、UpdatePendingTornadoes が発生時に残り寿命を竜巻へ合わせる
	for ( int32 i = 0; i < DeferredCircles.Num(); ++i )
	{
		const FDeferredCircle& Circle = DeferredCircles[i];

		FPendingTornado Pending;
		Pending.Center = Circle.Center;
		Pending.bIsLarge = Circle.bIsLarge;
		Pending.Scale = Circle.Scale;
		Pending.TimeRemaining = i * Interval;
		Pending.MarkComp = Circle.MarkComp;
		Pending.MarkSpawnTime = Circle.MarkSpawnTime;
		PendingTornadoes.Add( Pending );
	}

	DeferredCircles.Reset();
}

void USlidePassiveTornadoSubModule::UpdatePendingTornadoes( float DeltaTime )
{
	for ( int32 i = PendingTornadoes.Num() - 1; i >= 0; --i )
	{
		FPendingTornado& Pending = PendingTornadoes[i];
		Pending.TimeRemaining -= DeltaTime;
		if ( Pending.TimeRemaining <= 0.0f )
		{
			SpawnTornado( Pending.Center, Pending.bIsLarge, Pending.Scale );

			// 蓄積印は破棄・再スポーンせず、新 LifeTime = 経過時間 + 竜巻寿命 で竜巻と同時に消えるようにする。
			// HOLD 保持（UpdateDeferredMarkPersistence）は解除前の永続表示用で、ここで OUT 用の寿命に上書きする
			if ( UNiagaraComponent* Mark = Pending.MarkComp.Get() )
			{
				const UTidePlayerParamDataAsset* Params = GetParams();
				UWorld* World = OwnerCharacter ? OwnerCharacter->GetWorld() : nullptr;
				const float TornadoLife = Params
					? ( Pending.bIsLarge ? Params->SlidePassiveLargeLifeTime : Params->SlidePassiveSmallLifeTime )
					: FallbackTornadoDefaultLife;
				if ( World )
				{
					const float MarkAge = FMath::Max( 0.0f, World->GetTimeSeconds() - Pending.MarkSpawnTime );
					Mark->SetFloatParameter( TEXT( "LifeTime" ), MarkAge + TornadoLife );
				}

				// アセットが User.LifeTime で kill を配線し損ねた場合の保険
				RegisterVFXCleanup( Mark, TornadoLife + VFXCleanupMargin );
			}

			PendingTornadoes.RemoveAt( i );
		}
	}
}

void USlidePassiveTornadoSubModule::RegisterVFXCleanup( UNiagaraComponent* Comp, float Lifetime )
{
	if ( !Comp ) return;

	// 同一コンポーネントの二重登録を避け、締切だけ更新する
	for ( FTrackedVFX& Tracked : TrackedVFX )
	{
		if ( Tracked.Comp.Get() == Comp )
		{
			Tracked.TimeRemaining = Lifetime;
			return;
		}
	}

	FTrackedVFX Tracked;
	Tracked.Comp = Comp;
	Tracked.TimeRemaining = Lifetime;
	TrackedVFX.Add( Tracked );
}

void USlidePassiveTornadoSubModule::UpdateVFXCleanups( float DeltaTime )
{
	for ( int32 i = TrackedVFX.Num() - 1; i >= 0; --i )
	{
		FTrackedVFX& Tracked = TrackedVFX[i];
		UNiagaraComponent* Comp = Tracked.Comp.Get();
		if ( !Comp )
		{
			TrackedVFX.RemoveAt( i );	// 既にアセット側で片付いた
			continue;
		}

		Tracked.TimeRemaining -= DeltaTime;
		if ( Tracked.TimeRemaining <= 0.0f )
		{
			Comp->Deactivate();
			Comp->DestroyComponent();
			TrackedVFX.RemoveAt( i );
		}
	}
}

void USlidePassiveTornadoSubModule::DestroyAllTrackedVFX()
{
	auto DestroyComp = []( TWeakObjectPtr<UNiagaraComponent>& Weak )
	{
		if ( UNiagaraComponent* Comp = Weak.Get() )
		{
			Comp->Deactivate();
			Comp->DestroyComponent();
		}
		Weak = nullptr;
	};

	// 竜巻発生前で保留中の印も含め、追跡している全 VFX を片付ける
	for ( FDeferredCircle& Circle : DeferredCircles ) DestroyComp( Circle.MarkComp );
	for ( FPendingTornado& Pending : PendingTornadoes ) DestroyComp( Pending.MarkComp );
	for ( FTrackedVFX& Tracked : TrackedVFX )          DestroyComp( Tracked.Comp );

	DeferredCircles.Reset();
	PendingTornadoes.Reset();
	TrackedVFX.Reset();
}

void USlidePassiveTornadoSubModule::BeginDestroy()
{
	// CDO や GC 中の呼び出しで無効なコンポーネントを触らないようにする
	if ( !HasAnyFlags( RF_ClassDefaultObject ) )
	{
		DestroyAllTrackedVFX();
	}
	Super::BeginDestroy();
}

UNiagaraComponent* USlidePassiveTornadoSubModule::SpawnMarkEffect( const FVector& Center, bool bIsLarge, float LifeTimeOverride, float ScaleOverride )
{
	const UTidePlayerParamDataAsset* Params = GetParams();
	if ( !OwnerCharacter || !OwnerCharacter->NiagaraSystemDataAsset || !Params ) return nullptr;

	// 印は単一アセット。大小は Scale パラメータで調整する
	UNiagaraSystem* EffectSys = OwnerCharacter->NiagaraSystemDataAsset->GetNiagaraSystem( PlayerNiagaraTags::PASSIVE_MARK );
	if ( !EffectSys ) return nullptr;

	UNiagaraComponent* Effect = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		OwnerCharacter->GetWorld(),
		EffectSys,
		Center,
		FRotator::ZeroRotator,
		FVector::OneVector,
		true,	// bAutoDestroy（単発。寿命は LifeTime パラメータで制御）
		true	// bAutoActivate
	);

	if ( Effect )
	{
		const float Scale = ( ScaleOverride > 0.0f )
			? ScaleOverride
			: ( bIsLarge ? Params->SlidePassiveLargeScale : Params->SlidePassiveSmallScale );
		// 印は竜巻より遅延分だけ早く出るため、その分だけ寿命を延ばして竜巻と同時に消えるようにする
		// （LifeTimeOverride 指定時はそれを使う）
		const float BaseLife = bIsLarge ? Params->SlidePassiveLargeLifeTime : Params->SlidePassiveSmallLifeTime;
		const float LifeTime = ( LifeTimeOverride >= 0.0f )
			? LifeTimeOverride
			: BaseLife + FMath::Max( 0.0f, Params->SlidePassiveTornadoDelay );
		Effect->SetFloatParameter( TEXT( "LifeTime" ), LifeTime );
		Effect->SetFloatParameter( TEXT( "Scale" ), Scale );
	}

	return Effect;
}

void USlidePassiveTornadoSubModule::DebugSpawnTornadoInFront( bool bIsLarge, float ForwardDistance )
{
	if ( !OwnerCharacter ) return;

	FVector Forward = OwnerCharacter->GetActorForwardVector();
	Forward.Z = 0.0f;
	Forward = Forward.GetSafeNormal();

	// 竜巻は地面基準で発生するため足元 Z へ合わせる
	FVector Center = OwnerCharacter->GetActorLocation() + Forward * ForwardDistance;
	if ( const UCapsuleComponent* Capsule = OwnerCharacter->GetCapsuleComponent() )
	{
		Center.Z -= Capsule->GetScaledCapsuleHalfHeight();
	}

	SpawnTornado( Center, bIsLarge );
}

void USlidePassiveTornadoSubModule::SpawnTornado( const FVector& Center, bool bIsLarge, float ScaleOverride )
{
	const UTidePlayerParamDataAsset* Params = GetParams();
	if ( !OwnerCharacter || !OwnerCharacter->NiagaraSystemDataAsset || !Params ) return;

	UWorld* World = OwnerCharacter->GetWorld();
	if ( !World ) return;

	MaybeTriggerBirdEscort( Center );

	const FName Tag = bIsLarge ? PlayerNiagaraTags::PASSIVE_TORNADO_LARGE : PlayerNiagaraTags::PASSIVE_TORNADO_SMALL;
	UNiagaraSystem* EffectSys = OwnerCharacter->NiagaraSystemDataAsset->GetNiagaraSystem( Tag );

	const float LifeTime = bIsLarge ? Params->SlidePassiveLargeLifeTime : Params->SlidePassiveSmallLifeTime;
	// Scale は描いた円の半径由来（見た目＋判定カプセル＋巻き上げ高さに効く）
	const float Scale = ( ScaleOverride > 0.0f )
		? ScaleOverride
		: ( bIsLarge ? Params->SlidePassiveLargeScale : Params->SlidePassiveSmallScale );

	// ジャンプ強化倍率は見た目スケールと切り離し、小／大で固定値を使う
	const float JumpBoost = bIsLarge ? Params->WindJumpBoostMultiplierLarge : Params->WindJumpBoostMultiplierSmall;

	// アクターが設定されていれば、スリップダメージ判定付きで発生させる
	if ( Params->SlidePassiveTornadoClass )
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = OwnerCharacter;
		SpawnParams.Instigator = OwnerCharacter;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		ASlidePassiveTornado* Tornado = World->SpawnActor<ASlidePassiveTornado>(
			Params->SlidePassiveTornadoClass, Center, FRotator::ZeroRotator, SpawnParams );
		if ( Tornado )
		{
			// 行が無ければアクター既定値のまま
			if ( const FPlayerAttackParameterRow* AttackRow = FindTornadoAttackRow( OwnerCharacter, bIsLarge ) )
			{
				Tornado->Damage = OwnerCharacter->CalculateDamage( AttackRow->DamageMultiplier );
			}
			Tornado->Activate( EffectSys, Scale, LifeTime, bIsLarge, JumpBoost );
			return;
		}
	}

	// フォールバック: クラス未設定／スポーン失敗時は竜巻 Niagara のみ発生
	if ( EffectSys )
	{
		UNiagaraComponent* Effect = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			World, EffectSys, Center, FRotator::ZeroRotator, FVector::OneVector, true, true );
		if ( Effect )
		{
			Effect->SetFloatParameter( TEXT( "LifeTime" ), LifeTime );
			Effect->SetFloatParameter( TEXT( "Scale" ), Scale );

			RegisterVFXCleanup( Effect, LifeTime + VFXCleanupMargin );	// kill 未配線時の保険
		}
	}
}

void USlidePassiveTornadoSubModule::MaybeTriggerBirdEscort( const FVector& Center )
{
	if ( bTornadoEscortTriggered ) return;

	const UTidePlayerParamDataAsset* Params = GetParams();
	if ( !Params || !Params->bGodBirdSlidePassiveEscort || !OwnerCharacter ) return;

	// セッション 1 個目の竜巻に限り起動する（リセットは OnTrackingStarted）。
	// 前セッションの演出が続行中でも、フラグで多重起動を防ぐ
	if ( OwnerCharacter->StartGodBirdTornadoEscort( Center ) )
	{
		bTornadoEscortTriggered = true;
	}
}

#if !UE_BUILD_SHIPPING
void USlidePassiveTornadoSubModule::DrawDebugTrajectory() const
{
	if ( !OwnerCharacter ) return;
	UWorld* World = OwnerCharacter->GetWorld();
	if ( !World ) return;

	// 軌跡の線
	const FColor LineColor = ( FMath::Abs( AccumulatedTurnDeg ) >= 180.0f ) ? FColor::Yellow : FColor::Cyan;
	for ( int32 i = 1; i < Trajectory.Num(); ++i )
	{
		DrawDebugLine( World, Trajectory[i - 1] + FVector( 0, 0, 5.0f ), Trajectory[i] + FVector( 0, 0, 5.0f ), LineColor, false, -1.0f, 0, 3.0f );
	}

	// 始点マーカー
	if ( Trajectory.Num() > 0 )
	{
		DrawDebugSphere( World, Trajectory[0] + FVector( 0, 0, 5.0f ), 20.0f, 8, FColor::Green, false, -1.0f, 0, 2.0f );
	}

	// 完成時のフィードバック（一定時間、印の円を強調表示）
	if ( CompletionFlashTimer > 0.0f )
	{
		const FColor FlashColor = bLastCompletedIsLarge ? FColor( 255, 120, 0 ) : FColor( 0, 200, 255 );
		DrawDebugCircle( World, LastCompletedCenter + FVector( 0, 0, 5.0f ), LastCompletedRadius, 48, FlashColor,
			false, -1.0f, 0, 6.0f, FVector( 1, 0, 0 ), FVector( 0, 1, 0 ), false );
	}
}

void USlidePassiveTornadoSubModule::DrawDebugImGui()
{
	ImGui::SeparatorText( "Tornado" );
	ImGui::Indent();

	ImGui::Checkbox( "軌跡をデバッグ描画", &bDebugDrawTrajectory );

	ImGui::Text( "軌跡点数: %d", Trajectory.Num() );
	ImGui::Text( "累積旋回角: %.1f 度", AccumulatedTurnDeg );

	// いま一周ぶんの区間ができているなら、その円フィットを表示する（閉じ・円らしさの閾値調整用）
	{
		FCircleFit Fit;
		const int32 LoopStart = FindLoopStartIndex();
		if ( LoopStart != INDEX_NONE && EvaluateCircleFit( LoopStart, Fit ) )
		{
			ImGui::Text( "候補区間: R=%.0f 閉じ=%.2f 円らしさ=%.2f", Fit.Radius, Fit.ClosureRatio, Fit.RoundnessRatio );
		}
		else
		{
			ImGui::Text( "候補区間: なし" );
		}
	}

	if ( const UTidePlayerParamDataAsset* Params = GetParams() )
	{
		ImGui::Text( "一周閾値: %.0f 度 / 閉じ<=%.2f / 円らしさ<=%.2f",
			Params->SlidePassiveLoopAngleThreshold,
			Params->SlidePassiveCircleClosureRatio,
			Params->SlidePassiveCircleRoundnessRatio );
		ImGui::Text( "小の竜巻: %s / 成立最小半径: %.0f",
			Params->bEnableSlidePassiveSmallTornado ? "ON" : "OFF（大のみ）", GetEffectiveMinCircleRadius() );

		if ( Params->bEnableSlidePassiveRadiusScale )
		{
			const float MaxRadius = ( Params->SlidePassiveScaleMaxRadius > 0.0f )
				? Params->SlidePassiveScaleMaxRadius
				: Params->SlidePassiveLargeRadiusThreshold;
			ImGui::Text( "半径連動Scale: 半径 %.0f→%.0f で %.2f→%.2f",
				GetEffectiveMinCircleRadius(), MaxRadius, Params->SlidePassiveSmallScale, Params->SlidePassiveLargeScale );
		}
		else
		{
			ImGui::Text( "半径連動Scale: OFF（大小2値固定）" );
		}

		const bool bDefer = Params->bSlidePassiveDeferOnRelease;
		ImGui::Text( "解除時まとめ竜巻: %s", bDefer ? "ON" : "OFF" );
		if ( bDefer )
		{
			ImGui::Text( "溜め中の円: %d", DeferredCircles.Num() );
		}
	}

	if ( CompletionFlashTimer > 0.0f )
	{
		ImGui::TextColored( ImVec4( 0.2f, 1.0f, 0.4f, 1.0f ), "円完成! (%s) R=%.0f Scale=%.2f",
			bLastCompletedIsLarge ? "大" : "小", LastCompletedRadius, LastCompletedScale );
	}

	ImGui::Unindent();
}
#endif
