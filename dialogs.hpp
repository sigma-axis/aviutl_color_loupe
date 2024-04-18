/*
The MIT License (MIT)

Copyright (c) 2024 sigma-axis

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#pragma once

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "settings.hpp"

namespace dialogs
{
	// opens a dialog for settings of this app.
	bool open_settings(HWND parent);

	// external functions that are helpful for the dialog functionality.
	namespace ExtFunc
	{
		using namespace sigma_lib::W32::GDI;

		std::tuple<int, int> ScaleFromZoomLevel(int zoom_level);
		void DrawTip(HDC hdc, const SIZE& canvas, const RECT& box,
			Color pixel_color, const POINT& pix, const SIZE& screen, bool& prefer_above,
			const Settings::TipDrag& tip_drag, const Settings::ColorScheme& color_scheme);
		void DrawToast(HDC hdc, const SIZE& canvas, const wchar_t* message,
			const Settings::Toast& toast, const Settings::ColorScheme& color_scheme);

	}
}
