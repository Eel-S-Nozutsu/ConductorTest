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

## デバッグ表示 (ImGui)
- ImGui への依存はアプリ側 (`ConductorWindow`) に置き、プラグインは ImGui に依存させない
- ただし `UContentConductor` の `PhaseConditions` / `StartCondition` / `SpawnedActors` などが private のため、今のままではアプリ側から `GetDebugText()` を表示できない
- `#if !UE_BUILD_SHIPPING` で囲んだ読み取り専用 getter をプラグイン側に用意する

## エディタモジュール
- 現状、データ検証 (`UContentConductor::ValidateData`) は実行時にしか走らない
- 必要になったら `ConductorEditor` モジュールを追加し、`IsDataValid` やエディタのバリデータで保存時に検証する

## 移行の後始末
- `Config/DefaultEngine.ini` の `[CoreRedirects]` は、旧 `/Script/ConductorTest.*` を参照するアセットを読むためのもの
- `Content/Conductor/` 以下のアセットをエディタで全部リセーブしたら、リダイレクトは削除してよい
