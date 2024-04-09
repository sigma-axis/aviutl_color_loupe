/*
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


////////////////////////////////
// クリック操作選択．
////////////////////////////////
class click_action : public dialog_base {
	using Command = Settings::ClickActions::Command;
protected:
	const wchar_t* template_id() const override {
		return MAKEINTRESOURCEW(IDD_SETTINGS_FORM_CLICK_ACTION);
	}

	bool on_init(HWND) override
	{
		constexpr struct {
			const wchar_t* desc; // TODO: move these strings to resource.
			Command data;
		} combo_data[] = {
			{ L"(何もしない)",			Command::none					},
			{ L"画面の中央へ移動",		Command::centralize				},
			{ L"カーソル追従切り替え",	Command::toggle_follow_cursor	},
			{ L"拡大率切り替え",			Command::swap_scale_level		},
			{ L"グリッド表示切替",		Command::toggle_grid			},
			{ L"カラーコードをコピー",	Command::copy_color_code		},
			{ L"メニューを表示",			Command::context_menu			},
			{ L"設定ウィンドウ",			Command::settings				},
		};

		// suppress notifications from the combo boxes.
		auto sc = suppress_callback();
		for (int id : { IDC_COMBO1, IDC_COMBO2 }) {
			HWND combo = ::GetDlgItem(hwnd, id);
			Command curr;
			switch (id) {
			case IDC_COMBO1:
				curr = cmd_single; break;
			case IDC_COMBO2:
				curr = cmd_double; break;
			default: std::unreachable();
			}

			init_combo_items(combo, curr, combo_data);
		}

		return false;
	}

	void on_change_single(Command data_new) { cmd_single = data_new; }
	void on_change_double(Command data_new) { cmd_double = data_new; }

	bool handler(UINT message, WPARAM wparam, LPARAM lparam) override
	{
		auto combo = reinterpret_cast<HWND>(lparam);
		switch (message) {
		case WM_COMMAND:
			switch (auto id = 0xffff & wparam, code = wparam >> 16; code) {
			case CBN_SELCHANGE:
				switch (id) {
				case IDC_COMBO1:
					on_change_single(get_combo_data<Command>(combo));
					return true;
				case IDC_COMBO2:
					on_change_double(get_combo_data<Command>(combo));
					return true;
				}
				break;
			}
			break;
		}
		return false;
	}

public:
	Command& cmd_single, & cmd_double;
	click_action(
		Command& cmd_single,
		Command& cmd_double)
		: cmd_single{ cmd_single }, cmd_double{ cmd_double } {}

};


////////////////////////////////
// ドラッグのキー設定．
////////////////////////////////
class drag_keys : public dialog_base {
	using KeysActivate = Settings::KeysActivate;
	using Button = KeysActivate::MouseButton;
	using KeyCond = KeysActivate::State;
protected:
	const wchar_t* template_id() const override {
		return MAKEINTRESOURCEW(IDD_SETTINGS_FORM_DRAG_KEYS);
	}

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

		// suppress notifications from the combo boxes.
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

		::SendMessageW(::GetDlgItem(hwnd, IDC_EDIT_DESC1), WM_SETTEXT,
			{}, reinterpret_cast<LPARAM>(desc_data));

		return false;
	}

	void on_change_button(Button data_new) { keys.button = data_new; }
	void on_change_ctrl(KeyCond data_new) { keys.ctrl = data_new; }
	void on_change_shift(KeyCond data_new) { keys.shift = data_new; }
	void on_change_alt(KeyCond data_new) { keys.alt = data_new; }

	bool handler(UINT message, WPARAM wparam, LPARAM lparam) override
	{
		auto combo = reinterpret_cast<HWND>(lparam);
		switch (message) {
		case WM_COMMAND:
			switch (auto id = 0xffff & wparam, code = wparam >> 16; code) {
			case CBN_SELCHANGE:
				switch (id) {
				case IDC_COMBO1:
					on_change_button(get_combo_data<Button>(combo));
					return true;
				case IDC_COMBO2:
					on_change_ctrl(get_combo_data<KeyCond>(combo));
					return true;
				case IDC_COMBO3:
					on_change_shift(get_combo_data<KeyCond>(combo));
					return true;
				case IDC_COMBO4:
					on_change_alt(get_combo_data<KeyCond>(combo));
					return true;
				}
				break;
			}
			break;
		}
		return false;
	}

public:
	KeysActivate& keys;
	drag_keys(KeysActivate& keys) : keys{ keys } {}
};


////////////////////////////////
// ドラッグの有効範囲．
////////////////////////////////
class drag_range : public dialog_base {
	using DragInvalidRange = Settings::DragInvalidRange;
protected:
	const wchar_t* template_id() const override {
		return MAKEINTRESOURCEW(IDD_SETTINGS_FORM_DRAG_RANGE);
	}

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

		// suppress notifications from the combo boxes.
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

		::SendMessageW(::GetDlgItem(hwnd, IDC_EDIT_DESC1), WM_SETTEXT,
			{}, reinterpret_cast<LPARAM>(desc_data));

		return false;
	}

	void on_change_distance(int16_t data_new) {
		range.distance = std::clamp(data_new, range.distance_min, range.distance_max);
	}
	void on_change_timespan(int16_t data_new) {
		range.timespan = std::clamp(data_new, range.timespan_min, range.timespan_max);
	}

	bool handler(UINT message, WPARAM wparam, LPARAM lparam) override
	{
		auto spin = reinterpret_cast<HWND>(lparam);
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

public:
	DragInvalidRange& range;
	drag_range(DragInvalidRange& range) : range{ range } {}
};


////////////////////////////////
// ドラッグ中のホイール操作．
////////////////////////////////


////////////////////////////////
// ルーペ一位置ドラッグ固有．
////////////////////////////////

// lattice, rail_mode.


////////////////////////////////
// 色座標表示ドラッグ固有(一部).
////////////////////////////////

// mode, rail_mode, (possibly color/corrd format)


////////////////////////////////
// 拡張編集ドラッグ固有．
////////////////////////////////

// KeyDisguise shift, alt;


////////////////////////////////
// 通知メッセージ設定．
////////////////////////////////


////////////////////////////////
// グリッド設定．
////////////////////////////////


////////////////////////////////
// 一部コマンドの設定．
////////////////////////////////

// ZoomPivot swap_scale_level_pivot = ZoomPivot::cursor;


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
	};
	std::map<tab_kind, std::unique_ptr<vscroll_form>> pages{};

	SIZE prev_size{};

	vscroll_form* create_page(tab_kind kind)
	{
		switch (kind) {
			{
				Settings::ClickActions::Command* cmd1, * cmd2;
		case tab_kind::action_left:
			cmd1 = &curr.commands.left_click;
			cmd2 = &curr.commands.left_dblclk;
			goto click_pages;

		case tab_kind::action_right:
			cmd1 = &curr.commands.right_click;
			cmd2 = &curr.commands.right_dblclk;
			goto click_pages;

		case tab_kind::action_middle:
			cmd1 = &curr.commands.middle_click;
			cmd2 = &curr.commands.middle_dblclk;
			goto click_pages;

		case tab_kind::action_x1_button:
			cmd1 = &curr.commands.x1_click;
			cmd2 = &curr.commands.x1_dblclk;
			goto click_pages;

		case tab_kind::action_x2_button:
			cmd1 = &curr.commands.x2_click;
			cmd2 = &curr.commands.x2_dblclk;

		click_pages:
			return new vscroll_form{
				new click_action{ *cmd1, *cmd2 },
			};
			}
			{
				Settings::KeysActivate* keys;
				Settings::DragInvalidRange* range;
		case tab_kind::drag_move_loupe:
			keys = &curr.loupe_drag.keys;
			range = &curr.loupe_drag.range;
			goto drag_pages;
		case tab_kind::drag_show_tip:
			keys = &curr.tip_drag.keys;
			range = &curr.tip_drag.range;
			goto drag_pages;
		case tab_kind::drag_exedit:
			keys = &curr.exedit_drag.keys;
			range = &curr.exedit_drag.range;
		drag_pages:
			return new vscroll_form{
				new drag_keys{ *keys },
				new drag_range{ *range },
			};
			}
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
	const wchar_t* template_id() const override {
		return MAKEINTRESOURCEW(IDD_SETTINGS);
	}

	bool on_init(HWND) override
	{
		constexpr struct {
			const wchar_t* desc;
			tab_kind data;
		} headers[] = {
			{ L"左クリック",			tab_kind::action_left		},
			{ L"右クリック",			tab_kind::action_right		},
			{ L"ホイールクリック",	tab_kind::action_middle		},
			{ L"X1 クリック",		tab_kind::action_x1_button	},
			{ L"X2 クリック",		tab_kind::action_x2_button	},
			{ L"ルーペ移動ドラッグ",	tab_kind::drag_move_loupe	},
			{ L"色・座標表示",		tab_kind::drag_show_tip		},
			{ L"拡張編集ドラッグ",	tab_kind::drag_exedit		},
		};

		{
			RECT rc; ::GetClientRect(hwnd, &rc);
			prev_size.cx = rc.right; prev_size.cy = rc.bottom;
		}

		auto list = ::GetDlgItem(hwnd, IDC_LIST1);
		{
			auto sc = suppress_callback();
			init_list_items(list, tab_kind::action_left, headers);
		}

		// update to create the first page.
		::SendMessageW(hwnd, WM_COMMAND,
			IDC_LIST1 | (LBN_SELCHANGE << 16),
			reinterpret_cast<LPARAM>(list));

		// control the focus.
		::SetFocus(list);

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

	void on_apply() const
	{
		settings = curr;
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
					::EndDialog(hwnd, TRUE);
					return true;
				case IDCANCEL:
					::EndDialog(hwnd, FALSE);
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
	void open_settings(HWND parent)
	{
		auto ret = setting_dlg{ settings }.modal(parent);
		// TODO: update/redraw the window if OK was pressed.
		// TODO: hide the tip if necessary (like when it's disabled).
	}
}
