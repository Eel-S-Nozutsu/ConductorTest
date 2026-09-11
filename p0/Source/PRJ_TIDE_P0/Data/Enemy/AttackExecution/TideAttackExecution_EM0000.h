// Copyright (c) 2026, S.Nozutsu EelGameStudio, Inc. All Rights Reserved.

#pragma once

#include "PRJ_TIDE_P0/Data/Enemy/AttackExecution/TideAttackExecution.h"
#include "Engine/HitResult.h"
#include "TideAttackExecution_EM0000.generated.h"

class UProjectileProfile;
class UEnemyAnimInstance;
class AFlameStreamHazard;
class UDecalComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class AEnemyProjectile;

/**
 * 弾発射攻撃
 * 
 * 開始→ループ→終了の3構成 / async。GetMontage() がnullptrで動作し自前で進める。
 *   1. OnAttackBegin: StartMontageを再生。
 *   2. StartMontage終了で発射開始(bLineUpBeforeFire=横一列待機 / それ以外=順次バースト)。同時にLoopMontageをループ再生。
 *   3. 撃ちきるまでループし、経過後にEndMontageを再生、その終了でFinishDelegate。
 * 発射元はメッシュのMuzzleSocketNameソケットでプレイヤーへ向けてピッチ/ヨーを補正。
 * 横一列(bLineUpBeforeFire)はbLineUpParallelで全弾を平行に直進させる(収束させない)。
 * さらにbLineUpLevelPitchでピッチを0に固定し、生成高さを保ったまま水平に飛ばせる。
 */
UCLASS(meta = (DisplayName = "弾発射攻撃"))
class PRJ_TIDE_P0_API UProjectileAttackExecution : public UTideAttackExecution
{
	GENERATED_BODY()

public:

	// 開始モンタージュ(構え)。終了で発射を開始する
	UPROPERTY(EditAnywhere, Category = "Montage")
	TObjectPtr<UAnimMontage> StartMontage;

	// ループモンタージュ(発射中)。撃ちきるまでループする
	UPROPERTY(EditAnywhere, Category = "Montage")
	TObjectPtr<UAnimMontage> LoopMontage;

	// 終了モンタージュ(後隙)。撃ちきったら再生し、終了で攻撃完了
	UPROPERTY(EditAnywhere, Category = "Montage")
	TObjectPtr<UAnimMontage> EndMontage;

	// 撃ちきった後、終了モンタージュへ移るまでの追加待機秒数
	UPROPERTY(EditAnywhere, Category = "Montage", meta = (ClampMin = "0.0"))
	float PostFireHold = 0.0f;

	// 発射する弾プロファイル
	UPROPERTY(EditAnywhere)
	TObjectPtr<UProjectileProfile> Profile;

	// 発射する弾の総数
	UPROPERTY(EditAnywhere, meta = (ClampMin = "1"))
	int32 ProjectileCount = 3;

	// 弾と弾の間の発射間隔
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"))
	float FireInterval = 0.2f;

	// 発射元ソケット名
	UPROPERTY(EditAnywhere)
	FName MuzzleSocketName = FName("middle_01_r");

	// プレイヤーへヨーを補正する最大角度(度)。これを超える差なら正面のまま撃つ
	UPROPERTY(EditAnywhere)
	float ProjectileYawCorrectionAngle = 30.0f;

	// 拡散射撃モード。trueで「PCを狙う弾」と「散らす弾」を弾ごとに抽選する(下記Spread各値)
	// falseで毎回PCの現在位置を素直に狙う(追尾弾プロファイルと組み合わせる用)
	UPROPERTY(EditAnywhere)
	bool bUseSpread = false;

	// 弾ごとに「PCを狙う弾」かどうかを抽選する確率。1 = 必ず狙う / 0 = 必ず散らす
	// 抽選に外れた弾は狙い方向へ上下左右ランダムを加えて散らす(棒立ち対策で一定割合は当てる)
	UPROPERTY(EditAnywhere, Category = "Spread", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AimAtPlayerChance = 0.5f;

	// 散らし弾の左右ランダム角の振れ幅(度)。[-値, +値] の一様乱数を狙いヨーへ加える
	UPROPERTY(EditAnywhere, Category = "Spread", meta = (ClampMin = "0.0"))
	float SpreadYawRangeDeg = 25.0f;

	// 散らし弾の上下ランダム角の振れ幅(度)。[-値, +値] の一様乱数を狙いピッチへ加える
	UPROPERTY(EditAnywhere, Category = "Spread", meta = (ClampMin = "0.0"))
	float SpreadPitchRangeDeg = 15.0f;

	// 狙う場所(迎撃点)にデバッグ球を描画し、発射方向と線で結ぶ(狙い弾=緑 / 散らし弾=橙)
	UPROPERTY(EditAnywhere, Category = "Debug")
	bool bDebugDrawAim = false;

	// 横一列に並べて待機してから発射するモード
	// ON  : オーナーの左右へ中心対称・等間隔でProjectileCount発を一直線に生成し、
	// 各弾がLineUpHoldDuration秒待機してから発射する
	// OFF : 従来どおりMuzzleSocketからFireInterval間隔で順次発射する
	UPROPERTY(EditAnywhere, Category = "LineUp")
	bool bLineUpBeforeFire = true;

	// 横一列の全弾を平行に直進させる(各弾がPCへ収束せず、横幅を保ったまま前進＝弾幕の壁)
	// falseで各弾がPCを狙う(中央へ収束)
	UPROPERTY(EditAnywhere, Category = "LineUp", meta = (EditCondition = "bLineUpBeforeFire"))
	bool bLineUpParallel = true;

	// 平行発射のピッチを0に固定して水平に撃つ(ヨーだけPCへ向ける)
	// 生成高さ(LineUpHeightOffset)を保ったまま飛ぶので、
	// PCのジャンプや高低差で壁が斜めにならない
	// falseでPCの頭へピッチを向ける(壁ごと上下に傾く)
	UPROPERTY(EditAnywhere, Category = "LineUp", meta = (EditCondition = "bLineUpBeforeFire && bLineUpParallel"))
	bool bLineUpLevelPitch = true;

	// true: 生成時点のPC方向を初期値として直線の隊列を保ったまま、
	// LineUpHoldDurationの間敵を中心に位置・向きともにPCの現在位置へ追従し続ける
	// (PCが動いた場合のみ回転で補正する。敵の現在の向きからの大きな振り向き演出はしない)
	// false: 従来通り生成時に1回だけ狙いを決めて固定する
	UPROPERTY(EditAnywhere, Category = "LineUp", meta = (EditCondition = "bLineUpBeforeFire && bLineUpParallel"))
	bool bLineUpSweepToTarget = false;

	// 追従回転の最大速度(度/秒)。PCが急に動いても急に振り向かせず、この速度でクランプする
	// LineUpHoldDurationが短い／PCの移動が速い場合、
	// 追いつかず途中で発射されることがある
	UPROPERTY(EditAnywhere, Category = "LineUp", meta = (ClampMin = "0.0", EditCondition = "bLineUpBeforeFire && bLineUpParallel && bLineUpSweepToTarget"))
	float MaxSweepTurnRateDegPerSec = 180.0f;

	// 生成してから発射するまでの待機秒数
	UPROPERTY(EditAnywhere, Category = "LineUp", meta = (ClampMin = "0.0", EditCondition = "bLineUpBeforeFire"))
	float LineUpHoldDuration = 1.0f;

	// 隣り合う弾同士の間隔。中心を挟んで左右対称・等間隔に一直線で並ぶ
	UPROPERTY(EditAnywhere, Category = "LineUp", meta = (ClampMin = "0.0", EditCondition = "bLineUpBeforeFire"))
	float LineUpSpacing = 80.0f;

	// 生成列を敵からPCの方向(水平)へどれだけ出すか。PC不在時は敵の正面をフォールバックにする
	UPROPERTY(EditAnywhere, Category = "LineUp", meta = (EditCondition = "bLineUpBeforeFire"))
	float LineUpForwardOffset = 80.0f;

	// 生成列の高さオフセット
	UPROPERTY(EditAnywhere, Category = "LineUp", meta = (EditCondition = "bLineUpBeforeFire"))
	float LineUpHeightOffset = 0.0f;

	// asyncモード(自前で開始→ループ→終了を進める)
	virtual UAnimMontage* GetMontage() const override { return nullptr; }
	virtual void OnAttackBegin(AEnemyCharacter* Enemy) override;
	virtual void OnAttackTick(AEnemyCharacter* Enemy, float DeltaTime) override;
	virtual void OnAttackEnd(AEnemyCharacter* Enemy) override;

private:

	// 発射開始: LoopMontage再生 + バースト/横一列発射 + 撃ちきり時間
	// (FireLoopDuration)の算出
	void BeginLoop();
	// 撃ちきり後: EndMontage再生(無ければ即完了)
	void BeginEnd();

	void BeginFireBurst(AEnemyCharacter* Enemy);
	void FireOne();

	// 横一列モード: オーナーの左右へ交互にオフセットして全弾を待機付きで生成する
	void BeginLineUp(AEnemyCharacter* Enemy);

	// FromLocationから目標への発射回転を求める(リード/拡散込み)
	// bOutAimAtPlayerは抽選結果
	FRotator ComputeAimRotation(AEnemyCharacter* Enemy,
		const FVector& FromLocation, bool& bOutAimAtPlayer) const;

	// 指定位置に弾を1発生成する。HoldDuration>0ならその秒数だけ待機してから発射する
	// OverrideRotationを渡すと照準計算を使わずその回転で撃つ(横一列の平行発射用)
	// 戻り値は生成した弾 (失敗時nullptr)。ワインドアップ回転の追従対象を拾うのに使う
	AEnemyProjectile* SpawnProjectileAt(AEnemyCharacter* Enemy, const FVector& Location, float HoldDuration,
		const FRotator* OverrideRotation = nullptr);

	// bLineUpSweepToTarget用: LineUpHoldDurationの間、
	// 直線隊列を保ったまま敵中心にPCへ向けて回転させる
	void UpdateLineUpSweep(AEnemyCharacter* Enemy, float DeltaTime);

	// 開始→ループの繋ぎ、および撃ちきるまでのループ再トリガ
	UFUNCTION()
	void OnMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted);
	// 終了モンタージュ完了でFinishDelegate
	UFUNCTION()
	void OnEndMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	enum class EProjectilePhase : uint8 { None, Starting, Looping, Ending };
	EProjectilePhase Phase = EProjectilePhase::None;

