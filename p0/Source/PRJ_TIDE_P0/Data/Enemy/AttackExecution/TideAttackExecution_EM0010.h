// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "PRJ_TIDE_P0/Data/Enemy/AttackExecution/TideAttackExecution.h"
#include "TideAttackExecution_EM0010.generated.h"

class ABossShockwaveActor;
class ABossMineHazard;
class AHaloHazard;
class UProjectileProfile;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UDecalComponent;
class UNiagaraSystem;
class UNiagaraComponent;

/**
 * 振り向き薙ぎ払い
 *
 * ルートモーション無しで背後を薙ぎ払う攻撃。アニメはその場で再生されるためカプセルは動かず、
 * 最終ポーズ (向き・位置) だけがカプセルからずれる。そのまま終わるとモンタージュが抜けた瞬間に
 * 元の姿勢へ戻ってしまうので、終了時にメッシュのルートが積んだ差分をカプセルへ移し替える。
 *
 * NOTE: アニメがroot回転付きで再出力されたら素の「モンタージュ攻撃」で足りる。その時点で撤去する
 */
UCLASS(meta = (DisplayName = "振り向き薙ぎ払い"))
class PRJ_TIDE_P0_API UTurnSweepAttackExecution : public UTideAttackExecution
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere)
	TObjectPtr<UAnimMontage> Montage;

	// 差分を読むボーン ※アニメが移動/回転を焼いている階層
	UPROPERTY(EditAnywhere, meta = (DisplayName = "参照ボーン"))
	FName SourceBoneName = TEXT("root");

	// NOTE: 位置と向きは分けて適用できない。片方だけ当てると、カプセル原点から離れている
	// メッシュが振り回されて弧を描く。必ず1つの変換としてまとめて解く

	virtual UAnimMontage* GetMontage() const override { return Montage; }
	virtual void OnAttackBegin(AEnemyCharacter* Enemy) override;
	virtual void OnAttackTick(AEnemyCharacter* Enemy, float DeltaTime) override;
	virtual void OnAttackEnd(AEnemyCharacter* Enemy) override;

private:

	// 参照ボーンのコンポーネント空間変換。アクターの移動/回転からは独立して読める
	FTransform ReadBoneComp(const AEnemyCharacter* Enemy) const;

	// ポーズが薄れたぶんをカプセルへ埋め戻し、見た目を動かさないまま実体を最終姿勢へ寄せる
	void ApplyCommit(AEnemyCharacter* Enemy) const;

	UFUNCTION()
	void HandleMontageBlendingOut(UAnimMontage* InMontage, bool bInterrupted);

	TWeakObjectPtr<AEnemyCharacter> CachedEnemy;
	TWeakObjectPtr<UAnimInstance> CachedAnimInstance;

	// ブレンドアウト開始時点の参照ボーンとアクター姿勢。ここが「見せたい最終姿勢」の基準
	FTransform CommitBoneComp = FTransform::Identity;
	FVector    BaseLocation   = FVector::ZeroVector;
	float      BaseYaw        = 0.0f;
	bool       bCommitting    = false;

	// commit中だけ止めるCMCの回転駆動。中断経路も含めて必ず戻す
	bool bSavedUseControllerDesiredRotation = false;

};

/**
 * ホーミングミサイル攻撃
 *
 * AnimNotifyがMissileStart/MissileShotタグを発火することを前提
 * MissileShot1回につき左右2発スポーンしMissileCount回到達でEndセクションへジャンプ
 */
UCLASS(meta = (DisplayName = "ホーミングミサイル攻撃"))
class PRJ_TIDE_P0_API UMissileAttackExecution : public UTideAttackExecution
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere)
	TObjectPtr<UAnimMontage> Montage;

	// 弾プロファイル
	UPROPERTY(EditAnywhere)
	TObjectPtr<UProjectileProfile> MissileProfile;

	// 1Notifyで左右2発スポーン この回数だけループ
	UPROPERTY(EditAnywhere)
	int32 MissileCount = 4;

	UPROPERTY(EditAnywhere)
	float MissileSpawnBackOffset = 80.0f;

	UPROPERTY(EditAnywhere)
	float MissileSpawnHeightOffset = 100.0f;

	UPROPERTY(EditAnywhere)
	float MissileSideOffset = 400.0f;

	UPROPERTY(EditAnywhere)
	float MissileDeployHeightOffset = 0.0f;

	// 左右の展開先の高さ差 (左が+ 右が-)
	UPROPERTY(EditAnywhere)
	float MissileDeployHeightStagger = 100.0f;

	virtual UAnimMontage* GetMontage() const override { return Montage; }
	virtual void OnAttackBegin(AEnemyCharacter* Enemy) override;
	virtual void OnAttackEvent(AEnemyCharacter* Enemy, FGameplayTag EventTag) override;
	virtual void OnAttackEnd(AEnemyCharacter* Enemy) override;

private:

	void SpawnMissilePair(AEnemyCharacter* Enemy);

	int32 MissileFiredCount = 0;

};

/**
 * ショックウォール攻撃
 * AnimNotifyがShockwallStart / ShockwallLandタグを発火することを前提とする。
 * ShockwallStartでジャンプし着地を検知、ShockwallLandでShockwaveActorをスポーンする。
 */
UCLASS(meta = (DisplayName = "ショックウォール攻撃"))
class PRJ_TIDE_P0_API UShockwallAttackExecution : public UTideAttackExecution
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere)
	TObjectPtr<UAnimMontage> Montage;

	// 
	UPROPERTY(EditAnywhere)
	TSubclassOf<ABossShockwaveActor> ShockwaveActorClass;

	// 地雷ギミッククラス ※未設定なら地雷投射なし
	UPROPERTY(EditAnywhere)
	TSubclassOf<ABossMineHazard> ShockwallMineClass;

	// 地雷投射を行う発動者HPの上限割合 (0.5 = HP50%以下のときだけ投射)。1.0で常時投射
	// ショックウォール攻撃自体は通常どおり実行し、地雷を投射するかだけをこのHP条件で決める
	// NOTE: P0では暫定で攻撃ロジックに条件を持たせている。将来は汎用の条件機構へ移す想定
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ShockwallMineHPThreshold = 0.5f;

	// 地雷投射を開始するAttackEventタグ
	UPROPERTY(EditAnywhere)
	FGameplayTag ShockwallMineBurstEvent = TAG_AttackEvent_ShockwallMineBurst;

	// 1回のバーストで投射する地雷数
	UPROPERTY(EditAnywhere)
	int32 ShockwallMineCount = 10;

	// 地雷を順次投射する秒間隔
	UPROPERTY(EditAnywhere)
	float ShockwallMineInterval = 0.03f;

	// 地雷の投射位置(ボス頭上)オフセット
	UPROPERTY(EditAnywhere)
	float ShockwallMineSpawnHeightOffset = 60.0f;

	// 地雷スポーン位置を頭上中心から分散させる半径
	UPROPERTY(EditAnywhere)
	float ShockwallMineSpawnSpreadRadius = 45.0f;

	// 地雷投射初速の最小/最大
	UPROPERTY(EditAnywhere)
	float ShockwallMineLaunchSpeedMin = 900.0f;

	UPROPERTY(EditAnywhere)
	float ShockwallMineLaunchSpeedMax = 1300.0f;

	// 山なり投擲のための上向き角(度)
	UPROPERTY(EditAnywhere)
	float ShockwallMineLaunchPitchMinDeg = 35.0f;

	UPROPERTY(EditAnywhere)
	float ShockwallMineLaunchPitchMaxDeg = 60.0f;

	// 着地後に "Ed" セクションへジャンプするまでの待機秒数
	UPROPERTY(EditAnywhere)
	float ShockwallLandDuration = 2.0f;

	// 跳躍着地点をプレイヤー周囲のリング上に取る際の方位分割数
	// 開始角は毎回ランダムに回すので、この数だけの均等方位からランダム選択する
	UPROPERTY(EditAnywhere, Category = "Leap", meta = (ClampMin = "1"))
	int32 ShockwallRingDirectionCount = 8;

	// 着地リングの距離帯候補。この中からランダムで半径を選ぶ
	// 空の場合はShockwallRingFallbackRadiusを使う
	UPROPERTY(EditAnywhere, Category = "Leap")
	TArray<float> ShockwallRingRadii = { 500.0f, 800.0f, 1100.0f };

	// 距離帯候補が空のときに使う半径
	UPROPERTY(EditAnywhere, Category = "Leap", meta = (ClampMin = "0.0"))
	float ShockwallRingFallbackRadius = 700.0f;

	// 着地点バリデーション(Nav投影 + カプセルOverlap)のリトライ回数
	// 全滅したら跳躍せずその場ジャンプにフォールバックする
	UPROPERTY(EditAnywhere, Category = "Leap", meta = (ClampMin = "1"))
	int32 ShockwallLandPointMaxTries = 5;

	// 跳躍の滞空秒数 ※この時間で始点から着地点まで放物線移動する
	UPROPERTY(EditAnywhere, Category = "Leap", meta = (ClampMin = "0.05"))
	float ShockwallAirDuration = 0.8f;

	// 放物線アークの最高到達点の高さ
	UPROPERTY(EditAnywhere, Category = "Leap", meta = (ClampMin = "0.0"))
	float ShockwallArcHeight = 600.0f;

	// 着地候補をデバッグ描画する(採用 = 緑 / 棄却 = 赤)
	UPROPERTY(EditAnywhere, Category = "Leap")
	bool bDebugDrawLandPoint = false;

	// 発動条件: 両足とも足光輪が破壊されていたらこの攻撃は発動しない (片足残っていれば発動する)
	// 破壊状態はUPartDestructionComponent::IsPartDestroyed
	// (タグ) で照会する
	UPROPERTY(EditAnywhere, Category = "PartGate")
	FName LeftFootPartTag;

	UPROPERTY(EditAnywhere, Category = "PartGate")
	FName RightFootPartTag;

	virtual UAnimMontage* GetMontage() const override { return Montage; }
	virtual void OnAttackEvent(AEnemyCharacter* Enemy, FGameplayTag EventTag) override;
	virtual void OnAttackTick(AEnemyCharacter* Enemy, float DeltaTime) override;
	virtual void OnAttackEnd(AEnemyCharacter* Enemy) override;
	// 両足とも足光輪が破壊されていたらfalse (片足でも残っていれば発動する)
	virtual bool CanActivate(const AEnemyCharacter* Enemy) const override;

