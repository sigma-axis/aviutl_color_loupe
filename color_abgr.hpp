/*
The MIT License (MIT)

Copyright (c) 2024 sigma-axis

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#pragma once

#include <cstdint>
#include <concepts>
#include <bit>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace sigma_lib::W32::GDI
{
	using byte = uint8_t;
	// Win32標準の COLORREF を拡張．
	union Color {
		COLORREF raw;
		struct { byte R, G, B, A; };

		constexpr Color() : raw{ 0 } {}
		template<std::convertible_to<COLORREF> TVal>
		constexpr Color(TVal c) : raw{ static_cast<COLORREF>(c) } {}
		template<std::convertible_to<byte> TVal>
		constexpr Color(TVal r, TVal g, TVal b) : R{ static_cast<byte>(r) }, G{ static_cast<byte>(g) }, B{ static_cast<byte>(b) }, A{ 0 } {}
		template<std::convertible_to<byte> TVal>
		constexpr Color(TVal r, TVal g, TVal b, TVal a) : R{ static_cast<byte>(r) }, G{ static_cast<byte>(g) }, B{ static_cast<byte>(b) }, A{ static_cast<byte>(a) } {}

		// conversion between COLORREF, avoiding accidental copies.
		static Color& from(COLORREF& c) { return reinterpret_cast<Color&>(c); }
		static const Color& from(const COLORREF& c) { return reinterpret_cast<const Color&>(c); }
		constexpr operator COLORREF& () { return raw; }
		constexpr operator const COLORREF& () const { return raw; }

		constexpr Color remove_alpha() const { return { raw & 0x00ffffff }; }
		// note: doesn't negate the alpha channel.
		constexpr Color negate() const { return { raw ^ 0x00ffffff }; }
		constexpr static Color interpolate(Color c1, Color c2, int wt1, int wt2)
		{
			auto i = [=](byte x1, byte x2) { return static_cast<byte>((wt1 * x1 + wt2 * x2) / (wt1 + wt2)); };
			return { i(c1.R, c2.R), i(c1.G, c2.G), i(c1.B, c2.B), i(c1.A, c2.A) };
		}
		constexpr Color interpolate_to(Color col2, int wt1, int wt2) const {
			return interpolate(*this, col2, wt1, wt2);
		}

		// compatibilities with #AARRGGBB format.
	private:
		static constexpr uint32_t swapRB(uint32_t c) { return (0xff00ff00 & c) | std::rotl(0x00ff00ff & c, 16); }
	public:
		constexpr uint32_t to_formattable() const { return swapRB(raw); }
		template<std::convertible_to<uint32_t> TVal>
		static constexpr Color fromARGB(TVal c) { return { swapRB(static_cast<uint32_t>(c)) }; }

		// brightness properties.

		// lightness of HLS coordinate; MAX+MIN.
		// values range from 0 to 510, inclusive.
		constexpr static short lightness(byte R, byte G, byte B)
		{
			const byte* x = &R, * y = &G, * z = &B;
			if (*x > *y) std::swap(x, y);
			if (*x > *z) std::swap(x, z);
			if (*y > *z) std::swap(y, z);
			return *x + *z;
		}
		constexpr short lightness() const { return lightness(R, G, B); }
		constexpr static short max_lightness = 510;

		// luma of Y'CrCb coordinate; weighted sum of R, G, B channels.
		// values range from 0 to 65535.
		static constexpr uint16_t luma(byte r, byte g, byte b) {
			return 77 * r + 151 * g + 29 * b;
		}
		constexpr uint16_t luma() const { return luma(R, G, B); }
		constexpr static uint16_t max_luma = 65535;
	};
	static_assert(sizeof(Color) == 4);


	// ダブルバファリング用の HDC ラッパー．
	class WrappedDC {
		HWND const owner;
		HDC const front_dc;
		RECT const rect;
		HDC back_dc;
		HGDIOBJ bmp;

	public:
		WrappedDC(HWND hwnd, int width, int height, bool use_back = true)
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
		WrappedDC(HWND hwnd, SIZE const& sz, bool use_back) : WrappedDC(hwnd, sz.cx, sz.cy, use_back) {}
		WrappedDC(HWND hwnd, bool use_back = true) : WrappedDC{ hwnd, client_size(hwnd), use_back } {}

		~WrappedDC()
		{
			if (is_wrapped()) {
				::BitBlt(front_dc, 0, 0, rect.right, rect.bottom, back_dc, 0, 0, SRCCOPY);
				::DeleteObject(::SelectObject(back_dc, bmp));
				::DeleteDC(back_dc);
			}

			::ReleaseDC(owner, front_dc);
		}
		WrappedDC(const WrappedDC&) = delete;
		WrappedDC(WrappedDC&&) = delete;

		HWND hwnd() const { return owner; }
		constexpr HDC hdc() const { return is_wrapped() ? back_dc : front_dc; }
		// width of the client.
		constexpr int wd() const { return rect.right; }
		// height of the client.
		constexpr int ht() const { return rect.bottom; }

		// returns the reference to the client rect.
		constexpr RECT const& rc() const { return rect; }
		constexpr bool is_wrapped() const { return back_dc != nullptr; }

		static SIZE client_size(HWND hwnd) {
			RECT rc; ::GetClientRect(hwnd, &rc);
			return { rc.right, rc.bottom };
		}
	};
}