	TWeakObjectPtr<AEnemyCharacter> CachedEnemy;
	TWeakObjectPtr<UAnimInstance> CachedAnimInstance;
	FTimerHandle FireTimerHandle;
	int32 RemainingShots = 0;

	float FireLoopElapsed = 0.0f;
	float FireLoopDuration = 0.0f;

	// bLineUpSweepToTarget用の追従状態
	struct FLineUpSweepEntry
	{
		TWeakObjectPtr<AEnemyProjectile> Projectile;
		float IndexOffset = 0.0f; // 中心からの段オフセット (LineUpSpacing倍率)

	};
	TArray<FLineUpSweepEntry> LineUpSweepEntries;
	bool bLineUpSweeping = false;
	float LineUpSweepElapsed = 0.0f;
	// 現在の回転角(MaxSweepTurnRateDegPerSecでクランプしながら目標へ近づける、
	// 毎フレーム更新する状態)
	float LineUpSweepCurrentYaw = 0.0f;
	float LineUpSweepCurrentPitch = 0.0f;

};

/**
 * 扇状爆撃攻撃(開始→ループ→終了の3構成 / async)
 *
 * GetMontage() がnullptrのasyncモードで動作し、開始→展開ループ→終了を自前で進める。
 *   1. OnAttackBegin: StartMontageを再生。
 *   2. StartMontage終了で展開: BeginVolleyでEMの左右2つの円に弾を黄金角の螺旋状で一斉生成し、
 *      奥→手前へRowInterval間隔で1段ずつ発射する。同時にLoopMontageをループ再生。
 *   3. 撃ちきるまで(= SpawnHoldDuration + (RowCount-1)*RowInterval + PostFireHold秒)ループし、
 *      経過後にEndMontageを再生、その終了でFinishDelegate。
 *
 * 各弾は生成位置で待機したのち山なり弧でEM前方(PC方向)の扇状エリアへ着弾。
 * 着弾点は角度方向ColumnCount列×奥行きRowCount段の極座標グリッド。
 */
UCLASS(meta = (DisplayName = "扇状爆撃攻撃"))
class PRJ_TIDE_P0_API UFanBombardAttackExecution : public UTideAttackExecution
{
	GENERATED_BODY()

public:

	// 開始モンタージュ(構え)。終了で展開＋発射を開始する
	UPROPERTY(EditAnywhere, Category = "Montage")
	TObjectPtr<UAnimMontage> StartMontage;

	// ループモンタージュ(発射中)。撃ちきるまでループする
	UPROPERTY(EditAnywhere, Category = "Montage")
	TObjectPtr<UAnimMontage> LoopMontage;

	// 終了モンタージュ(後隙)。撃ちきったら再生し、終了で攻撃完了
	UPROPERTY(EditAnywhere, Category = "Montage")
	TObjectPtr<UAnimMontage> EndMontage;

	// 撃ちきった後、終了モンタージュへ移るまでの追加待機秒数
	UPROPERTY(EditAnywhere, Category = "Montage", meta = (ClampMin = "0.0"))
	float PostFireHold = 0.0f;

	// 発射する弾プロファイル
	UPROPERTY(EditAnywhere)
	TObjectPtr<UProjectileProfile> BombardProfile;

	// 扇の角度幅(度)。EM→PC方向を中心に左右へ均等に広がる
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0", ClampMax = "360.0"))
	float FanAngleDeg = 90.0f;

	// 角度方向(横)の着弾点数
	UPROPERTY(EditAnywhere, meta = (ClampMin = "1"))
	int32 ColumnCount = 5;

	// 奥行き方向(距離)の段数。この段数を奥→手前に順次発射する
	UPROPERTY(EditAnywhere, meta = (ClampMin = "1"))
	int32 RowCount = 4;