private:

	UFUNCTION()
	void OnShockwallLanded(const FHitResult& Hit);

	void BeginMineBurst(AEnemyCharacter* Enemy);
	void SpawnShockwallMine();
	bool ComputeLeapTarget(AEnemyCharacter* Enemy, FVector& OutTarget) const;
	void JumpToLandSection();

	TWeakObjectPtr<AEnemyCharacter> CachedEnemy;
	FTimerHandle ShockwallLandTimerHandle;
	FTimerHandle ShockwallMineBurstTimerHandle;
	int32 RemainingShockwallMineCount = 0;
	int32 ShockwallMineSpawnedCount = 0;

	// 放物線跳躍の実行時状態
	FVector LeapStartLocation = FVector::ZeroVector;
	FVector LeapTargetLocation = FVector::ZeroVector;
	float LeapElapsed = 0.0f;
	bool bLeaping = false;

};

/** チャージ突進中に出す常駐VFX 1つ分の設定。スケールはNiagaraのUser float "Scale" に流す */
USTRUCT()
struct FChargeVFXEntry
{
	GENERATED_BODY()

	// 再生するNiagara。未設定なら出さない
	UPROPERTY(EditAnywhere)
	TObjectPtr<UNiagaraSystem> System = nullptr;

	// アタッチ先ソケット/ボーン名。NoneならアクターRoot (進行方向基準)、
	// 指定時はメッシュのそのボーン
	UPROPERTY(EditAnywhere)
	FName SocketName = NAME_None;

	// アタッチ先基準の相対オフセット
	UPROPERTY(EditAnywhere)
	FVector Offset = FVector::ZeroVector;

	// アタッチ先基準の相対回転 (VFXの向き調整用)
	UPROPERTY(EditAnywhere)
	FRotator Rotation = FRotator::ZeroRotator;

	// NiagaraのUser floatパラメータ "Scale" に流し込むスケール
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"))
	float Scale = 1.0f;

};

/**
 * モンタージュ再生＋VFX攻撃
 * Montageを再生し、AnimNotifyのChargeStartで風切り／手元の砂ぼこりVFXを出し、ChargeEndで止める。
 * 移動 (突進) は行わない。突進が要る攻撃はモンタージュのRootMotion側で扱う。
 */
UCLASS(meta = (DisplayName = "チャージ攻撃"))
class PRJ_TIDE_P0_API UChargeAttackExecution : public UTideAttackExecution
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere)
	TObjectPtr<UAnimMontage> Montage;

	// 前方の風切りVFX (SocketName=NoneでアクターRootにアタッチ＝進行方向基準)
	UPROPERTY(EditAnywhere, Category = "VFX")
	FChargeVFXEntry WindVFX;

	// 手元の砂ぼこりVFX (SocketNameに手ボーンを指定してメッシュにアタッチ)
	UPROPERTY(EditAnywhere, Category = "VFX")
	FChargeVFXEntry DustVFX;

	virtual UAnimMontage* GetMontage() const override { return Montage; }
	virtual void OnAttackEvent(AEnemyCharacter* Enemy, FGameplayTag EventTag) override;
	virtual void OnAttackEnd(AEnemyCharacter* Enemy) override;

private:

	// ChargeStart/ChargeEndで常駐VFXを生成／停止する
	void StartChargeVFX(AEnemyCharacter* Enemy);
	void StopChargeVFX();
	// FChargeVFXEntry 1つを生成・アタッチし、"Scale" を注入して返す
	UNiagaraComponent* SpawnChargeVFX(AEnemyCharacter* Enemy, const FChargeVFXEntry& Entry) const;

	TWeakObjectPtr<UNiagaraComponent> WindVFXComp;
	TWeakObjectPtr<UNiagaraComponent> DustVFXComp;

};

/**
 * 踏みつけ攻撃(左右交互連結・回数ランダム)
 *
 * 左右2つのモンタージュを交互に再生して連続踏みつけを行う。GetMontage() はnullptrを返す
 * asyncモードで動作し、モンタージュ再生は自前で管理する。
 *
 * 各モンタージュの連結点 (例: 85F) に置いたAnimNotifyがTAG_AttackEvent_StompConnectを
 * 発火する。受信時にまだ踏みつけ回数が残っていれば、反対足のモンタージュを連結フレーム
 * (左足へ入るとき = LeftConnectInFrame / 右足へ入るとき = RightConnectInFrame) から再生する。
 * 残り回数が無ければ何もせず、現在のモンタージュを最後まで再生して攻撃を終了する。
 *
 * 踏みつけ回数はStompCountMin..StompCountMaxの範囲でランダム抽選する。
 * 当たり判定は各モンタージュ内のAnimNotifyState_CommonAttackに委ねる。
 *
 * 足の光輪が壊れていても踏みつけ自体は通常どおり行う。光輪の有無は衝撃波が出るかどうかだけを
 * 左右し、壊れた足で踏んだときは踏みつけの当たり判定のみになる。
 */
UCLASS(meta = (DisplayName = "踏みつけ攻撃"))
class PRJ_TIDE_P0_API UStompAttackExecution : public UTideAttackExecution
{
	GENERATED_BODY()

public:

	// 右足踏みつけモンタージュ (AM_stomp_r)
	UPROPERTY(EditAnywhere)
	TObjectPtr<UAnimMontage> RightStompMontage = nullptr;

	// 左足踏みつけモンタージュ (AM_stomp_l)
	UPROPERTY(EditAnywhere)
	TObjectPtr<UAnimMontage> LeftStompMontage = nullptr;

	// 踏みつけ回数の抽選下限 (この回数は最低でも踏む)
	UPROPERTY(EditAnywhere, meta = (ClampMin = "1"))
	int32 StompCountMin = 2;

	// 踏みつけ回数の抽選上限
	UPROPERTY(EditAnywhere, meta = (ClampMin = "1"))
	int32 StompCountMax = 4;

	// 左足モンタージュへ連結する際に飛び込むフレーム (右足の連結点から接続)
	UPROPERTY(EditAnywhere)
	float LeftConnectInFrame = 7.0f;

	// 右足モンタージュへ連結する際に飛び込むフレーム (左足の連結点から接続)
	UPROPERTY(EditAnywhere)
	float RightConnectInFrame = 13.0f;

	// 連結フレームを秒へ換算するフレームレート (Frame / FrameRate = 秒)
	UPROPERTY(EditAnywhere, meta = (ClampMin = "1.0"))
	float FrameRate = 60.0f;

	// 踏み下ろし時に足元から発生させる衝撃波。未設定なら衝撃波なし
	UPROPERTY(EditAnywhere, Category = "Shockwave")
	TSubclassOf<ABossShockwaveActor> ShockwaveActorClass = nullptr;

	// 衝撃波の発生位置を取る足ソケット名 (右足 / 左足)
	UPROPERTY(EditAnywhere, Category = "Shockwave")
	FName RightFootSocket;

	UPROPERTY(EditAnywhere, Category = "Shockwave")
	FName LeftFootSocket;

	// 光輪の破壊状態を照会する部位タグ (右足 / 左足)。破壊済みなら衝撃波を出さない
	UPROPERTY(EditAnywhere, Category = "Shockwave")
	FName RightFootPartTag;

	UPROPERTY(EditAnywhere, Category = "Shockwave")
	FName LeftFootPartTag;

	virtual UAnimMontage* GetMontage() const override { return nullptr; }
	virtual void OnAttackBegin(AEnemyCharacter* Enemy) override;
	virtual void OnAttackEvent(AEnemyCharacter* Enemy, FGameplayTag EventTag) override;
	virtual void OnAttackEnd(AEnemyCharacter* Enemy) override;

private:

	void PlayStomp(bool bRight, float StartTime);
	void SpawnStompShockwave(AEnemyCharacter* Enemy);
	// 指定足の光輪が破壊されていなければtrue (衝撃波を出せるか)
	bool IsFootHaloIntact(const AEnemyCharacter* Enemy, bool bRight) const;

	UFUNCTION()
	void OnStompMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	TWeakObjectPtr<UAnimInstance> CachedAnimInstance;
	TWeakObjectPtr<UAnimMontage> ActiveMontage;
	int32 TotalStomps = 0;
	int32 StompsPlayed = 0;
	bool bCurrentFootRight = false;

	// 連結再生で旧モンタージュが中断されたときの通知を「自己中断」として無視するためのマーカー
	// 同一モンタージュ連続再生でもポインタ比較に頼らず判定でき、
	// 中断が非同期発火でも取りこぼさない
	bool bReplaying = false;

	// OnAttackBeginで確定する各足の光輪状態
	// 踏みつけ自体は両足とも行い、これは衝撃波の可否だけを決める
	// 技の途中で光輪が壊れたり復活したりしても分岐がぶれないよう、開始時の値で最後まで通す
	bool bLeftFootHaloIntact = true;
	bool bRightFootHaloIntact = true;

};

