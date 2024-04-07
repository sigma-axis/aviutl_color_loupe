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

#include "resource.h"
extern HMODULE this_dll;

#include "dialogs_basics.hpp"
#include "settings.hpp"

using namespace dialogs::basics;

////////////////////////////////
// クリック操作選択．
////////////////////////////////
class click_action : public dialog_base {
protected:
	const wchar_t* template_id() const override {
		return MAKEINTRESOURCEW(IDD_SETTINGS_FORM_CLICK_ACTION);
	}

	bool on_init(HWND) override
	{
		using cc = Settings::CommonCommand;
		constexpr struct {
			const wchar_t* desc; // TODO: move these strings to resource.
			Settings::CommonCommand data;
		} combo_data[] = {
			{ L"(何もしない)",			cc::none					},
			{ L"画面の中央へ移動",		cc::centralize				},
			{ L"カーソル追従切り替え",	cc::toggle_follow_cursor	},
			{ L"拡大率切り替え",			cc::swap_scale_level		},
			{ L"グリッド表示切替",		cc::toggle_grid				},
			{ L"カラーコードをコピー",	cc::copy_color_code			},
			{ L"メニューを表示",			cc::context_menu			},
		};

		constexpr auto add_item = [](HWND combo, const auto& item) {
			WPARAM idx = ::SendMessageW(combo, CB_ADDSTRING, {}, reinterpret_cast<LPARAM>(item.desc));
			::SendMessageW(combo, CB_SETITEMDATA, idx, static_cast<LPARAM>(item.data));
			return idx;
		};

		// suppress notifications from the combo boxes.
		auto sc = suppress_callback();
		for (int id : { IDC_CLICK_ACTION_SELECTOR, IDC_DBLCLICK_ACTION_SELECTOR }) {
			HWND combo = ::GetDlgItem(hwnd, id);
			cc curr;
			switch (id) {
			case IDC_CLICK_ACTION_SELECTOR:
				curr = cmd_single; break;
			case IDC_DBLCLICK_ACTION_SELECTOR:
				curr = cmd_double; break;
			default: std::unreachable();
			}

			for (const auto& item : combo_data) {
				auto idx = add_item(combo, item);
				if (item.data == curr)
					::SendMessageW(combo, CB_SETCURSEL, idx, {});
			}
		}

		return false;
	}

	void on_change_single(Settings::CommonCommand cmd_new)
	{
		cmd_single = cmd_new;
	}

	void on_change_double(Settings::CommonCommand cmd_new)
	{
		cmd_double = cmd_new;
	}

	bool handler(UINT message, WPARAM wparam, LPARAM lparam) override
	{
		constexpr auto get_sel = [](LPARAM l) {
			auto combo = reinterpret_cast<HWND>(l);
			return static_cast<Settings::CommonCommand>(
				::SendMessageW(combo, CB_GETITEMDATA,
					::SendMessageW(combo, CB_GETCURSEL, {}, {}), {}));
		};
		switch (message) {
		case WM_COMMAND:
			switch (auto id = 0xffff & wparam, code = wparam >> 16; code) {
			case CBN_SELCHANGE:
				switch (id) {
				case IDC_CLICK_ACTION_SELECTOR:
					on_change_single(get_sel(lparam));
					return true;
				case IDC_DBLCLICK_ACTION_SELECTOR:
					on_change_double(get_sel(lparam));
					return true;
				}
				break;
			}
			break;
		}
		return false;
	}

public:
	Settings::CommonCommand& cmd_single, & cmd_double;
	click_action(
		Settings::CommonCommand& cmd_single,
		Settings::CommonCommand& cmd_double)
		: cmd_single{ cmd_single }, cmd_double{ cmd_double } {}

};


////////////////////////////////
// ドラッグ操作選択．
////////////////////////////////

// TODO:


////////////////////////////////
// ダイアログ本体．
////////////////////////////////
class setting_dlg : public dialog_base {
	constexpr static int window_width_min = 450, window_height_min = 200;

	static inline Settings::CommonCommand cmd_placeholder = Settings::CommonCommand::none;
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
				Settings::CommonCommand* cmd1, * cmd2;
		case tab_kind::action_left:
			cmd1 = &cmd_placeholder;
			cmd2 = &curr.commands.left_dblclk;
			goto click_pages;

		case tab_kind::action_right:
			cmd1 = &cmd_placeholder;
			cmd2 = &curr.commands.right_dblclk;
			goto click_pages;

		case tab_kind::action_middle:
			cmd1 = &curr.commands.middle_click;
			cmd2 = &cmd_placeholder;
			goto click_pages;

		case tab_kind::action_x1_button:
		case tab_kind::action_x2_button:
			cmd1 = &cmd_placeholder;
			cmd2 = &cmd_placeholder;

		click_pages:
			return new vscroll_form{
				new click_action{ *cmd1, *cmd2 }
			};
			}
		}
		return nullptr;
	}

	RECT calc_page_rect() {
		// the gap between the list box and the property page, in dialog unit.
		constexpr int gap_ctrl = 7;

		RECT rc1, rc2;
		::GetWindowRect(::GetDlgItem(hwnd, IDC_SETTINGS_ACTION_LIST), &rc1);
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
			const wchar_t* name;
			tab_kind kind;
		} headers[] = {
			{ L"左クリック",			tab_kind::action_left },
			{ L"右クリック",			tab_kind::action_right },
			{ L"ホイールクリック",	tab_kind::action_middle },
			{ L"X1 クリック",		tab_kind::action_x1_button },
			{ L"X2 クリック",		tab_kind::action_x2_button },
		};

		constexpr auto add_item = [](HWND list, const auto& item) {
			WPARAM idx = ::SendMessageW(list, LB_ADDSTRING, {}, reinterpret_cast<LPARAM>(item.name));
			::SendMessageW(list, LB_SETITEMDATA, idx, static_cast<LPARAM>(item.kind));
			return idx;
		};

		{
			RECT rc; ::GetClientRect(hwnd, &rc);
			prev_size.cx = rc.right; prev_size.cy = rc.bottom;
		}

		auto list = ::GetDlgItem(hwnd, IDC_SETTINGS_ACTION_LIST);
		{
			auto sc = suppress_callback();
			for (auto& item : headers) add_item(list, item);
			::SendMessageW(list, LB_SETCURSEL, 0, {});
		}

		// update to create the first page.
		::SendMessageW(hwnd, WM_COMMAND,
			IDC_SETTINGS_ACTION_LIST | (LBN_SELCHANGE << 16),
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
		ctrl = ::GetDlgItem(hwnd, IDC_SETTINGS_ACTION_LIST);
		get_window_rect(ctrl, hwnd, rc);
		::MoveWindow(ctrl, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top + diff_cy, TRUE);

		// the page.
		if (auto& page = pages[static_cast<tab_kind>(
			::SendMessageW(ctrl, LB_GETITEMDATA,
				::SendMessageW(ctrl, LB_GETCURSEL, {}, {}), {}))];
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
				if (id == IDC_SETTINGS_ACTION_LIST) {
					auto list = reinterpret_cast<HWND>(lparam);
					on_select(static_cast<tab_kind>(
						::SendMessageW(list, LB_GETITEMDATA,
							::SendMessageW(list, LB_GETCURSEL, {}, {}), {})));
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
	}
}