	// 着弾距離の最小(手前)/最大(奥)。EM中心からの水平距離
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"))
	float NearRadius = 300.0f;

	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"))
	float FarRadius = 1500.0f;

	// 段(奥→手前)ごとの発射間隔
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"))
	float RowInterval = 0.25f;

	// スポーン位置: EM頭上の高さオフセット
	UPROPERTY(EditAnywhere)
	float SpawnHeightOffset = 300.0f;

	// ---- 生成オフセット(左右2円・同心円/螺旋配置)----
	
	// 生成してから発射を開始するまでの基準待機秒数
	UPROPERTY(EditAnywhere, Category = "Spawn", meta = (ClampMin = "0.0"))
	float SpawnHoldDuration = 0.6f;

	// 生成する左右の円の半径
	UPROPERTY(EditAnywhere, Category = "Spawn", meta = (ClampMin = "0.0"))
	float SpawnCircleRadius = 250.0f;

	// 左右の円の中心を、オーナー中心から左右へどれだけ離すか
	UPROPERTY(EditAnywhere, Category = "Spawn", meta = (ClampMin = "0.0"))
	float SpawnCircleSideSeparation = 350.0f;

	// 生成円をオーナーから前方へどれだけ出すか
	UPROPERTY(EditAnywhere, Category = "Spawn")
	float SpawnCircleForwardOffset = 100.0f;

	// asyncモード(自前で開始→ループ→終了を進める)
	virtual UAnimMontage* GetMontage() const override { return nullptr; }
	virtual void OnAttackBegin(AEnemyCharacter* Enemy) override;
	virtual void OnAttackTick(AEnemyCharacter* Enemy, float DeltaTime) override;
	virtual void OnAttackEnd(AEnemyCharacter* Enemy) override;

private:

	// 展開: LoopMontage再生 + BeginVolley + 撃ちきり時間
	// (FireLoopDuration)の算出
	void BeginLoop();
	// 撃ちきり後: EndMontage再生(無ければ即完了)
	void BeginEnd();

	// 左右2円に螺旋配置で全弾を待機付き生成し、段ごとに発射遅延を与える
	void BeginVolley(AEnemyCharacter* Enemy);
	// 生成位置でHoldDuration待機してからLandingへ弧で発射する弾を1発生成する
	void SpawnBombardHeld(AEnemyCharacter* Enemy, const FVector& SpawnPos,
		const FVector& Landing, float HoldDuration);

	// 開始→ループの繋ぎ、および撃ちきるまでのループ再トリガ
	UFUNCTION()
	void OnMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted);
	// 終了モンタージュ完了でFinishDelegate
	UFUNCTION()
	void OnEndMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	enum class EBombardPhase : uint8 { None, Starting, Looping, Ending };
	EBombardPhase Phase = EBombardPhase::None;

	TWeakObjectPtr<AEnemyCharacter> CachedEnemy;
	TWeakObjectPtr<UAnimInstance> CachedAnimInstance;

	float FireLoopElapsed = 0.0f;
	float FireLoopDuration = 0.0f;

};

/**
 * 火炎放射の照準の振る舞い。
 *
 * 噴射中の見た目はUEnemyAnimInstance::FlamethrowerAimBlend(-1=左/0=正面/+1=右) をX軸に取る
 * BlendSpaceで作られる。ここはその値と、火炎の実方向(SetExternalYaw)の両方を決める
 */
UENUM()
enum class EFlameAimMode : uint8
{
	// PCを追い続ける。左右前モーションがブレンドし、火炎も一緒に振れる
	Track       UMETA(DisplayName = "追尾する"),

	// 噴射開始の向きで固定する。モーションも火炎も開始時の角度のまま動かない
	LockOnFire  UMETA(DisplayName = "噴射開始で固定"),

	// 常に正面。首振りモーションを一切使わず、体の正面へ真っ直ぐ噴射する
	ForwardOnly UMETA(DisplayName = "正面のみ(首振りなし)"),
};

/**
 * 火炎放射の隊形。人数・立ち位置・向き・狙う方向をまとめて決める。
 *
 * これらは独立した設定ではなく「背中合わせとはこういうもの」という一組なので、
 * 個別のフラグに分けず種類で選ばせる。尺や間合い(スタンバイ時間・発射間隔・射程倍率など)は
 * 別プロパティで種類に依らず調整できる
 */
UENUM()
enum class EFlameFormationType : uint8
{
	// 単体噴射。募集しない
	Solo UMETA(DisplayName = "単体"),

	// 2体・背中合わせ。協力者はリーダーの真後ろへ回り、逆方向を向いて逆方向を狙う
	Duo  UMETA(DisplayName = "2体・背中合わせ"),

	// 3体・連続。リーダーが中央、協力者は左右対称。撃つ順はTrioFireOrderが決める
	Trio UMETA(DisplayName = "3体・連続"),
};

/**
 * 3体・連続の発射順。中央(リーダー)が常に1番手で、2・3番手をどちらの側から出すか。
 *
 * 左右は「リーダーから見た」向き。PC側から画面を見ると左右が入れ替わることに注意
 */
UENUM()
enum class EFlameTrioFireOrder : uint8
{
	// PCのいる側を2番手にする。PCの位置で順番が変わる
	PlayerSide UMETA(DisplayName = "PCのいる側から"),

	// 常に左→右。PCの位置に依らず順番が固定される
	LeftFirst  UMETA(DisplayName = "左→右で固定"),

	// 常に右→左
	RightFirst UMETA(DisplayName = "右→左で固定"),
};

/**
 * 火炎放射攻撃
 *
 * GetMontage() がnullptrのasyncモードで動作し、開始→ループ→終了を自前で進める。
 *   1. OnAttackBegin: StartMontageをスロットで再生する。この時点ではループ噴射は無効
 *      (bFlamethrowerActive=false)で、開始モーションが単独で見える。
 *   2. StartMontageのブレンドアウト開始でbFlamethrowerActive=trueにしてLoopingへ。
 *      開始のブレンドアウトとループBlendSpaceのフェードインがクロスし棒立ちが出ない。
 *      LoopDuration秒の間、毎フレームPC位置からFlamethrowerAimBlend(-1..+1) を更新し、
 *      左右前3モーションをPCの左右位置に応じてブレンドさせる。
 *   3. LoopDuration経過でEndMontageを再生(BlendSpaceを覆う)し、
 *      さらにRecoveryHold秒の後隙をとってからFinishDelegate。
 *
 * ループの左右前ブレンドはUEnemyAnimInstanceのFlamethrowerAimBlendをX軸に取る
 * 1D BlendSpaceをAnimBP側で組んで実現する(本クラスはパラメータ供給のみ担う)。
 *
 * 参加人数違い(3体連続 / 2体背中合わせ / 単体)は本クラスのBPサブクラスを人数分作り、
 * DT_Attackの行を分けて表現する。行を分けることで距離グループ・抽選重み・トークン保持時間を
 * 人数ごとに振れる。CDは行をまたいで共有したいのでFAttackEntryのCD共有グループを使う。
 * 詳細はDocs/FlamethrowerFormation.md
 */
UCLASS(meta = (DisplayName = "火炎放射攻撃"))
class PRJ_TIDE_P0_API UFlamethrowerAttackExecution : public UTideAttackExecution
{
	GENERATED_BODY()

public:

