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
				::SetPropW(hwnd, prop_name, reinterpret_cast<HANDLE>(that));
				ret = that->on_init(reinterpret_cast<HWND>(wparam));
			}
			else {
				auto that = reinterpret_cast<dialog_base*>(::GetPropW(hwnd, prop_name));
				if (that == nullptr || that->no_callback || that->hwnd == nullptr) return FALSE;

				ret = that->handler(message, wparam, lparam);
				if (message == WM_NCDESTROY) that->hwnd = nullptr;
			}

			return ret ? TRUE : FALSE;
		}
		constexpr static auto prop_name = L"this";
		bool no_callback = false;

	protected:
		virtual const wchar_t* template_id() const = 0;
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
			::CreateDialogParamW(this_dll, template_id(), parent, proc, reinterpret_cast<LPARAM>(this));
		}
		intptr_t modal(HWND parent)
		{
			// TODO: find the top-level window containing `parent`,
			// for the cases like SplitWindow or Nest is applied.
			return ::DialogBoxParamW(this_dll, template_id(), parent, proc, reinterpret_cast<LPARAM>(this));
		}

		// make sure derived classes finalize their fields.
		virtual ~dialog_base() {}
	};


	////////////////////////////////
	// Scroll Viewer.
	////////////////////////////////
	class vscroll_form : public dialog_base {
		int prev_scroll_pos = 0;
	protected:
		const wchar_t* template_id() const override {
			return MAKEINTRESOURCEW(IDD_VSCROLLFORM);
		}

		bool on_init(HWND) override
		{
			for (auto& child : children) {
				child.inst->create(hwnd);
				RECT rc;
				::GetClientRect(child.inst->hwnd, &rc);
				child.w = rc.right; child.h = rc.bottom;
				::ShowWindow(child.inst->hwnd, SW_SHOW);
			}

			// measure all children by updating the size and scroll.
			RECT rc;
			::GetClientRect(hwnd, &rc);
			::SendMessageW(hwnd, WM_SIZE, 0, (0xffff & rc.right) | (rc.bottom << 16));
			::SendMessageW(hwnd, WM_VSCROLL, SB_THUMBTRACK, {});

			//// show all children.
			//for (auto& child : children)
			//	::ShowWindow(child.inst->hwnd, SW_SHOW);
			return false;
		}

		void on_scroll(int pos) {
			auto diff = prev_scroll_pos - pos;
			prev_scroll_pos = pos;
			for (auto& child : children) {
				if (diff > 0) {
					// to prevent wrong drawings.
					RECT rc{ 0, 0, child.w, diff };
					::InvalidateRect(child.inst->hwnd, &rc, FALSE);
				}
				::MoveWindow(child.inst->hwnd, child.x, child.y - pos, child.w, child.h, TRUE);
			}
		}

		void  on_resize(int w, int h)
		{
			auto sw = ::GetSystemMetrics(SM_CXVSCROLL);

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
			}

			// set the scroll range.
			SCROLLINFO si{ .cbSize = sizeof(si), .fMask = SIF_RANGE | SIF_PAGE };
			si.nMin = 0; si.nMax = tot_h + max_h;
			si.nPage = h;
			::SetScrollInfo(hwnd, SB_VERT, &si, TRUE);

			// redraw.
			si.fMask = SIF_POS;
			::GetScrollInfo(hwnd, SB_VERT, &si);
			on_scroll(si.nPos);
		}

		bool handler(UINT message, WPARAM wparam, LPARAM lparam)
		{
			switch (message) {
			case WM_SIZE:
				on_resize(static_cast<int16_t>(lparam), static_cast<int16_t>(lparam >> 16));
				return true;
			case WM_VSCROLL:
			{
				SCROLLINFO si{ .cbSize = sizeof(si), .fMask = SIF_ALL };
				::GetScrollInfo(hwnd, SB_VERT, &si);
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
				}
				si.nPos = std::clamp<int>(si.nPos, si.nMin, si.nMax - si.nPage);
				if (code != SB_THUMBTRACK) {
					si.fMask = SIF_POS;
					::SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
				}
				on_scroll(si.nPos);
				return true;
			}
			case WM_MOUSEWHEEL:
			{
				constexpr size_t def_wheel_amount = 120;

				SCROLLINFO si{ .cbSize = sizeof(si), .fMask = SIF_ALL };
				::GetScrollInfo(hwnd, SB_VERT, &si);
				auto wheel_delta = static_cast<int16_t>(wparam >> 16);
				si.nPos -= wheel_delta * scroll_wheel / def_wheel_amount;
				si.nPos = std::clamp<int>(si.nPos, si.nMin, si.nMax - si.nPage);
				::SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
				on_scroll(si.nPos);
				return true;
			}
			}
			return false;
		}
	public:
		size_t scroll_delta = 8, scroll_wheel = 32;

	private:
		struct child {
			std::unique_ptr<dialog_base> inst{};
			int x = 0, y = 0, w = 0, h = 0;
		};
		std::vector<child> children;

	public:
		// children are of the type dialog_base*.
		vscroll_form(auto*... children)
		{
			this->children.resize(sizeof...(children));
			int i = 0;
			(this->children[i++].inst.reset(children), ...);
		}
	};
}
