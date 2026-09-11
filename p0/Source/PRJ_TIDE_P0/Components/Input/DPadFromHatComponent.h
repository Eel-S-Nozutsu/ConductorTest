// Copyright (c) 2026, Y.Hanashiro EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InputCoreTypes.h"

#include "DPadFromHatComponent.generated.h"

class APlayerController;

// ハット軸(POV)の値域の解釈方法。Raw Input 経由で D-Pad がどの値で来るかはデバイス依存なので実機確認に合わせて切り替える
UENUM( BlueprintType )
enum class EDPadHatValueMode : uint8
{
	// 値の大きさから自動判定。方向入力で |値| > AutoRawThreshold を一度でも観測したらRaw、それまではNormalized扱い
	Auto		UMETA( DisplayName = "Auto (自動判定)" ),
	// 生の 0〜8（0=上/2=右/4=下/6=左/8=無入力、奇数は斜め）
	Raw_0_8		UMETA( DisplayName = "Raw 0-8" ),
	// 正規化 0.0〜1.0（NormalizedScale を掛けてインデックス化）
	Normalized	UMETA( DisplayName = "Normalized 0-1" ),
};

// 8方向 + ニュートラル
UENUM( BlueprintType )
enum class EDPadDirection : uint8
{
	None,
	Up,
	UpRight,
	Right,
	DownRight,
	Down,
	DownLeft,
	Left,
	UpLeft
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam( FOnDPadDirectionChanged, EDPadDirection, Direction );

// Raw Input で来るハットスイッチ(D-Pad)の Axis1D 値を、UE 標準の Gamepad_DPad_* の合成キー入力へ変換するコンポーネント。
// PlayerController（または所有 Pawn）に付与すると、既存の IMC が Gamepad_DPad_* をバインドしていればそのまま反応する
UCLASS( ClassGroup = ( Custom ), meta = ( BlueprintSpawnableComponent ) )
class PRJ_TIDE_P0_API UDPadFromHatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDPadFromHatComponent();

	virtual void TickComponent( float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction ) override;

	// 実行時に監視する軸キーを差し替える（実機確認で軸が判明したとき用）
	UFUNCTION( BlueprintCallable, Category = "Tide|Input|DPad" )
	void SetHatAxisKey( FKey InKey ) { HatAxisKey = InKey; }

	// 現在デコードされている方向
	UFUNCTION( BlueprintPure, Category = "Tide|Input|DPad" )
	EDPadDirection GetCurrentDirection() const { return CurrentDirection; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay( const EEndPlayReason::Type EndPlayReason ) override;

public:
	// 方向が変化したとき（デバッグ/独自処理フック用）
	UPROPERTY( BlueprintAssignable, Category = "Tide|Input|DPad" )
	FOnDPadDirectionChanged OnDirectionChanged;

	// --- 設定 -----------------------------------------------------------

	// D-Padが来る軸キー。※実機で ShowDebug Input を見て確定させること（デフォルトは暫定）
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Tide|Input|DPad" )
	FKey HatAxisKey;

	// 値域の解釈モード
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Tide|Input|DPad" )
	EDPadHatValueMode ValueMode = EDPadHatValueMode::Auto;

	// Normalized時：インデックス = round(値 * NormalizedScale)。0.0-1.0を0-8に割るなら8
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Tide|Input|DPad", meta = ( ClampMin = "1.0" ) )
	float NormalizedScale = 8.0f;

	// Auto時：|値| がこの閾値を超えたらRaw(0-8)とみなす
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Tide|Input|DPad", AdvancedDisplay )
	float AutoRawThreshold = 1.5f;

	// 負値・範囲外を無入力とみなす（多くのデバイスでニュートラルが範囲外で来るため）
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Tide|Input|DPad", AdvancedDisplay )
	bool bTreatOutOfRangeAsNeutral = true;

	// Gamepad_DPad_* の合成キー入力を注入するか（falseなら OnDirectionChanged だけ発火）
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Tide|Input|DPad" )
	bool bInjectSyntheticDPadKeys = true;

	// 画面に生値とデコード結果を表示（軸/値域の特定に使う）
	UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Tide|Input|DPad|Debug" )
	bool bDrawDebug = false;

private:
	APlayerController* ResolvePlayerController();

	// 生の軸値から POV インデックス(0-7)を返す。無入力は INDEX_NONE
	int32 DecodePovIndex( float RawValue );

	// 4方向の押下状態を確定し、変化分を合成キーとして注入
	void ApplyDirections( bool bUp, bool bRight, bool bDown, bool bLeft );

	void InjectKey( const FKey& Key, bool bPressed );

	// 押しっぱなし解除（EndPlay/無効化時のスタック防止）
	void ReleaseAll();

	static EDPadDirection PovIndexToDirection( int32 PovIndex );

private:
	TWeakObjectPtr<APlayerController> CachedPC;

	// 前フレームの各方向押下状態（エッジ検出用）
	bool bPrevUp = false;
	bool bPrevRight = false;
	bool bPrevDown = false;
	bool bPrevLeft = false;

	EDPadDirection CurrentDirection = EDPadDirection::None;

	// Autoモードのラッチ：一度でも大きな値を観測したらRaw確定
	bool bAutoDetectedRaw = false;
};