/**
 * 散弾爆撃攻撃(さいころの六の目型)
 *
 * MissileStartで着弾パターンを計算・保持し、MissileShot 1回で左右1対の
 * ABossBombardProjectileをスポーンする。ShotCount回到達で "End" セクションへジャンプ。
 *
 * 着弾位置はプレイヤー座標を中心に2列 × ShotCount行のグリッドで展開する。
 * 敵からの距離がMinDistFromEnemy未満の着弾点は押し出してクランプする。
 */
UCLASS(meta = (DisplayName = "散弾爆撃攻撃"))
class PRJ_TIDE_P0_API UScatterBombardAttackExecution : public UTideAttackExecution
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere)
	TObjectPtr<UAnimMontage> Montage;

	// 爆撃弾の弾プロファイル(Movementに「爆撃(ベジェ弧)」ビヘイビアを設定する)
	UPROPERTY(EditAnywhere)
	TObjectPtr<UProjectileProfile> BombardProfile;

	// BombardShot 1回で左右2発スポーン。この回数でループする
	UPROPERTY(EditAnywhere)
	int32 ShotCount = 3;

	// 左右の横オフセット
	UPROPERTY(EditAnywhere)
	float SideOffset = 250.0f;

	// ショット間の縦オフセット間隔。プレイヤーを中心に前後へ均等展開する
	UPROPERTY(EditAnywhere)
	float RowSpacing = 400.0f;

	// 4way化。ONで内側2列に加えて、外側へ扇状に広がる2列を追加する(計4列)
	UPROPERTY(EditAnywhere, Category = "FourWay")
	bool bFourWay = true;

	// 外側2列の横オフセット。内側SideOffsetより広めに置くと隊形が分離する
	UPROPERTY(EditAnywhere, Category = "FourWay", meta = (EditCondition = "bFourWay"))
	float OuterSideOffset = 450.0f;

	// 外側2列の扇の開き角(度)。奥の行ほど外へ広がり扇形になる。0 = 平行
	UPROPERTY(EditAnywhere, Category = "FourWay", meta = (EditCondition = "bFourWay", ClampMin = "0.0", ClampMax = "89.0"))
	float OuterFanAngleDeg = 25.0f;

	// 着弾点の発射主からの最小距離クランプ
	UPROPERTY(EditAnywhere)
	float MinDistFromEnemy = 1400.0f;

	// スポーン位置の後方オフセット
	UPROPERTY(EditAnywhere)
	float SpawnBackOffset = 80.0f;

	// スポーン位置の高さオフセット
	UPROPERTY(EditAnywhere)
	float SpawnHeightOffset = 200.0f;

	// 爆撃弾の飛行秒数
	UPROPERTY(EditAnywhere)
	float BombardFlightDuration = 1.5f;

	// 爆撃弾の基本弧高。制御点未指定時に使う
	UPROPERTY(EditAnywhere)
	float BombardArcHeight = 600.0f;

	// 蛇行ノイズの最大振幅
	UPROPERTY(EditAnywhere)
	float BombardWeaveNoiseAmplitude = 180.0f;

	// 蛇行ノイズ周波数
	UPROPERTY(EditAnywhere)
	float BombardWeaveNoiseFrequency = 6.0f;

	// 蛇行ノイズ包絡の鋭さ
	UPROPERTY(EditAnywhere)
	float BombardWeaveEnvelopePower = 3.0f;

	// 射出直後に初期方位を維持する時間(秒)
	UPROPERTY(EditAnywhere)
	float BombardHeadingHoldTime = 0.1f;

	// 目標方位へ向く最大旋回速度(deg/s)
	UPROPERTY(EditAnywhere)
	float BombardTurnRateDegPerSec = 1200.0f;

	// 射出方向をランダム化する
	UPROPERTY(EditAnywhere)
	bool bRandomizeLaunchDirection = true;

	// 射出方向の水平ランダム角(度)
	UPROPERTY(EditAnywhere)
	float LaunchYawJitterDeg = 45.0f;

	// trueのとき左右完全ミラー(同じ絶対角を左右反転)で射出する
	UPROPERTY(EditAnywhere)
	bool bLaunchPerfectMirror = true;

	// 左右ペアを外側へ開く基準角(度)
	UPROPERTY(EditAnywhere)
	float LaunchOutwardYawDeg = 80.0f;

	// 射出方向の上向きランダム角(度)最小値
	UPROPERTY(EditAnywhere)
	float LaunchPitchMinDeg = 45.0f;

	// 射出方向の上向きランダム角(度)最大値
	UPROPERTY(EditAnywhere)
	float LaunchPitchMaxDeg = 42.0f;

	// 射出直後に背面へ逃がす距離
	UPROPERTY(EditAnywhere)
	float LaunchKickDistance = 2000.0f;

	// 背面クリアランス最低値(敵中心から -Forward方向cm)
	UPROPERTY(EditAnywhere)
	float LaunchBackClearance = 200.0f;

	virtual UAnimMontage* GetMontage() const override { return Montage; }
	virtual void OnAttackBegin(AEnemyCharacter* Enemy) override;
	virtual void OnAttackEvent(AEnemyCharacter* Enemy, FGameplayTag EventTag) override;
	virtual void OnAttackEnd(AEnemyCharacter* Enemy) override;

private:

	void SpawnBombardPair(AEnemyCharacter* Enemy);

	FVector CachedPlayerPos = FVector::ZeroVector;
	FVector CachedForward2D = FVector::ForwardVector;
	FVector CachedRight2D = FVector::RightVector;
	int32 ShotFiredCount = 0;

};

/**
 * 弧状爆撃攻撃(EM中心円弧・左右自動判定)
 *
 * MissileStartでカウンターをリセットし、1発目のMissileShot受信時に
 * プレイヤー位置と敵の向きから弧の基準を確定する。
 *
 * 着弾位置: 敵を中心とした半径ArcRadiusの円弧上にShotCount回配置する。
 *   - 1回のShotで、円弧上の基準点と、その外側へずらした点の計2発を同時に落とす
 *   - 1発目: 敵の実際の前方向き延長線上(ArcRadius cm先)
 *   - 掃引の左右は初段発射時にPCの移動方向(velocity)から確定する。
 *     PCが向かう側とは逆へArcStepAngleずつ弧を掃引する(PCが右なら左から弧を描く。停止中は右へ掃引)。
 * 敵からの距離がMinDistFromEnemy未満の着弾点は押し出してクランプする。
 *
 * 弾は上方向ではなく左右(水平)にふくらむ山なり弧で飛ばし、カメラ内に収める。
 */
UCLASS(meta = (DisplayName = "弧状爆撃攻撃"))
class PRJ_TIDE_P0_API UArcBombardAttackExecution : public UTideAttackExecution
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere)
	TObjectPtr<UAnimMontage> Montage;

	// 爆撃弾の弾プロファイル(Movementに「爆撃(ベジェ弧)」ビヘイビアを設定する)
	UPROPERTY(EditAnywhere)
	TObjectPtr<UProjectileProfile> BombardProfile;

	// 発射回数(= BombardShotを受け取る回数)
	UPROPERTY(EditAnywhere)
	int32 ShotCount = 3;

	// Shot 1個あたりの角度ステップ量(絶対値のみ有効。左右は実行時に自動決定される)
	UPROPERTY(EditAnywhere)
	float ArcStepAngle = 60.0f;

	// --- 展開ルーチン(ONで「展開→待機→着弾点へ誘導→途中からホーミング」)---

	// ONで展開ルーチンを使う。各弾はまずEM周囲の展開位置へ飛んで待機してから発射する
	UPROPERTY(EditAnywhere, Category = "Deploy")
	bool bUseDeployRoutine = true;

	// 展開位置のEMからの半径
	UPROPERTY(EditAnywhere, Category = "Deploy", meta = (EditCondition = "bUseDeployRoutine", ClampMin = "0.0"))
	float DeployRadius = 1000.0f;

	// 展開位置のEM足元からの高さ
	UPROPERTY(EditAnywhere, Category = "Deploy", meta = (EditCondition = "bUseDeployRoutine"))
	float DeployHeight = 500.0f;

	// 展開位置の開始角(度)。EM→PC方向を0度とする
	UPROPERTY(EditAnywhere, Category = "Deploy", meta = (EditCondition = "bUseDeployRoutine"))
	float DeployStartAngleDeg = 90.0f;

	// 1発ごとに展開位置を円周上へずらす角度(度)。円周を時計回りに移動させるイメージ
	// 符号で回る向きが変わる(逆回りにしたいときは負値)
	UPROPERTY(EditAnywhere, Category = "Deploy", meta = (EditCondition = "bUseDeployRoutine"))
	float DeployAngleStepDeg = -35.0f;

	// 展開位置での待機時間(秒)
	UPROPERTY(EditAnywhere, Category = "Deploy", meta = (EditCondition = "bUseDeployRoutine", ClampMin = "0.0"))
	float DeployHoverDuration = 0.4f;

	// 本発射の飛行割合がこの値を超えたら、着弾点誘導をやめてPCへホーミングする(0..1,
	// 0=切替なし)
	UPROPERTY(EditAnywhere, Category = "Deploy", meta = (EditCondition = "bUseDeployRoutine", ClampMin = "0.0", ClampMax = "1.0"))
	float HomingHandoffRatio = 0.6f;

	// ホーミングの旋回速度(度/秒)
	UPROPERTY(EditAnywhere, Category = "Deploy", meta = (EditCondition = "bUseDeployRoutine", ClampMin = "0.0"))
	float HomingTurnRateDegPerSec = 180.0f;

	// 着弾点の発射主からの最小距離クランプ
	UPROPERTY(EditAnywhere)
	float MinDistFromEnemy = 1400.0f;

	// 同一Shotの2発目を円弧基準点の外側へどれだけ離して落とすか
	UPROPERTY(EditAnywhere)
	float OuterLandingOffset = 400.0f;

	// スポーン位置の後方オフセット
	UPROPERTY(EditAnywhere)
	float SpawnBackOffset = 80.0f;

	// スポーン位置の高さオフセット
	UPROPERTY(EditAnywhere)
	float SpawnHeightOffset = 200.0f;

	// 爆撃弾の飛行秒数
	UPROPERTY(EditAnywhere)
	float BombardFlightDuration = 1.5f;

	// 弾道の垂直方向の山なり高さ。上方向のふくらみ。カメラに収めるため小さめ推奨(0で水平のみ)
	UPROPERTY(EditAnywhere)
	float BombardArcHeight = 100.0f;

	// 弾道の左右方向の山なりふくらみ。掃引方向側へ水平に弧を描く主役パラメータ
	UPROPERTY(EditAnywhere)
	float BombardArcSideOffset = 1200.0f;

	virtual UAnimMontage* GetMontage() const override { return Montage; }
	virtual void OnAttackBegin(AEnemyCharacter* Enemy) override;
	virtual void OnAttackEvent(AEnemyCharacter* Enemy, FGameplayTag EventTag) override;
	virtual void OnAttackEnd(AEnemyCharacter* Enemy) override;

