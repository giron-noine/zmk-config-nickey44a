# Nickey44A-PAD: 左右TPS43

> この文書は左右にTPS43を搭載する案の記録。現在のファームウェアは右側のみTPS43を有効にしており、左側には未実装のため設定していない。左側の起動時に存在しないTPS43を待たないよう、左側のI2C・入力分割設定も無効化した。

右central／左peripheral。左右とも以下の配線・アドレスを使用する。

| 信号・設定 | 左手のTPS43 | 右手のTPS43 |
|---|---|---|
| SDA | P0.09 | P0.09 |
| SCL | P0.10 | P0.10 |
| RDY | P1.10 | P1.10 |
| NRST | P0.16 | P0.16 |
| I2Cアドレス | `0x74` | `0x74` |

各MCUに独立したI2Cバスがあるため、アドレスは同じでよい。
westのrevision、ZMK本体、Azoteqドライバーは変更しない。

## ジェスチャの割り当て

現在の設定上の割り当て。左手・右手は、それぞれのTPS43を指す。
右の1本指カーソル方向は実機確認済み。新しい2本指スクロール方向は未確認。

| ジェスチャ・操作 | 左手のTPS43 | 右手のTPS43 |
|---|---|---|
| 1本指で移動 | - | カーソル移動 |
| 1本指でタップ | クリックしない | クリックしない |
| 1本指で長押し・保持 | クリックしない | クリックしない |
| 2本指でタップ | クリックしない | クリックしない |
| 2本指で上下に動かす | 水平スクロール | Macの自然なスクロール：指を上へ動かすと内容も上へ、下へ動かすと下へ |
| 2本指で左右に動かす | 左スワイプ：左のタブ／右スワイプ：右のタブ | Macの自然なスクロール：指を左へ動かすと内容も左へ、右へ動かすと右へ |
| 縦スクロール後に指を離す | 慣性処理なし | 慣性スクロール |
| Layer 1で1本指移動 | - | 通常レイヤーと同じ速度 |
| Layer 1で2本指左右スワイプ | 左スワイプ：左のタブ／右スワイプ：右のタブ | 通常レイヤーと同じ水平スクロール |

ドラッグは、キーマップの左クリックキーを押しながら右TPS43でカーソルを動かす。
左TPS43の指の動きはカーソル座標に影響しない。

左手のタブ移動は、左スワイプで`Ctrl+Shift+Tab`、右スワイプで`Ctrl+Tab`を
送信する。ブラウザにフォーカスがある状態で使用する。1回のタッチにつき
最大1回切り替え、再度切り替えるには指を離してからスワイプする。

右手はレイヤーに関係なく2本指の上下・左右をスクロールとして送信する。
ブラウザの進む／戻るへの変換は行わない。Mac側では自然なスクロールを有効にする。

## イベント経路

```text
左TPS43 → 左input-splitの標準processors → BLE split (reg=0)
        → 右input-split proxy (reg=0) → 左専用input-listenerのbutton filter・tab swipe → 共通HID
右TPS43 → 右input-listenerのbutton filter・cursor/scroll orientation・inertia → 共通HID
```

左のprocessor順序は次の通り。1〜4はBLE転送前、5は右central側で行う。

| 順序 | Processor | 結果 |
|---|---|---|
| 1 | `zip_xy_scaler 0 1` | REL_X/Yを0化 |
| 2 | `left_swipe_axis_mapper` | 元のREL_HWHEELをREL_DIALへ変換し、左右スワイプ用に保持 |
| 3 | `left_wheel_to_hwheel` | REL_WHEELをREL_HWHEELへ変換、値と符号は保持 |
| 4 | `left_middle_click_mapper` | BTN_1をBTN_2へ変換、DOWN/UPは保持 |
| 5 | `left_tps43_tab_swipe` | REL_DIALからタブ移動を判定し、この軸のスクロール出力を抑止。REL_HWHEELは水平スクロールとして通過 |

左TPS43の`invert-scroll-y`でWHEELの符号を反転してからHWHEELへ変換する。
上下スワイプによる水平スクロール方向は、初期のデュアルTPS43設定と逆になる。
左右スワイプは`invert-scroll-x`で符号を反転する。判定閾値は8、
左右方向の累積量が上下方向の総移動量の2倍以上の場合にタブを切り替える。
実機では右スワイプが負のREL_DIALに対応するため、processorの
`left-keycode`に`Ctrl+Tab`、`right-keycode`に`Ctrl+Shift+Tab`を割り当てる。

