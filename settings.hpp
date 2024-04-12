﻿/*
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
	using MouseButton = sigma_lib::W32::custom::mouse::MouseButton;
	using RailMode = sigma_lib::W32::custom::mouse::RailMode;
	using DragInvalidRange = sigma_lib::W32::custom::mouse::DragInvalidRange;

	struct KeysActivate {
		enum State : int8_t {
			off	= 0,
			on	= 1,
			dontcare = -1,
		};
		MouseButton button;
		State ctrl, shift, alt;

		bool match(MouseButton button, bool ctrl, bool shift, bool alt) const {
			constexpr auto req = [](State r, bool k) {
				switch (r) {
				case off: return !k;
				case on: return k;
				case dontcare:
				default:
					return true;
				}
			};
			return button != MouseButton::none
				&& this->button == button
				&& req(this->ctrl,	ctrl)
				&& req(this->shift,	shift)
				&& req(this->alt,	alt);
		}
	};

	struct WheelZoom {
		enum Pivot : uint8_t { center = 0, cursor = 1, };

		bool enabled, reverse_wheel;
		Pivot pivot;

		// TODO: add a field "zoom speed".
	};

	struct LoupeDrag {
		KeysActivate keys{ MouseButton::left, KeysActivate::dontcare, KeysActivate::dontcare, KeysActivate::off };
		DragInvalidRange range{ .distance = 2, .timespan = 800 };
		WheelZoom wheel{ true, false, WheelZoom::cursor };

		bool lattice = false;
		RailMode rail_mode = RailMode::cross;
	} loupe_drag;

	struct TipDrag {
		KeysActivate keys{ MouseButton::left, KeysActivate::off, KeysActivate::dontcare, KeysActivate::dontcare };
		DragInvalidRange range = DragInvalidRange::AlwaysValid();
		WheelZoom wheel{ true, false, WheelZoom::cursor };

		enum Mode : uint8_t {
			frail = 0, stationary = 1, sticky = 2,
		};
		Mode mode = frail;
		RailMode rail_mode = RailMode::cross;

		wchar_t font_name[LF_FACESIZE]{ L"Consolas" };
		int8_t font_size		= 16;

		int8_t box_inflate		= 4;
		int8_t chrome_thick		= 1;
		int8_t chrome_radius	= 3;

		constexpr static int8_t font_size_min		= 4, font_size_max		= 72;
		constexpr static int8_t box_inflate_min		= 0, box_inflate_max	= 16;
		constexpr static int8_t chrome_thick_min	= 0, chrome_thick_max	= 16;
		constexpr static int8_t chrome_radius_min	= 0, chrome_radius_max	= 32;
	} tip_drag{};

	struct ExEditDrag {
		KeysActivate keys{ MouseButton::left, KeysActivate::on, KeysActivate::dontcare, KeysActivate::dontcare };
		DragInvalidRange range{ .distance = 2, .timespan = 800 };
		WheelZoom wheel{ true, false, WheelZoom::cursor };

		enum KeyDisguise : uint8_t {
			flat	= 0,
			off		= 1,
			on		= 2,
			invert	= 3,
		};
		KeyDisguise shift = flat, alt = off; // no ctrl key; it's kind of special.
	} exedit_drag;

	struct ZoomBehavior {
		WheelZoom wheel{
			.enabled = true,
			.reverse_wheel = false,
			.pivot = WheelZoom::center,
		};
		int8_t min_scale_level = min_scale_level_min,
			max_scale_level = max_scale_level_max;

		constexpr static int8_t min_scale_level_min = -13, min_scale_level_max = 20;
		constexpr static int8_t max_scale_level_min = -13, max_scale_level_max = 20;
	} zoom;

	struct ColorScheme {
		Color
			// Light theme
			chrome		= { 0x76,0x76,0x76 },
			back_top	= { 0xff,0xff,0xff },
			back_bottom	= { 0xe4,0xec,0xf7 },
			text		= { 0x3b,0x3d,0x3f },
			blank		= { 0xf0,0xf0,0xf0 };

			// Dark theme
		//	chrome		= { 0x76,0x76,0x76 },
		//	back_top	= { 0x2b,0x2b,0x2b },
		//	back_bottom	= { 0x2b,0x2b,0x2b },
		//	text		= { 0xff,0xff,0xff },
		//	blank		= { 0x33,0x33,0x33 };
	} color;

	struct Toast {
		bool notify_scale = true;
		bool notify_follow_cursor = true;
		bool notify_grid = true;
		bool notify_clipboard = true;

		enum class Placement : uint8_t {
			top_left = 0, top = 1, top_right = 2,
			left = 3, center = 4, right = 5,
			bottom_left = 6, bottom = 7, bottom_right = 8,
		};
		Placement placement = Placement::bottom_right;

		enum class ScaleFormat : uint8_t {
			fraction = 0, decimal = 1, percent = 2,
		};
		ScaleFormat scale_format = ScaleFormat::decimal;

		int duration = 3000;

		wchar_t font_name[LF_FACESIZE]{ L"Yu Gothic UI" };
		int8_t font_size = 18;

		int8_t chrome_thick = 1;
		int8_t chrome_radius = 3;

		constexpr static int	duration_min		= 200,	duration_max		= 60'000;
		constexpr static int8_t	font_size_min		= 4,	font_size_max		= 72;
		constexpr static int8_t	chrome_thick_min	= 0,	chrome_thick_max	= 16;
		constexpr static int8_t	chrome_radius_min	= 0,	chrome_radius_max	= 32;
	} toast{};

	struct Grid {
		int8_t least_scale_thin = 8;
		int8_t least_scale_thick = 12;
		constexpr static int8_t
			least_scale_thin_min	= 6, // x 3.00
			least_scale_thin_max	= ZoomBehavior::max_scale_level_max + 1,
			least_scale_thick_min	= least_scale_thin_min,
			least_scale_thick_max	= least_scale_thin_max;

		// 0: no grid, 1: thin grid, 2: thick grid.
		uint8_t grid_thick(int scale_level) const {
			if (scale_level < least_scale_thin) return 0;
			if (scale_level < least_scale_thick) return 1;
			return 2;
		}
	} grid;

	struct ClickActions {
		enum Command : uint8_t {
			none = 0,
			centralize = 1,
			toggle_follow_cursor = 2,
			swap_scale_level = 3,
			copy_color_code = 4,
			context_menu = 5,
			toggle_grid = 6,
			settings = 7,
			// TODO: add the following new commands.
			// scale_step_up, scale_step_down
			// copy_coord_tl (copy coordinate relative to the top-left of the image)
			// copy_coord_c (copy coordinate relative to the center of the image)
			// centralize_2 (move the clicked point to the center of the loupe)
		};
		Command
			left_click		= none,
			right_click		= context_menu,
			middle_click	= toggle_follow_cursor,
			x1_click		= none,
			x2_click		= none,

			left_dblclk		= swap_scale_level,
			right_dblclk	= copy_color_code,
			middle_dblclk	= none,
			x1_dblclk		= none,
			x2_dblclk		= none;

		WheelZoom::Pivot swap_scale_level_pivot = WheelZoom::cursor;
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
			[&](auto y) { return std::clamp(y, section.tgt##_min, section.tgt##_max); }, /*id*/)
