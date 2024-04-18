/*
The MIT License (MIT)

Copyright (c) 2024 sigma-axis

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#pragma once

#include <bit>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace sigma_lib::W32::GDI
{
	// ダブルバファリング用の HDC ラッパー．
	class BufferedDC {
		HWND const owner;
		HDC const front_dc;
		RECT const rect;
		HDC back_dc;
		HGDIOBJ bmp;

	public:
		BufferedDC(HWND hwnd, int width, int height, bool use_back = true)
			: owner{ hwnd }, front_dc{ ::GetDC(hwnd) }, rect{ 0, 0, width, height }
		{
			if (use_back) {
				back_dc = ::CreateCompatibleDC(front_dc);
				bmp = ::SelectObject(back_dc, ::CreateCompatibleBitmap(front_dc, width, height));
			}
			else {
				back_dc = nullptr;
				bmp = nullptr;
			}
		}
		BufferedDC(HWND hwnd, SIZE const& sz, bool use_back) : BufferedDC(hwnd, sz.cx, sz.cy, use_back) {}
		BufferedDC(HWND hwnd, bool use_back = true) : BufferedDC{ hwnd, client_size(hwnd), use_back } {}

		~BufferedDC()
		{
			if (is_wrapped()) {
				::BitBlt(front_dc, 0, 0, rect.right, rect.bottom, back_dc, 0, 0, SRCCOPY);
				::DeleteObject(::SelectObject(back_dc, bmp));
				::DeleteDC(back_dc);
			}

			::ReleaseDC(owner, front_dc);
		}
		BufferedDC(const BufferedDC&) = delete;
		BufferedDC(BufferedDC&&) = delete;

		HWND hwnd() const { return owner; }
		constexpr HDC hdc() const { return is_wrapped() ? back_dc : front_dc; }
		// width of the client.
		constexpr int wd() const { return rect.right; }
		// height of the client.
		constexpr int ht() const { return rect.bottom; }

		// returns the reference to the client rect.
		constexpr RECT const& rc() const { return rect; }
		constexpr SIZE const& sz() const { return std::bit_cast<SIZE const*>(&rect)[1]; }
		constexpr bool is_wrapped() const { return back_dc != nullptr; }

		static SIZE client_size(HWND hwnd) {
			RECT rc; ::GetClientRect(hwnd, &rc);
			return { rc.right, rc.bottom };
		}
	};
}