	// 噴射開始の予備動作モンタージュ
	UPROPERTY(EditAnywhere, Category = "Flamethrower")
	TObjectPtr<UAnimMontage> StartMontage;

	// 噴射停止の後隙モンタージュ
	UPROPERTY(EditAnywhere, Category = "Flamethrower")
	TObjectPtr<UAnimMontage> EndMontage;

	// ループ噴射の継続時間
	UPROPERTY(EditAnywhere, Category = "Flamethrower", meta = (ClampMin = "0.0"))
	float LoopDuration = 12.0f;

	// 終了モンタージュの後にさらに硬直する秒数。攻撃の後隙を伸ばす
	UPROPERTY(EditAnywhere, Category = "Flamethrower", meta = (ClampMin = "0.0"))
	float RecoveryHold = 0.0f;

	// 噴射中の照準の振る舞い
	UPROPERTY(EditAnywhere, Category = "Flamethrower")
	EFlameAimMode AimMode = EFlameAimMode::Track;

	// 照準ブレンドを±1に張り付かせるEM→PCのyaw差。正面のみ(首振りなし)では使わない
	UPROPERTY(EditAnywhere, Category = "Flamethrower", meta = (EditCondition = "AimMode != EFlameAimMode::ForwardOnly", ClampMin = "1.0"))
	float AimBlendMaxYaw = 90.0f;

	// 照準ブレンドの追従補間速度0即時反映
	UPROPERTY(EditAnywhere, Category = "Flamethrower", meta = (EditCondition = "AimMode != EFlameAimMode::ForwardOnly", ClampMin = "0.0"))
	float AimBlendInterpSpeed = 0.6f;

	// 放射する火炎の円錐ハザード。ループ中だけActivateされる
	// nullなら火炎判定なし(アニメのみ)
	UPROPERTY(EditAnywhere, Category = "Flamethrower|Hazard")
	TSubclassOf<AFlameStreamHazard> FlameHazardClass;

	// 火炎の噴射口ソケット。ここへアタッチし、円錐の原点(位置)がソケットに追従する
	// 向きはソケット任せではなくプログラム制御(EMの向き +
	// AimBlend×AimBlendMaxYawをSetExternalYawで渡す)
	UPROPERTY(EditAnywhere, Category = "Flamethrower|Hazard")
	FName FlameSocketName = FName("flamethrower");

	// ソケット基準の位置オフセット。噴射口の微調整用
	UPROPERTY(EditAnywhere, Category = "Flamethrower|Hazard")
	FVector FlameRelativeOffset = FVector::ZeroVector;

	// ソケット基準の回転オフセット。ボーン軸と火炎の向きがずれる場合に補正する
	UPROPERTY(EditAnywhere, Category = "Flamethrower|Hazard")
	FRotator FlameRelativeRotation = FRotator(0.0f, 0.0f, 25.0f);

	// 火炎の到達距離を発生源から徐々に伸ばす秒数。0で即最大(従来どおり予兆なし)
	// 噴射開始直後は手前しか届かず、時間経過で奥へ伸びるので離れた相手に反応の猶予が出る
	UPROPERTY(EditAnywhere, Category = "Flamethrower|Hazard", meta = (ClampMin = "0.0"))
	float FlameGrowDuration = 1.6f;

	// 伸び始めの到達距離。0で発生源から。到達距離の上限はFlameRange
	UPROPERTY(EditAnywhere, Category = "Flamethrower|Hazard", meta = (ClampMin = "0.0", EditCondition = "FlameGrowDuration > 0.0"))
	float FlameStartRange = 0.0f;

	// 火炎の到達距離。噴射口から前方へどれだけ届くか＝攻撃判定の長さ
	// 0でHazard BPのRangeをそのまま使う。1つのHazard BPを人数違いの隊形で
	// 使い回しつつ、間合いだけ行ごとに変えるための上書き
	UPROPERTY(EditAnywhere, Category = "Flamethrower|Hazard", meta = (ClampMin = "0.0"))
	float FlameRange = 0.0f;

	// --- フォーメーション
	// (協調攻撃) リーダーは開始時に同種の近くの敵を
	// RequiredCoopCount体ちょうど集めて指名し、
	// スロットへ飛び込ませる。全員が着地したらスタンバイを挟み、発射順に従って噴射させる
	// 協力者は同じ行を実行し、Directorの予約有無で自動的に「飛び込み→スタンバイ
	// →噴射」モードへ分岐する
	//
	// 人数違いの隊形はBPサブクラス＋DT行を分けて表現する:  Flame_Trio
	// (協力者2/近距離)  Flame_Duo (協力者1/中距離)  Flame_Solo
	// (隊形OFF)3行に同じCD共有グループを付けて、どれを撃っても火炎放射全体がCDに入るようにする

	// 隊形の種類。人数・立ち位置・向き・狙う方向はこれで決まる
	UPROPERTY(EditAnywhere, Category = "Flamethrower|Formation")
	EFlameFormationType FormationType = EFlameFormationType::Solo;

	// 3体連続のとき、左右の協力者を「どこに立たせるか」。PC方向を0度とした弧上の角度
	// 立ち位置だけを決める値で、向きには効かない (向きはTrioOutwardYawDeg)
	UPROPERTY(EditAnywhere, Category = "Flamethrower|Formation", meta = (EditCondition = "FormationType == EFlameFormationType::Trio", ClampMin = "0.0", ClampMax = "180.0"))
	float TrioSpreadAngleDeg = 45.0f;

	// 3体連続のとき、左右の協力者をPC方向から何度「外へ向けるか」。左は左へ、右は右へ開く
	// 中央(リーダー)は常にPCを向く。0で全員PC方向を向く (従来の見た目)
	// 正面のみ(首振りなし)では火炎の実方向もこの向きになるので、3人で扇状に面を張れる
	UPROPERTY(EditAnywhere, Category = "Flamethrower|Formation", meta = (EditCondition = "FormationType == EFlameFormationType::Trio", ClampMin = "0.0", ClampMax = "90.0"))
	float TrioOutwardYawDeg = 20.0f;

	// 同種を探す半径
	UPROPERTY(EditAnywhere, Category = "Flamethrower|Formation", meta = (EditCondition = "FormationType != EFlameFormationType::Solo", ClampMin = "0.0"))
	float RecruitRadius = 1500.0f;

	// リーダーから協力者を配置する距離。隊形の密度
	UPROPERTY(EditAnywhere, Category = "Flamethrower|Formation", meta = (EditCondition = "FormationType != EFlameFormationType::Solo", ClampMin = "0.0"))
	float FormationArcRadius = 160.0f;

	// 立ち位置とリーダーの足元の高低差の許容値。これを超える段差は別の床とみなし、
	// 隊形を組まない。崖の下や、地形の下に敷いてある床へ協力者が飛び込むのを防ぐ
	UPROPERTY(EditAnywhere, Category = "Flamethrower|Formation", meta = (EditCondition = "FormationType != EFlameFormationType::Solo", ClampMin = "0.0"))
	float MaxSlotHeightDiff = 120.0f;

