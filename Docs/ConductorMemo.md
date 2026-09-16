# Conductor メモ

保留中の検討事項。

## 拡張ポイント

### Module の指定方法
- 現状は `TSubclassOf<UContentConductorModule>` / `TSubclassOf<UMapConductorModule>` (Row の `Modules`)
- パラメータ違いはコンテンツごとに BP サブクラスを作る運用 (例: `BP_CCModule_Shine_A`)
- C++ の汎用 Module を設定値だけ変えて使い回したくなったら、`Instanced` + `EditInlineNew` のインスタンス指定に変える
- ただし Module は DataTable の行 (`FContentConductorRow`) にあるので、Instanced UObject は載らない (行エディタが構造体スコープのため)
- その場合は StateTree のタスク側に寄せるか、Module だけ別アセットに出すかの判断が先に必要
- 変えるとアセット形式が変わるので、使用箇所が増える前に判断したい

### アクター状態制御の拡張 (AI 停止など)
- `UContentConductor::ApplyState` は HiddenInGame / Collision / Tick の3つだけを操作していて、アプリ側から拡張できない
- 「AI を止めたい」のような要望が出たら、`IConductorActor` のようなインターフェースで状態変化を通知し、アクター側で反応できるようにする
- 後から足しても互換性は壊れない

## UContentConductor の分割
- `ContentConductor.cpp` が約485行で一番大きい
- 中身は「起動/停止/Tick」「StateTree の器」「データ検証」「アクター管理」の4つ

### アクター管理を切り出す (第一候補)
- 対象: `EnsureActorsScanned` / `ScanPlacedActors` / `VerifyPlacedActors` / `ApplyActorsForPhase` / `ApplyState` / `ResolvePlacedActor` / `EnsureSpawned` / `DestroySpawned` / `DestroyAllSpawned` / `FindManagedActor` / `GetGroupActors` (約200行)
- 使うデータが `ActorTable` / `PlacedActors` / `SpawnedActors` で閉じていて、Conductor との接点は「初回フェーズ適用時の走査」「フェーズ適用」「停止時の全破棄」「検索」だけ
- 形は `UConductorObjectBase` 派生の UObject サブオブジェクト (`NewObject(this)`) にする
  - `GetWorld()` が Outer 経由で取れる / `ActorTable` を `UPROPERTY` で持てる / 画面ログの WorldContext にそのまま使える
  - USTRUCT でもできるが、WorldContext を別途渡す必要がある
- `ContentId` は初期化時に受け取る (Outer から `UContentConductor` を辿ると相互依存になる)
- BP 公開の `FindManagedActor` / `GetGroupActors` は `UContentConductor` に転送関数として残し、既存 BP を壊さない
- 呼び出し順 (アクター適用 → Module 通知) は進行役の責務なので Conductor 側に残す
- Module にはしない (Module はアプリ側が付け外しする拡張部品で、必須機能の置き場ではない)
- `UContentConductor` は実行時生成のみで保存されないため、アセットへの影響・リダイレクトは不要
- 上の「アクター状態制御の拡張」や、下の「デバッグ表示」の getter の置き場もこのクラスになる
- クラス名は未定 (案: `UConductorActorController`)

### データ検証を切り出す (第二候補)
- `ValidateData` (約50行) と `CollectTreePhases` (約20行) は、下の「エディタモジュール」をやるタイミングで一緒に分ける
- フェーズグラフの検証は StateTree に移ったので、残っているのは「アクター表 × ツリーのフェーズ名」の突き合わせが中心
- 「メッセージ (重要度 + 文言) の一覧を返す関数」にして、実行時の画面ログとエディタの `IsDataValid` の両方から使う
- 今だけ別ファイルに移しても出力先が画面ログのままなので得るものが少ない

### 分割後
- 両方分けると `UContentConductor` は約200行で、ライフサイクル・StateTree の器・Module の管理が残る

## デバッグ表示 (ImGui)
- ImGui への依存はアプリ側 (`ConductorWindow`) に置き、プラグインは ImGui に依存させない
- StateTree 化で表示すべき中身が変わった。以前の目玉だった「各遷移の条件と現在値」は
  `UConductorCondition::GetDebugText()` に依存していたが、Condition ごと削除された
- 代わりに見るのは StateTree の実行状態
  - `FStateTreeExecutionContext::GetActiveStateNames()` (public) でアクティブなステートパスが取れる
  - 遷移条件の中身まで見せるのは StateTree 側のデバッガ (StateTreeDebugger) の責務と割り切るのが現実的
- Conductor 側で見せたいのは、StateTree が持たない情報
  - 現フェーズ (`GetCurrentPhase()` は public)、配置アクターの解決数、生成物の一覧
  - 強制遷移は `SendStateTreeEvent(FGameplayTag)` 経由。ツリー側にイベント遷移を置いておく必要がある
