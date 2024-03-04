# 色ルーペ AviUtl プラグイン

色ピッカー付き拡大鏡プラグインです．

[ダウンロードはこちら．](https://github.com/sigma-axis/aviutl_color_loupe/releases) [紹介動画．](https://www.nicovideo.jp/watch/sm43213687)

![色や座標を表示してくれます．](https://github.com/sigma-axis/aviutl_color_loupe/assets/132639613/659ac33c-d5f5-461b-8400-713c4bf24d33)
![ダークモードっぽい色合いにもできます．](https://github.com/sigma-axis/aviutl_color_loupe/assets/132639613/d12a7cee-a2d3-4f76-9222-240b3a8011e5)


## 動作要件

- AviUtl 1.10 (1.00 でも動作するが 1.10 推奨)

  http://spring-fragrance.mints.ne.jp/aviutl

- Visual C++ 再頒布可能パッケージ（\[2015/2017/2019/2022\] の x86 対応版が必要）

  https://learn.microsoft.com/ja-jp/cpp/windows/latest-supported-vc-redist

## 導入方法

以下のフォルダのいずれかに `color_loupe.auf` と `color_loupe.ini` をコピーしてください．

1. `aviutl.exe` のあるフォルダ
1. (1) のフォルダにある `plugins` フォルダ
1. (2) のフォルダにある任意のフォルダ

## 使い方

### 基本操作

- 左クリックドラッグ

  ルーペ位置の移動．

  - Shift+ドラッグで上下左右の軸に沿って移動できます．（レールスクロール）

- ホイールスクロール

  拡大率の増減．

  - 拡大率の最大は 32 倍，最小は 0.1 倍．

- 右クリックドラッグ

  色・座標の情報表示．

  - Shift+ドラッグで上下左右の軸に沿って移動できます．（レールスクロール）

  ![色・座標の情報表示．](https://github.com/sigma-axis/aviutl_color_loupe/assets/132639613/659ac33c-d5f5-461b-8400-713c4bf24d33)

- AviUtlメインウィンドウの画面上でホイールクリックドラッグ

  ルーペ位置をメイン画面上の位置へ移動．

  - ホイールクリックだけでは反応しません．ホイールを押したままマウスを動かす必要があります．

- Ctrl+右クリック

  設定メニューの表示．

  - 各種設定ができます．


  ![設定メニュー．](https://github.com/sigma-axis/aviutl_color_loupe/assets/132639613/f592582a-fe21-4ffb-8c7c-0fd691fff38a)

- 拡大率変更など一部操作をすると通知メッセージが表示されます．

  ![拡大率変更通知．](https://github.com/sigma-axis/aviutl_color_loupe/assets/132639613/569760af-dfa8-4098-8c2e-68a1b8af099e)

### 特殊操作

以下の操作はデフォルトでの割り当てです．設定で変更可能です．

- 左ダブルクリック

  拡大率切り替え．

  - 拡大率がいったん等倍に変わります．もう一度ダブルクリックで元の倍率に戻ります．
  - 正確には裏に「もう1つの拡大率」の設定があって，それと入れ替えます．なので例えば「もう1つの拡大率」を 0.25 倍にしてサムネイル確認と切り替えるなどの使い方もできます．

- ホイールクリック

  カーソル追従モードの切り替え．

  - ONにすると，メイン画面でカーソルを動かすと（ホイールクリックがなくても）ルーペ位置が追従するようになります．
  - モードを変更すると通知メッセージが表示されます．

    ![カーソル追従モード変更通知．](https://github.com/sigma-axis/aviutl_color_loupe/assets/132639613/37b6cd6b-e761-4855-8cab-72c3d6c497d7)

- 右ダブルクリック

  カーソル位置のピクセルのカラーコードをクリップボードにコピー．

  - `rrggbb` の16進数6桁の形式でコピーします．

  - コピーした際，通知メッセージが表示されます．

     ![カラーコードコピー通知．](https://github.com/sigma-axis/aviutl_color_loupe/assets/132639613/f99cde99-4e2d-4e21-9642-a286ddce4d1d)

  - 蛇色様の[カラーコード追加プラグイン](https://github.com/hebiiro/AviUtl-Plugin-AddColorCode)との併用が便利になる目的で実装しました．

- （初期設定では割り当てなし）

  画像の中央へ移動．

  - ルーペ位置を初期化します．

  - 初期設定では割り当てがありませんが，Ctrl+右クリックのメニューから，左ダブルクリック・ホイールクリック・右ダブルクリックの操作に割り当てられます．

- （初期設定では割り当てなし）

  グリッド表示切り替え

  - グリッドの表示 / 非表示を切り替えます．
  - 小さい拡大率だと細い線のグリッド，大きい拡大率だと太い線のグリッドになります．拡大率が一定以下の場合は表示されません．

  - 初期設定では割り当てがありませんが，Ctrl+右クリックのメニューから，左ダブルクリック・ホイールクリック・右ダブルクリックの操作に割り当てられます．


### 設定メニュー

![設定メニュー．](https://github.com/sigma-axis/aviutl_color_loupe/assets/132639613/f592582a-fe21-4ffb-8c7c-0fd691fff38a)

Ctrl+右クリックで表示させられ，以下の操作ができます．

- メインウィンドウでカーソル追従

  カーソル追従モードの切り替え．

  - ONにすると，メイン画面でカーソルを動かすと（ホイールクリックがなくても）ルーペ位置が追従するようになります．

- 画像の中央へ移動

  ルーペ位置を初期化します．

- グリッド表示

  グリッドの表示 / 非表示を切り替えます．

- マウスホイールでの拡縮反転

  ホイールの回転方向を反転します．

- 拡大縮小の中心基準点

  拡大率が変化したときの位置計算の基準点を変更します．

  - ウィンドウ中央

    ウィンドウの中央を中心にして拡大縮小します．

  - マウス位置

    マウスカーソルの位置を中心にして拡大縮小します．

  また，通常状態のホイールによる拡大縮小・ドラッグ操作中のホイールによる拡大縮小・拡大率切り替え（初期設定で左ダブルクリック）による拡大縮小で，個別に設定ができます．

- ピクセル単位でドラッグ移動

  左クリックドラッグ時に，ドラッグ位置がピクセル単位にスナップされます．

- Shiftキーで移動方向固定

  左クリックでのレールスクロールの設定です．

  - しない

    レールスクロール機能を使用しません．

  - 十字

    上下左右に固定してのスクロールをします．

  - 8方向

    上下左右に加えて斜め45度の方向に固定します．

- 色・座標の情報表示

  右クリックドラッグ時に表示される情報の設定です．

  - 表示方式

    - 表示しない

      機能をOFFにします．

    - ホールド / 右クリックだけ表示

      マウス右ボタンを押してる間だけ表示して，話すと非表示になります．初期値です．

    - トグル / 右クリック中だけ移動

      マウス右ボタンを離しても表示が残ります．もう一度右クリックで消えます．情報表示のピクセル位置は右ボタンを押してる間だけ移動します．

    - トグル / 常にカーソル追従

      マウス右ボタンを離しても表示が残ります．もう一度右クリックで消えます．右ボタンを離しても情報表示のピクセル位置がマウスカーソルに追従します．

  - Shiftキーで移動方向固定

    右クリック情報表示でのレールスクロールの設定です．左クリックの設定と同様です．

- 通知メッセージ表示

  拡大率が変化したときなどの通知メッセージの設定です．

  - 拡大率の変更

    拡大率が変更されたときに，その拡大率を表示します．チェックを外すと非表示になります．

    ![拡大率変更通知．](https://github.com/sigma-axis/aviutl_color_loupe/assets/132639613/569760af-dfa8-4098-8c2e-68a1b8af099e)


  - カーソル追従の切り替え

    カーソル追従モードが変更されたときに通知を表示します．チェックを外すと非表示になります．

    ![カーソル追従モード変更通知．](https://github.com/sigma-axis/aviutl_color_loupe/assets/132639613/37b6cd6b-e761-4855-8cab-72c3d6c497d7)

  - カラーコードのコピー

    カラーコードをコピーしたときに通知を表示します．チェックを外すと非表示になります．

    ![カラーコードコピー通知．](https://github.com/sigma-axis/aviutl_color_loupe/assets/132639613/f99cde99-4e2d-4e21-9642-a286ddce4d1d)

  - 表示位置

    表示位置をウィンドウの4隅と中央の5か所から選べます．

  - 拡大率の形式

    拡大率が変更されたときの表示形式を分数・少数・百分率(%表示)の3種類から選べます．

- 左ダブルクリック時・右ダブルクリック時・ホイールクリック時

  これら3操作に割り当てる動作を設定します．

  - 何もしない

    操作を割り当てません．

  - カーソル追従の切り替え

    カーソル追従モードを切り替えます．ホイールクリックの初期値です．

  - 拡大率切り替え

    拡大率を「もう1つの拡大率設定」と切り替えます．左ダブルクリックの初期値です．

  - 画像の中央へ移動

    ルーペ位置を初期化します．

  - グリッド表示切り替え

    グリッドの表示 / 非表示を切り替えます．

  - カラーコードをコピー

    カーソル位置のカラーコードをコピーします．右ダブルクリックの初期値です．

  - このメニューを開く

    設定メニューを開きます．Ctrl+右クリック以外の操作でも開けるようになります．


## 設定ファイルについて

`color_loupe.ini` ファイルをテキストエディタで編集することでカスタマイズできます．Ctrl+右クリックの設定メニューでもある程度設定できますが，設定ファイルを直接編集しないと調整できない項目もあります．

設定ファイルはエンコード UTF8 で保存してください．設定の変更は次回起動時に反映されます．

以下の項目は直接ファイル編集する必要があります．

- 色設定

  `[color]` 以下に色・座標情報表示や通知メッセージの色設定が記述されています．プリセットとしてWindows標準のライトモードやダークモードを模した色設定を記述しています．

  - デフォルト設定．

    ![デフォルト設定．](https://github.com/sigma-axis/aviutl_color_loupe/assets/132639613/659ac33c-d5f5-461b-8400-713c4bf24d33)

  - ダークモードっぽい設定．

    ![ダークモードっぽい設定．](https://github.com/sigma-axis/aviutl_color_loupe/assets/132639613/d12a7cee-a2d3-4f76-9222-240b3a8011e5)

- グリッド設定

  `[grid]` 以下で細線グリッド，太線グリッドの表示条件を調整できます．

  - `least_scale_thin` でグリッドが表示される最小の拡大率レベルを指定します．
  - `least_scale_thick` でグリッドが太線になる最小の拡大率レベルを指定します．

  拡大率レベルと実際の拡大率との対応は以下の通りです．（一部）

  |レベル|$\dots$|`0`|`1`|`2`|`3`|`4`|`5`|`6`|`7`|`8`|`9`|`10`|`11`|`12`|`13`|`14`|`15`|`16`|$\dots$|
  |:-:|-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-:|:-|
  |倍率|$\dots$|1.00|1.25|1.50|1.75|2.0|2.5|3.0|3.5|4|5|6|7|8|10|12|14|16|$\dots$|

- 色・座標情報表示

  `[tip]` 以下の次の項目が設定できます．

  - フォント設定

    `font_name` や `font_size` でフォントやフォントサイズを指定できます．

  - ピクセルを囲う四角形の大きさ

    情報表示中のピクセルを囲う四角形は少し拡大して表示しています．`box_inflate` で何ピクセルだけ上下左右に膨らませて表示させるかを指定できます．

  - 背景枠設定

    `chrome_thick` で背景枠の線の太さを， `chrome_radius` で背景枠の丸角半径をそれぞれ指定できます．

- 通知メッセージ表示

  `[toast]` 以下の次の項目が設定できます．

  - メッセージ表示時間

    `duration` 表示されたメッセージが消えるまでの時間をミリ秒単位で指定できます．

  - フォント設定

    `font_name` や `font_size` でフォントやフォントサイズを指定できます．

  - 背景枠設定

    `chrome_thick` で背景枠の線の太さを， `chrome_radius` で背景枠の丸角半径をそれぞれ指定できます．

## 改版履歴

- **v1.13** (2024-03-04)

  - 編集のレジューム機能が有効でかつ，プロジェクト内で参照中のファイル見つからない場合，起動時に例外が発生していたのを修正．

  - ルーペウィンドウの IME 入力を無効化．IME が有効な状態だとショートカットキーが使えなかった問題に対処．

- **v1.12** (2024-02-28)

  - 初期化周りを少し改善．

  - 無駄な関数呼び出しを抑える処理が働いていなかったのを修正．

- **v1.11** (2024-01-14)

  - グリッド表示に必要な最小の拡大率の上限を緩めた．

    今までは最大拡大率にすると必ず太線グリッドになっていたが，細線グリッドのまま表示できるように．

  - リソースIDの隙間埋め．

- **v1.10** (2024-01-08)

  - グリッド機能追加．

  - コード整理．

  - README の語句修正．

  - ライセンス文の年表記を更新．

- **v1.01** (2023-12-27)

  - 編集データがない状態で画像データを要求していたのを修正．
  
  - 微修正でコードサイズ微減少．

- **v1.00** (2023-12-27)

  初版．

## ライセンス

このプログラムの利用・改変・再頒布等に関しては MIT ライセンスに従うものとします．

---

The MIT License (MIT)

Copyright (C) 2024 sigma-axis

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

https://mit-license.org/

#  Credits

##  aviutl_exedit_sdk

https://github.com/ePi5131/aviutl_exedit_sdk

---

1条項BSD

Copyright (c) 2022
ePi All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
THIS SOFTWARE IS PROVIDED BY ePi “AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL ePi BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#  連絡・バグ報告

- GitHub: https://github.com/sigma-axis
- Twitter: https://twitter.com/sigma_axis
- nicovideo: https://www.nicovideo.jp/user/51492481
- Misskey.io: https://misskey.io/@sigma_axis
