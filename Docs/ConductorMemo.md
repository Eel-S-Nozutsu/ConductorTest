# Conductor メモ

保留中の検討事項。

## 拡張ポイント

### Module の指定方法
- 現状は `TSubclassOf<UContentConductorModule>` / `TSubclassOf<UMapConductorModule>` (Row の `Modules`)
- パラメータ違いはコンテンツごとに BP サブクラスを作る運用 (例: `BP_CCModule_Shine_A`)
- C++ の汎用 Module を設定値だけ変えて使い回したくなったら、Condition / Action と同じく `Instanced` + `EditInlineNew` のインスタンス指定に変える
- 変えるとアセット形式が変わるので、使用箇所が増える前に判断したい

### PhaseAction は同期実行のみ
- `DuplicateObject` した Action をどこにも保持していないため、`Execute` 後に GC で回収されうる
- 非同期 Action (フェード待ち、シーケンス再生など) を作る場合は、フェーズ終了まで保持して終了時に片付ける仕組みが必要
- 同期のみである旨はヘッダ (`ConductorPhaseAction.h`) にも記載済み

### Condition と Action の Conductor 受け渡しが不揃い
- Condition: `BeginEvaluation` で保持し、`GetContentConductor()` で取得
- Action: `Execute(UContentConductor*)` の引数で受け取る
- 継承する側から見て揃っていないので、どちらかに寄せたい

### アクター状態制御の拡張 (AI 停止など)
- `UContentConductor::ApplyState` は HiddenInGame / Collision / Tick の3つだけを操作していて、アプリ側から拡張できない
- 「AI を止めたい」のような要望が出たら、`IConductorActor` のようなインターフェースで状態変化を通知し、アクター側で反応できるようにする
- 後から足しても互換性は壊れない

## UContentConductor の分割
- `ContentConductor.cpp` が約630行で一番大きい
- 中身は「起動/停止/Tick」「フェーズ進行」「データ検証」「アクター管理」の4つ

### アクター管理を切り出す (第一候補)
- 対象: `ScanPlacedActors` / `VerifyPlacedActors` / `ApplyActorsForPhase` / `ApplyState` / `ResolvePlacedActor` / `EnsureSpawned` / `DestroySpawned` / `DestroyAllSpawned` / `FindManagedActor` / `GetGroupActors` (約200行)
- 使うデータが `ActorTable` / `PlacedActors` / `SpawnedActors` で閉じていて、Conductor との接点は「開始時の走査」「フェーズ適用」「停止時の全破棄」「検索」だけ
- 形は `UConductorObjectBase` 派生の UObject サブオブジェクト (`NewObject(this)`) にする
  - `GetWorld()` が Outer 経由で取れる / `ActorTable` を `UPROPERTY` で持てる / 画面ログの WorldContext にそのまま使える
  - USTRUCT でもできるが、WorldContext を別途渡す必要がある
- `ContentId` は初期化時に受け取る (Outer から `UContentConductor` を辿ると相互依存になる)
- BP 公開の `FindManagedActor` / `GetGroupActors` は `UContentConductor` に転送関数として残し、既存 BP を壊さない
- 呼び出し順 (アクター適用 → EntryActions → Module 通知 → 条件構築) は進行役の責務なので Conductor 側に残す
- Module にはしない (Module はアプリ側が付け外しする拡張部品で、必須機能の置き場ではない)
- `UContentConductor` は実行時生成のみで保存されないため、アセットへの影響・リダイレクトは不要
- 上の「アクター状態制御の拡張」や、下の「デバッグ表示」の getter の置き場もこのクラスになる
- クラス名は未定 (案: `UConductorActorController`)

### データ検証を切り出す (第二候補)
- `ValidateData` (約120行) は、下の「エディタモジュール」をやるタイミングで一緒に分ける
- 「メッセージ (重要度 + 文言) の一覧を返す関数」にして、実行時の画面ログとエディタの `IsDataValid` の両方から使う
- 今だけ別ファイルに移しても出力先が画面ログのままなので得るものが少ない

### 分割後
- 両方分けると `UContentConductor` は約300行で、ライフサイクル・フェーズ進行・Module の管理が残る

## デバッグ表示 (ImGui)
- ImGui への依存はアプリ側 (`ConductorWindow`) に置き、プラグインは ImGui に依存させない
- ただし `UContentConductor` の `PhaseConditions` / `StartCondition` / `SpawnedActors` などが private のため、今のままではアプリ側から `GetDebugText()` を表示できない
- `#if !UE_BUILD_SHIPPING` で囲んだ読み取り専用 getter をプラグイン側に用意する

## エディタモジュール
- 現状、データ検証 (`UContentConductor::ValidateData`) は実行時にしか走らない
- 必要になったら `ConductorEditor` モジュールを追加し、`IsDataValid` やエディタのバリデータで保存時に検証する

## Category 指定の漏れ
- プラグインの `UFUNCTION` / `UPROPERTY` に `Category = "Conductor"` が1つも書かれていない (UFUNCTION 21件、指定子付き UPROPERTY 23件)
- 無いと BP のアクションメニューや詳細パネルでクラス名などのデフォルトカテゴリに散らばり、プラグインの機能として探しにくい
- プラグインをエンジン側 (`Engine/Plugins` など) に置くと、UHT が「Category 指定が必須」としてエラーにする
  - 対象は `BlueprintCallable` / `BlueprintPure` の関数と、`EditAnywhere` / `BlueprintReadOnly` 等のプロパティ
  - `BlueprintNativeEvent` だけの関数は対象外 (UE 5.8 の `UhtFunction.cs` / `UhtProperty.cs` で確認)
- `BlueprintNativeEvent` への Category の効果
  - 戻り値なしのもの (`OnStart` / `OnTick` / `OnEnterPhase` / `Execute` など) は、グラフ右クリックの「Add Event」以下に `Add Event > Conductor > ...` と分類される
  - 戻り値ありのもの (`Evaluate`) はイベントにならず、My Blueprint の「Override」一覧 (名前順のフラット表示) から実装するので効果なし
  - 必須ではないが、揃えるために付けておく
- `BlueprintCallable` / `BlueprintPure` の関数と、エディタや BP に公開するプロパティには全部付ける
- 必要なら `Category = "Conductor|Phase"` のようにサブカテゴリで分ける
- 指定子なしの `UPROPERTY()` (GC 参照用) には不要

## 移行の後始末
- `Config/DefaultEngine.ini` の `[CoreRedirects]` は、旧 `/Script/ConductorTest.*` を参照するアセットを読むためのもの
- `Content/Conductor/` 以下のアセットをエディタで全部リセーブしたら、リダイレクトは削除してよい