- `PlacedActors` / `SpawnedActors` は private なので、`#if !UE_BUILD_SHIPPING` で囲んだ読み取り専用 getter をプラグイン側に用意する

## エディタモジュール
- 現状、データ検証 (`UContentConductor::ValidateData`) は実行時にしか走らない
- 必要になったら `ConductorEditor` モジュールを追加し、`IsDataValid` やエディタのバリデータで保存時に検証する

## Category 指定の漏れ
- プラグインの `UFUNCTION` / `UPROPERTY` に `Category = "Conductor"` が1つも書かれていない (UFUNCTION 15件、うち Callable/Pure 10件。指定子付き UPROPERTY 16件)
- 無いと BP のアクションメニューや詳細パネルでクラス名などのデフォルトカテゴリに散らばり、プラグインの機能として探しにくい
- プラグインをエンジン側 (`Engine/Plugins` など) に置くと、UHT が「Category 指定が必須」としてエラーにする
  - 対象は `BlueprintCallable` / `BlueprintPure` の関数と、`EditAnywhere` / `BlueprintReadOnly` 等のプロパティ
  - `BlueprintNativeEvent` だけの関数は対象外 (UE 5.8 の `UhtFunction.cs` / `UhtProperty.cs` で確認)
- `BlueprintNativeEvent` への Category の効果
  - 戻り値なしのもの (`OnStart` / `OnTick` / `OnEnterPhase` / `OnExitPhase` など) は、グラフ右クリックの「Add Event」以下に `Add Event > Conductor > ...` と分類される
  - 戻り値ありのものはイベントにならず、My Blueprint の「Override」一覧 (名前順のフラット表示) から実装するので効果なし
  - 必須ではないが、揃えるために付けておく
- `BlueprintCallable` / `BlueprintPure` の関数と、エディタや BP に公開するプロパティには全部付ける
- 必要なら `Category = "Conductor|Phase"` のようにサブカテゴリで分ける
- 指定子なしの `UPROPERTY()` (GC 参照用) には不要

## 移行の後始末
- `Config/DefaultEngine.ini` の `[CoreRedirects]` は、旧 `/Script/ConductorTest.*` を参照するアセットを読むためのもの
- `Content/Conductor/` 以下のアセットをエディタで全部リセーブしたら、リダイレクトは削除してよい
- ただし `ConductorCondition` / `ConductorPhaseAction` / `ContentConductorPhaseSet` の3件は別扱い
  - StateTree 化でクラス自体が存在しないため、リダイレクト先が無い
  - リセーブを待たずに削除してよい

## StateTree タスクがコンテキストを経由していない
- `UConductorStateTreeSchema` は `ContextName_Conductor` を宣言し、`SetContextRequirements` で `UContentConductor` を差している
- ところが `FConductorTask_ApplyPhase` / `FConductorTask_Log` は `Cast<UContentConductor>(Context.GetOwner())` で取っている
- 動くのは実行コンテキストのオーナーが Conductor 自身だから。オーナーを変える改修が入ると静かに壊れる
- 宣言したコンテキストは BP 側のバインド (ツリー上で Conductor のメンバを参照する) に必要なので消さないこと
- C++ タスクも `FStateTreeExternalDataHandle` 経由に揃えると、オーナーへの依存が切れる

## FConductorStTask_OpenLevel でステートが固まる
- `Data.Level.IsNull()` のとき `EStateTreeRunStatus::Running` を返してそのままステートに留まる
- 遷移条件を置いていないとそこで進まなくなり、原因が分かりにくい
- 警告を出すか `EStateTreeRunStatus::Failed` を返すほうが安全

## StateTree まわりの命名とファイル配置
- プラグイン側 `ConductorStateTreeTasks` / アプリ側 `ConductorStTasks` で略し方が違う
- アプリ側が `Source/ConductorTest/Conductor/StateTree/` にあり、プラグイン化で消した `Conductor/` を作り直している形になっている
- アプリ固有のタスク置き場として `Source/ConductorTest/StateTree/` などに寄せると、プラグインとの境界が読みやすい

## 階層ステートと CurrentPhase (要検討)
- 親子のステート両方に「フェーズを適用」タスクを置くと、子から兄弟への遷移で子の ExitState だけが走る
- その結果 `CurrentPhase` が `NAME_None` になる (親はまだ生きているのに)
- アクター状態は差分適用なので親の適用は残るが、`GetCurrentPhase()` を読む側 (ImGui デバッグ窓・モジュール・外部コード) は「フェーズが無い」と見える
- 単一の `CurrentPhase` は階層と本質的に噛み合わない
- 当面は「フェーズを適用は1階層にしか置かない」という運用で縛る。スタックで持つかは必要になってから