	// 全員が配置に着いてから最初の噴射が始まるまでの静止秒数
	UPROPERTY(EditAnywhere, Category = "Flamethrower|Formation", meta = (EditCondition = "FormationType != EFlameFormationType::Solo", ClampMin = "0.0"))
	float StandbyDuration = 0.5f;

	// 協力者の着地を待つ上限秒数。超えたら着地していない個体を捨ててスタンバイへ進む
	// 経路や地形で飛び込みが完了しない個体が出ても、隊形全体が固まらないための足切り
	UPROPERTY(EditAnywhere, Category = "Flamethrower|Formation", meta = (EditCondition = "FormationType != EFlameFormationType::Solo", ClampMin = "0.0"))
	float StandbyJoinTimeout = 3.0f;

	// 発射順が1つ後ろの個体が噴射を始めるまでの間隔。0で全員同時
	UPROPERTY(EditAnywhere, Category = "Flamethrower|Formation", meta = (EditCondition = "FormationType != EFlameFormationType::Solo", ClampMin = "0.0"))
	float FireInterval = 0.0f;

	// 3体連続で2・3番手をどちらから撃たせるか。左右はリーダーから見た向き
	UPROPERTY(EditAnywhere, Category = "Flamethrower|Formation", meta = (EditCondition = "FormationType == EFlameFormationType::Trio"))
	EFlameTrioFireOrder TrioFireOrder = EFlameTrioFireOrder::PlayerSide;

	// 2番手を左右どちらから撃たせるかをPCの方位で決めるときの不感帯(度)
	// PCがほぼ正面ならこの角度内とみなし、右側を先に撃たせる
	UPROPERTY(EditAnywhere, Category = "Flamethrower|Formation", meta = (EditCondition = "FormationType == EFlameFormationType::Trio && TrioFireOrder == EFlameTrioFireOrder::PlayerSide", ClampMin = "0.0"))
	float SideDecisionDeadzoneDeg = 10.0f;

	// 協力者に実行させる攻撃エントリのRowName
	// 通常は**この攻撃自身のRowName**(自己参照)
	// 強制攻撃は抽選(距離/CD/CanActivate)を通らないので、
	// 同じ行をそのまま協力者にも実行させられる
	// 同じ行＝同じBPなので、隊形の全員が同じ噴射の尺・射程・追尾で揃う
	UPROPERTY(EditAnywhere, Category = "Flamethrower|Formation", meta = (EditCondition = "FormationType != EFlameFormationType::Solo"))
	FName CoopAttackRowName;

	// 募集のデバッグ描画。募集半径/近傍の同種と採否理由(色)/スロットとそのNav可否を出す
	// 緑=採用 白=非戦闘(ターゲット未取得) 橙=攻撃中 紫=予約済 赤=RowName引けず
	UPROPERTY(EditAnywhere, Category = "Flamethrower|Formation")
	bool bDrawFormationDebug = false;

	// デバッグ描画の表示秒数
	UPROPERTY(EditAnywhere, Category = "Flamethrower|Formation", meta = (EditCondition = "bDrawFormationDebug", ClampMin = "0.0"))
	float FormationDebugDuration = 3.0f;

	// スロットへ飛び込むジャンプモンタージュ(任意)。協力者が予約された時のみ使う
	UPROPERTY(EditAnywhere, Category = "Flamethrower|Formation")
	TObjectPtr<UAnimMontage> JumpMontage;

	// スロットへ飛び込む所要秒数。SetActorLocationによる曲線補間で必ず着地点へ届く
	UPROPERTY(EditAnywhere, Category = "Flamethrower|Formation", meta = (ClampMin = "0.05"))
	float JumpDuration = 0.6f;

	// 飛び込み放物線の頂点の高さ。0で直線移動
	UPROPERTY(EditAnywhere, Category = "Flamethrower|Formation", meta = (ClampMin = "0.0"))
	float JumpArcHeight = 300.0f;

	virtual UAnimMontage* GetMontage() const override { return nullptr; }
	virtual void OnAttackBegin(AEnemyCharacter* Enemy) override;
	virtual void OnAttackTick(AEnemyCharacter* Enemy, float DeltaTime) override;
	virtual void OnAttackEnd(AEnemyCharacter* Enemy) override;
	// 隊形に必要な人数の協力者を確保できる見込みがあるか (募集はせず数えるだけ)
	virtual bool CanActivate(const AEnemyCharacter* Enemy) const override;

private:

	void BeginLoop();
	void BeginEnd();
	// 終了モンタージュの後の後隙。RecoveryHoldが0なら即完了する
	void BeginRecovery();
	void UpdateAimBlend(AEnemyCharacter* Enemy, float DeltaTime);

	// 募集条件を満たす同種の敵を近い順に集める
	// CanActivateと実際の募集で条件をそろえるため共用する
	TArray<AEnemyCharacter*> GatherCandidates(const AEnemyCharacter* Leader) const;

	// --- 隊形の形状。FormationTypeから導く ---
	// 集める協力者の人数
	int32 GetRequiredCoopCount() const;
	// 協力者のスロット角(度)。0=PC方向 +右 -左180=真後ろ
	TArray<float> GetSlotAngles() const;
	// 背中合わせか。協力者がリーダーと逆を向き、狙いの基準もPCの反対側になる
	bool IsBackToBack() const { return FormationType == EFlameFormationType::Duo; }

	// 立ち位置の足場を確定する。Navには「歩ける場所か」だけを聞き、高さは実ジオメトリの
	// トレースで取り直す(NavポリゴンのZはボクセル化の分だけ実際の床から上下にずれるので、
	// そのまま着地点にすると曲面地形でカプセルが埋まる/浮く)
	// 置けない・リーダーと違う床ならfalse(隊形は不成立＝単体噴射へ落ちる)
	bool ResolveSlotGround(const AEnemyCharacter* Leader, FVector& InOutSlot) const;

	// リーダー: RequiredCoopCount体ちょうどをスロットへ指名する
	// 1人でも欠けたら予約を巻き戻してfalse (隊形は不成立=単体噴射へ落ちる)
	bool TryFormFormation(AEnemyCharacter* Leader);

	// リーダー: 協力者の着地を待ち、スタンバイ経過後に発射順を配る
	void TickStandbyAsLeader(AEnemyCharacter* Leader, float DeltaTime);
	// 協力者: リーダーから発射待ち秒数が届くのを待ち、届いたら数えて噴射へ移る
	void TickStandbyAsCoordinator(AEnemyCharacter* Enemy, float DeltaTime);
	// 発射順(中央=リーダー→PCのいる側→残り)を確定し、各協力者へ待ち秒数を書き込む
	void AssignFireOrder(AEnemyCharacter* Leader);

	// 協力者: 予約スロットへの飛び込みを開始する (MOVE_Flying +
	// SetActorLocationで曲線補間)
	void BeginJumpToSlot(AEnemyCharacter* Enemy, const FVector& Slot);
	// 飛び込みの放物線補間を進める。到達したらスタンバイへ移る (OnAttackTickから呼ぶ)
	void TickJump(AEnemyCharacter* Enemy, float DeltaTime);
	// 飛び込みで変更した移動モード/スーパーアーマーを元へ戻す (着地/中断で呼ぶ)
	void EndJump(AEnemyCharacter* Enemy);

