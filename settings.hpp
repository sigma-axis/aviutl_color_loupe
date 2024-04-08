/*
The MIT License (MIT)

Copyright (c) 2024 sigma-axis

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#pragma once

#include <algorithm>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "color_abgr.hpp"
#include "drag_states.hpp"


////////////////////////////////
// 設定項目．
////////////////////////////////
inline constinit struct Settings {
	using Color = sigma_lib::W32::GDI::Color;
	using RailMode = sigma_lib::W32::custom::mouse::RailMode;

	enum class MouseButton : uint8_t {
		none = 0,
		left = 1,
		right = 2,
		middle = 3,
		x1 = 4,
		x2 = 5,
	};
	enum class ZoomPivot : uint8_t {
		center = 0, cursor = 1,
	};
	enum class TipMode : uint8_t {
		// TODO: remove `none`.
		none = 0, frail = 1, stationary = 2, sticky = 3,
	};
	enum class ToastPlacement : uint8_t {
		center = 0, left_top = 1, right_top = 2, right_bottom = 3, left_bottom = 4,
	};
	enum class ToastScaleFormat : uint8_t {
		fraction = 0, decimal = 1, percent = 2,
	};
	enum class CommonCommand : uint8_t {
		none = 0,
		centralize = 1,
		toggle_follow_cursor = 2,
		swap_scale_level = 3,
		copy_color_code = 4,
		context_menu = 5,
		toggle_grid = 6,
	};

	struct KeysActivate {
		enum Requirement : int8_t {
			off = 0,
			on = 1,
			dontcare = -1,
		};
		MouseButton button;
		Requirement ctrl, shift, alt;

		bool match(MouseButton button, bool ctrl, bool shift, bool alt) const {
			constexpr auto req = [](Requirement r, bool k) {
				switch (r) {
				case off: return !k;
				case on: return k;
				case dontcare:
				default:
					return true;
				}
			};
			return this->button == button
				&& req(this->ctrl, ctrl)
				&& req(this->shift, shift)
				&& req(this->alt, alt);
		}
	};

	struct ZoomBehavior {
		// TODO: let ZoomPivot be contained in ZoomBehavior structure.
		bool enabled, reverse_wheel;
		ZoomPivot pivot;
	};

	struct {
		bool lattice = false;
		RailMode rail_mode = RailMode::cross;
	} positioning;

	struct {
		bool reverse_wheel = false;
		ZoomPivot pivot = ZoomPivot::center,
			pivot_drag = ZoomPivot::cursor,
			pivot_swap = ZoomPivot::cursor;
	} zoom;

	struct {
		Color
			// Light theme
			chrome = { 0x76,0x76,0x76 },
			back_top = { 0xff,0xff,0xff },
			back_bottom = { 0xe4,0xec,0xf7 },
			text = { 0x3b,0x3d,0x3f },
			blank = { 0xf0,0xf0,0xf0 };

		// Dark theme
	//	chrome		= { 0x76,0x76,0x76 },
	//	back_top	= { 0x2b,0x2b,0x2b },
	//	back_bottom	= { 0x2b,0x2b,0x2b },
	//	text		= { 0xff,0xff,0xff },
	//	blank		= { 0x33,0x33,0x33 };
	} color;

	struct {
		TipMode mode = TipMode::frail;
		RailMode rail_mode = RailMode::cross;

		wchar_t font_name[LF_FACESIZE]{ L"Consolas" };
		int8_t font_size = 16;

		int8_t box_inflate = 4;
		int8_t chrome_thick = 1;
		int8_t chrome_radius = 3;

		constexpr static int8_t min_font_size = 4, max_font_size = 72;
		constexpr static int8_t min_box_inflate = 0, max_box_inflate = 16;
		constexpr static int8_t min_chrome_thick = 0, max_chrome_thick = 16;
		constexpr static int8_t min_chrome_radius = 0, max_chrome_radius = 32;
	} tip{};

	struct {
		bool notify_scale = true;
		bool notify_follow_cursor = true;
		bool notify_grid = true;
		bool notify_clipboard = true;

		ToastPlacement placement = ToastPlacement::right_bottom;
		ToastScaleFormat scale_format = ToastScaleFormat::decimal;

		int duration = 3000;

		wchar_t font_name[LF_FACESIZE]{ L"Yu Gothic UI" };
		int8_t font_size = 18;

		int8_t chrome_thick = 1;
		int8_t chrome_radius = 3;

		constexpr static int min_duration = 200, max_duration = 60'000;
		constexpr static int8_t min_font_size = 4, max_font_size = 72;
		constexpr static int8_t min_chrome_thick = 0, max_chrome_thick = 16;
		constexpr static int8_t min_chrome_radius = 0, max_chrome_radius = 32;
	} toast{};

	struct {
		int8_t least_scale_thin = 8;
		int8_t least_scale_thick = 12;
		constexpr static int8_t min_least_scale_thin = 6, max_least_scale_thin = 21;
		constexpr static int8_t min_least_scale_thick = 6, max_least_scale_thick = 21;

		// 0: no grid, 1: thin grid, 2: thick grid.
		uint8_t grid_thick(int scale_level) const {
			if (scale_level < least_scale_thin) return 0;
			if (scale_level < least_scale_thick) return 1;
			return 2;
		}
	} grid;

	struct {
		CommonCommand
			left_dblclk = CommonCommand::swap_scale_level,
			right_dblclk = CommonCommand::copy_color_code,
			middle_click = CommonCommand::toggle_follow_cursor;
	} commands;

	// loading from .ini file.
	void load(const char* ini_file)
	{
		auto read_raw = [&](auto def, const char* section, const char* key) {
			return static_cast<decltype(def)>(
				::GetPrivateProfileIntA(section, key, static_cast<int32_t>(def), ini_file));
		};
#define load_gen(section, tgt, read, write)	section.tgt = read(read_raw(write(section.tgt), #section, #tgt))
#define load_int(section, tgt)		load_gen(section, tgt, \
			[](auto y) { return std::clamp(y, decltype(section)::min_##tgt, decltype(section)::max_##tgt); }, /*id*/)
