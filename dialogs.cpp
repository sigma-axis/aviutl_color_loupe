﻿/*
The MIT License (MIT)

Copyright (c) 2024 sigma-axis

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <algorithm>
#include <map>
#include <memory>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <CommCtrl.h>

#include "resource.h"
extern HMODULE this_dll;

#include "dialogs_basics.hpp"
#include "settings.hpp"

using namespace dialogs::basics;

////////////////////////////////
// Commonly used patterns.
////////////////////////////////
static void init_combo_items(HWND combo, auto curr, const auto& items) {
	constexpr auto add_item = [](HWND combo, const auto& item) {
		WPARAM idx = ::SendMessageW(combo, CB_ADDSTRING, {}, reinterpret_cast<LPARAM>(item.desc));
		::SendMessageW(combo, CB_SETITEMDATA, idx, static_cast<LPARAM>(item.data));
		return idx;
	};

	for (const auto& item : items) {
		auto idx = add_item(combo, item);
		if (item.data == curr)
			::SendMessageW(combo, CB_SETCURSEL, idx, {});
	}
}

template<class data>
static data get_combo_data(HWND combo) {
	return static_cast<data>(
		::SendMessageW(combo, CB_GETITEMDATA,
			::SendMessageW(combo, CB_GETCURSEL, {}, {}), {}));
}

static void init_list_items(HWND list, auto curr, const auto& items) {
	constexpr auto add_item = [](HWND list, const auto& item) {
		WPARAM idx = ::SendMessageW(list, LB_ADDSTRING, {}, reinterpret_cast<LPARAM>(item.desc));
		::SendMessageW(list, LB_SETITEMDATA, idx, static_cast<LPARAM>(item.data));
		return idx;
	};

	for (const auto& item : items) {
		auto idx = add_item(list, item);
		if (item.data == curr)
			::SendMessageW(list, LB_SETCURSEL, idx, {});
	}
}

template<class data>
static data get_list_data(HWND list) {
	return static_cast<data>(
		::SendMessageW(list, LB_GETITEMDATA,
			::SendMessageW(list, LB_GETCURSEL, {}, {}), {}));
}

static void init_spin(HWND spin, int curr, int min, int max) {
	::SendMessageW(spin, UDM_SETRANGE32, static_cast<WPARAM>(min), static_cast<LPARAM>(max));
	::SendMessageW(spin, UDM_SETPOS32, {}, static_cast<LPARAM>(curr));
}

static int get_spin_value(HWND spin) {
	return static_cast<int>(::SendMessageW(spin, UDM_GETPOS32, {}, {}));
}

static void set_slider_value(HWND slider, int curr) {
	::SendMessageW(slider, TBM_SETPOS, TRUE, static_cast<LPARAM>(curr));
}
static void init_slider(HWND slider, int curr, int min, int max) {
	::SendMessageW(slider, TBM_SETRANGEMIN, FALSE, static_cast<LPARAM>(min));
	::SendMessageW(slider, TBM_SETRANGEMAX, FALSE, static_cast<LPARAM>(max));
	set_slider_value(slider, curr);
}
static void init_slider(HWND slider, int curr, int min, int max, size_t delta, size_t page) {
	::SendMessageW(slider, TBM_SETLINESIZE, FALSE, static_cast<LPARAM>(delta));
	::SendMessageW(slider, TBM_SETPAGESIZE, FALSE, static_cast<LPARAM>(page));
	init_slider(slider, curr, min, max);
}

static int get_slider_value(HWND slider) {
	return static_cast<int>(::SendMessageW(slider, TBM_GETPOS, 0, 0));
}


////////////////////////////////
// クリック操作選択．
////////////////////////////////
class click_action : public dialog_base {
	using Button = Settings::ClickActions::Button;
	using Command = Settings::ClickActions::Command;

public:
	Button& btn;
	click_action(Button& btn)
		: btn{ btn } {}

private:
	void on_change_single(Command data_new) { btn.click = data_new; }
	void on_change_double(Command data_new) { btn.dblclk = data_new; }

protected:
	uintptr_t template_id() const override { return IDD_SETTINGS_FORM_CLICK_ACTION; }

	bool on_init(HWND) override
	{
		constexpr struct {
			const wchar_t* desc; // TODO: move these strings to resource.
			Command data;
		} combo_data[] = {
			{ L"(何もしない)",		Command::none					},
			{ L"拡大率切り替え",		Command::swap_scale_level		},
			{ L"カラーコードをコピー",	Command::copy_color_code		},
			{ L"カーソル追従切り替え",	Command::toggle_follow_cursor	},
			{ L"編集画面の中央へ移動",	Command::centralize				},
			{ L"グリッド表示切替",	Command::toggle_grid			},
			{ L"拡大率を上げる",		Command::scale_step_up			},
			{ L"拡大率を下げる",		Command::scale_step_down		},
			{ L"メニューを表示",		Command::context_menu			},
			{ L"このウィンドウを表示",	Command::settings				},
		};

		// suppress notifications from controls.
		auto sc = suppress_callback();
		for (int id : { IDC_COMBO1, IDC_COMBO2 }) {
			HWND combo = ::GetDlgItem(hwnd, id);
			Command curr;
			switch (id) {
			case IDC_COMBO1:
				curr = btn.click; break;
			case IDC_COMBO2:
				curr = btn.dblclk; break;
			default: std::unreachable();
			}

			init_combo_items(combo, curr, combo_data);
		}

		return false;
	}

	bool handler(UINT message, WPARAM wparam, LPARAM lparam) override
	{
		auto ctrl = reinterpret_cast<HWND>(lparam);
		switch (message) {
		case WM_COMMAND:
			switch (auto id = 0xffff & wparam, code = wparam >> 16; code) {
			case CBN_SELCHANGE:
				switch (id) {
				case IDC_COMBO1:
					on_change_single(get_combo_data<Command>(ctrl));
					return true;
				case IDC_COMBO2:
					on_change_double(get_combo_data<Command>(ctrl));
					return true;
				}
				break;
			}
			break;
		}
		return false;
	}
};


////////////////////////////////
// ドラッグのキー設定．
////////////////////////////////
class drag_keys : public dialog_base {
	using Button = Settings::MouseButton;
	using KeysActivate = Settings::KeysActivate;
	using KeyCond = KeysActivate::State;

public:
	KeysActivate& keys;
	drag_keys(KeysActivate& keys) : keys{ keys } {}

private:
	void on_change_button(Button data_new) { keys.button = data_new; }
	void on_change_ctrl(KeyCond data_new) { keys.ctrl = data_new; }
	void on_change_shift(KeyCond data_new) { keys.shift = data_new; }
	void on_change_alt(KeyCond data_new) { keys.alt = data_new; }

protected:
	uintptr_t template_id() const override { return IDD_SETTINGS_FORM_DRAG_KEYS; }

	bool on_init(HWND) override
	{
		constexpr struct {
			const wchar_t* desc; // TODO: move these strings to resource.
			Button data;
		} combo_data1[] = {
			{ L"(なし)",		Button::none	},
			{ L"左ボタン",	Button::left	},
			{ L"右ボタン",	Button::right	},
			{ L"中央ボタン",	Button::middle	},
			{ L"X1 ボタン",	Button::x1		},
			{ L"X2 ボタン",	Button::x2		},
		};
		constexpr struct {
			const wchar_t* desc; // TODO: move these strings to resource.
			KeyCond data;
		} combo_data2[] = {
			{ L"指定なし",	KeyCond::dontcare	},
			{ L"ON",		KeyCond::on			},
			{ L"OFF",		KeyCond::off		},
		};
		constexpr auto desc_data = // TODO: move this string to resource.
			L"Ctrl, Shift, Alt の修飾キーの条件が全て成立している場合，"
			L"指定したマウスのボタンでドラッグを開始できます．\r\n"
			L"ドラッグの最中なら修飾キーを押したり離したりしてもドラッグは継続します．";

		// suppress notifications from controls.
		auto sc = suppress_callback();
		init_combo_items(::GetDlgItem(hwnd, IDC_COMBO1), keys.button, combo_data1);
		for (int id : { IDC_COMBO2, IDC_COMBO3, IDC_COMBO4 }) {
			HWND combo = ::GetDlgItem(hwnd, id);
			KeyCond curr;
			switch (id) {
			case IDC_COMBO2:
				curr = keys.ctrl; break;
			case IDC_COMBO3:
				curr = keys.shift; break;
			case IDC_COMBO4:
				curr = keys.alt; break;
			default: std::unreachable();
			}

			init_combo_items(combo, curr, combo_data2);
		}

		::SendMessageW(::GetDlgItem(hwnd, IDC_EDIT1), WM_SETTEXT,
			{}, reinterpret_cast<LPARAM>(desc_data));

		return false;
	}

	bool handler(UINT message, WPARAM wparam, LPARAM lparam) override
	{
		auto ctrl = reinterpret_cast<HWND>(lparam);
		switch (message) {
		case WM_COMMAND:
			switch (auto id = 0xffff & wparam, code = wparam >> 16; code) {
			case CBN_SELCHANGE:
				switch (id) {
				case IDC_COMBO1:
					on_change_button(get_combo_data<Button>(ctrl));
					return true;
				case IDC_COMBO2:
					on_change_ctrl(get_combo_data<KeyCond>(ctrl));
					return true;
				case IDC_COMBO3:
					on_change_shift(get_combo_data<KeyCond>(ctrl));
					return true;
				case IDC_COMBO4:
					on_change_alt(get_combo_data<KeyCond>(ctrl));
					return true;
				}
				break;
			}
			break;
		}
		return false;
	}
};


////////////////////////////////
// ドラッグの有効範囲．
////////////////////////////////
class drag_range : public dialog_base {
	using DragInvalidRange = Settings::DragInvalidRange;

public:
	DragInvalidRange& range;
	drag_range(DragInvalidRange& range) : range{ range } {}

private:
	void on_change_distance(int16_t data_new) {
		range.distance = std::clamp(data_new, DragInvalidRange::distance_min, DragInvalidRange::distance_max);
	}
	void on_change_timespan(int16_t data_new) {
		range.timespan = std::clamp(data_new, DragInvalidRange::timespan_min, DragInvalidRange::timespan_max);
	}

protected:
	uintptr_t template_id() const override { return IDD_SETTINGS_FORM_DRAG_RANGE; }

	bool on_init(HWND) override
	{
		constexpr struct {
			int id, min, max;
		} spin_data[] = {
			{ IDC_SPIN1, DragInvalidRange::distance_min, DragInvalidRange::distance_max },
			{ IDC_SPIN2, DragInvalidRange::timespan_min, DragInvalidRange::timespan_max },
		};
		constexpr auto desc_data = // TODO: move this string to resource.
			L"ボタンを押してから離すまでが指定の距離・時間以内の場合，"
			L"ドラッグではなくクリックの判定となり，クリックに割り当てられたコマンドを実行します．\r\n"
			L"クリック判定せずボタンを押した瞬間から即ドラッグしたい場合，"
			L"ピクセル距離に -1 を指定してください．";

		// suppress notifications from controls.
		auto sc = suppress_callback();
		for (auto& item : spin_data) {
			int curr;
			switch (item.id) {
			case IDC_SPIN1:
				curr = range.distance; break;
			case IDC_SPIN2:
				curr = range.timespan; break;
			default: std::unreachable();
			}

			init_spin(::GetDlgItem(hwnd, item.id), curr, item.min, item.max);
		}

		::SendMessageW(::GetDlgItem(hwnd, IDC_EDIT3), WM_SETTEXT,
			{}, reinterpret_cast<LPARAM>(desc_data));

		return false;
	}

	bool handler(UINT message, WPARAM wparam, LPARAM lparam) override
	{
		switch (message) {
		case WM_COMMAND:
			switch (auto id = 0xffff & wparam, code = wparam >> 16; code) {
			case EN_CHANGE:
				switch (id) {
				case IDC_EDIT1:
					on_change_distance(get_spin_value(::GetDlgItem(hwnd, IDC_SPIN1)));
					return true;
				case IDC_EDIT2:
					on_change_timespan(get_spin_value(::GetDlgItem(hwnd, IDC_SPIN2)));
					return true;
				}
				break;
			}
			break;
		}
		return false;
	}
};


////////////////////////////////
// ホイールの拡大縮小操作．
////////////////////////////////
class wheel_zoom : public dialog_base {
	using WheelZoom = Settings::WheelZoom;

public:
	WheelZoom& wheel;
	wheel_zoom(WheelZoom& wheel) : wheel{ wheel } {}

private:
	void on_change_enabled(bool data_new) { wheel.enabled = data_new; }
	void on_change_reverse(bool data_new) { wheel.reversed = data_new; }
	void on_change_steps(uint8_t data_new) {
		wheel.num_steps = std::clamp(data_new, WheelZoom::num_steps_min, WheelZoom::num_steps_max);
	}
	void on_change_pivot(WheelZoom::Pivot data_new) { wheel.pivot = data_new; }

protected:
	uintptr_t template_id() const override { return IDD_SETTINGS_FORM_WHEEL_ZOOM; }

	bool on_init(HWND) override
	{
		constexpr struct {
			const wchar_t* desc; // TODO: move these strings to resource.
			WheelZoom::Pivot data;
		} combo_data[] = {
			{ L"ウィンドウ中央",	WheelZoom::center	},
			{ L"カーソル位置",	WheelZoom::cursor	},
		};
		constexpr auto desc_data = // TODO: move this string to resource.
			L"ホイール入力1回で拡大率を何段階変化させるかを指定します．"
			L"4段階増える(減る)ごとに拡大率は2倍(半分)になります．";

		// suppress notifications from controls.
		auto sc = suppress_callback();
		::SendMessageW(::GetDlgItem(hwnd, IDC_CHECK1), BM_SETCHECK,
			wheel.enabled ? BST_CHECKED : BST_UNCHECKED, {});
		::SendMessageW(::GetDlgItem(hwnd, IDC_CHECK2), BM_SETCHECK,
			wheel.reversed ? BST_CHECKED : BST_UNCHECKED, {});
		init_combo_items(::GetDlgItem(hwnd, IDC_COMBO1), wheel.pivot, combo_data);
		init_spin(::GetDlgItem(hwnd, IDC_SPIN1), wheel.num_steps, WheelZoom::num_steps_min, WheelZoom::num_steps_max);
		::SendMessageW(::GetDlgItem(hwnd, IDC_EDIT2), WM_SETTEXT,
			{}, reinterpret_cast<LPARAM>(desc_data));

		return false;
	}

	bool handler(UINT message, WPARAM wparam, LPARAM lparam) override
	{
		auto ctrl = reinterpret_cast<HWND>(lparam);
		switch (message) {
		case WM_COMMAND:
			switch (auto id = 0xffff & wparam, code = wparam >> 16; code) {
			case BN_CLICKED:
			{
				bool checked = ::SendMessageW(ctrl, BM_GETCHECK, {}, {}) == BST_CHECKED;
				switch (id) {
				case IDC_CHECK1:
					on_change_enabled(checked);
					return true;
				case IDC_CHECK2:
					on_change_reverse(checked);
					return true;
				}
				break;
			}
			case CBN_SELCHANGE:
				switch (id) {
				case IDC_COMBO1:
					on_change_pivot(get_combo_data<WheelZoom::Pivot>(ctrl));
					return true;
				}
				break;
			case EN_CHANGE:
				switch (id) {
				case IDC_EDIT1:
					on_change_steps(get_spin_value(::GetDlgItem(hwnd, IDC_SPIN1)));
					return true;
				}
				break;
			}
			break;
		}
		return false;
	}
};


////////////////////////////////
// ルーペ一位置ドラッグ固有．
////////////////////////////////
class loupe_drag : public dialog_base {
	using RailMode = Settings::RailMode;

public:
	Settings::LoupeDrag& drag;
	loupe_drag(Settings::LoupeDrag& drag) : drag{ drag } {}

private:
	void on_change_lattice(bool data_new) { drag.lattice = data_new; }
	void on_change_rail(RailMode data_new) { drag.rail_mode = data_new; }

protected:
	uintptr_t template_id() const override { return IDD_SETTINGS_FORM_DRAG_LOUPE; }

	bool on_init(HWND) override
	{
		constexpr struct {
			const wchar_t* desc; // TODO: move these strings to resource.
			RailMode data;
		} combo_data[] = {
			{ L"しない",	RailMode::none		},
			{ L"十字",	RailMode::cross		},
			{ L"8方向",	RailMode::octagonal	},
		};

		// suppress notifications from controls.
		auto sc = suppress_callback();
		::SendMessageW(::GetDlgItem(hwnd, IDC_CHECK1), BM_SETCHECK,
			drag.lattice ? BST_CHECKED : BST_UNCHECKED, {});
		init_combo_items(::GetDlgItem(hwnd, IDC_COMBO1), drag.rail_mode, combo_data);

		return false;
	}

	bool handler(UINT message, WPARAM wparam, LPARAM lparam) override
	{
		auto ctrl = reinterpret_cast<HWND>(lparam);
		switch (message) {
		case WM_COMMAND:
			switch (auto id = 0xffff & wparam, code = wparam >> 16; code) {
			case BN_CLICKED:
			{
				bool checked = ::SendMessageW(ctrl, BM_GETCHECK, {}, {}) == BST_CHECKED;
				switch (id) {
				case IDC_CHECK1:
					on_change_lattice(checked);
					return true;
				}
				break;
			}
			case CBN_SELCHANGE:
				switch (id) {
				case IDC_COMBO1:
					on_change_rail(get_combo_data<RailMode>(ctrl));
					return true;
				}
				break;
			}
			break;
		}
		return false;
	}
};


////////////////////////////////
// 色座標表示ドラッグ固有(一部).
////////////////////////////////
class tip_drag : public dialog_base {
	using TipMode = Settings::TipDrag::Mode;
	using RailMode = Settings::RailMode;

public:
	Settings::TipDrag& drag;
	tip_drag(Settings::TipDrag& drag) : drag{ drag } {}

private:
	void on_change_mode(TipMode data_new) { drag.mode = data_new; }
	void on_change_rail(RailMode data_new) { drag.rail_mode = data_new; }
	void select_desc(TipMode mode) {
		const wchar_t* s;
		// TODO: move the strings below to resource.
		switch (mode) {
		case TipMode::frail:
			s = L"ボタンを押している間だけ情報ウィンドウが表示されます．"
				L"ボタンを離すと消えます．";
			break;
		case TipMode::stationary:
			s = L"ボタンを離しても情報表示が残ります．"
				L"もう一度ボタンを押すと消えます．"
				L"ボタンを離している間は表示位置が動きません．";
			break;
		case TipMode::sticky:
			s = L"ボタンを離しても情報表示が残ります．"
				L"もう一度ボタンを押すと消えます．"
				L"ボタンを離していても情報表示がマウスカーソルを追従します．";
			break;
		default: return;
		}
		::SendMessageW(::GetDlgItem(hwnd, IDC_EDIT1), WM_SETTEXT,
			{}, reinterpret_cast<LPARAM>(s));
	}

protected:
	uintptr_t template_id() const override { return IDD_SETTINGS_FORM_DRAG_TIP; }

	bool on_init(HWND) override
	{
		constexpr struct {
			const wchar_t* desc; // TODO: move these strings to resource.
			TipMode data;
		} combo_data1[] = {
			{ L"ホールド",		TipMode::frail		},
			{ L"トグル / 固定",	TipMode::stationary	},
			{ L"トグル / 追従",	TipMode::sticky		},
		};

		constexpr struct {
			const wchar_t* desc; // TODO: move these strings to resource.
			RailMode data;
		} combo_data2[] = {
			{ L"しない",	RailMode::none		},
			{ L"十字",	RailMode::cross		},
			{ L"8方向",	RailMode::octagonal	},
		};

		// suppress notifications from controls.
		auto sc = suppress_callback();
		init_combo_items(::GetDlgItem(hwnd, IDC_COMBO1), drag.mode, combo_data1);
		select_desc(drag.mode);
		init_combo_items(::GetDlgItem(hwnd, IDC_COMBO2), drag.rail_mode, combo_data2);

		return false;
	}

	bool handler(UINT message, WPARAM wparam, LPARAM lparam) override
	{
		auto ctrl = reinterpret_cast<HWND>(lparam);
		switch (message) {
		case WM_COMMAND:
			switch (auto id = 0xffff & wparam, code = wparam >> 16; code) {
			case CBN_SELCHANGE:
				switch (id) {
				case IDC_COMBO1:
					on_change_mode(get_combo_data<TipMode>(ctrl));
					select_desc(drag.mode);
					return true;
				case IDC_COMBO2:
					on_change_rail(get_combo_data<RailMode>(ctrl));
					return true;
				}
				break;
			}
			break;
		}
		return false;
	}
};


////////////////////////////////
// 拡張編集ドラッグ固有．
////////////////////////////////
class exedit_drag : public dialog_base {
	using KeyDisguise = Settings::ExEditDrag::KeyDisguise;

public:
	Settings::ExEditDrag& drag;
	exedit_drag(Settings::ExEditDrag& drag) : drag{ drag } {}

private:
	void on_change_shift(KeyDisguise data_new) { drag.shift = data_new; }
	void on_change_alt(KeyDisguise data_new) { drag.alt = data_new; }

protected:
	uintptr_t template_id() const override { return IDD_SETTINGS_FORM_DRAG_EXEDIT; }

	bool on_init(HWND) override
	{
		constexpr struct {
			const wchar_t* desc; // TODO: move these strings to resource.
			KeyDisguise data;
		} combo_data[] = {
			{ L"そのまま",	KeyDisguise::flat	},
			{ L"ON 固定",	KeyDisguise::on		},
			{ L"OFF 固定",	KeyDisguise::off	},
			{ L"反転",		KeyDisguise::invert	},
		};
		constexpr auto desc_data = // TODO: move this string to resource.
			L"拡張編集では Shift で方向を上下左右に固定したり Alt で拡大率の操作ができますが，"
			L"実際のキー入力に対するドラッグ操作でのキー認識を変更・上書きできます．\r\n"
			L"※ Ctrl キーの設定は変更できません．";

		// suppress notifications from controls.
		auto sc = suppress_callback();
		init_combo_items(::GetDlgItem(hwnd, IDC_COMBO1), drag.shift, combo_data);
		init_combo_items(::GetDlgItem(hwnd, IDC_COMBO2), drag.alt, combo_data);

		::SendMessageW(::GetDlgItem(hwnd, IDC_EDIT1), WM_SETTEXT,
			{}, reinterpret_cast<LPARAM>(desc_data));

		return false;
	}

	bool handler(UINT message, WPARAM wparam, LPARAM lparam) override
	{
		auto ctrl = reinterpret_cast<HWND>(lparam);
		switch (message) {
		case WM_COMMAND:
			switch (auto id = 0xffff & wparam, code = wparam >> 16; code) {
			case CBN_SELCHANGE:
				switch (id) {
				case IDC_COMBO1:
					on_change_shift(get_combo_data<KeyDisguise>(ctrl));
					return true;
				case IDC_COMBO2:
					on_change_alt(get_combo_data<KeyDisguise>(ctrl));
					return true;
				}
				break;
			}
			break;
		}
		return false;
	}
};


////////////////////////////////
// 拡大率の最大・最小．
////////////////////////////////
class zoom_options : public dialog_base {
	using Zoom = Settings::ZoomBehavior;
public:
	Zoom& zoom;
	zoom_options(Zoom& zoom) : zoom{ zoom } {}

private:
	void on_change_min(int8_t data_new) {
		zoom.min_scale_level = std::clamp(data_new, Zoom::min_scale_level_min, Zoom::min_scale_level_max);
	}
	void on_change_max(int8_t data_new) {
		zoom.max_scale_level = std::clamp(data_new, Zoom::max_scale_level_min, Zoom::max_scale_level_max);
	}
	void set_text(int id, int level) {
		auto [n, d] = Settings::HelperFunctions::ZoomScaleFromLevel(level);
		wchar_t buf[std::size(L"0123.45")];
		std::swprintf(buf, std::size(buf), L"%.2f", static_cast<double>(n) / d);
		::SendMessageW(::GetDlgItem(hwnd, id), WM_SETTEXT,
			{}, reinterpret_cast<LPARAM>(buf));
	}

protected:
	uintptr_t template_id() const override { return IDD_SETTINGS_FORM_ZOOM; }

	bool on_init(HWND) override
	{
		// suppress notifications from controls.
		auto sc = suppress_callback();
		auto slider = ::GetDlgItem(hwnd, IDC_SLIDER1);
		init_slider(slider, zoom.min_scale_level,
			Zoom::min_scale_level_min, Zoom::min_scale_level_max, 1, 4);
		::SendMessageW(slider, TBM_SETTIC, 0, static_cast<LPARAM>(0));
		set_text(IDC_EDIT1, zoom.min_scale_level);
		slider = ::GetDlgItem(hwnd, IDC_SLIDER2);
		init_slider(slider, zoom.max_scale_level,
			Zoom::max_scale_level_min, Zoom::max_scale_level_max, 1, 4);
		::SendMessageW(slider, TBM_SETTIC, 0, static_cast<LPARAM>(0));
		set_text(IDC_EDIT2, zoom.max_scale_level);
		return false;
	}

	bool handler(UINT message, WPARAM wparam, LPARAM lparam) override
	{
		auto ctrl = reinterpret_cast<HWND>(lparam);
		switch (message) {
		case WM_HSCROLL:
			switch (auto code = 0xffff & wparam) {
			case TB_ENDTRACK:
				// make sure minimum is less than maximum or equals.
				if (zoom.min_scale_level > zoom.max_scale_level) {
					int id_slider, id_edit;
					switch (::GetDlgCtrlID(ctrl)) {
					case IDC_SLIDER2:
						id_slider = IDC_SLIDER1; id_edit = IDC_EDIT1;
						zoom.min_scale_level = zoom.max_scale_level;
						break;
					default:
						id_slider = IDC_SLIDER2; id_edit = IDC_EDIT2;
						zoom.max_scale_level = zoom.min_scale_level;
						break;
					}
					auto sc = suppress_callback();
					set_slider_value(::GetDlgItem(hwnd, id_slider), zoom.min_scale_level);
					set_text(id_edit, zoom.min_scale_level);
					return true;
				}
				break;

			case TB_THUMBPOSITION:
				// simply ignore; TB_ENDTRACK will follow immediately after.
				break;

			default:
				switch (::GetDlgCtrlID(ctrl)) {
				case IDC_SLIDER1:
					on_change_min(get_slider_value(ctrl));
					set_text(IDC_EDIT1, zoom.min_scale_level);
					return true;
				case IDC_SLIDER2:
					on_change_max(get_slider_value(ctrl));
					set_text(IDC_EDIT2, zoom.max_scale_level);
					return true;
				}
				break;
			}
			break;
		}
		return false;
	}
};


////////////////////////////////
// 通知メッセージ設定．
////////////////////////////////
class toast_functions : public dialog_base {
	using Toast = Settings::Toast;
	using Placement = Toast::Placement;
	using ScaleFormat = Toast::ScaleFormat;

public:
	Toast& toast;
	toast_functions(Toast& toast) : toast{ toast } {}

private:
	void on_change_notify_scale(bool data_new) { toast.notify_scale = data_new; }
	void on_change_notify_follow_cursor(bool data_new) { toast.notify_follow_cursor = data_new; }
	void on_change_notify_grid(bool data_new) { toast.notify_grid = data_new; }
	void on_change_notify_clipboard(bool data_new) { toast.notify_clipboard = data_new; }
	void on_change_placement(Placement data_new) { toast.placement = data_new; }
	void on_change_duration(int data_new) {
		toast.duration = std::clamp(data_new, Toast::duration_min, Toast::duration_max);
	}
	void on_change_scale_format(ScaleFormat data_new) { toast.scale_format = data_new; }

protected:
	uintptr_t template_id() const override { return IDD_SETTINGS_FORM_TOAST; }

	bool on_init(HWND) override
	{
		constexpr struct {
			int id;
			Placement data;
		} radio_data[] = {
			{ IDC_RADIO1,	Placement::top_left		},
			{ IDC_RADIO2,	Placement::top			},
			{ IDC_RADIO3,	Placement::top_right	},
			{ IDC_RADIO4,	Placement::left			},
			{ IDC_RADIO5,	Placement::center		},
			{ IDC_RADIO6,	Placement::right		},
			{ IDC_RADIO7,	Placement::bottom_left	},
			{ IDC_RADIO8,	Placement::bottom		},
			{ IDC_RADIO9,	Placement::bottom_right	},
		};

		constexpr struct {
			const wchar_t* desc; // TODO: move these strings to resource.
			ScaleFormat data;
		} combo_data[] = {
			{ L"分数",	ScaleFormat::fraction	},
			{ L"小数",	ScaleFormat::decimal	},
			{ L"%表示",	ScaleFormat::percent	},
		};

		// suppress notifications from controls.
		auto sc = suppress_callback();
		::SendMessageW(::GetDlgItem(hwnd, IDC_CHECK1), BM_SETCHECK,
			toast.notify_scale ? BST_CHECKED : BST_UNCHECKED, {});
		::SendMessageW(::GetDlgItem(hwnd, IDC_CHECK2), BM_SETCHECK,
			toast.notify_follow_cursor ? BST_CHECKED : BST_UNCHECKED, {});
		::SendMessageW(::GetDlgItem(hwnd, IDC_CHECK3), BM_SETCHECK,
			toast.notify_grid ? BST_CHECKED : BST_UNCHECKED, {});
		::SendMessageW(::GetDlgItem(hwnd, IDC_CHECK4), BM_SETCHECK,
			toast.notify_clipboard ? BST_CHECKED : BST_UNCHECKED, {});
		for (auto& item : radio_data) {
			auto radio = ::GetDlgItem(hwnd, item.id);
			::SetWindowLongW(radio, GWL_USERDATA, static_cast<LONG>(item.data));
			if (item.data == toast.placement)
				::SendMessageW(radio, BM_SETCHECK, BST_CHECKED, {});
		}
		init_spin(::GetDlgItem(hwnd, IDC_SPIN1), toast.duration, toast.duration_min, toast.duration_max);
		init_combo_items(::GetDlgItem(hwnd, IDC_COMBO1), toast.scale_format, combo_data);

		return false;
	}

	bool handler(UINT message, WPARAM wparam, LPARAM lparam) override
	{
		auto ctrl = reinterpret_cast<HWND>(lparam);
		switch (message) {
		case WM_COMMAND:
		{
			bool checked = ::SendMessageW(ctrl, BM_GETCHECK, {}, {}) == BST_CHECKED;
			switch (auto id = 0xffff & wparam, code = wparam >> 16; code) {
			case BN_CLICKED:
				switch (id) {
				case IDC_RADIO1: case IDC_RADIO2: case IDC_RADIO3:
				case IDC_RADIO4: case IDC_RADIO5: case IDC_RADIO6:
				case IDC_RADIO7: case IDC_RADIO8: case IDC_RADIO9:
					on_change_placement(static_cast<Placement>(::GetWindowLongW(ctrl, GWL_USERDATA)));
					return true;
				case IDC_CHECK1:
					on_change_notify_scale(checked);
					return true;
				case IDC_CHECK2:
					on_change_notify_follow_cursor(checked);
					return true;
				case IDC_CHECK3:
					on_change_notify_grid(checked);
					return true;
				case IDC_CHECK4:
					on_change_notify_clipboard(checked);
					return true;
				}
				break;
			case EN_CHANGE:
				switch (id) {
				case IDC_EDIT1:
					on_change_duration(get_spin_value(::GetDlgItem(hwnd, IDC_SPIN1)));
					return true;
				}
				break;
			case CBN_SELCHANGE:
				switch (id) {
				case IDC_COMBO1:
					on_change_scale_format(get_combo_data<ScaleFormat>(ctrl));
					return true;
				}
				break;
			}
			break;
		}
		}
		return false;
	}
};


////////////////////////////////
// グリッド設定．
////////////////////////////////
class grid_options : public dialog_base {
	using Grid = Settings::Grid;
	constexpr static auto scale_level_max = Settings::ZoomBehavior::max_scale_level_max;
public:
	Grid& grid;
	grid_options(Grid& grid) : grid{ grid } {}

private:
	void on_change_thin(int8_t data_new) {
		grid.least_scale_thin = std::clamp(data_new, Grid::least_scale_thin_min, Grid::least_scale_thin_max);
	}
	void on_change_thick(int8_t data_new) {
		grid.least_scale_thick = std::clamp(data_new, Grid::least_scale_thick_min, Grid::least_scale_thick_max);
	}
	void set_text(int id, int level) {
		wchar_t buf[std::size(L"0123.45")]{ L"----" };
		if (level <= scale_level_max) {
			auto [n, d] = Settings::HelperFunctions::ZoomScaleFromLevel(level);
			std::swprintf(buf, std::size(buf), L"%.2f", static_cast<double>(n) / d);
		}
		::SendMessageW(::GetDlgItem(hwnd, id), WM_SETTEXT,
			{}, reinterpret_cast<LPARAM>(buf));
	}

protected:
	uintptr_t template_id() const override { return IDD_SETTINGS_FORM_GRID; }

	bool on_init(HWND) override
	{
		constexpr auto desc_data = // TODO: move this string to resource.
			L"グリッドには2種類あり，細いグリッドは1ピクセル幅，太いグリッドは2ピクセル幅の線で描画されます．\r\n"
			L"表示には拡大率が一定以上必要で，その最小拡大率を細いグリッド，太いグリッドでそれぞれ指定します．\r\n"
			L"いずれも 3 倍以上の拡大率が必要で，太いグリッドの描画が優先されます．";

		// suppress notifications from controls.
		auto sc = suppress_callback();
		auto slider = ::GetDlgItem(hwnd, IDC_SLIDER1);
		init_slider(slider, grid.least_scale_thin,
			Grid::least_scale_thin_min, Grid::least_scale_thin_max, 1, 4);
		::SendMessageW(slider, TBM_SETTIC, 0, static_cast<LPARAM>(scale_level_max));
		set_text(IDC_EDIT1, grid.least_scale_thin);
		slider = ::GetDlgItem(hwnd, IDC_SLIDER2);
		init_slider(slider, grid.least_scale_thick,
			Grid::least_scale_thick_min, Grid::least_scale_thick_max, 1, 4);
		::SendMessageW(slider, TBM_SETTIC, 0, static_cast<LPARAM>(scale_level_max));
		set_text(IDC_EDIT2, grid.least_scale_thick);
		::SendMessageW(::GetDlgItem(hwnd, IDC_EDIT3), WM_SETTEXT, {}, reinterpret_cast<LPARAM>(desc_data));
		return false;
	}

	bool handler(UINT message, WPARAM wparam, LPARAM lparam) override
	{
		auto ctrl = reinterpret_cast<HWND>(lparam);
		switch (message) {
		case WM_HSCROLL:
			switch (auto code = 0xffff & wparam) {
			case TB_ENDTRACK:
			case TB_THUMBPOSITION:
				break;

			default:
				switch (::GetDlgCtrlID(ctrl)) {
				case IDC_SLIDER1:
					on_change_thin(get_slider_value(ctrl));
					set_text(IDC_EDIT1, grid.least_scale_thin);
					return true;
				case IDC_SLIDER2:
					on_change_thick(get_slider_value(ctrl));
					set_text(IDC_EDIT2, grid.least_scale_thick);
					return true;
				}
				break;
			}
			break;
		}
		return false;
	}
};


////////////////////////////////
// 一部コマンドの設定．
////////////////////////////////
class command_swap_scale : public dialog_base {
	using Pivot = Settings::WheelZoom::Pivot;

public:
	Pivot& pivot;
	command_swap_scale(Pivot& pivot) : pivot{ pivot } {}

private:
	void on_change_swap_pivot(Pivot data_new) { pivot = data_new; }

protected:
	uintptr_t template_id() const override { return IDD_SETTINGS_FORM_CMD_SWAP; }

	bool on_init(HWND) override
	{
		constexpr struct {
			const wchar_t* desc; // TODO: move these strings to resource.
			Pivot data;
		} combo_data[] = {
			{ L"ウィンドウ中央",	Pivot::center	},
			{ L"カーソル位置",	Pivot::cursor	},
		};

		// suppress notifications from controls.
		auto sc = suppress_callback();
		init_combo_items(::GetDlgItem(hwnd, IDC_COMBO1), pivot, combo_data);

		return false;
	}

	bool handler(UINT message, WPARAM wparam, LPARAM lparam) override
	{
		auto ctrl = reinterpret_cast<HWND>(lparam);
		switch (message) {
		case WM_COMMAND:
			switch (auto id = 0xffff & wparam, code = wparam >> 16; code) {
			case CBN_SELCHANGE:
				switch (id) {
				case IDC_COMBO1:
					on_change_swap_pivot(get_combo_data<Pivot>(ctrl));
					return true;
				}
				break;
			}
			break;
		}
		return false;
	}
};


class command_step_scale : public dialog_base {
	using Pivot = Settings::WheelZoom::Pivot;
	using ClickActions = Settings::ClickActions;

public:
	Pivot& pivot;
	uint8_t& steps;
	command_step_scale(Pivot& pivot, uint8_t& steps) : pivot{ pivot }, steps{ steps } {}

private:
	void on_change_step_pivot(Pivot data_new) { pivot = data_new; }
	void on_change_num_steps(uint8_t data_new) {
		steps = std::clamp(data_new, ClickActions::scale_step_num_steps_min, ClickActions::scale_step_num_steps_max);
	}

protected:
	uintptr_t template_id() const override { return IDD_SETTINGS_FORM_CMD_ZOOM_STEP; }

	bool on_init(HWND) override
	{
		constexpr struct {
			const wchar_t* desc; // TODO: move these strings to resource.
			Pivot data;
		} combo_data[] = {
			{ L"ウィンドウ中央",	Pivot::center	},
			{ L"カーソル位置",	Pivot::cursor	},
		};
		constexpr auto desc_data = // TODO: move this string to resource. (note that this is shared by another dialog form.)
			L"ホイール入力1回で拡大率を何段階変化させるかを指定します．"
			L"4段階増える(減る)ごとに拡大率は2倍(半分)になります．";

		// suppress notifications from controls.
		auto sc = suppress_callback();
		init_combo_items(::GetDlgItem(hwnd, IDC_COMBO1), pivot, combo_data);
		init_spin(::GetDlgItem(hwnd, IDC_SPIN1), steps, ClickActions::scale_step_num_steps_min, ClickActions::scale_step_num_steps_max);
		::SendMessageW(::GetDlgItem(hwnd, IDC_EDIT2), WM_SETTEXT,
			{}, reinterpret_cast<LPARAM>(desc_data));

		return false;
	}

	bool handler(UINT message, WPARAM wparam, LPARAM lparam) override
	{
		auto ctrl = reinterpret_cast<HWND>(lparam);
		switch (message) {
		case WM_COMMAND:
			switch (auto id = 0xffff & wparam, code = wparam >> 16; code) {
			case CBN_SELCHANGE:
				switch (id) {
				case IDC_COMBO1:
					on_change_step_pivot(get_combo_data<Pivot>(ctrl));
					return true;
				}
				break;
			case EN_CHANGE:
				switch (id) {
				case IDC_EDIT1:
					on_change_num_steps(get_spin_value(::GetDlgItem(hwnd, IDC_SPIN1)));
					return true;
				}
				break;
			}
			break;
		}
		return false;
	}
};


////////////////////////////////
// ダイアログ本体．
////////////////////////////////
class setting_dlg : public dialog_base {
	constexpr static int window_width_min = 450, window_height_min = 200;

	enum class tab_kind : uint8_t {
		action_left,
		action_right,
		action_middle,
		action_x1_button,
		action_x2_button,

		drag_move_loupe,
		drag_show_tip,
		drag_exedit,
		//drag_zoom,

		wheel_zoom,

		toast,
		grid,
		commands,
		//color,
	};
	std::map<tab_kind, std::unique_ptr<vscroll_form>> pages{};
	static inline constinit tab_kind last_selected_tab = tab_kind::wheel_zoom;

	SIZE prev_size{};

	vscroll_form* create_page(tab_kind kind)
	{
		switch (kind) {
			{
				Settings::ClickActions::Button* btn;
		case tab_kind::action_left:
			btn = &curr.commands.left;
			goto click_pages;

		case tab_kind::action_right:
			btn = &curr.commands.right;
			goto click_pages;

		case tab_kind::action_middle:
			btn = &curr.commands.middle;
			goto click_pages;

		case tab_kind::action_x1_button:
			btn = &curr.commands.x1;
			goto click_pages;

		case tab_kind::action_x2_button:
			btn = &curr.commands.x2;

		click_pages:
			return new vscroll_form{
				new click_action{ *btn },
			};
			}

		case tab_kind::drag_move_loupe:
			return new vscroll_form{
				new drag_keys{ curr.loupe_drag.keys },
				new drag_range{ curr.loupe_drag.range },
				new wheel_zoom{ curr.loupe_drag.wheel },
				new loupe_drag{ curr.loupe_drag },
			};
		case tab_kind::drag_show_tip:
			return new vscroll_form{
				new drag_keys{ curr.tip_drag.keys },
				new drag_range{ curr.tip_drag.range },
				new wheel_zoom{ curr.tip_drag.wheel },
				new tip_drag{ curr.tip_drag },
				// TODO: formattings?
				// TODO: fonts?
			};
		case tab_kind::drag_exedit:
			return new vscroll_form{
				new drag_keys{ curr.exedit_drag.keys },
				new drag_range{ curr.exedit_drag.range },
				new wheel_zoom{ curr.exedit_drag.wheel },
				new exedit_drag{ curr.exedit_drag },
			};

		case tab_kind::wheel_zoom:
			return new vscroll_form{
				new wheel_zoom{ curr.zoom.wheel },
				new zoom_options{ curr.zoom },
			};

		case tab_kind::toast:
			return new vscroll_form{
				new toast_functions{ curr.toast },
				// TODO: fonts?
			};
		case tab_kind::grid:
			return new vscroll_form{
				new grid_options{ curr.grid },
			};
		case tab_kind::commands:
			return new vscroll_form{
				new command_swap_scale{ curr.commands.swap_scale_level_pivot },
				new command_step_scale{ curr.commands.scale_step_pivot, curr.commands.scale_step_num_steps },
			};
		}
		return nullptr;
	}

	RECT calc_page_rect() {
		// the gap between the list box and the property page, in dialog unit.
		constexpr int gap_ctrl = 7;

		RECT rc1, rc2;
		::GetWindowRect(::GetDlgItem(hwnd, IDC_LIST1), &rc1);
		::GetWindowRect(::GetDlgItem(hwnd, IDCANCEL), &rc2);

		rc1.left = rc1.right;
		rc1.right = rc2.right;

		::MapWindowPoints(nullptr, hwnd, reinterpret_cast<POINT*>(&rc1), 2);
		rc1.left += gap_ctrl;

		return rc1;
	}

protected:
	uintptr_t template_id() const override { return IDD_SETTINGS; }

	bool on_init(HWND) override
	{
		constexpr struct {
			const wchar_t* desc;
			tab_kind data;
		} headers[] = {
			{ L"ズーム操作",				tab_kind::wheel_zoom		},
			{ L"ルーペ移動ドラッグ",		tab_kind::drag_move_loupe	},
			{ L"色・座標表示",			tab_kind::drag_show_tip		},
			{ L"拡張編集ドラッグ",		tab_kind::drag_exedit		},
			{ L"左クリック",				tab_kind::action_left		},
			{ L"右クリック",				tab_kind::action_right		},
			{ L"ホイールクリック",		tab_kind::action_middle		},
			{ L"X1 クリック",			tab_kind::action_x1_button	},
			{ L"X2 クリック",			tab_kind::action_x2_button	},
			{ L"クリックコマンドの設定",	tab_kind::commands			},
			{ L"通知メッセージ",			tab_kind::toast				},
			{ L"グリッド",				tab_kind::grid				},
		};

		{
			RECT rc; ::GetClientRect(hwnd, &rc);
			prev_size.cx = rc.right; prev_size.cy = rc.bottom;
		}

		auto list = ::GetDlgItem(hwnd, IDC_LIST1);
		{
			auto sc = suppress_callback();
			init_list_items(list, last_selected_tab, headers);
		}

		// update to create the first page.
		::SendMessageW(hwnd, WM_COMMAND,
			IDC_LIST1 | (LBN_SELCHANGE << 16),
			reinterpret_cast<LPARAM>(list));

		// set the first focus to the list box.
		::SetFocus(list);

		// don't set the first focus to the OK button.
		return false;
	}

	void on_resize(int new_cx, int new_cy)
	{
		// dynamic layout.
		int diff_cx = new_cx - prev_size.cx,
			diff_cy = new_cy - prev_size.cy;
		prev_size = { new_cx, new_cy };

		constexpr auto get_window_rect = [](HWND ctrl, HWND parent, RECT& rc) {
			::GetWindowRect(ctrl, &rc);
			::MapWindowPoints(nullptr, parent, reinterpret_cast<POINT*>(&rc), 2);
		};
		HWND ctrl; RECT rc;

		// OK button.
		ctrl = ::GetDlgItem(hwnd, IDOK);
		get_window_rect(ctrl, hwnd, rc);
		::MoveWindow(ctrl, rc.left + diff_cx, rc.top + diff_cy, rc.right - rc.left, rc.bottom - rc.top, TRUE);

		// cancel button.
		ctrl = ::GetDlgItem(hwnd, IDCANCEL);
		get_window_rect(ctrl, hwnd, rc);
		::MoveWindow(ctrl, rc.left + diff_cx, rc.top + diff_cy, rc.right - rc.left, rc.bottom - rc.top, TRUE);

		// list box.
		ctrl = ::GetDlgItem(hwnd, IDC_LIST1);
		get_window_rect(ctrl, hwnd, rc);
		::MoveWindow(ctrl, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top + diff_cy, TRUE);

		// the page.
		if (auto& page = pages[get_list_data<tab_kind>(ctrl)];
			page && page->hwnd != nullptr) {
			ctrl = page->hwnd;
			get_window_rect(ctrl, hwnd, rc);
			::MoveWindow(ctrl, rc.left, rc.top, rc.right - rc.left + diff_cx, rc.bottom - rc.top + diff_cy, TRUE);
		}

		::InvalidateRect(hwnd, nullptr, FALSE);
	}

	void on_apply() const { settings = curr; }
	void on_close(bool accept)
	{
		last_selected_tab = get_list_data<tab_kind>(::GetDlgItem(hwnd, IDC_LIST1));
		::EndDialog(hwnd, accept ? TRUE : FALSE);
	}

	void on_select(tab_kind kind)
	{
		for (auto& it : pages) {
			if (it.first != kind) ::ShowWindow(it.second->hwnd, SW_HIDE);
		}

		auto& page = pages[kind];
		if (!page) page.reset(create_page(kind));
		if (!page) return;

		if (page->hwnd == nullptr) page->create(hwnd);
		if (page->hwnd == nullptr) return;

		auto rc = calc_page_rect();
		::MoveWindow(page->hwnd, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, TRUE);
		::ShowWindow(page->hwnd, SW_SHOW);
	}

	bool handler(UINT message, WPARAM wparam, LPARAM lparam)
	{
		switch (message) {
		case WM_COMMAND:
			switch (auto id = 0xffff & wparam, code = wparam >> 16; code) {
			case BN_CLICKED:
				switch (id) {
				case IDOK:
					on_apply();
					on_close(true);
					return true;
				case IDCANCEL:
					on_close(false);
					return true;
				}
				break;

			case LBN_SELCHANGE:
				if (id == IDC_LIST1) {
					auto list = reinterpret_cast<HWND>(lparam);
					on_select(get_list_data<tab_kind>(list));
					return true;
				}
				break;
			}
			break;

		case WM_SIZING:
			return clamp_on_sizing(wparam, lparam, { window_width_min, window_height_min });
		case WM_SIZE:
			on_resize(static_cast<int16_t>(0xffff & lparam), static_cast<int16_t>(lparam >> 16));
			return true;
		}
		return false;
	}

public:
	Settings curr;
	setting_dlg(const Settings& base)
		: curr{ base }
	{
	}
};

namespace dialogs
{
	bool open_settings(HWND parent)
	{
		return setting_dlg{ settings }.modal(parent) != FALSE;
	}
}