	// 隊形中だけCMCによる向きの上書きを止める
	// 敵は既定でbUseControllerDesiredRotation=trueなので、
	// 放っておくと全員がPCの方を向いてしまい背中合わせや扇状の配置が崩れる
	// SetActorRotationで決めた向きを保たせるために外す
	void LockFacing(AEnemyCharacter* Enemy);
	void UnlockFacing(AEnemyCharacter* Enemy);
	// 火炎の開始→ループ→終了シーケンスを開始する (単体は即、隊形はスタンバイ明けに呼ぶ)
	void BeginFlameSequence(AEnemyCharacter* Enemy);

	// 火炎ハザードを生成しマズルソケットへアタッチする(この時点では非アクティブ)
	void SpawnFlame(AEnemyCharacter* Enemy);
	// 火炎ハザードを破棄する(中断・完了の後片付け)
	void DestroyFlame();

	UEnemyAnimInstance* GetEnemyAnimInstance() const;

	UFUNCTION()
	void OnStartMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted);
	UFUNCTION()
	void OnEndMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	enum class EFlamePhase : uint8 { None, Jumping, Standby, Starting, Looping, Ending, Recovering };
	EFlamePhase Phase = EFlamePhase::None;

	// 予約されて飛び込む協力者側か (OnAttackBeginで判定)
	bool bIsCoordinator = false;

	// リーダーとして隊形が成立したか。falseなら単体噴射として進む
	bool bFormationActive = false;

	// 飛び込みでスーパーアーマーを付けたか。中断経路でも確実に外すための記録
	bool bJumpSuperArmorApplied = false;

	// CMCの向き上書きを止めているか / 止める前の値。中断経路でも確実に戻すための記録
	bool bFacingLocked = false;
	bool bSavedUseControllerDesiredRotation = false;

	// 飛び込み補間の状態 (SetActorLocation駆動)
	// 開始位置/着地点(カプセル中心)/経過秒
	FVector JumpStart = FVector::ZeroVector;
	FVector JumpTarget = FVector::ZeroVector;
	float JumpElapsed = 0.0f;

	// スタンバイの計測。着地待ちと、全員そろってからの静止時間を別々に数える
	float StandbyJoinElapsed = 0.0f;
	float StandbyElapsed = 0.0f;
	bool bAllCoordinatorsJoined = false;

	// 自分の発射待ち秒数。負値 = 未確定 (協力者はリーダーが書き込むまで待つ)
	float FireDelayRemaining = -1.0f;

	// リーダーが発射順を配り終えたか
	// 配る前に中断されたら協力者を解放して待ちを解く必要があるが、
	// 配った後の解放は「まだ自分の番を待っている協力者」を暴発させるので行わない
	bool bFireOrderAssigned = false;

	// 後隙(RecoveryHold)の経過秒
	float RecoveryElapsed = 0.0f;

	// 噴射開始時に固定した火炎のワールドyaw (bLatchAimOnFire用)
	float LatchedFlameYaw = 0.0f;

	// リーダーが指名した協力者と、その担当スロット角。発射順の左右判定に使う
	TArray<TWeakObjectPtr<AEnemyCharacter>> RecruitedCoops;
	TArray<float> RecruitedSlotAngles;

	TWeakObjectPtr<AEnemyCharacter> CachedEnemy;
	TWeakObjectPtr<UAnimInstance> CachedAnimInstance;

	// 生成した火炎ハザード。ループ中のみ有効。OnAttackEndで破棄する
	UPROPERTY()
	TObjectPtr<AFlameStreamHazard> ActiveFlame = nullptr;

	// SpawnFlame時にキャッシュした本来の到達距離 (徐々に伸ばしきる先)
	float FlameFullRange = 0.0f;

	float LoopElapsed = 0.0f;
	float CurrentAimBlend = 0.0f;

};

//-----------------------------------------------------------------------------
//! 検証用攻撃I-Fukunaka
//-----------------------------------------------------------------------------
UCLASS(meta = (DisplayName = "重力弾攻撃"))
class PRJ_TIDE_P0_API UGravityShotAttackExecution : public UTideAttackExecution
{
	GENERATED_BODY()

public:

	// 開始モンタージュ(構え)。終了で発射を開始する
	UPROPERTY(EditAnywhere, Category = "Montage")
	TObjectPtr<UAnimMontage> StartMontage;
	// ループモンタージュ(発射中)。撃ちきるまでループする
	UPROPERTY(EditAnywhere, Category = "Montage")
	TObjectPtr<UAnimMontage> LoopMontage;
	// 終了モンタージュ(後隙)。撃ちきったら再生し、終了で攻撃完了
	UPROPERTY(EditAnywhere, Category = "Montage")
	TObjectPtr<UAnimMontage> EndMontage;

	// 開始モンタージュ終了から弾生成までの待機秒数。
	// この間、予兆はStart終了時点の位置で固定表示される
	UPROPERTY(EditAnywhere, Category = "Telegraph", meta = (ClampMin = "0.0"))
	float PostStartFireDelay = 0.1f;
	// 撃ちきった後、終了モンタージュへ移るまでの追加待機秒数
	UPROPERTY(EditAnywhere, Category = "Montage", meta = (ClampMin = "0.0"))
	float PostFireHold = 0.0f;

	// 発射する弾プロファイル
	UPROPERTY(EditAnywhere)
	TObjectPtr<UProjectileProfile> Profile;

	// 予兆デカールのマテリアル(進行度スカラーでフェード)。未設定なら予兆を出さない
	UPROPERTY(EditAnywhere, Category = "Telegraph")
	TObjectPtr<UMaterialInterface> TelegraphDecalMaterial = nullptr;
	// 予兆デカールのサイズ (X=地面への投影深さ, Y/Z=半径)
	UPROPERTY(EditAnywhere, Category = "Telegraph")
	FVector TelegraphDecalSize = FVector(200.0f, 300.0f, 300.0f);
	// フェード進行度のスカラーパラメータ名
	UPROPERTY(EditAnywhere, Category = "Telegraph")
	FName TelegraphDecalProgressParam = TEXT("Progress");

	// 予兆のフェードイン秒数。0以下ならStartMontageの長さを使う
	// StartMontageより長く設定すると、
	// 開始モンタージュ終了後もPostStartFireDelayの待機中フェードが続き、
	// この秒数で完了する(発射までにフェードが終わらなければ未完了のまま消える)
	UPROPERTY(EditAnywhere, Category = "Telegraph", meta = (ClampMin = "0.0"))
	float TelegraphFadeDuration = 0.0f;

	// プレイヤー足元の地面判定トレースの下方距離(cm)。プレイヤーのカプセル底からこの距離だけ下を見る
	UPROPERTY(EditAnywhere, Category = "Telegraph", meta = (ClampMin = "0.0"))
	float GroundTraceDistance = 2000.0f;

	// 見た目専用の弾道インジケーター弾のプロファイル。未設定なら出さない
	// (ダメージには使われない。コリジョンは生成後に無効化する)
	UPROPERTY(EditAnywhere, Category = "Telegraph")
	TObjectPtr<UProjectileProfile> IndicatorProjectileProfile = nullptr;