左右ともsingle-tap、two-finger-tap、press-and-holdを設定上無効にし、
scrollは有効。固定TPS43ドライバーはセンサーからタップ通知が来ると
設定フラグを再確認せずクリックを送るため、central側の
`zip_button_behaviors`でBTN_0/BTN_1/BTN_2を`&none`へ送って止める。
左のBTN_1→BTN_2変換は残るが、変換後のBTN_2も止める。
標準scalerはイベントを破棄せず値を0にするため、X/YのBLE通知件数は
減らない。左右スワイプの値とtouch、sync情報もそのまま転送される。

右の1本指カーソルは、Y軸反転により指の方向と一致することを実機確認した。
右の2本指スクロールは縦横の交換を外し、ドライバー側の`invert-scroll-y`と
listener側のX軸反転でMacの自然なスクロールに合わせる。
左は上下操作をドライバーの
WHEELとして扱うため、右のコントローラー設定からswitch-xyを省き、
invert-xは維持する。左に追加のorientation processorはない。
同じ物理向きの実装を前提とした設定であり、左の取り付け向きと実機の
上下・左右判定は実機で確認する。

右TPS43のボタンイベントはfilterで止め、縦横スクロール、
inertia、orientation、感度、電源管理、RST/RDYを維持する。
低速時の動きに使う右TPS43の`filter-dynamic-bottom`は`3`。

## 自動確認

左右のFWを通常のbuild.yaml設定でビルドしてから実行する。
Zephyr付属のpython-devicetreeを使い、生成されたDevicetreeと.configを検証する。

```powershell
python scripts/check_dual_tps43.py `
  --zephyr-base <Zephyrのパス> `
  --left-build <左buildディレクトリ> `
  --right-build <右buildディレクトリ>
```

確認対象はピン、RSTのActive HIGH、GPIO/pinctrl競合、gesture設定、
split reg/device/listener経路、central/peripheral、バッテリー・sleep・Studioの
有効設定、processor順序。生成されたprocessorの設定値からイベント変換を
モデル化し、負値を含むX/Y抑止、スワイプ軸と水平スクロール軸の分離、
タブ移動のキー設定、WHEEL変換、BTN_1変換、BTN_0とsync保持、
タップ・長押しクリックの無効設定と、両listenerのボタン抑止を確認する。

この検証は実際のC処理・BLE通信・TPS43のgesture判定・HIDタイミングを
実行するものではない。実機動作の合格を意味しない。

2026-10-06の確認結果：左右TPS43ドライバーとinput-splitを含むFWビルド成功。
左の標準scaler/code mapperもコンパイル成功。上記自動確認はすべてPASS。
Devicetree/bindingエラーなし。非推奨設定、peripheralのZMK_USB依存、
右ZMKのcombo/event_manager配列境界についてビルド警告あり。

2026-10-07の確認結果：左手のタブ移動を追加した左右FWのビルド成功。
スワイプ軸と水平スクロール軸の分離、タブ移動のキー設定を含む自動確認は
すべてPASS。実機でのスワイプ方向とブラウザの動作確認は未実施。

2026-10-07時点ではLayer 1の右手タブ移動追加後も、右FWのビルドと
自動確認はPASS。通常時の進む／戻るとLayer 1のタブ移動のキー設定を検証済み。
その後、動作確認用にLayer 1のタブ移動とSnipeを設定から外した。
この変更後のFWビルドと実機動作は別途確認する。

## 実機確認（継続中）

左右それぞれ新しいUF2を書き込み、通常のBLE split接続後に確認する。
既存のペアリング情報はまず維持する。settings_resetは通常の更新では不要。

ジェスチャの動作は冒頭の割り当て表と照合する。加えて、次の点を確認する。

- 左右とも1本指・2本指のタップと長押しではクリックが発生しない。
  キーマップのクリックキーを押しながら右手でドラッグできる。
- 左手の右スワイプで右のタブ、左スワイプで左のタブへ移動し、1回のタッチで
  複数回切り替わらない。上下操作の水平スクロールでタブが切り替わらない。
- 右手の2本指を上下左右へ動かし、各方向へ内容がスクロールする。
  左右ではブラウザの進む／戻るが発火しない。Layer 1でも同じ方向になる。
- 左右のキー入力と両側のバッテリー取得が従来通りに動作する。
- 左右同時操作を繰り返しても、クラッシュ・ボタン固着・入力欠落がない。
- 両側のDeep Sleep後にキーで復帰すると、splitが再接続し、両TPS43とキーを操作できる。

必要ならホストのHIDイベント表示で、左右TPS43のタップ・長押しによる
BTN_0/BTN_1/BTN_2 DOWN/UPがないこと、水平wheel、左由来の非ゼロX/Yが
ないことを確認する。キーマップのクリックキーは別に確認する。

Deep Sleepでは起動時の既存RST処理を利用する。新たなsleep hookはない。
