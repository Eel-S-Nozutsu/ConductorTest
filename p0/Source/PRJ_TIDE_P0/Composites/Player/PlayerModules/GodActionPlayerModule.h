// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "PRJ_TIDE_P0/Composites/Player/PlayerModules/TidePlayerModule.h"
#include "PRJ_TIDE_P0/Utilities/AutomaticTimer.h"

#include "GodActionPlayerModule.generated.h"

class ATidePlayerCharacter;
class UGodActionSubModule;
class UGodSlashSubModule;
class UGodGuidanceSubModule;
class UGodFrolicSubModule;
struct FDamageInfo;

// 神鳥検証モード（DebugFlags「神鳥検証」ON 時）の 3 択神技。Frolic=鳥の戯れ / Guidance=鳥の導き / Possession=神鳥の憑依
enum class EGodArt : uint8
{
	Frolic,		// 戯れ（鳥が飛んで一閃）
	Guidance,	// 導き（前方へ巨大な鳥が突進）
	Possession,	// 憑依（現行 GodSlash＝プレイヤーが実行）
	Count
};

// 神技（ゲージ技）傘モジュール。共通要素の「神技ゲージ」を所有し、満タンで各神技を発動する。
// 実体は UGodActionSubModule 派生で、本モジュールはゲージの所有・蓄積演出と、
// 入力／状態問い合わせ／演出カメラ／HUD／デバッグの橋渡しに徹する
UCLASS()
class PRJ_TIDE_P0_API UGodActionPlayerModule : public UTidePlayerModule
{
	GENERATED_BODY()

public:
	virtual void Initialize( ATidePlayerCharacter* InOwner ) override;
	virtual void OnModuleUpdate( float DeltaTime ) override;

	// --- 入力要求（TidePlayerController から委譲。各神技サブモジュールへ橋渡し） ---

	void RequestLockOnStart();		// L2 押下。Idle ならマルチロックオン開始（消費は実行時）
	void RequestLockOnRelease();	// L2 解除。ゲージ・対象があれば発動、無ければキャンセル
	void RequestCancel();			// B 入力。ロックオン中ならキャンセルして Idle へ

	// 構え中なら解除し、L2 を離すまで再突入を禁止する（押しっぱなしで構えへ戻らないよう押し直しを要求）
	void NotifyOwnerDamaged();

	// 以下は神鳥検証モードの構え中のみ作用し、それ以外は何もしない（呼び出し側で通常アクションへ回す）
	void RequestGodArtExecute();				// R2：選択中の神技を発動
	void RequestGodArtCycle( int32 Delta );		// 十字キー左右でカーソル移動（-1 左 / +1 右）
	void RequestGodArtSelectAt( int32 ArtIndex );	// X／Y／B で直接選択（0 戯 / 1 導 / 2 依）

	// L スティック水平で 1 段だけ動かし、ニュートラルへ戻るまで再入力しない（フリック 1 回＝1 段）。
	// 0 付近を渡すと再アームする
	void RequestGodArtStickSelect( float AxisX );

	// --- 神鳥検証モード（3択メニュー）の状態問い合わせ（HUD 用） ---

	bool IsGodBirdModeEnabled() const;			// DebugFlags「神鳥検証」
	bool IsGodArtSelecting() const { return bGodArtSelecting; }
	int32 GetSelectedArtIndex() const { return (int32)SelectedArt; }	// 0=戯れ / 1=導き / 2=憑依

	// X／Y／B の直接選択モードか。入力の振り分け（カーソル移動系の無効化・X/B のマスク）と HUD 表示に使う
	bool IsFaceButtonSelectEnabled() const;

	// --- ゲージ（神技共通） ---

	// 攻撃ヒット時に呼ぶ。DamageInfo の攻撃種別・ギアから加算量を決定する
	void OnDealtDamage( const FDamageInfo& DamageInfo );