	// インジケーター弾の発射位置。敵の頭上へのオフセット(cm)
	UPROPERTY(EditAnywhere, Category = "Telegraph", meta = (EditCondition = "IndicatorProjectileProfile != nullptr"))
	float IndicatorLaunchHeight = 500.0f;

	// インジケーター弾の放物線の頂点の高さ(cm)。0で直線移動になる
	UPROPERTY(EditAnywhere, Category = "Telegraph", meta = (EditCondition = "IndicatorProjectileProfile != nullptr"))
	float IndicatorArcHeight = 300.0f;

	// asyncモード(自前で開始→ループ→終了を進める)
	virtual UAnimMontage* GetMontage() const override { return nullptr; }
	virtual void OnAttackBegin(AEnemyCharacter* Enemy) override;
	virtual void OnAttackTick(AEnemyCharacter* Enemy, float DeltaTime) override;
	virtual void OnAttackEnd(AEnemyCharacter* Enemy) override;

private:

	// LoopMontageの再生のみ行う(見た目の切り替え。BlendOut時点で呼ぶ)
	void PlayLoopMontageVisual();
	// 発射開始ロジック: Phase遷移 + 予兆位置の固定 + 撃ちきり関連カウンタのリセット
	// (Ended時点で呼ぶ)
	void BeginLoop();
	// 撃ちきり後: EndMontage再生(無ければ即完了)
	void BeginEnd();

	void BeginFireBurst(AEnemyCharacter* Enemy);
	void FireOne();

	// 指定位置に弾を1発生成する
	void SpawnProjectileAt(AEnemyCharacter* Enemy, const FVector& Location);

	// StartMontage開始と同時にプレイヤー足元へ予兆デカールを生成する
	void BeginTelegraph(AEnemyCharacter* Enemy);
	// Starting中、毎フレーム足元へ追従移動＋進行度(Progress)を更新する
	void UpdateTelegraph(AEnemyCharacter* Enemy, float DeltaTime);
	// フェード進行度(Progress)だけを更新する(位置は動かさない)
	// Starting/Looping両方から呼べる
	void UpdateTelegraphFade(float DeltaTime);
	// 予兆デカールの現在位置をFrozenTelegraphLocationへロックする
	// (フェード完了時点で呼ぶ)
	void FreezeTelegraphPosition();
	// 予兆デカールを破棄する(StartMontage終了・攻撃中断のいずれでも呼ぶ)
	void EndTelegraph();

	// プレイヤー足元から地形へラインドレースし、実際の地面の位置を求める
	// (ヒットしない場合、PreviousLocation指定時はそこに留まり、
	// 未指定〈初回〉なら敵の足元にフォールバックする。プレイヤーの位置は参照しない)
	FVector ComputeGroundLocationBelowPlayer(AEnemyCharacter* Enemy, ACharacter* Player, const FVector* PreviousLocation = nullptr) const;

	// BeginTelegraphと同時に、敵頭上から見た目専用のインジケーター弾を生成する(コリジョンは無効化する)
	void BeginIndicatorProjectile(AEnemyCharacter* Enemy);
	// UpdateTelegraph/UpdateTelegraphFadeと同じタイミングで、TelegraphElapsed/TelegraphDurationの
	// 進行度に応じて発射位置→TelegraphFollowLocation(予兆と共通の追従先)へ放物線移動させる
	void UpdateIndicatorProjectile();
	// EndTelegraphと同じタイミングでインジケーター弾を破棄する
	void EndIndicatorProjectile();

	// 開始→ループの繋ぎ、および撃ちきるまでのループ再トリガ
	UFUNCTION()
	void OnMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted);
	// 終了モンタージュ完了でFinishDelegate
	UFUNCTION()
	void OnMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	enum class EProjectilePhase : uint8 { None, Starting, Looping, Ending };
	EProjectilePhase Phase = EProjectilePhase::None;

	TWeakObjectPtr<AEnemyCharacter> CachedEnemy;
	TWeakObjectPtr<UAnimInstance> CachedAnimInstance;

	float FireLoopElapsed = 0.0f;
	float FireLoopDuration = 0.0f;

	TWeakObjectPtr<UDecalComponent> TelegraphDecal;
	TWeakObjectPtr<UMaterialInstanceDynamic> TelegraphDecalMID;
	float TelegraphElapsed = 0.0f;
	float TelegraphDuration = 0.0f;
	// 現在の追従先(プレイヤー足元)。デカール・インジケーター弾の両方が参照する
	FVector TelegraphFollowLocation = FVector::ZeroVector;

	// StartMontage終了時点でロックする弾の生成位置(予兆の最終位置)
	FVector FrozenTelegraphLocation = FVector::ZeroVector;
	// Looping開始からの経過時間。PostStartFireDelayに達すると発射開始
	float PostStartElapsed = 0.0f;
	// 発射(BeginFireBurst)を開始済みかどうか
	bool bHasFired = false;

	// 見た目専用のインジケーター弾と、その発射位置(敵頭上、生成時に1回だけ固定)
	TWeakObjectPtr<AEnemyProjectile> IndicatorProjectile;
	FVector IndicatorLaunchLocation = FVector::ZeroVector;

};

/**
 * 竜巻弾攻撃(開始→段階生成ループ→終了の3構成 / async)
 *
 * GetMontage() がnullptrのasyncモードで動作し、開始→段階生成ループ→終了を自前で進める。
 *   1. OnAttackBegin: StartMontageを再生すると同時に、最外周の段(Stage 0)の予兆を
 *      敵周囲のランダムな位置にStageProjectileCounts[0] 個生成する(フェード時間 = StartMontageの長さ)。
 *   2. StartMontage終了で、その段の予兆位置へ常駐弾(Profile)を生成し予兆を片付ける。
 *      残り段があれば1段内側の予兆を新たに生成しつつLoopMontageを再生する。
 *   3. LoopMontage再生完了のたびに2と同じ処理を繰り返す(外側→内側へStageProjectileCounts.Num() 段)。
 *   4. 最後の段の弾を生成し終えたらLoopMontageを再トリガせずEndMontageへ接続し、
 *      その終了でFinishDelegate。
 *
 * 各段の弾は、敵中心からその段の半径帯(OuterRadius〜InnerRadiusを段階数で等分し
 * 外側から順に内側へ)内でランダムな角度・ランダムな半径の位置に、StageProjectileCounts[段番号]個生成する。
 * 段階数(StageCount)はStageProjectileCountsの要素数で決まる。
 */
UCLASS(meta = (DisplayName = "竜巻弾攻撃"))
class PRJ_TIDE_P0_API UTornadoShotAttackExecution : public UTideAttackExecution
{
	GENERATED_BODY()

public:

	// 開始モンタージュ(構え)。終了で最初の段(最外周)の弾を生成する
	UPROPERTY(EditAnywhere, Category = "Montage")
	TObjectPtr<UAnimMontage> StartMontage;

	// ループモンタージュ。再生し終えるたびに1段ずつ内側の弾を生成する
	UPROPERTY(EditAnywhere, Category = "Montage")
	TObjectPtr<UAnimMontage> LoopMontage;

	// 終了モンタージュ(後隙)。全段生成し終えたら再生し、終了で攻撃完了
	UPROPERTY(EditAnywhere, Category = "Montage")
	TObjectPtr<UAnimMontage> EndMontage;