private:

	void SpawnBombard(AEnemyCharacter* Enemy);

	FVector CachedPlayerPos = FVector::ZeroVector;
	FVector CachedForward2D = FVector::ForwardVector; // 敵->PC方向 (弧の基準)
	float CachedEffectiveRadius = 0.0f; // 初弾時に敵->PC距離から算出した着弾半径
	float DynamicStepAngle = 0.0f; // 符号込みの掃引ステップ (初段発射時に確定)
	int32 ShotFiredCount = 0;

};

/**
 * 光輪配置攻撃
 *
 * AttackEvent.HaloPlace発火時に、腕の光輪を非表示にし、ボス前方の左右2ラインへ
 * AHaloHazardを等間隔で配置する。各ハザードは上空へ発射 -> 落下して着地する。
 * 配置された光輪はショックウォール攻撃のショックウェーブで破砕される。
 */
UCLASS(meta = (DisplayName = "光輪配置攻撃"))
class PRJ_TIDE_P0_API UHaloLineAttackExecution : public UTideAttackExecution
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere)
	TObjectPtr<UAnimMontage> Montage;

	UPROPERTY(EditAnywhere)
	TSubclassOf<AHaloHazard> HaloHazardClass;

	// 光輪メッシュコンポーネントの名前 (発射元 / 発射中は非表示にする)
	UPROPERTY(EditAnywhere)
	FName HaloComponentName = FName("Halo");

	// 1ラインあたりの配置数
	UPROPERTY(EditAnywhere)
	int32 CountPerLine = 4;

	// ライン方向の配置間隔
	UPROPERTY(EditAnywhere)
	float Spacing = 250.0f;

	// 1個目をボスからどれだけ前方に置くか
	UPROPERTY(EditAnywhere)
	float StartDistance = 200.0f;

	// 左右2ラインの間隔
	UPROPERTY(EditAnywhere)
	float LineGap = 300.0f;

	// 順次落下の秒間隔 ※0で全て同時に落とす
	UPROPERTY(EditAnywhere)
	float DropInterval = 0.15f;

	// 落下順を反転する。false = ボス側 (StartDistance側) から / true =
	// 奥 (ふち側) から
	UPROPERTY(EditAnywhere)
	bool bReverseDropOrder = false;

	// 着地点を地形に合わせる下方向トレースの上端高さ (着地点XYから上へcm)
	UPROPERTY(EditAnywhere)
	float GroundTraceUpDistance = 1000.0f;

	// 着地点を地形に合わせる下方向トレースの距離
	UPROPERTY(EditAnywhere)
	float GroundTraceDownDistance = 5000.0f;

	// 着地点の地面からの高さオフセット
	UPROPERTY(EditAnywhere)
	float GroundOffset = 0.0f;

	// 着地トレースをデバッグ描画する (緑 = ヒット / 赤 = ミス)
	UPROPERTY(EditAnywhere)
	bool bDebugDrawGroundTrace = false;

	virtual UAnimMontage* GetMontage() const override { return Montage; }
	virtual void OnAttackEvent(AEnemyCharacter* Enemy, FGameplayTag EventTag) override;
	virtual void OnAttackEnd(AEnemyCharacter* Enemy) override;

private:

	TWeakObjectPtr<USceneComponent> HaloComp;

};

/**
 * 弾幕炎舞
 *
 * 1. n秒で6方向円形放射上に火球が出現する 正面が1つ目で反時計回りにn個 大きくなりながら出現
 * 2. n秒間エネミーの周りをぐるぐる回り始める 接触ダメージあり
 * 3. n秒間連射する形で外側に向かって真っすぐ前に撃ち始める (2)で回転している火球から弾が出現
 *    このタイミングで硬化装鋼が溶けて殴れる 弾幕をかいくぐって攻撃する遊びが成立
 *    出しきったらEndセクションへジャンプ
 */
UCLASS(meta = (DisplayName = "弾幕炎舞"))
class PRJ_TIDE_P0_API UFlameDanceAttackExecution : public UTideAttackExecution
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, Category = "Orbit")
	TObjectPtr<UAnimMontage> Montage;

	// 弾プロファイル
	UPROPERTY(EditAnywhere, Category = "Orbit")
	TObjectPtr<UProjectileProfile> Profile;

	// 火球のアクタクラス
	UPROPERTY(EditAnywhere, Category = "Orbit")
	TSubclassOf<AActor> FireballClass;

	// 火球の数
	UPROPERTY(EditAnywhere, Category = "Orbit")
	int32 FireballCount = 3;

	// 火球の展開半径
	UPROPERTY(EditAnywhere, Category = "Orbit")
	float OrbitRadius = 600.0f;

	// 火球の高さオフセット
	UPROPERTY(EditAnywhere, Category = "Orbit")
	float OrbitHeightOffset = -150.0f;

	// 着地時に発生させるショックウェーブ
	UPROPERTY(EditAnywhere, Category = "Leap")
	TSubclassOf<ABossShockwaveActor> ShockwaveActorClass = nullptr;

	// 跳躍の滞空秒数
	UPROPERTY(EditAnywhere, Category = "Leap", meta = (ClampMin = "0.05"))
	float LeapAirDuration = 0.8f;

	// 放物線アークの最高到達点の高さ
	UPROPERTY(EditAnywhere, Category = "Leap", meta = (ClampMin = "0.0"))
	float LeapArcHeight = 600.0f;

	// 着地点をPCから離す目標距離の候補。PC中心のリング半径として使い、着地距離 ≒ この値になる
	// 降順に試し、ある距離でLeapLandPointMaxTries方位ぶん失敗したら次の
	// (より小さい) 候補へ緩める(狭いエリアや探索失敗時の救済)
	// 空または0以下は距離指定なし (エリア内一様) として扱う
	UPROPERTY(EditAnywhere, Category = "Leap")
	TArray<float> MinLeapDistanceCandidates = { 2000.0f, 1000.0f, 500.0f };

	// 1候補距離あたりの着地点探索リトライ回数。全候補で全滅したらその場着地にフォールバックする
	UPROPERTY(EditAnywhere, Category = "Leap", meta = (ClampMin = "1"))
	int32 LeapLandPointMaxTries = 8;

	// スポナーエリアの外縁からこの距離以内は着地候補から除外する
	// エリアのきわきわに着地しないための内側マージン。実効半径 = エリア半径 - この値
	UPROPERTY(EditAnywhere, Category = "Leap", meta = (ClampMin = "0.0"))
	float LeapAreaEdgeMargin = 400.0f;

	// 着地候補をデバッグ描画する (採用=緑 / 棄却=赤 / PC近すぎ=橙)
	UPROPERTY(EditAnywhere, Category = "Leap")
	bool bDebugDrawLeapPoint = false;

	// 火球生成が始まるまでの待機秒数 ※経過後に固定0.2秒で一斉拡大
	UPROPERTY(EditAnywhere, Category = "Orbit")
	float SpawnDuration = 2.0f;

	// 攻撃終了時に火球を縮小して消す秒数 ※0で即時消滅
	UPROPERTY(EditAnywhere, Category = "Orbit", meta = (ClampMin = "0.0"))
	float FireballDespawnDuration = 0.3f;

	// 連射に移る前に回転のみを行う秒数
	UPROPERTY(EditAnywhere, Category = "Orbit")
	float OrbitDuration = 4.0f;

	// 回転速度 (度/秒) マイナスで逆回転
	UPROPERTY(EditAnywhere, Category = "Orbit")
	float OrbitRotationSpeed = 200.0f;

	// 連射フェーズ全体の秒数
	UPROPERTY(EditAnywhere, Category = "Shooting")
	float ShootingDuration = 12.0f;

	// 1回の射出秒間隔
	UPROPERTY(EditAnywhere, Category = "Shooting")
	float ShootInterval = 0.05f;

	// 射出フェーズ中は常に回転を停止するかどうか
	UPROPERTY(EditAnywhere, Category = "Shooting")
	bool bStopRotationDuringShoot = false;

	// 回転と停止を繰り返すパターンのオンオフ (bStopRotationDuringShootより優先)
	UPROPERTY(EditAnywhere, Category = "Shooting")
	bool bToggleRotation = false;

	// 回転/停止を切り替える秒間隔
	UPROPERTY(EditAnywhere, Category = "Shooting")
	float RotationToggleInterval = 1.0f;

	// 発射/停止を繰り返すパターンのオンオフ (バババッ発射 -> 停止 -> 発射 ... を作る)
	UPROPERTY(EditAnywhere, Category = "Shooting")
	bool bToggleShooting = false;

	// 発射/停止を切り替える秒間隔
	UPROPERTY(EditAnywhere, Category = "Shooting")
	float ShootingToggleInterval = 1.0f;

	// --- 着地バースト (着地時に360度放射) ---

	// 着地バーストで発射する弾プロファイル。未設定ならバーストなし
	UPROPERTY(EditAnywhere, Category = "LandBurst")
	TObjectPtr<UProjectileProfile> LandBurstProfile = nullptr;

	// 360度を等分して発射する弾数
	UPROPERTY(EditAnywhere, Category = "LandBurst", meta = (ClampMin = "0"))
	int32 LandBurstCount = 36;

	// ボス中心からのスポーン半径。この円周上から外向きに発射する
	UPROPERTY(EditAnywhere, Category = "LandBurst", meta = (ClampMin = "0.0"))
	float LandBurstRadius = 200.0f;

	// 着弾距離。ボス中心から外向きにこの距離の地点へ落ちる (山なり弾=BezierArc用の着弾点)
	// BezierArc以外のMovementでは使われない (その場合は直進)
	UPROPERTY(EditAnywhere, Category = "LandBurst", meta = (ClampMin = "0.0"))
	float LandBurstDistance = 3000.0f;

	// スポーン高さオフセット。ボス足元基準
	UPROPERTY(EditAnywhere, Category = "LandBurst")
	float LandBurstHeightOffset = 50.0f;

	// 縦に積む層数。1で単層、2以上で同じリングを高さ違いで上に重ねる (着弾距離は共通)
	UPROPERTY(EditAnywhere, Category = "LandBurst", meta = (ClampMin = "1"))
	int32 LandBurstLayerCount = 2;

	// 層ごとの高さ間隔。上の層ほどZ+ される
	UPROPERTY(EditAnywhere, Category = "LandBurst", meta = (ClampMin = "0.0"))
	float LandBurstLayerHeightSpacing = 200.0f;

	// --- 隊形 (層) ---

	// 積む層数。1 = 単一リング、2以上で上に積む
	UPROPERTY(EditAnywhere, Category = "Formation", meta = (ClampMin = "1"))
	int32 LayerCount = 1;

	// 上段(2層目以降)の配置スロット間隔。1=フルリング、2以上で間引いて交互積み
	// (層ごとに1スロットずれる)
	UPROPERTY(EditAnywhere, Category = "Formation", meta = (ClampMin = "1"))
	int32 UpperLayerSlotStride = 1;

	// 層の高さ間隔。上の層ほどZ+ される
	UPROPERTY(EditAnywhere, Category = "Formation", meta = (ClampMin = "0.0"))
	float LayerHeightSpacing = 200.0f;

	virtual UAnimMontage* GetMontage() const override { return nullptr; }
	virtual void OnAttackBegin(AEnemyCharacter* Enemy) override;
	virtual void OnAttackEvent(AEnemyCharacter* Enemy, FGameplayTag EventTag) override;
	virtual void OnAttackTick(AEnemyCharacter* Enemy, float DeltaTime) override;
	virtual void OnAttackEnd(AEnemyCharacter* Enemy) override;