	// 全経路（攻撃ヒット・被弾・加速ギミック取得）の共通入口。玉の演出が ON なら OrbStartWorldLoc から
	// 飛ばして到達時に加算、OFF なら即時加算する
	void AddGauge( float Amount, const FVector& OrbStartWorldLoc, float OrbRadiusScale = 1.0f, float OrbAlphaScale = 1.0f, bool bAnchorOrbToOwner = false );

	float GetGauge() const { return CurrentGauge; }
	float GetGaugeRate() const;		// 0.0〜1.0
	bool IsGaugeFull() const;

	// Fraction は消費割合 0..1（1=全消費）。デバッグの無限ゲージ時は消費しない
	void ConsumeGauge( float Fraction );

	// HUD が描く飛行中の玉のスナップショット。「到達でゲージ加算」はゲームプレイなので本モジュールが所有し
	// （HUD が無い shipping でもゲージは正しく増える）、HUD はこの情報で見た目だけを描く
	struct FGodGaugeOrbView
	{
		FVector StartWorldLoc = FVector::ZeroVector;	// 出発点（被弾箇所のワールド座標）
		float Progress = 0.0f;							// 飛行進捗 0..1
		float Lateral = 0.0f;							// 発生点→ゲージの線に対する横ぶれ
		float RadiusScale = 1.0f;
		float AlphaScale = 1.0f;
	};
	void GetGaugeOrbViews( TArray<FGodGaugeOrbView>& Out ) const;

	// --- 状態問い合わせ（各神技サブモジュールへ橋渡し） ---

	bool IsActive() const;
	bool IsLockingOn() const;
	bool IsSlashing() const;

	// 戯れ／導きの鳥が出ている間は別個体が出るため、常駐追従鳥を隠す判定に使う
	bool IsFrolicActive() const;
	bool IsGuidanceActive() const;

	// 発動確定後の専有フェーズ（憑依の一閃 Slashing）中か。構え選択中は false＝通常アクション自由で、
	// 戯れ／導きは発動後すぐ自由行動へ戻るため専有扱いしない。アクションのブロック判定はすべてこれで行う
	bool IsExecuting() const;

	// ワイドカット演出の切り抜け中（Loop）か。通常ヒットストップの誤発火抑止ガードに使う
	bool IsGodSlashWideCutLoopPassthrough() const;

	// --- HUD 表示用 ---
	int32 GetLockedTargetCount() const;
	int32 GetMaxLockOnCount() const;	// 現在のギア段階で可能な最大数
	void GetLockedTargetLocations( TArray<FVector>& OutLocations ) const;	// レティクル描画用

	// 演出カメラ（UGodSlashExCameraMode）へフレーミング情報を渡す。発動中なら true
	bool GetCameraFraming( FVector& OutFocusLocation, FVector& OutTargetCenter, float& OutTargetRadius, int32& OutCutIndex, bool& bOutCutLanded, bool& bOutWideCut, bool& bOutWideCutFinishing ) const;

#if !UE_BUILD_SHIPPING
public:
	void DrawDebugImGui();
#endif

private:
	// 到達した玉のぶんだけゲージへ加算する
	void UpdateGaugeOrbs( float DeltaTime );

	// --- 神鳥検証モード（3択メニュー） ---
	void BeginGodArtSelect();
	void CancelGodArtSelect();
	void ExecuteSelectedArt();
	void CycleSelection( int32 Delta );		// -1 左 / +1 右。両端で折り返す
	void SetSelectedArt( EGodArt NewArt );

	// 構え中の効果一式：ワールドスロー（プレイヤーは CustomTimeDilation で補正）と落下の緩和（重力スケールを
	// 下げて空中滞空で狙えるようにする）。神鳥自身をスロー対象外にする補正は GodBirdPlayerModule 側が担う
	void EnterGodArtStance();
	void ExitGodArtStance();

	bool IsPlayerAirborne() const;	// 足元の地形トレース。構え中の飛行→着地判定に使う

	// 構え中（移動禁止モード）の水平慣性を指数減衰させ、チャージスライドのまま滑るのを抑える
	void UpdateGodArtStanceBraking( float DeltaTime );

