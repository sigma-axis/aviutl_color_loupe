# 色ルーペ AviUtl プラグイン

色ピッカー付き拡大鏡プラグインです．他にも拡大画面のまま拡張編集のオブジェクトをつかんでドラッグ移動できたりもします．

[ダウンロードはこちら．](https://github.com/sigma-axis/aviutl_color_loupe/releases) [紹介動画．](https://www.nicovideo.jp/watch/sm43213687)

![色や座標を表示してくれます．](https://github.com/sigma-axis/aviutl_color_loupe/assets/132639613/d2c57a35-ab0d-438b-8181-4fa3093f2128)
![ダークモードっぽい色合いにもできます．](https://github.com/sigma-axis/aviutl_color_loupe/assets/132639613/ee36c902-3544-4cc0-9a62-5766dd3592e5)

## 動作要件

- AviUtl 1.10 (1.00 でも動作するが 1.10 推奨)

  http://spring-fragrance.mints.ne.jp/aviutl

- 拡張編集 0.92 (「[拡張編集ドラッグ](#拡張編集ドラッグ)」の機能を利用する場合)

  - 0.93rc1 でも動作するはずだが未確認 / 非推奨．

- Visual C++ 再頒布可能パッケージ（\[2015/2017/2019/2022\] の x86 対応版が必要）

  https://learn.microsoft.com/ja-jp/cpp/windows/latest-supported-vc-redist

## 導入方法

以下のフォルダのいずれかに `color_loupe.auf` と `color_loupe.ini` をコピーしてください．

1. `aviutl.exe` のあるフォルダ
1. (1) のフォルダにある `plugins` フォルダ
1. (2) のフォルダにある任意のフォルダ

## 初期設定での操作

以下は初期状態での操作方法です．操作はマウスボタンの割り当てやキーの組み合わせなど[カスタマイズ](#操作のカスタマイズ)できます．

- **左クリックドラッグ**

  [ルーペ移動ドラッグ](#ルーペ移動ドラッグ)．

  - Shift+ドラッグで上下左右の軸に沿って移動できます．

- **ホイールスクロール**

  ズーム操作．

  - 拡大率を増減します．
  - 最大で 32 倍，最小で 0.1 倍です．[設定を変更](#ズーム操作の設定)すれば最大で 64 倍，最小で 0.05 倍まで範囲を広げられます．

- **右クリックドラッグ**

  [色・座標の情報表示](#色座標の情報表示)．

  - 現在マウスカーソルのある点のカラーコードと座標が表示されます．
  - 初期状態だとカラーコードは `#RRGGBB`の形式で座標は左上を原点とした値ですが，[設定](#ドラッグ操作の設定)で変更できます．
  - Shift+ドラッグで上下左右の軸に沿って移動できます．

  ![色・座標の情報表示](https://github.com/sigma-axis/aviutl_color_loupe/assets/132639613/d2c57a35-ab0d-438b-8181-4fa3093f2128)

- **Ctrl+左クリックドラッグ**

  [拡張編集ドラッグ](#拡張編集ドラッグ)．

  - 拡張編集で配置されたオブジェクトやアンカーをドラッグ移動します．メイン画面をドラッグしているような操作を，ルーペで拡大した状態でできます．
  - ドラッグ開始時に Ctrl キーを押していれば，そのあと離してもドラッグ操作を続けられます．
  - メイン画面でのドラッグと同様に Alt での拡大縮小や Shift での上下左右の方向固定もできます．

- **AviUtlメインウィンドウの画面上でホイールクリックドラッグ**

  ルーペ位置をメイン画面上の位置へ移動．

  - ホイールクリックだけでは反応しません．ホイールを押したままマウスを動かす必要があります．

- **Ctrl+右クリック / Shift+F10 キー**

  [設定メニュー](#設定メニュー)の表示．

  - 各種コマンド実行や設定ができます．

  ![設定メニュー](https://github.com/sigma-axis/aviutl_color_loupe/assets/132639613/ca96e1a6-c5aa-40ff-aa10-f5146c589869)

- **左ダブルクリック**

  ズーム切り替え．

  - 拡大率がいったん等倍に変わります．もう一度ダブルクリックで元の倍率に戻ります．
  - 正確には裏に「もう1つの拡大率」があって，それと入れ替えます．予め設定しておいた小さい拡大率と大きい拡大率を瞬時に切り替えて操作できます．

- **ホイールクリック**

  カーソル追従切り替え．

  - 「カーソル追従」を「ON」にすると，メイン画面でカーソルを動かすと（ホイールクリックがなくても）ルーペ位置が追従するようになります．
  - 「カーソル追従」が「ON」だと，ホイールクリックを押している状態に限りメイン画面のカーソル位置を追従します．
  - 初期状態だと「OFF」です．

- **右ダブルクリック**

  カラーコードをコピー．

  - カーソル位置のピクセルのカラーコードをクリップボードにコピー．
  - `rrggbb` の16進6桁の形式でコピーします．カラーコードの書式は[設定](#各種クリックコマンドの設定)で変更できます．
  - 蛇色様の[カラーコード追加プラグイン](https://github.com/hebiiro/AviUtl-Plugin-AddColorCode)との併用が便利になる目的で実装しました．

- 拡大率変更など一部操作をすると通知メッセージが表示されます．通知項目や表示位置，表示時間は[設定](#通知メッセージの設定)で変更できます．

  ![拡大率変更通知．](https://github.com/sigma-axis/aviutl_color_loupe/assets/132639613/80176dc8-7a0b-423c-80ff-5f01e68031dc) ![カラーコードのコピーの通知．](https://github.com/sigma-axis/aviutl_color_loupe/assets/132639613/ff33c524-a485-4f19-bf37-8ec0e06b726d) ![カーソル追従切り替えの通知．](https://github.com/sigma-axis/aviutl_color_loupe/assets/132639613/61d2a5e4-8fe0-4dbb-b1d3-2f10083f6cb6) ![グリッド表示の切り替え通知．](https://github.com/sigma-axis/aviutl_color_loupe/assets/132639613/d48b4f6c-a7f0-4c6c-83e5-454322d5045e)

## ドラッグ操作について

ドラッグ操作には4種類あります:
- [ルーペ移動ドラッグ](#ルーペ移動ドラッグ)
- [色・座標の情報表示](#色座標の情報表示)
- [拡大率ドラッグ](#拡大率ドラッグ)
- [拡張編集ドラッグ](#拡張編集ドラッグ)

### ルーペ移動ドラッグ

拡大表示する位置を移動できます．

[設定](#ドラッグ操作の設定)によってはピクセル単位にスナップしたり，Shift キーと組み合わせて上下左右や斜め 45° の直線上に沿って動かしたりすることもできます．

### 色・座標の情報表示

現在マウスカーソルがあるピクセルのカラーコードや座標を表示します．

![色・座標の情報表示](https://github.com/sigma-axis/aviutl_color_loupe/assets/132639613/d2c57a35-ab0d-438b-8181-4fa3093f2128)

[設定](#ドラッグ操作の設定)を変更すればカラーコードの書式を変更したり，表示される座標の原点を画像の左上から中央に変更したりもできます．

### 拡大率ドラッグ

マウスカーソルを上下に動かすことで拡大率を操作します．
- 初期状態でマウスボタンの割り当てはありません．使用するには[設定](#ドラッグ操作の設定)でボタンを割り当ててください．

[設定](#ドラッグ操作の設定)で倍率変化の速度や拡大縮小の中心を変更したり，ドラッグ方向を反転したりできます．

### 拡張編集ドラッグ

メイン画面をドラッグして拡張編集のオブジェクトを移動するのと同等の操作が，拡大したルーペ画面上で行えます．Shift キーで上下左右に方向を固定したり，Alt キーで拡大率を変化させたりなどのキー操作も可能です．

[設定](#ドラッグ操作の設定)によっては Shift キーや Alt キーの押下状態を，実際のキー入力とは違うものに上書きすることもできます．

## 操作のカスタマイズ

ポップアップメニュー (デフォルトだと Ctrl+右クリック，Shift+F10 でも可能) の「色ルーペの設定...」を選択するとダイアログが表示され，操作のカスタマイズなど各種設定ができます．

![「色ルーペの設定」ダイアログ](https://github.com/sigma-axis/aviutl_color_loupe/assets/132639613/ff963c78-4a0b-4688-b465-a43a3ec8b242)

設定項目の中には説明書きが添えられていることもあるので，そちらもご参照ください．

以下の動作を変更できます．

### ドラッグ操作の設定

[ルーペ移動ドラッグ](#ルーペ移動ドラッグ)，[色・座標の情報表示](#色座標の情報表示)，[拡張編集ドラッグ](#拡張編集ドラッグ)のタブで各種ドラッグ操作の設定ができます．

- ボタンの割り当てや修飾キーの組み合わせが変更できます．

  ![ボタンや修飾キーの設定](https://github.com/sigma-axis/aviutl_color_loupe/assets/132639613/824d6b28-b281-42cf-80e6-f1f1c227926a)

- 通常のクリック操作と判別するための条件を，移動距離とボタンを押す時間で指定することができます．

  ![クリック判定の基準](https://github.com/sigma-axis/aviutl_color_loupe/assets/132639613/f5e146ee-4788-4b99-8ef1-668c2b701943)

  - 「ピクセル距離」に `-1` を指定するとボタンを押した瞬間からドラッグ判定になります．同じボタンに割り当てた通常のクリックのコマンドがない場合は使いやすくなります．

- マウスホイールによるズーム操作を，選択タブのドラッグ操作中限定の挙動として設定変更できます．ここでの設定は通常のズーム操作の設定とは別枠で，独立に指定できます．

  ![ドラッグ中のホイールによるズーム操作の設定](https://github.com/sigma-axis/aviutl_color_loupe/assets/132639613/054f24dd-f6a4-45e0-af7e-c883c41da6e8)

- その他，各種ドラッグ操作に関する設定項目もあります．

  主要なものだけピックアップ:

  - **ピクセル単位で移動**

    [ルーペ移動ドラッグ](#ルーペ移動ドラッグ)時に，ピクセル単位にスナップされます．

  - **Shift キーで方向固定**

    Shift キーを押している間，ドラッグの開始地点から上下左右や斜め 45° の直線上に沿ってドラッグ移動できます．

  - **書式設定**

    [色・座標の情報表示](#色座標の情報表示)でのカラーコードや座標の表示方式を設定できます．

  - **フォントの設定**

    ![フォントの設定](https://github.com/sigma-axis/aviutl_color_loupe/assets/132639613/de423163-8e81-4f53-980e-79b041c975d3)

    [色・座標の情報表示](#ドラッグ操作の設定)に使われるフォント名やフォントサイズを指定できます．
    - **太字**，*斜体*，***太字斜体***も指定できます．
    - ここでの指定は[通知メッセージ](#通知メッセージの設定)とは別枠で，独立に指定できます．

  - **修飾キーの上書き**

    [拡張編集ドラッグ](#拡張編集ドラッグ)で，Shift キーによる方向固定や Alt キーによる拡大率操作でのキー認識を上書き・変更できます．

    ![修飾キーの上書き設定](https://github.com/sigma-axis/aviutl_color_loupe/assets/132639613/3ac91be4-e9aa-4996-9753-1c931e60ade2)

    - そのまま

      キー入力の認識に操作・介入しません．

    - ON 固定

      実際のキー入力に関わらず，Shift の方向固定や Alt の拡大率操作が常に有効になります．

    - OFF 固定

      実際のキー入力に関わらず，Shift の方向固定や Alt の拡大率操作が常に無効になります．

    - 反転

      実際のキー入力と ON/OFF が真逆になります．

### 各マウスボタンのクリック割り当て

各マウスボタンのシングルクリック，ダブルクリックに対してコマンドを割り当てることができます．

![クリックコマンドの割り当て](https://github.com/sigma-axis/aviutl_color_loupe/assets/132639613/8a7f1f0f-1d75-4146-8356-cfa3ff576206)

1.  以下のコマンドを割り当てられます:

    |コマンド|
    |---|
    |(何もしない)|
    |ズーム切り替え|
    |カラーコードをコピー|
    |座標をコピー|
    |カーソル追従切り替え|
    |編集画面の中央へ移動|
    |ルーペの中央へ移動|
    |グリッド表示切り替え|
    |ズームダウン|
    |ズームアップ|
    |設定メニューを表示|
    |このウィンドウを表示|

1.  「他ボタンのドラッグをキャンセルする」のチェックを外すと，他ボタンでドラッグ操作中にも指定のクリックコマンドが実行できるようになります．

1.  「各コマンドの説明」の枠からコマンドの内容を確認できます．

その他以下の点に留意してください．
- シングルクリックのコマンド指定時，同じマウスボタンにドラッグ操作が割り当てられている場合，[ドラッグ操作の設定](#ドラッグ操作の設定)の「クリック判定範囲」でシングルクリックとドラッグを判別する条件を設定してください．
- ダブルクリック時には，1回目のクリックでシングルクリックも発生します．両方にコマンドを割り当てる際には注意してください．
- 各コマンドの細かい設定変更は[クリックコマンドの設定](#各種クリックコマンドの設定)タブできます．

### 各種クリックコマンドの設定

クリックコマンドに対しての追加設定ができます．一部の設定は[ポップアップメニュー](#設定メニュー)でのコマンドにも影響します．

![クリックコマンドの動作設定](https://github.com/sigma-axis/aviutl_color_loupe/assets/132639613/773baafa-b748-40fd-968b-0eb0fb5b406c)

### ズーム操作の設定

マウスホイールによる拡大率の操作に関する設定ができます．

- 「マウスホイールで拡大縮小」枠

  有効/無効化，ホイールの方向を反転，拡大縮小の中心，ズームの段数（ホイール入力に対する拡大率変化のスピード）を設定できます．

  ![通常のホイールによるズーム操作の設定](https://github.com/sigma-axis/aviutl_color_loupe/assets/132639613/4f293d02-2e7d-4efb-8192-c3d750788507)

  - これはドラッグ操作中のホイールによるズーム操作の設定とは別枠で，独立に指定できます．

- 拡大率の範囲．

  拡大率の最大，最小値を設定できます．初期状態だと最小値は `0.10` 倍，最大値は `32` 倍ですが，広げたり使いやすい範囲に制限したりできます．

  ![拡大率の範囲の設定](https://github.com/sigma-axis/aviutl_color_loupe/assets/132639613/32184ebc-7df5-44df-b34a-7978a78da4d2)

  - こちらはドラッグ操作中のホイールによるズーム操作にも影響します．

### グリッドの設定

グリッドの表示される最小の拡大率を指定できます．

![グリッドの表示条件の設定](https://github.com/sigma-axis/aviutl_color_loupe/assets/132639613/e87c4ebe-f0d3-4f08-bd7e-fd8f82b06b32)

- グリッドには2種類あり，「細いグリッド」「太いグリッド」でそれぞれ設定できます．

  | 細いグリッド | 太いグリッド|
  |:---:|:---:|
  | ![細いグリッド](https://github.com/sigma-axis/aviutl_color_loupe/assets/132639613/9ad4fbaf-4fb0-4913-b6f4-227ec1e6d7b2) | ![太いグリッド](https://github.com/sigma-axis/aviutl_color_loupe/assets/132639613/0a106fb3-2729-4cc9-969f-dd8aeaae176f) |
  | 線幅 1ピクセル | 線幅 2ピクセル |
  | 全て同じ罫線 | 5, 10 の倍数ごとに目立つ罫線 |

### 通知メッセージの設定

拡大率が変化したり，クリップボードにテキストをコピーしたときなどに表示される通知メッセージの設定ができます．

![通知メッセージの設定](https://github.com/sigma-axis/aviutl_color_loupe/assets/132639613/84609f90-4d0d-4481-b603-9f987d1f2a4f)

- 拡大率の表示形式は「分数」「小数」「%表示」の3つから選びます．等倍以上と等倍未満の2通りで個別に指定します．

  表示例:

  | 分数 | 小数 | %表示 |
  |---:|---:|---:|
  | `x 2` | `x 2.00` | `200%` |
  | `x 3/2` | `x 1.50` | `150%` |
  | `x 1/2` | `x 0.50` | `50%` |

- ここでのフォント指定は[色・座標の情報表示](#ドラッグ操作の設定)とは別枠で，独立に指定できます．

### 色の設定

![色の設定](https://github.com/sigma-axis/aviutl_color_loupe/assets/132639613/79db70aa-2ca1-4fed-8fd2-c320b1968380)

色・座標の情報表示や通知メッセージに表示される枠の色や，色ルーペウィンドウの背景の色を設定できます．

- 「プリセット...」を選ぶと「ライトモード」「ダークモード」の2種類から既定の色設定に切り替えられます．

## 設定メニュー

Ctrl+右クリック（デフォルト設定の場合）や Shift+F10 で表示されます．

![設定メニュー](https://github.com/sigma-axis/aviutl_color_loupe/assets/132639613/ca96e1a6-c5aa-40ff-aa10-f5146c589869)

以下のコマンドや設定変更が利用できます．

- **この点のカラーコードをコピー**

  ポップアップメニューを開く際にクリックした点のカラーコードをクリップボードにコピーします．
  - クリックコマンドの「カラーコードをコピー」と同機能です．
  - Shift+F10 で表示させた場合は利用できません．

- **この点の座標をコピー**

  ポップアップメニューを開く際にクリックした点の座標をクリップボードにコピーします．
  - クリックコマンドの「座標をコピー」と同機能です．
  - Shift+F10 で表示させた場合は利用できません．

- **この点のをルーペ中央へ移動**

  ポップアップメニューを開く際にクリックした点がルーペウィンドウの中央になるようにルーペ位置を移動します．
  - クリックコマンドの「ルーペの中央へ移動」と同機能です．
  - Shift+F10 で表示させた場合は利用できません．

- **メインウィンドウでカーソル追従**

  カーソル追従モードを切り替えます．メイン画面をカーソル移動した際，ホイールクリックがなくてもルーペ位置が移動するようにしたり，移動しないようにできます．
  - クリックコマンドの「カーソル追従切り替え」と同機能です．

- **グリッドの表示**

  グリッドの表示/非表示の状態を切り替えます．拡大率が一定のしきい値以上でないと表示されません．
  - クリックコマンドの「グリッド表示切り替え」と同機能です．

- **ズーム切り替え**

  "裏にあるもう1つの拡大率" と現在の拡大率を入れ替えます．大きい拡大率と小さい拡大率を瞬時に切り替えて操作できます．
  - クリックコマンドの「ズーム切り替え」と同機能です．

- **編集画面の中央へ移動**

  ルーペの位置を初期位置にリセットします．
  - クリックコマンドの「編集画面の中央へ移動」と同機能です．

- **色・座標表示のモード**

  ドラッグ操作の[色・座標の情報表示](#色座標の情報表示)のモードを「ホールド」「トグル / 固定」「トグル / 追従」から指定します．
  - 「色ルーペの設定」ダイアログでも[設定変更](#ドラッグ操作の設定)できます．詳しい動作の説明はそちらに記述があります．

- **マウスホイールでの拡縮反転**

  マウスホイールによる拡大縮小操作のホイール方向を反転します．
  - 通常のホイール操作に加えて，各種ドラッグ中のホイール操作での個別設定も反転します．

- **色ルーペの設定...**

  「色ルーペの設定」ダイアログを表示します．
  - クリックコマンドの「このウィンドウを表示」と同機能です．

## 設定ファイルについて

カスタマイズした設定や現在のルーペの拡大率などの情報は `color_loupe.ini` に保存されています．

全ての設定項目は[設定メニュー](#設定メニュー)（デフォルトだと Ctrl+右クリック）から開ける[色ルーペの設定](#操作のカスタマイズ)で設定できますが，必要な場合は直接編集して変更することもできます．ただしその場合，AviUtl を終了した状態で編集を行い，エンコード方式を UTF8 にして保存してください．設定の変更は次回起動時に反映されます．

## TIPS

- 各ドラッグ操作は ESC キーや他のマウスボタンでキャンセルできます．
  - [ルーペ移動ドラッグ](#ルーペ移動ドラッグ)をキャンセルした場合，ルーペ位置と拡大率がドラッグ前まで戻ります．
  - [色・座標の情報表示](#色座標の情報表示)をキャンセルした場合，表示モードの選択によらず非表示になります．
  - [拡大率ドラッグ](#拡大率ドラッグ)をキャンセルした場合，拡大率がドラッグ前まで戻ります．
  - [拡張編集ドラッグ](#拡張編集ドラッグ)をキャンセルした場合，「マウスカーソルを元の位置に戻してボタンを離す」という操作をシミュレートします．
    
    **注意**: 拡張編集ドラッグに関しては，ドラッグ中に Ctrl キーや Alt キーを押したり離したりした場合，キャンセルしても元の状態に戻るとは限りません．この場合，AviUtl のコマンドの「元に戻す」を利用してください．あくまでも単純なドラッグ操作に対する手短なキャンセル手段として利用してください．

- [色・座標の情報表示](#色座標の情報表示)やクリップボードにコピーされる座標は厳密に言うなら，注目ピクセルの左上の位置を表す座標です．

  例えばシーンのサイズが $3\times 3$ ピクセルだったとして中央のピクセルを表示させた場合，左上を原点とする基準だと `X: 1, Y: 1`, 中央を原点とした場合 `X: -0.5, Y: -0.5` と表示されます．
  - この場合注目ピクセルの中央の座標は左上原点で $(1.5, 1.5)$, 中央原点で $(0, 0)$ となっています．

- ポップアップメニューはデフォルト設定での Ctrl+右クリックの他にも Shift+F10 でも表示できます．

  うっかり設定を変更してポップアップメニューを表示させる手段を消してしまった際の，設定をやり直す手段として用意しています．この Shift+F10 のキーは変更できません．

- 各種クリック操作のコマンドに直接は修飾キーの条件は設定できません．デフォルト設定のポップアップメニュー表示の「Ctrl+右クリック」は，次の仕組みで実現しています:
  1. [色・座標の情報表示](#色座標の情報表示)は「Ctrl を押していない右ボタンドラッグ」として割り当てられている．
  1. Ctrl を押していない右クリックは，ドラッグ操作の[色・座標の情報表示](#色座標の情報表示)が優先するのでクリック操作と認識されない．
  1. Ctrl を押しているときは[色・座標の情報表示](#色座標の情報表示)の条件に当てはまらないので，クリック操作と認識される．
  1. 右クリックにはポップアップメニューの表示が割り当てられていて，それが実行される．

- 各種ドラッグ操作のボタン割り当てによっては，複数のドラッグ操作に当てはまる場合があります（例: 「ルーペ移動ドラッグ」と「拡張編集ドラッグ」に左ボタンを，修飾キー条件を指定せず割り当てるなど）．この場合，次の優先順位でドラッグ操作が決定されます:
  > ルーペ移動ドラッグ > 色・座標の情報表示 > 拡大率ドラッグ > 拡張編集ドラッグ

  この優先順位は hard-coded で変更不可能です．

## 改版履歴

- **v2.22** (2024-04-27)

  - フォント選択のコンボボックスに直接フォント名を入力できるように変更．
    
    半角空白に続いて後ろに `bold`, `italic`, `bold italic` と付け足すことで **太字**, *斜体*, ***太字斜体*** が使えます．

- **v2.21** (2024-04-21)

  - バージョン表記が間違っていたのを修正．

- **v2.20** (2024-04-21)

  - 「拡大率ドラッグ」の機能を追加．ドラッグ操作で拡大率を操作できるように．

  - 通知メッセージで表示される拡大率の形式を，等倍以上と縮小した場合とで個別に指定できるように．

- **v2.10** (2024-04-20)

  - 全ての設定項目に対して GUI を用意．テキストエディタで `.ini` ファイルを編集しなくてもダイアログ上でフルにカスタマイズできます．

  - 設定ファイル内の項目のうち `chrome_radius` を `chrome_corner` に名前変更 (2箇所)．**この項目を変更していた場合は再設定が必要になります．**
    - 描画関数への指定が，丸角の円の半径 (radius) ではなく直径だったことに気付いていなかったのが原因．名前からの推測による齟齬を避けるため改名．
    - 更新後は `chrome_radius` の項目を削除しても問題ありません．

  - 各マウスボタンに「他ボタンのドラッグをキャンセルする」オプションを追加(初期値は ON). 以前までは必ずドラッグをキャンセルしていたのを，キャンセルせずにドラッグしたままクリックコマンドを実行できるように．

  - クリックコマンド「ルーペの中央へ移動」を追加．クリックしたピクセルをルーペ中央へ持ってくるようにルーペを移動．

  - 色・座標の情報表示や通知メッセージの丸角矩形の描画を改善．
    - 高 DPI の設定で「システム(拡張)」を選んでいたときには特に綺麗に表示されるように．
    - グラデーションでない単色指定の場合は軽量な描画方法を選ぶように変更．

  - 色・座標の情報表示や通知メッセージの位置やサイズを微調整できる設定項目を増やした．

  - 拡大率の最大，最小値を広げた．ただしデフォルトだと以前の範囲に制限されているので，必要なら設定を変更して範囲を拡大してください．

  - いくつかの設定が正常に保存・復元されていなかったのを修正．

  - 色・座標情報の表示や通知メッセージで日本語フォントが正しく表示されなかったのを修正．

  - ルーペ移動ドラッグ中に拡大率を変化させると，直後の移動が不連続になっていたのを修正．

- **v2.00** (2024-04-17)

  **v1.20 の設定ファイルとは互換性がなくなりました．更新した際には以前の設定ファイルが正しく読み込まれない可能性があるため `.auf` ファイル，`.ini` ファイルの両方を上書き更新してください．以前の設定を参照したい場合は `.ini` ファイルのバックアップを取って，新しいバージョンで再設定をお願いします．**

  **拡張編集のオブジェクトをドラッグする機能のデフォルト設定を，v1.20 の「Alt+左ドラッグ」から「Ctrl+左ドラッグ」に変更しました．「Ctrl の方が都合が良さそう」との考えで変更しましたが，これは設定で v1.20 の挙動に変更できます．**

  - 設定変更の GUI 追加．設定メニューを刷新．

  - 各種ドラッグ操作のマウスボタンや修飾キーを設定変更できるように．

  - ドラッグ操作やクリック操作に，左，右，中，X1, X2 の5つのマウスボタンを利用できるように．クリック操作も全ボタンでシングルクリック，ダブルクリックの両方に各々コマンドを割り当てられるように．

  - 新しいクリックコマンドを追加:
    - 「座標をコピー」: クリックした点の座標を `123,456` の形式でコピーします．
    - 「ズームアップ」「ズームダウン」: 拡大率を指定した段階だけ上げ下げします．

  - カラーコードの表示に `RGB(  8, 64,255)` の形式も選べるように．クリップボードへのコピーでも形式の選択ができます．

  - 座標の表示に画面中央からの相対座標が選べるように．クリップボードへのコピーでも形式の選択ができます．

  - ホイールスクロールでの拡大縮小で，1回ホイール入力するごとに変化する段階数を変更できるように．

  - 拡大縮小の最大，最小倍率を変更できるように．

  - 一部クリックコマンドの名前を変更．

  - ファイルを開いていない状態でも通知メッセージが表示されるように．

  - 通知メッセージの配置を左や上にしていた場合の余白が消えていたのを修正．

- **v1.20** (2024-04-01)

  - Alt+左クリックドラッグで，拡張編集のオブジェクトやアンカーのドラッグ操作をルーペ画面でできる機能を追加．

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
- Bluesky: https://bsky.app/profile/sigma-axis.bsky.social