private:

	UFUNCTION()
	void OnFlameDanceMontageEnded(UAnimMontage* EndedMontage, bool bInterrupted);

	enum class EFlameDancePhase : uint8
	{
		None,
		Spawning,
		Orbiting,
		Shooting,
		Despawning
	};

	void PlaySequenceMontage(AEnemyCharacter* Enemy, UAnimMontage* InMontage);
	void JumpToLandSection();
	// 着地点をスポナーエリア(BBのPatrolOrigin
	// /Radius)内・Nav到達可能・PCから一定距離で抽選する。見つかればtrue
	bool ComputeLeapTarget(AEnemyCharacter* Enemy, FVector& OutTarget) const;
	void BeginSpawning(AEnemyCharacter* Enemy);
	void SpawnLandingShockwave(AEnemyCharacter* Enemy) const;
	// 着地時にボスを中心に360度等間隔で外向きに弾を一斉発射する
	void SpawnLandBurst(AEnemyCharacter* Enemy);
	void UpdateOrbit(float DeltaTime);
	void ShootProjectiles(AEnemyCharacter* Enemy);

	// バリアントに応じて火球の配置 (スロットと層) を構築する
	void BuildFireballPlacements();
	// 全火球の位置を現在状態から再計算して適用する (SpawnScaleで出現アニメを乗算)
	void UpdateFireballTransforms(float SpawnScale);

	EFlameDancePhase CurrentPhase = EFlameDancePhase::None;
	TWeakObjectPtr<AEnemyCharacter> CachedEnemy;
	TWeakObjectPtr<UAnimInstance> CachedAnimInstance;
	TWeakObjectPtr<UAnimMontage> ActiveMontage;
	TArray<TWeakObjectPtr<AActor>> SpawnedFireballs;

	float PhaseTimer = 0.0f;
	float ShootTimer = 0.0f;
	float RotationToggleTimer = 0.0f;
	float ShootingToggleTimer = 0.0f;

	float CurrentOrbitAngle = 0.0f;
	FVector LeapStartLocation = FVector::ZeroVector;
	FVector LeapTargetLocation = FVector::ZeroVector;
	float LeapElapsed = 0.0f;
	bool bIsRotating = true;
	bool bShootingActive = true; // 発射トグルが現在「発射」状態か
	bool bHasSpawned = false;
	bool bLeaping = false;

	// 各火球の配置 (X=Slot, Y=Layer)。OnAttackBeginの生成時に構築する
	TArray<FIntPoint> FireballPlacements;

};

/**
 * 鉄鋼爆雨
 *
 * 1. n秒で大量のミサイルがエネミーの付近にぱらぱらと出現
 * 2. n秒でミサイルが一気に上空へ打ち出される (はるか上空へ打ち上げる演出が欲しいだけNiagaraでやる)
 * 3. ミサイルが以下のどれかのパターンで落ちてくる 落下地点にデカールを出してからn秒後に着弾
 *    デカールの濃さや大きさで着弾までの時間を表現 このタイミングで硬化装鋼が溶けて殴れる
 *    出しきったらEndセクションへジャンプ
 *
 * 3-1. エネミーを中心に外側に向かって順に円形に落ちてくる(PCはEMから離れるように逃げる)
 * 3-2. エネミーを中心に内側に向かって順に円形に落ちてくる(PCはEMに近づくように逃げる)
 * 3-3. エネミーの周辺にランダムに落下してくる(PCはEMの周りをジグザグに逃げる)
 */
UCLASS(meta = (DisplayName = "鉄鋼爆雨"))
class PRJ_TIDE_P0_API UIronRainAttackExecution : public UTideAttackExecution
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere)
	TObjectPtr<UAnimMontage> Montage;

	// 打ち上げ用プロファイル(Niagara等で演出するためのダミーまたは発射用)
	UPROPERTY(EditAnywhere, Category = "IronRain")
	TObjectPtr<UProjectileProfile> LaunchProfile;

	// 落下着弾用プロファイル
	UPROPERTY(EditAnywhere, Category = "IronRain")
	TObjectPtr<UProjectileProfile> FallProfile;

	// 打ち上げ演出で発射するミサイルの数
	UPROPERTY(EditAnywhere, Category = "IronRain")
	int32 LaunchMissileCount = 32;

	// 生成フェーズの所要秒数
	UPROPERTY(EditAnywhere, Category = "IronRain|Phase")
	float SpawnDuration = 0.5f;

	// 発射フェーズの所要秒数
	UPROPERTY(EditAnywhere, Category = "IronRain|Phase")
	float LaunchDuration = 1.0f;

	// 落下フェーズの所要秒数
	UPROPERTY(EditAnywhere, Category = "IronRain|Phase")
	float RainDuration = 4.0f;

	// 落下地点の予告表示から着弾までの遅延秒数
	UPROPERTY(EditAnywhere, Category = "IronRain|Drop")
	float DropDelay = 1.5f;

	// 落下の最小半径 ボス周辺の安全圏
	UPROPERTY(EditAnywhere, Category = "IronRain|Drop")
	float MinDropRadius = 1200.0f;

	// 落下の最大半径
	UPROPERTY(EditAnywhere, Category = "IronRain|Drop")
	float MaxDropRadius = 8000.0f;

	// 落下の高度
	UPROPERTY(EditAnywhere, Category = "IronRain|Drop")
	float DropHeight = 6000.0f;

	// 打ち上げ時の拡散コーン半角 (度)
	UPROPERTY(EditAnywhere, Category = "IronRain|Launch")
	float LaunchConeHalfAngle = 30.0f;

	// 落下パターン (0=外側へ広がる, 1=内側へ狭まる, 2=ランダム)
	UPROPERTY(EditAnywhere, Category = "IronRain|Drop", meta = (ClampMin = "0", ClampMax = "2"))
	int32 DropPattern = 0;

	// 同心円状の層数。外側/内側パターンでは、この層ごとにまとめて落としてから次の層へ進む
	UPROPERTY(EditAnywhere, Category = "IronRain|Drop", meta = (ClampMin = "1"))
	int32 DropLayerCount = 6;

	// 同じ層に並ぶ落下点同士の目標間隔。各層の発数は半径に応じて自動算出する
	UPROPERTY(EditAnywhere, Category = "IronRain|Drop", meta = (ClampMin = "1.0"))
	float DropTargetSpacing = 1400.0f;

	// 予告デカールのマテリアル (進行度スカラーで黄->赤フェード)。未設定なら予告を出さない
	UPROPERTY(EditAnywhere, Category = "IronRain|Decal")
	TObjectPtr<UMaterialInterface> DropDecalMaterial = nullptr;

	// 予告デカールのサイズ (X=地面への投影深さ, Y/Z=半径cm)
	UPROPERTY(EditAnywhere, Category = "IronRain|Decal")
	FVector DropDecalSize = FVector(200.0f, 300.0f, 300.0f);

	// フェード進行度のスカラーパラメータ名 (0=出現/黄, 1=着弾直前/赤)
	UPROPERTY(EditAnywhere, Category = "IronRain|Decal")
	FName DropDecalProgressParam = TEXT("Progress");

	// 予告デカール補正距離 目安はミサイルの半分
	UPROPERTY(EditAnywhere, Category = "IronRain|Decal")
	float DropDecalImpactLeadDistance = 100.0f;

	virtual UAnimMontage* GetMontage() const override { return Montage; }
	virtual void OnAttackBegin(AEnemyCharacter* Enemy) override;
	virtual void OnAttackEvent(AEnemyCharacter* Enemy, FGameplayTag EventTag) override;
	virtual void OnAttackTick(AEnemyCharacter* Enemy, float DeltaTime) override;
	virtual void OnAttackEnd(AEnemyCharacter* Enemy) override;

