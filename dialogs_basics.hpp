/*
The MIT License (MIT)

Copyright (c) 2024 sigma-axis

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#pragma once

#include <algorithm>
#include <vector>
#include <memory>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "resource.h"
extern HMODULE this_dll;

namespace dialogs::basics
{
	////////////////////////////////
	// Base for dialog classes.
	////////////////////////////////
	class dialog_base {
		static intptr_t CALLBACK proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
		{
			bool ret;
			if (message == WM_INITDIALOG) {
				auto that = reinterpret_cast<dialog_base*>(lparam);
				that->hwnd = hwnd;
				::SetWindowLongW(hwnd, GWL_USERDATA, reinterpret_cast<LONG>(that));
				ret = that->on_init(reinterpret_cast<HWND>(wparam));
			}
			else {
				auto that = reinterpret_cast<dialog_base*>(::GetWindowLongW(hwnd, GWL_USERDATA));
				if (that == nullptr || that->no_callback || that->hwnd != hwnd) return FALSE;

				ret = that->handler(message, wparam, lparam);
				if (message == WM_NCDESTROY) that->hwnd = nullptr;
			}

			return ret ? TRUE : FALSE;
		}
		bool no_callback = false;

	protected:
		virtual uintptr_t template_id() const = 0;
		virtual bool on_init(HWND def_control) = 0;
		virtual bool handler(UINT message, WPARAM wparam, LPARAM lparam) = 0;

		const auto suppress_callback() {
			struct backyard {
				backyard(dialog_base& host)
					: ptr{ host.no_callback ? nullptr : &host.no_callback }
				{
					if (ptr != nullptr) *ptr = true;
				}
				~backyard() {
					if (ptr != nullptr) *ptr = false;
				}
			private:
				bool* const ptr;
			};
			return backyard{ *this };
		}

		static bool clamp_on_sizing(WPARAM direction, LPARAM prect,
			const SIZE& sz_min, const SIZE& sz_max = { 0x7fff, 0x7fff })
		{
			auto& rc = *reinterpret_cast<RECT*>(prect);
			bool ret = false;

			// clamp horizontally.
			if (int w = rc.right - rc.left,
				overshoot = std::min<int>(w - sz_min.cx, 0) + std::max<int>(w - sz_max.cx, 0);
				overshoot != 0) {
				switch (direction) {
				case WMSZ_LEFT:
				case WMSZ_TOPLEFT:
				case WMSZ_BOTTOMLEFT:
					rc.left += overshoot;
					break;
				default:
					rc.right -= overshoot;
					break;
				}
				ret = true;
			}

			// clamp vertically.
			if (int h = rc.bottom - rc.top,
				overshoot = std::min<int>(h - sz_min.cy, 0) + std::max<int>(h - sz_max.cy, 0);
				overshoot != 0) {
				switch (direction) {
				case WMSZ_TOP:
				case WMSZ_TOPLEFT:
				case WMSZ_TOPRIGHT:
					rc.top += overshoot;
					break;
				default:
					rc.bottom -= overshoot;
					break;
				}
				ret = true;
			}
			return ret;
		}

	public:
		HWND hwnd = nullptr;
		void create(HWND parent)
		{
			if (hwnd == nullptr)
				::CreateDialogParamW(this_dll, MAKEINTRESOURCEW(template_id()),
					parent, proc, reinterpret_cast<LPARAM>(this));
		}
		intptr_t modal(HWND parent)
		{
			if (hwnd != nullptr) return 0;

			// find the top-level window containing `parent`,
			// for the cases like SplitWindow or Nest is applied.
			parent = ::GetAncestor(parent, GA_ROOT);
			return ::DialogBoxParamW(this_dll, MAKEINTRESOURCEW(template_id()),
				parent, proc, reinterpret_cast<LPARAM>(this));
		}

		virtual ~dialog_base() {
			if (hwnd != nullptr) ::DestroyWindow(hwnd);
		}
	};


	////////////////////////////////
	// Scroll Viewer.
	////////////////////////////////
	class vscroll_form : public dialog_base {
		int prev_scroll_pos = -1;
	protected:
		uintptr_t template_id() const override { return IDD_VSCROLLFORM; }

		bool on_init(HWND) override
		{
			for (auto& child : children) {
				child.inst->create(hwnd);
				RECT rc;
				::GetClientRect(child.inst->hwnd, &rc);
				child.w = rc.right; child.h = rc.bottom;
				::ShowWindow(child.inst->hwnd, SW_SHOW);
			}

			// no default control to focus.
			return false;
		}

		void on_scroll(int pos, bool force = false) {
			auto diff = prev_scroll_pos - pos;
			if (!force && diff == 0) return;

			prev_scroll_pos = pos;
			for (auto& child : children) {
				if (!force && diff > 0) {
					// to prevent wrong drawings.
					RECT rc{ 0, 0, child.w, diff };
					::InvalidateRect(child.inst->hwnd, &rc, FALSE);
				}
				::MoveWindow(child.inst->hwnd, child.x, child.y - pos, child.w, child.h, TRUE);
			}
		}

		void on_resize(int w, int h)
		{
			auto const sw = ::GetSystemMetrics(SM_CXVSCROLL);

			// calculate the layout.
			auto max_w = w - sw;
			for (auto& child : children)
				max_w = std::max(max_w, child.w);

			int tot_w = 0, tot_h = 0, max_h = 0;
			for (auto& child : children) {
				auto W = child.w, H = child.h;
				if (tot_w + W > max_w) {
					tot_w = 0;
					tot_h += max_h;
					max_h = 0;
				}
				child.x = tot_w;
				child.y = tot_h;

				tot_w += W;
				max_h = std::max(max_h, H);

				::InvalidateRect(child.inst->hwnd, nullptr, FALSE);
			}

			// set the scroll range and redraw.
			SCROLLINFO si{ .cbSize = sizeof(si), .fMask = SIF_RANGE | SIF_PAGE };
			si.nMin = 0; si.nMax = tot_h + max_h;
			si.nPage = h;
			on_scroll(::SetScrollInfo(hwnd, SB_VERT, &si, TRUE), true);
		}

		bool handler(UINT message, WPARAM wparam, LPARAM lparam)
		{
			switch (message) {
			case WM_SIZE:
				on_resize(static_cast<int16_t>(lparam), static_cast<int16_t>(lparam >> 16));
				return true;
			case WM_VSCROLL:
			{
				SCROLLINFO si = get_scroll_info(SIF_POS | SIF_RANGE | SIF_PAGE);
				auto code = 0xffff & wparam;
				switch (code) {
				case SB_THUMBPOSITION:
				case SB_THUMBTRACK:
					si.nPos = static_cast<int16_t>(wparam >> 16);
					break;

				case SB_LINEUP:		si.nPos -= scroll_delta; break;
				case SB_LINEDOWN:	si.nPos += scroll_delta; break;
				case SB_PAGEUP:		si.nPos -= si.nPage;	break;
				case SB_PAGEDOWN:	si.nPos += si.nPage;	break;
				case SB_TOP:		si.nPos = si.nMin;		break;
				case SB_BOTTOM:		si.nPos = si.nMax;		break;
				default: return false; // includes SB_ENDSCROLL.
				}
				si.nPos = std::clamp(si.nPos, si.nMin, std::max<int>(0, si.nMax - si.nPage));
				if (code != SB_THUMBTRACK) {
					si.fMask = SIF_POS;
					si.nPos = ::SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
				}
				on_scroll(si.nPos);
				return true;
			}
			case WM_MOUSEWHEEL:
			{
				auto wheel_delta = static_cast<int16_t>(wparam >> 16);

				set_scroll_pos(get_scroll_pos() - scroll_wheel * wheel_delta / WHEEL_DELTA);
				return true;
			}
			}
			return false;
		}
	public:
		int scroll_delta = 8, scroll_wheel = 32;

	private:
		struct child {
			std::unique_ptr<dialog_base> inst;
			int x, y, w, h;
			child(dialog_base* ptr) : inst{ ptr } { x = y = w = h = 0; }
		};
		std::vector<child> children;

	public:
		vscroll_form(std::initializer_list<dialog_base*> children)
		{
			this->children.reserve(children.size());
			for (auto* child : children) this->children.emplace_back(child);
		}

		bool get_scroll_info(SCROLLINFO& si) { return ::GetScrollInfo(hwnd, SB_VERT, &si) != FALSE; }
		SCROLLINFO get_scroll_info(UINT flags = SIF_ALL) {
			SCROLLINFO si{ .cbSize = sizeof(si), .fMask = flags };
			get_scroll_info(si);
			return si;
		}
		int get_scroll_pos() { return get_scroll_info(SIF_POS).nPos; }
		int set_scroll_pos(int pos) {
			SCROLLINFO si{ .cbSize = sizeof(si), .fMask = SIF_POS, .nPos = pos };
			int ret = ::SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
			on_scroll(ret);
			return ret;
		}
	};
}