#define load_bool(section, tgt)		load_gen(section, tgt, \
			[](auto y) { return y != 0; }, [](auto x) { return x ? 1 : 0; })
#define load_enum(section, tgt)		load_gen(section, tgt, \
			[](auto y) { return static_cast<decltype(section.tgt)>(y); }, [](auto x) { return static_cast<int>(x); })
#define load_color(section, tgt)	load_gen(section, tgt, \
			[](auto y) { return Color::fromARGB(y); }, [](auto x) { return x.to_formattable(); })

		load_bool(positioning, lattice);
		load_enum(positioning, rail_mode);

		load_bool(zoom, reverse_wheel);
		load_enum(zoom, pivot);
		load_enum(zoom, pivot_drag);
		load_enum(zoom, pivot_swap);

		load_color(color, chrome);
		load_color(color, back_top);
		load_color(color, back_bottom);
		load_color(color, text);
		load_color(color, blank);

		load_enum(tip, mode);
		load_enum(tip, rail_mode);
		load_int(tip, font_size);
		load_int(tip, box_inflate);
		load_int(tip, chrome_thick);
		load_int(tip, chrome_radius);

		load_bool(toast, notify_scale);
		load_bool(toast, notify_follow_cursor);
		load_bool(toast, notify_grid);
		load_bool(toast, notify_clipboard);
		load_enum(toast, placement);
		load_enum(toast, scale_format);
		load_int(toast, duration);
		load_int(toast, font_size);
		load_int(toast, chrome_thick);
		load_int(toast, chrome_radius);

		{
			char buf_ansi[3 * std::extent_v<decltype(tip.font_name)>];
			if (::GetPrivateProfileStringA("tip", "font_name", "", buf_ansi, std::size(buf_ansi), ini_file) > 0)
				::MultiByteToWideChar(CP_UTF8, 0, buf_ansi, -1, tip.font_name, std::size(tip.font_name));

			if (::GetPrivateProfileStringA("toast", "font_name", "", buf_ansi, std::size(buf_ansi), ini_file) > 0)
				::MultiByteToWideChar(CP_UTF8, 0, buf_ansi, -1, toast.font_name, std::size(toast.font_name));
		}

		load_int(grid, least_scale_thin);
		load_int(grid, least_scale_thick);

		load_enum(commands, left_dblclk);
		load_enum(commands, right_dblclk);
		load_enum(commands, middle_click);

#undef load_color
#undef load_enum
#undef load_bool
#undef load_int
#undef load_gen
	}

	// saving to .ini file.
	void save(const char* ini_file)
	{
		auto save_raw = [&](int32_t val, const char* section, const char* key, bool hex = false) {
			char buf[std::size("+4294967296")];
			std::snprintf(buf, std::size(buf), hex ? "0x%08x" : "%d", val);
			::WritePrivateProfileStringA(section, key, buf, ini_file);
		};

#define save_gen(section, tgt, write, hex)	save_raw(static_cast<int32_t>(write(section.tgt)), #section, #tgt, hex)
#define save_dec(section, tgt)		save_gen(section, tgt, /* id */, false)
#define save_color(section, tgt)	save_gen(section, tgt, [](auto y) { return y.formattable(); }, true)
#define save_bool(section, tgt)		::WritePrivateProfileStringA(#section, #tgt, section.tgt ? "1" : "0", ini_file)

		save_dec(positioning, lattice);
		save_dec(positioning, rail_mode);

		save_bool(zoom, reverse_wheel);
		save_dec(zoom, pivot);
		save_dec(zoom, pivot_drag);
		save_dec(zoom, pivot_swap);

		//save_color(color, chrome);
		//save_color(color, back_top);
		//save_color(color, back_bottom);
		//save_color(color, text);
		//save_color(color, blank);

		save_dec(tip, mode);
		save_dec(tip, rail_mode);
		//save_dec(tip, font_size);
		//save_dec(tip, box_inflate);
		//save_dec(tip, chrome_thick);
		//save_dec(tip, chrome_radius);

		save_bool(toast, notify_scale);
		save_bool(toast, notify_follow_cursor);
		save_bool(toast, notify_grid);
		save_bool(toast, notify_clipboard);
		save_dec(toast, placement);
		save_dec(toast, scale_format);
		//save_dec(toast, duration);
		//save_dec(toast, font_size);
		//save_dec(toast, chrome_thick);
		//save_dec(toast, chrome_radius);

		//{
		//	char buf_ansi[3 * std::extent_v<decltype(tip.font_name)>];
		//	::WideCharToMultiByte(CP_UTF8, 0, tip.font_name, -1, buf_ansi, std::size(buf_ansi), nullptr, nullptr);
		//	::WriteProfileStringA("tip", "font_name", buf_ansi);

		//	::WideCharToMultiByte(CP_UTF8, 0, toast.font_name, -1, buf_ansi, std::size(buf_ansi), nullptr, nullptr);
		//	::WriteProfileStringA("toast", "font_name", buf_ansi);
		//}

		//save_dec(grid, least_scale_thin);
		//save_dec(grid, least_scale_thick);

		save_dec(commands, left_dblclk);
		save_dec(commands, right_dblclk);
		save_dec(commands, middle_click);

		// lines commented out are setting items that threre're no means to change at runtime.

#undef save_bool
#undef save_color
#undef save_dec
#undef save_gen
	}
} settings;