private:

	enum class EIronRainPhase : uint8
	{
		None,
		Spawning,
		Launching,
		Raining
	};

	void SpawnLaunchProjectiles(AEnemyCharacter* Enemy);
	void ProcessLaunching(AEnemyCharacter* Enemy, float DeltaTime);
	void ProcessRaining(AEnemyCharacter* Enemy, float DeltaTime);
	// 指定した層 (同心円リング) の落下点をまとめて全て落とす
	void DropLayer(AEnemyCharacter* Enemy, int32 LayerIndex);
	// 1点を指定方位/半径に落とす (地形トレースで着地Zを決めてスケジュール)
	void DropAt(AEnemyCharacter* Enemy, float AngleDeg, float Radius);
	void ScheduleDrop(AEnemyCharacter* Enemy, const FVector& DropLoc);
	void ExecuteDrop(FVector DropLoc);

	EIronRainPhase CurrentPhase = EIronRainPhase::None;
	TWeakObjectPtr<AEnemyCharacter> CachedEnemy;

	float TimeSinceLastLaunch = 0.0f;
	float LaunchInterval = 0.0f;
	float PhaseTimer = 0.0f;
	float RainInterval = 0.0f;
	float TimeSinceLastDrop = 0.0f;

	int32 LaunchedCount = 0;
	int32 DroppedCount = 0;
	int32 DroppedLayers = 0;   // 落下済みの層数 (外側/内側パターンの層単位進行用)
	int32 TotalDropCount = 0;
	float DropBaseAngleDeg = 0.0f;

	bool bHasStarted = false;
	TArray<int32> DropCountsPerLayer;

	// 遅延着弾のキュー管理用 (予告デカールも保持する)
	struct FPendingDrop
	{
		FVector Location = FVector::ZeroVector;
		float RemainingTime = 0.0f;
		float TotalTime = 0.0f;                            // 進行度の正規化用 (生成時の着弾までの総時間)
		TWeakObjectPtr<UDecalComponent> Decal;             // 予告デカール (LifeSpanで自動消滅)
		TWeakObjectPtr<UMaterialInstanceDynamic> DecalMID; // フェード用の動的マテリアル

	};
	TArray<FPendingDrop> PendingDrops;

};

/**
 * サーカス弾を撃つ技の共通基底
 *
 * 背中から弾を撃ち上げ、指定した着弾点へ叩き込む部分だけを持つ。着弾で炸裂し、着弾点には
 * 着弾までの予兆デカールを出す。プレイヤーはデカールを読んで逃げる遊びになる。
 *
 * 飛び方はUArcPathBehavior (ベジェ + 螺旋/ノイズ) の閉形式弾道。展開ルーチンは使わず、
 * 発射位置から着弾点までを一本の曲線として渡して即発射する。制御点は6点 (5次) で、
 * 登り / 頂点 / 降りへ向きの変化を配分する (下のArcセクション参照)。
 * 揺らぎの包絡が両端でゼロなので、どれだけ暴れさせても着弾点は動かない (予兆デカールと必ず一致)。
 *
 * 「いつ・どこへ撃つか」は派生クラスが決める。派生はSpawnCircusShotとAddImpactDecalを呼び、
 * OnAttackTickでUpdateImpactDecalsを回し、OnAttackEndでClearImpactDecalsすればよい。
 */