	// 生成する常駐弾のプロファイル
	// (ProjectileActorClassに
	// APersistentProjectile等を指定する想定)
	UPROPERTY(EditAnywhere, Category = "Tornado")
	TObjectPtr<UProjectileProfile> Profile;

	// 段ごとに生成する弾数(要素0=最外周〜末尾=最内周)
	// 配列のサイズが段階数(StageCount)になる
	UPROPERTY(EditAnywhere, Category = "Tornado", meta = (ClampMin = "0"))
	TArray<int32> StageProjectileCounts;

	// 最外周の半径
	UPROPERTY(EditAnywhere, Category = "Tornado", meta = (ClampMin = "0.0"))
	float OuterRadius = 800.0f;

	// 最内周の半径
	UPROPERTY(EditAnywhere, Category = "Tornado", meta = (ClampMin = "0.0"))
	float InnerRadius = 200.0f;

	// 常駐弾同士の最低限離す距離。全段を通して既に配置済みの弾との水平距離をチェックする
	UPROPERTY(EditAnywhere, Category = "Tornado", meta = (ClampMin = "0.0"))
	float MinProjectileSeparation = 100.0f;

	// 予兆デカールのマテリアル(進行度スカラーでフェード)。未設定なら予兆を出さない
	UPROPERTY(EditAnywhere, Category = "Telegraph")
	TObjectPtr<UMaterialInterface> TelegraphDecalMaterial = nullptr;

	// 予兆デカールのサイズ (X=地面への投影深さ, Y/Z=半径)
	UPROPERTY(EditAnywhere, Category = "Telegraph")
	FVector TelegraphDecalSize = FVector(200.0f, 150.0f, 150.0f);

	// フェード進行度のスカラーパラメータ名(各段の開始=0→生成=1)
	UPROPERTY(EditAnywhere, Category = "Telegraph")
	FName TelegraphDecalProgressParam = TEXT("Progress");

	// asyncモード(自前で開始→段階生成ループ→終了を進める)
	virtual UAnimMontage* GetMontage() const override { return nullptr; }
	virtual void OnAttackBegin(AEnemyCharacter* Enemy) override;
	virtual void OnAttackTick(AEnemyCharacter* Enemy, float DeltaTime) override;
	virtual void OnAttackEnd(AEnemyCharacter* Enemy) override;

private:

	// 現在の段の予兆をランダム位置に生成する(フェード時間は現在の段を統括するモンタージュの長さ)
	void BeginStageTelegraph(AEnemyCharacter* Enemy);
	// Starting/Looping中、毎フレーム現在の段の予兆のフェード進行度を更新する
	void UpdateStageTelegraphs(float DeltaTime);
	// 現在の段の予兆位置へ常駐弾を生成する
	void SpawnCurrentStageProjectiles(AEnemyCharacter* Enemy);
	// 現在の段の予兆デカールを破棄する
	void EndStageTelegraphs();

	// StartMontage/LoopMontageいずれかの再生完了で呼ぶ: 現在の段を生成し、
	// 次の段へ進める(残り段があればLoopMontageを再生して次段の予兆を出し、
	// 無ければBeginEndへ)
	void AdvanceStage(AEnemyCharacter* Enemy);
	// 終了: EndMontage再生(無ければ即完了)
	void BeginEnd();

	// 敵中心からHorizontalPointの水平位置へ地形をラインドレースし、実際の地面の位置を求める
	// (ヒットしなければ敵の現在地の高さにフォールバック)
	FVector ComputeGroundLocationAt(AEnemyCharacter* Enemy, const FVector& HorizontalPoint) const;

	// LoopMontageの見た目のみのシームレス再トリガ
	// (撃ちきり等のロジックはOnMontageEndedで行う)
	UFUNCTION()
	void OnMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted);
	// 各段の完了、および終了モンタージュ完了で呼ばれる
	UFUNCTION()
	void OnMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	enum class ETornadoPhase : uint8 { None, Starting, Looping, Ending };
	ETornadoPhase Phase = ETornadoPhase::None;

	TWeakObjectPtr<AEnemyCharacter> CachedEnemy;
	TWeakObjectPtr<UAnimInstance> CachedAnimInstance;

	int32 CurrentStageIndex = 0;

	struct FStageTelegraphEntry
	{
		FVector GroundLocation = FVector::ZeroVector;
		TWeakObjectPtr<UDecalComponent> Decal;
		TWeakObjectPtr<UMaterialInstanceDynamic> DecalMID;

	};
	TArray<FStageTelegraphEntry> CurrentStageTelegraphs;

	float StageTelegraphElapsed = 0.0f;
	float StageTelegraphDuration = 0.0f;

	// これまでに配置した弾の水平位置(全段累積)
	// MinProjectileSeparationの距離チェックに使う
	TArray<FVector> PlacedPositions;

};

USTRUCT()
struct FSpecialAttackProjectileEmitter
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere)
	TObjectPtr<UProjectileProfile> Profile;
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"))
	float Radius = 100.0f;
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"))
	float ArcDeg = 180.0f;
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0"))
	float HoldDuration = 1.0f;
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0"))
	int32 Count = 0;
	// 円の中心を、ターゲット方向基準の中心線から左右へどれだけずらすか。正=右、負=左
	UPROPERTY(EditAnywhere)
	float CenterSideOffset = 0.0f;
	// 円の中心の高さオフセット。正で上
	UPROPERTY(EditAnywhere)
	float CenterHeightOffset = 0.0f;

};

UCLASS(meta = (DisplayName = "特殊攻撃"))
class PRJ_TIDE_P0_API USpecialAttackExecution : public UTideAttackExecution
{
	GENERATED_BODY()

private:

	// asyncモード(自前で開始→ループ→終了を進める)
	virtual UAnimMontage* GetMontage() const override { return nullptr; }
	virtual void OnAttackBegin(AEnemyCharacter* Enemy) override;
	virtual void OnAttackTick(AEnemyCharacter* Enemy, float DeltaTime) override;
	virtual void OnAttackEnd(AEnemyCharacter* Enemy) override;

	UFUNCTION()
	void OnMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted);
	UFUNCTION()
	void OnMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	UEnemyAnimInstance* GetEnemyAnimInstance() const;

	void SpawnProjectiles();

public:

	// 開始モンタージュ(構え)。終了で展開＋発射を開始する
	UPROPERTY(EditAnywhere, Category = "Montage")
	TObjectPtr<UAnimMontage> PlayMontage;

	// 発射する弾プロファイル
	UPROPERTY(EditAnywhere, Category = "Profile")
	TArray<FSpecialAttackProjectileEmitter> Projectiles;
	UPROPERTY(EditAnywhere, Category = "Spawn")
	float FireInterval = 0.2f;
	UPROPERTY(EditAnywhere, Category = "Spawn")
	float SpawnForwardOffset = 100.0f;

private:

	TWeakObjectPtr<AEnemyCharacter> CachedEnemy;
	TWeakObjectPtr<UAnimInstance> CachedAnimInstance;
	FTimerHandle EmitTimerHandle;
	int32 EmitCount = 0;

};