#define load_bool(section, tgt)		load_gen(section, tgt, \
			[](auto y) { return y != 0; }, [](auto x) { return x ? 1 : 0; })
#define load_enum(section, tgt)		load_gen(section, tgt, \
			[](auto y) { return static_cast<decltype(section.tgt)>(y); }, [](auto x) { return static_cast<int>(x); })
#define load_color(section, tgt)	load_gen(section, tgt, \
			[](auto y) { return Color::fromARGB(y); }, [](auto x) { return x.to_formattable(); })

#define load_drag(section)	\
		load_enum(section, keys.button);\
		load_enum(section, keys.ctrl);\
		load_enum(section, keys.shift);\
		load_enum(section, keys.alt);\
		load_int(section, range.distance); \
		load_int(section, range.timespan);\
		load_bool(section, wheel.enabled);\
		load_bool(section, wheel.reverse_wheel);\
		load_enum(section, wheel.pivot)

		load_drag(loupe_drag);
		load_bool(loupe_drag, lattice);
		load_enum(loupe_drag, rail_mode);

		load_drag(tip_drag);
		load_enum(tip_drag, mode);
		load_enum(tip_drag, rail_mode);
		load_int(tip_drag, font_size);
		load_int(tip_drag, box_inflate);
		load_int(tip_drag, chrome_thick);
		load_int(tip_drag, chrome_radius);

		load_drag(exedit_drag);
		load_enum(exedit_drag, shift);
		load_enum(exedit_drag, alt);

		load_bool(zoom, wheel.enabled);
		load_bool(zoom, wheel.reverse_wheel);
		load_enum(zoom, wheel.pivot);
		load_int(zoom, min_scale_level);
		load_int(zoom, max_scale_level);

		load_color(color, chrome);
		load_color(color, back_top);
		load_color(color, back_bottom);
		load_color(color, text);
		load_color(color, blank);

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
			char buf_ansi[3 * std::extent_v<decltype(tip_drag.font_name)>];
			if (::GetPrivateProfileStringA("tip", "font_name", "", buf_ansi, std::size(buf_ansi), ini_file) > 0)
				::MultiByteToWideChar(CP_UTF8, 0, buf_ansi, -1, tip_drag.font_name, std::size(tip_drag.font_name));

			if (::GetPrivateProfileStringA("toast", "font_name", "", buf_ansi, std::size(buf_ansi), ini_file) > 0)
				::MultiByteToWideChar(CP_UTF8, 0, buf_ansi, -1, toast.font_name, std::size(toast.font_name));
		}

		load_int(grid, least_scale_thin);
		load_int(grid, least_scale_thick);

		load_enum(commands, left_click);
		load_enum(commands, right_click);
		load_enum(commands, middle_click);
		load_enum(commands, x1_click);
		load_enum(commands, x2_click);
		load_enum(commands, left_dblclk);
		load_enum(commands, right_dblclk);
		load_enum(commands, middle_dblclk);
		load_enum(commands, x1_dblclk);
		load_enum(commands, x2_dblclk);

		load_enum(commands, swap_scale_level_pivot);