UCLASS(Abstract)
class PRJ_TIDE_P0_API UCircusShotAttackExecutionBase : public UTideAttackExecution
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere)
	TObjectPtr<UAnimMontage> Montage;

	// サーカス弾のプロファイル (Movementに「弧パス」、
	// Effectsに「範囲ダメージ」を設定する)
	UPROPERTY(EditAnywhere)
	TObjectPtr<UProjectileProfile> CircusProfile;

	// --- 炸裂 ---

	// 着弾時の炸裂半径。0でプロファイル側の値をそのまま使う
	UPROPERTY(EditAnywhere, Category = "Splash", meta = (ClampMin = "0.0"))
	float SplashRadius = 200.0f;

	// --- 発射位置 / 展開 ---

	// 発射位置に使う背中のソケット名。未設定ならアクター基準のオフセットを使う
	UPROPERTY(EditAnywhere, Category = "Launch")
	FName MuzzleSocketName;

	// ソケット未設定時の後方オフセット
	UPROPERTY(EditAnywhere, Category = "Launch")
	float SpawnBackOffset = 120.0f;

	// ソケット未設定時の高さオフセット
	UPROPERTY(EditAnywhere, Category = "Launch")
	float SpawnHeightOffset = 200.0f;

	// --- 弾道 (5次ベジェの制御点) ---
	//
	// 上げて落とす弾道は進行方向が真上から真下まで約180度変わる。3次ベジェだとこの回転が
	// 曲線全体へ均等に配られてしまい、頂点で急に切り返す。制御点を6点に増やして
	// 「登り = 直進 / 頂点 = 回す / 降り = 直進」と区間へ配分する
	//
	//   P0発射位置 / P1登りの向き / P2頂点への入り / P3頂点からの出 /
	// P4降りの向き / P5着弾点

	// P1: 発射位置の真上へ置く距離。射出直後にどこまで垂直に伸びるかを決める
	UPROPERTY(EditAnywhere, Category = "Arc", meta = (ClampMin = "0.0"))
	float LaunchUpDistance = 1200.0f;

	// P1: 横へずらす距離。0なら真上へ撃ち上げる
	// ここが小さいと方位をばらしても差が出ない (ゼロベクトルを回しても向きは変わらないため)
	UPROPERTY(EditAnywhere, Category = "Arc")
	float LaunchSpreadDistance = 500.0f;

	// P2/P3: 頂点の高さ (cm、発射位置基準)。2点を同じ高さに置くことで頂点が平らになり、
	// 切り返しが1点に集中せずP2→P3の区間へ分散する
	UPROPERTY(EditAnywhere, Category = "Arc", meta = (ClampMin = "0.0"))
	float ApexHeight = 2400.0f;

	// P2/P3: 頂点で横へ膨らむ距離。頂点の水平方向の伸びしろ = 切り返しの緩さ
	// 急な切り返しが気になるときに最初に上げる値
	UPROPERTY(EditAnywhere, Category = "Arc")
	float ApexSpreadDistance = 900.0f;

	// P4: 着弾点の真上へ置く高さ。大きいほど垂直に近い角度で落ちてくる
	UPROPERTY(EditAnywhere, Category = "Arc", meta = (ClampMin = "0.0"))
	float ImpactApproachHeight = 1200.0f;

	// 飛行時間 (秒)。0以下でプロファイル側の値をそのまま使う
	// 閉形式の弾道なのでこの時間がそのまま着弾までの時間になり、予兆デカールと必ず一致する
	// 弧長等速なので、制御点を伸ばして経路が長くなったらここも伸ばさないと速くなる
	UPROPERTY(EditAnywhere, Category = "Arc", meta = (ClampMin = "0.0"))
	float FlightDuration = 1.8f;

	// 弾ごとに制御点の距離と螺旋をばらす割合 (0.5 = ±50%)
	// 着弾点と飛行時間は動かさないので、隊列の一斉着弾は保ったまま道筋だけが散る
	UPROPERTY(EditAnywhere, Category = "Arc", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ShotShapeJitter = 0.5f;

	// 制御点の横ずらし方位を弾ごとに振る角度 (度)。リング接線を基準に ±この範囲で回す
	// 距離だけ振っても全弾が同じ形の拡大縮小にしかならないので、ばらけ感はこちらが主役
	// 0 = 全弾そろって同じ向きへ回り込む / 180 = 方位完全ランダム (巻く向きも左右に散る)
	UPROPERTY(EditAnywhere, Category = "Arc", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float ControlPointAngleJitterDeg = 120.0f;

	// --- 予兆デカール ---

	// 予兆デカールのマテリアル (進行度スカラーで着弾までを表現)。未設定なら予兆を出さない
	UPROPERTY(EditAnywhere, Category = "Decal")
	TObjectPtr<UMaterialInterface> ImpactDecalMaterial = nullptr;

	// 進行度スカラーのパラメータ名 (0 = 出現 / 1 = 着弾)
	UPROPERTY(EditAnywhere, Category = "Decal")
	FName ImpactDecalProgressParam = TEXT("Progress");

	// デカール半径 = 炸裂半径xこの比率
	UPROPERTY(EditAnywhere, Category = "Decal", meta = (ClampMin = "0.0"))
	float ImpactDecalRadiusRatio = 1.0f;

	// デカールの地面への投影深さ
	UPROPERTY(EditAnywhere, Category = "Decal", meta = (ClampMin = "1.0"))
	float ImpactDecalDepth = 200.0f;

	// --- その他 ---

	// 技の実行中は硬化を解除して殴れる窓を開ける
	UPROPERTY(EditAnywhere, Category = "Misc")
	bool bOpenArmamentSoftWindow = true;

	// 着弾点と炸裂半径をデバッグ描画する
	UPROPERTY(EditAnywhere, Category = "Misc")
	bool bDebugDrawImpacts = false;

	virtual UAnimMontage* GetMontage() const override { return Montage; }

protected:

	// 発射位置 (ソケット指定があればそこ、無ければアクター基準のオフセット)
	FVector GetMuzzleLocation(const AEnemyCharacter* Enemy) const;
	// 1発を着弾点へ撃つ。着弾までの飛行時間を返す (予兆デカールの表示時間に使う)
	float SpawnCircusShot(AEnemyCharacter* Enemy, const FVector& SpawnLoc, const FVector& Target);
	// 着弾点へ予兆デカールを出し、進行度を回すキューへ積む
	void AddImpactDecal(AEnemyCharacter* Enemy, const FVector& Target, float LeadTime);
	// 予兆デカールの進行度を進め、着弾したものを破棄する (派生のOnAttackTickから呼ぶ)
	void UpdateImpactDecals(float DeltaTime);
	// 残っている予兆デカールを破棄する (派生のOnAttackEndから呼ぶ)
	void ClearImpactDecals();
	// XYはそのままに、真下の床へZを落とす
	FVector ProjectToGround(const AEnemyCharacter* Enemy, const FVector& Location) const;
	// 着弾点と炸裂半径を描く (bDebugDrawImpactsが有効なときだけ)
	void DebugDrawImpact(const AEnemyCharacter* Enemy, const FVector& Target) const;

private:

	// 着弾までの予兆デカール。進行度を回し、着弾時に破棄する
	struct FCircusDecal
	{
		float RemainingTime = 0.0f;
		float TotalTime = 0.0f;
		TWeakObjectPtr<UDecalComponent> Decal;
		TWeakObjectPtr<UMaterialInstanceDynamic> DecalMID;

	};
	TArray<FCircusDecal> PendingDecals;

};

/**
 * サーカス (リング展開)
 *
 * 自身を中心としたリング上の着弾点へ1波ぶんをまとめて撃ち込み、半径を広げながら
 * WaveCount回繰り返す。プレイヤーはデカールを読んで安全地帯へ逃げ込む。
 *
 * 半径が伸びるほど円周が長くなるので、弾数を据え置くと外側の波ほど着弾がスカスカになる。
 * 逃げ場の密度を波ごとに設計できるよう、リング半径と弾数はどちらも波ごとの配列で持つ
 * (WaveRadii / ShotsPerWave)。円周が2倍なら弾数も2倍にすると間隔が保たれる。
 * WaveAngleStepDegは波ごとに方位を回して、着弾点が放射状に一直線へ並ぶのを崩すためのもの。
 *
 * 発射の駆動はUMissileAttackExecutionと同じ。MissileStartで波カウンタをリセットし、
 * MissileShot 1回につき1波を撃ち、WaveCount回到達で "End" セクションへジャンプする
 * (モンタージュを共有するため波の刻みはNotify側が持つ)。
 */
UCLASS(meta = (DisplayName = "サーカス (リング展開)"))
class PRJ_TIDE_P0_API UCircusBarrageAttackExecution : public UCircusShotAttackExecutionBase
{
	GENERATED_BODY()

public:

	// 発射する波の数 (= 受け取るMissileShotの回数)
	UPROPERTY(EditAnywhere, Category = "Wave", meta = (ClampMin = "1"))
	int32 WaveCount = 3;

	// 波ごとの弾数 (= 着弾リングの分割数)。要素が足りない場合は最後の値を使い続ける
	// 外側の波ほど円周が伸びるので、間隔を保つには半径の比と同じだけ増やす
	UPROPERTY(EditAnywhere, Category = "Wave")
	TArray<int32> ShotsPerWave = { 8, 14, 20 };

	// 波ごとの着弾リング半径。要素が足りない場合は最後の半径にWaveRadiusStepを足して延長する
	UPROPERTY(EditAnywhere, Category = "Wave")
	TArray<float> WaveRadii = { 900.0f, 1700.0f, 2500.0f };

	// WaveRadiiが足りないときに1波ごとに広げる量
	UPROPERTY(EditAnywhere, Category = "Wave", meta = (ClampMin = "0.0"))
	float WaveRadiusStep = 800.0f;

	// 波ごとに着弾方位を回す角度 (度)。前の波の隙間へ落とすためのずらし量
	UPROPERTY(EditAnywhere, Category = "Wave")
	float WaveAngleStepDeg = 15.0f;

	// 攻撃ごとにリングの基準方位をランダム抽選する。falseならEMの正面が0度
	UPROPERTY(EditAnywhere, Category = "Wave")
	bool bRandomizeBaseAngle = true;

	// ダウン明けの反撃として使う場合にtrue
	// 技の実行中、破壊されていない部位光輪を発光させ、アクター中心のAOE内へ接触ダメージを与える
	// (BossDataAssetのRecoveryTouch設定を使う)
	UPROPERTY(EditAnywhere, Category = "Recovery")
	bool bEnableRecoveryTouchDamage = true;

	virtual void OnAttackBegin(AEnemyCharacter* Enemy) override;
	virtual void OnAttackEvent(AEnemyCharacter* Enemy, FGameplayTag EventTag) override;
	virtual void OnAttackTick(AEnemyCharacter* Enemy, float DeltaTime) override;
	virtual void OnAttackEnd(AEnemyCharacter* Enemy) override;

private:

	// 1波ぶん (その波の弾数) をまとめて撃つ
	void FireWave(AEnemyCharacter* Enemy, int32 WaveIndex);
	float GetWaveRadius(int32 WaveIndex) const;
	int32 GetWaveShotCount(int32 WaveIndex) const;

	int32 FiredWaveCount = 0;
	float BaseAngleDeg = 0.0f;

};

/**
 * サーカス (一斉降雨)
 *
 * 最初のShotを合図に、6秒ぶんの弾を全弾まとめて上空へ打ち上げてホバー保持させ、その場で "End" へ
 * ジャンプして攻撃を終える (ボスは即座に別の攻撃を抽選できる)。以降はコンダクター
 * (ACircusRainConductor) が6秒間、各弾を時間差で降下させる。
 *
 * 肝は「降下開始のその瞬間にPC位置を再サンプルして着弾点を確定する」こと。打ち上げは一斉でも、
 * 各弾は自分の降下時刻にPCの現在位置基準へ撃ち下ろすので、直進もジグザグも追従する。降下弧は
 * ホバー位置から着弾点までのUArcPathBehaviorで、予兆デカールは着弾点と一致する。
 * 最後の弾はDescentStartDelay + FireDuration + DescentFlightDurationあたりで着弾する。
 *
 * NOTE: 全弾 (最大ceil(FireDuration/FireInterval)*ShotsPerFire発) が同時に浮くので弾プールの
 * 容量に注意する。ホバー中に寿命切れしないよう、各弾のMaxLifeTimeはスポーン時に上書きする。
 */
UCLASS(meta = (DisplayName = "サーカス (一斉降雨)"))
class PRJ_TIDE_P0_API UCircusRainAttackExecution : public UCircusShotAttackExecutionBase
{
	GENERATED_BODY()

public:

	// 降下開始を散らす時間幅 (秒)。最初の降下から最後の降下までをこの幅に分布させる
	UPROPERTY(EditAnywhere, Category = "Rain", meta = (ClampMin = "0.1"))
	float FireDuration = 6.0f;

	// 総弾数の算出に使う名目上の発射間隔 (秒)
	// 総弾数 = ceil(FireDuration/FireInterval)*ShotsPerFire
	UPROPERTY(EditAnywhere, Category = "Rain", meta = (ClampMin = "0.02"))
	float FireInterval = 0.35f;

	// 総弾数の算出に使う名目上の1発射あたり弾数
	UPROPERTY(EditAnywhere, Category = "Rain", meta = (ClampMin = "1"))
	int32 ShotsPerFire = 1;

	// --- 打ち上げ / ホバー ---

	// 打ち上げ先 (ホバー) のボス頭上高さ
	UPROPERTY(EditAnywhere, Category = "Rain|Hover", meta = (ClampMin = "0.0"))
	float HoverHeight = 1500.0f;

	// ホバー位置をボス中心から散らす水平半径。全弾がここに散って浮かぶ
	UPROPERTY(EditAnywhere, Category = "Rain|Hover", meta = (ClampMin = "0.0"))
	float HoverSpreadRadius = 900.0f;

	// 全弾が上空へ上がりきるのを待って降下を始めるまでの遅延 (秒)
	// 上昇時間 (HoverHeight/展開速度) 以上を推奨
	UPROPERTY(EditAnywhere, Category = "Rain|Hover", meta = (ClampMin = "0.0"))
	float DescentStartDelay = 1.3f;

	// 降下開始時刻を弾ごとにばらす割合 (0 = 等間隔)
	UPROPERTY(EditAnywhere, Category = "Rain|Hover", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DescentTimeJitter = 0.3f;

	// --- 降下 ---

	// 降下 (ホバー → 着弾) の飛行時間 (秒) = 予兆デカールの表示時間
	UPROPERTY(EditAnywhere, Category = "Rain|Descent", meta = (ClampMin = "0.1"))
	float DescentFlightDuration = 1.6f;

	// 降下開始で頂点から一度上へ抜ける量
	// 大きいほど「上ってから曲がって落ちる」山越えが強調され、
	// 着弾点へガクッと向く感じが消える。小さいと最短で降りる
	UPROPERTY(EditAnywhere, Category = "Rain|Descent", meta = (ClampMin = "0.0"))
	float DescentSwoopHeight = 300.0f;

	// 着弾点の真上へ置く制御点の高さ。大きいほど垂直に近い角度で落ちる
	UPROPERTY(EditAnywhere, Category = "Rain|Descent", meta = (ClampMin = "0.0"))
	float DescentApproachHeight = 800.0f;

	// 着弾点を散らすプレイヤー中心の半径。降下開始時のPC位置基準で円内一様
	UPROPERTY(EditAnywhere, Category = "Rain|Descent", meta = (ClampMin = "0.0"))
	float ScatterRadius = 800.0f;

	// PCの速度から何秒先の位置を予測して着弾円の中心に置くか (0で現在位置)
	UPROPERTY(EditAnywhere, Category = "Rain|Descent", meta = (ClampMin = "0.0"))
	float PredictLeadTime = 0.3f;

	virtual void OnAttackBegin(AEnemyCharacter* Enemy) override;
	virtual void OnAttackEvent(AEnemyCharacter* Enemy, FGameplayTag EventTag) override;
	virtual void OnAttackEnd(AEnemyCharacter* Enemy) override;

private:

	// 全弾を上空ホバーへ打ち上げ、コンダクターを立ててそれぞれの降下時刻を予約する
	void LaunchVolley(AEnemyCharacter* Enemy);
	// 1発をSpawnLocからHoverPosへ打ち上げてホバー保持させる
	// ダメージ注入と寿命上書きも行う。RiseDuration秒でちょうど頂点へ着くよう上昇速度を調整し、
	// 静止ホバーを実質無くす
	class AEnemyProjectile* SpawnHoverMissile(AEnemyCharacter* Enemy, const FVector& SpawnLoc,
		const FVector& HoverPos, float MissileLifeSpan, float RiseDuration) const;

	// 同一攻撃内でShot通知が複数来ても再射出しないためのガード
	bool bFired = false;

};

/**
 * @brief 速度弾攻撃
 *
 * MissileStartで発射する全弾(行ごとの弾数の合計)を敵の頭上へ、LineFanAngleStepずつ
 * 角度をずらした扇状の待機フォーメーションでスポーンし待機させる(見た目は出るが移動しない)。
 * MissileShotを受けるたびに待機中の弾から1行分を取り出し、敵からターゲット方向へ
 * RowSpacing×(行番号+1)進めた半径上へ、同じくLineFanAngleStepの扇状の着弾として降らせる。
 *
 * @author I-Fukunaka
 */
UCLASS(meta = (DisplayName = "速度弾"))
class PRJ_TIDE_P0_API USpeedMissileAttackExecution : public UTideAttackExecution
{
	GENERATED_BODY()
public:

	UPROPERTY(EditAnywhere)
	TObjectPtr<UAnimMontage> Montage;

	// 弾プロファイル(Movementに「爆撃(ベジェ弧)」ビヘイビアを設定する)
	UPROPERTY(EditAnywhere)
	TObjectPtr<UProjectileProfile> BombardProfile;

	// 行(Line)ごとの弾数。配列の要素数がそのまま行数(= MissileShotを受け取る回数、
	// 頭上待機スポーンの行数)になる。先頭が敵に一番近い行
	UPROPERTY(EditAnywhere)
	TArray<int32> LineBulletCounts = { 5, 7, 9 };

	// 着弾の扇1発あたりの角度間隔(度)。中心方向 (敵→ターゲット) を基準に中心対称でずらす。
	// 行の弾数が多いほど扇全体の開き角も広がる
	UPROPERTY(EditAnywhere)
	float LineFanAngleStep = 12.0f;

	// 頭上待機フォーメーションの扇1発あたりの角度間隔(度)。着弾側のLineFanAngleStepとは別に調整する
	UPROPERTY(EditAnywhere)
	float HeadLineFanAngleStep = 12.0f;

	// 頭上待機フォーメーションの扇の半径 (Line 0 = 最初の行の半径。敵の向きを中心方向とする)
	UPROPERTY(EditAnywhere)
	float HeadLineFanRadius = 150.0f;

	// 行が1つ進むごとに頭上フォーメーションの半径を広げる量。中心 (SpawnHeightOffsetの位置)
	// はどの行も共通で、行ごとに半径だけ変えて同心円状に重ねる
	UPROPERTY(EditAnywhere)
	float HeadLineRadiusStep = 150.0f;

	// 行間隔。敵からターゲット方向へ、この間隔ずつ進めた半径上へ着弾の扇を並べる
	UPROPERTY(EditAnywhere)
	float RowSpacing = 400.0f;

	// 着弾点の発射主からの最小距離クランプ
	UPROPERTY(EditAnywhere)
	float MinDistFromEnemy = 1400.0f;

	// 頭上フォーメーションの中心の高さオフセット(アクター原点基準)。
	// どの行もこの同じ位置を中心として扇を広げる (行ごとの違いはHeadLineRadiusStepによる半径だけ)
	UPROPERTY(EditAnywhere)
	float SpawnHeightOffset = 200.0f;

	// 弾の飛行秒数
	UPROPERTY(EditAnywhere)
	float BombardFlightDuration = 1.5f;

	virtual UAnimMontage* GetMontage() const override { return Montage; }
	virtual void OnAttackBegin(AEnemyCharacter* Enemy) override;
	virtual void OnAttackEvent(AEnemyCharacter* Enemy, FGameplayTag EventTag) override;
	virtual void OnAttackEnd(AEnemyCharacter* Enemy) override;

private:

	// MissileStartで全弾(行ごとの弾数の合計)を敵頭上へ散らばらせてスポーンし待機させる
	void SpawnAllMissiles(AEnemyCharacter* Enemy);
	// MissileShotで待機中の弾から1行分を取り出し、着弾列へ向けて解放する
	void FireMissileLine(AEnemyCharacter* Enemy);
	// 行番号 (0 = 敵に一番近い行) に応じた、その行の弾数を返す (LineBulletCountsを参照)
	int32 GetLineBulletCountForRow(int32 RowIndex) const;

	FVector CachedPlayerPos = FVector::ZeroVector;
	FVector CachedForward2D = FVector::ForwardVector;
	int32 ShotFiredCount = 0;

	// MissileStartでスポーンし、MissileShotのたびに順次取り出して発射する待機中の弾
	TArray<TWeakObjectPtr<class AEnemyProjectile>> HeldMissiles;
	int32 NextHeldMissileIndex = 0;

};

/**
 * 追尾弾攻撃(左右どちらかへ逸らして発射→旋回ホーミング・時限自爆)
 *
 * MissileStartでカウンターをリセットし、1発目のMissileShot受信時にPCの移動方向から
 * 逸らす左右を確定する(PCが向かう側とは逆へ逸らす。停止中は右へ)。以降のショットも同じ向きを使う。
 *
 * 各ショットで敵の背後から1発スポーンし、ターゲット方向からInitialLaunchAngleDegだけ
 * 左右どちらかへ逸らした向きで撃ち出す。以降はプロファイル側のUHomingProjectileBehavior
 * (旋回速度制限ホーミング)がターゲットへ徐々に向き直すことで、弧を描いて追尾する見た目になる。
 * MaxLifeTime/bDetonateOnHit/bDetonateOnExpire等はすべてUProjectileProfile側の設定に一本化し、
 * 攻撃実行側からは上書きしない(ダメージ値の注入を除く)。命中時・寿命切れ時に自爆させたい場合は
 * プロファイル側のUSplashDamageBehaviorでbDetonateOnHit/bDetonateOnExpireをONにしておくこと。
 */
UCLASS(meta = (DisplayName = "追尾弾"))
class PRJ_TIDE_P0_API UHomingShotAttackExecution : public UTideAttackExecution
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere)
	TObjectPtr<UAnimMontage> Montage;

	// 追尾弾の弾プロファイル(Movementに「ホーミング」ビヘイビア、
	// Effectsに「範囲ダメージ (着弾炸裂)」を設定する)
	UPROPERTY(EditAnywhere)
	TObjectPtr<UProjectileProfile> HomingProfile;

	// 発射回数(= MissileShotを受け取る回数)
	UPROPERTY(EditAnywhere)
	int32 ShotCount = 3;

	// スポーン位置の後方オフセット
	UPROPERTY(EditAnywhere)
	float SpawnBackOffset = 80.0f;

	// スポーン位置の高さオフセット
	UPROPERTY(EditAnywhere)
	float SpawnHeightOffset = 200.0f;

	// 発射直後にターゲット方向から左右どちらかへ逸らす角度(度)。
	// ここからホーミングの旋回性能でターゲットへ収束するので、弧を描く度合いを左右する主役パラメータ
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0", ClampMax = "179.0"))
	float InitialLaunchAngleDeg = 45.0f;

	virtual UAnimMontage* GetMontage() const override { return Montage; }
	virtual void OnAttackBegin(AEnemyCharacter* Enemy) override;
	virtual void OnAttackEvent(AEnemyCharacter* Enemy, FGameplayTag EventTag) override;
	virtual void OnAttackEnd(AEnemyCharacter* Enemy) override;

private:

	void SpawnHomingShot(AEnemyCharacter* Enemy);

	FVector CachedForward2D = FVector::ForwardVector; // 敵->PC方向 (逸らし角の基準)
	float CurveSideSign = 1.0f; // +1で右、-1で左 (初弾発射時にPCの移動方向から確定)
	int32 ShotFiredCount = 0;

};