	// 構えモーション（GodSlashStart→終了後は GodSlashLoop）。移動禁止モードのときだけ流す
	void PlayGodArtStanceStartMontage();
	void UpdateGodArtStanceMontage( float DeltaTime );
	void StopGodArtStanceMontage();
	bool ShouldPlayStanceMontage() const;
	float GetGodArtStanceMontagePlayRate() const;	// プレイヤーのスローを相殺して実時間で等速再生する倍率

private:
	float CurrentGauge = 0.0f;

	// 飛行中のエネルギー玉（被弾箇所→左下ゲージ）。到達時に Gain を加算する。
	// bEnableGodGaugeOrbEffect が OFF のときは使わず OnDealtDamage で即時加算する
	struct FGodGaugeOrb
	{
		FVector StartWorldLoc = FVector::ZeroVector;
		float Gain = 0.0f;
		float Elapsed = 0.0f;
		float Duration = 0.0f;
		float Lateral = 0.0f;		// 発生点→ゲージの線に対する横ぶれ
		float RadiusScale = 1.0f;
		float AlphaScale = 1.0f;

		// true の間は毎フレーム StartWorldLoc をオーナー位置＋AnchorLocalOffset へ更新する。ワールド固定だと
		// ブースト等の高速移動で発生源が画面上を暴れたり、カメラ背後に回って投影に失敗する
		bool bAnchorToOwner = false;
		FVector AnchorLocalOffset = FVector::ZeroVector;
	};
	TArray<FGodGaugeOrb> GaugeOrbs;

	// 各神技の実体。傘は更新・入力・デバッグを橋渡しし、以下は型付き参照で個別の問い合わせに使う
	UPROPERTY( Transient )
	TArray<TObjectPtr<UGodActionSubModule>> Techniques;

	UPROPERTY( Transient )
	TObjectPtr<UGodSlashSubModule> SlashModule;			// 憑依（一閃）

	UPROPERTY( Transient )
	TObjectPtr<UGodGuidanceSubModule> GuidanceModule;	// 導き（前方へ巨大な鳥が突進）

	UPROPERTY( Transient )
	TObjectPtr<UGodFrolicSubModule> FrolicModule;		// 戯れ（鳥が自律で飛び回って範囲攻撃）

	// --- 神鳥検証モード（3択神技メニュー） ---
	EGodArt SelectedArt = EGodArt::Frolic;	// 発動後もリセットしない＝セッション中保持
	bool bGodArtSelecting = false;
	bool bGodArtStickArmed = true;			// true＝次のフリックを受け付ける
	bool bStanceBlockedUntilRelease = false;	// 被弾で解除した後、L2 を離すまで再突入を禁止する

	// --- 構え効果（スロー＋落下緩和） ---
	bool bGodArtStanceActive = false;
	float SavedPlayerGravityScale = 1.0f;	// 適用前の値。解除時に戻す
	bool bStanceFlying = false;				// 空中構えのため MOVE_Flying にしている最中か

	// --- 構えモーション（移動禁止モード時に流す GodSlashStart→GodSlashLoop） ---
	bool bGodArtStanceMontageActive = false;
	bool bGodArtStanceLoopStarted = false;
	FAutomaticTimer GodArtStanceMontageTimer;		// GodSlashStart の残り。満了で Loop へ移す
	FAutomaticTimer GodArtStanceBrakeRampTimer;		// 慣性ブレーキの効きを 0→1 へ立ち上げる
	bool bStanceBrakeParamsApplied = false;			// CMC の摩擦・ブレーキを上書き済みか

#if !UE_BUILD_SHIPPING
	// 構えブレーキの入出力速度（cm/秒）。「前フレームの Out」と「今フレームの In」がズレていれば、
	// ブレーキの後に別処理が velocity を押し戻している＝滑りの犯人が他にいる、と切り分けられる
	float DebugStanceBrakeSpeedIn = 0.0f;
	float DebugStanceBrakeSpeedOut = 0.0f;
	float DebugStanceBrakeAlpha = 0.0f;		// 立ち上がり係数（0＝効いていない／1＝フル）
#endif
};