#undef load_drag
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
#define save_drag(section)	\
		save_dec(section, keys.button);\
		save_dec(section, keys.ctrl);\
		save_dec(section, keys.shift);\
		save_dec(section, keys.alt);\
		save_dec(section, range.distance); \
		save_dec(section, range.timespan);\
		save_bool(section, wheel.enabled);\
		save_bool(section, wheel.reverse_wheel);\
		save_dec(section, wheel.pivot)

		save_drag(loupe_drag);
		save_dec(loupe_drag, lattice);
		save_dec(loupe_drag, rail_mode);

		save_drag(tip_drag);
		save_dec(tip_drag, mode);
		save_dec(tip_drag, rail_mode);
		//save_dec(tip_drag, font_size);
		//save_dec(tip_drag, box_inflate);
		//save_dec(tip_drag, chrome_thick);
		//save_dec(tip_drag, chrome_radius);

		save_drag(exedit_drag);
		save_dec(exedit_drag, shift);
		save_dec(exedit_drag, alt);

		save_bool(zoom, wheel.enabled);
		save_bool(zoom, wheel.reverse_wheel);
		save_dec(zoom, wheel.pivot);
		save_dec(zoom, min_scale_level);
		save_dec(zoom, max_scale_level);

		//save_color(color, chrome);
		//save_color(color, back_top);
		//save_color(color, back_bottom);
		//save_color(color, text);
		//save_color(color, blank);

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

		save_dec(commands, left_click);
		save_dec(commands, right_click);
		save_dec(commands, middle_click);
		save_dec(commands, x1_click);
		save_dec(commands, x2_click);
		save_dec(commands, left_dblclk);
		save_dec(commands, right_dblclk);
		save_dec(commands, middle_dblclk);
		save_dec(commands, x1_dblclk);
		save_dec(commands, x2_dblclk);

		save_dec(commands, swap_scale_level_pivot);

		// lines commented out are setting items that threre're no means to change at runtime.

#undef save_bool
#undef save_color
#undef save_dec
#undef save_gen
	}

	struct HelperFunctions {
		static std::tuple<int, int> ZoomScaleFromLevel(int scale_level);
	};
} settings;
