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
	using MouseButton = sigma_lib::W32::custom::mouse::MouseButton;
	using RailMode = sigma_lib::W32::custom::mouse::RailMode;
	using DragInvalidRange = sigma_lib::W32::custom::mouse::DragInvalidRange;

	enum class ColorFormat : uint8_t {
		hexdec6 = 0, dec3x3 = 1,
	};
	enum class CoordFormat : uint8_t {
		origin_top_left = 0, origin_center = 1,
	};
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

		bool enabled, reversed;
		uint8_t num_steps;
		Pivot pivot;

		constexpr static uint8_t num_steps_min = 1, num_steps_max = 16;
	};

	struct LoupeDrag {
		KeysActivate keys{ MouseButton::left, KeysActivate::off, KeysActivate::dontcare, KeysActivate::dontcare };
		DragInvalidRange range = DragInvalidRange::AlwaysValid();
		WheelZoom wheel{ true, false, 1, WheelZoom::cursor };

		bool lattice = false;
		RailMode rail_mode = RailMode::cross;
	} loupe_drag;

	struct TipDrag {
		KeysActivate keys{ MouseButton::right, KeysActivate::off, KeysActivate::dontcare, KeysActivate::dontcare };
		DragInvalidRange range = DragInvalidRange::AlwaysValid();
		WheelZoom wheel{ true, false, 1, WheelZoom::cursor };

		enum Mode : uint8_t {
			frail = 0, stationary = 1, sticky = 2,
		};
		Mode mode = frail;
		RailMode rail_mode = RailMode::cross;

		ColorFormat color_fmt = ColorFormat::hexdec6;
		CoordFormat coord_fmt = CoordFormat::origin_top_left;

		wchar_t font_name[LF_FACESIZE]{ L"Consolas" };
		int8_t font_size		= 16;

		int8_t box_inflate		= 4;
		int8_t box_tip_gap		= 10;
		int8_t chrome_thick		= 1;
		int8_t chrome_corner	= 5; // in diameter of the circle.
		int8_t chrome_margin_h	= 4;
		int8_t chrome_margin_v	= 4;
		int8_t chrome_pad_h		= 10;
		int8_t chrome_pad_v		= 3;

		constexpr static int8_t font_size_min		= 4,	font_size_max		= 72;
		constexpr static int8_t box_inflate_min		= 0,	box_inflate_max		= 16;
		constexpr static int8_t box_tip_gap_min		= -64,	box_tip_gap_max		= 64;
		constexpr static int8_t chrome_thick_min	= 0,	chrome_thick_max	= 16;
		constexpr static int8_t chrome_corner_min	= 0,	chrome_corner_max	= 32;
		constexpr static int8_t chrome_margin_h_min	= -16,	chrome_margin_h_max	= 100;
		constexpr static int8_t chrome_margin_v_min	= -16,	chrome_margin_v_max	= 100;
		constexpr static int8_t chrome_pad_h_min	= -16,	chrome_pad_h_max	= 32;
		constexpr static int8_t chrome_pad_v_min	= -16,	chrome_pad_v_max	= 32;
	} tip_drag{};

	struct ExEditDrag {
		KeysActivate keys{ MouseButton::left, KeysActivate::on, KeysActivate::dontcare, KeysActivate::dontcare };
		DragInvalidRange range = DragInvalidRange::AlwaysValid();
		WheelZoom wheel{ true, false, 1, WheelZoom::cursor };

		enum KeyFake : uint8_t {
			flat	= 0,
			off		= 1,
			on		= 2,
			invert	= 3,
		};
		KeyFake fake_shift = flat, fake_alt = flat; // no ctrl key; it's kind of special.
	} exedit_drag;

	struct Zoom {
		WheelZoom wheel{
			.enabled = true,
			.reversed = false,
			.num_steps = 1,
			.pivot = WheelZoom::center,
		};
		int8_t level_min = level_min_min,
			level_max = level_max_max;
		constexpr static int8_t level_min_min = -13, level_min_max = 20;
		constexpr static int8_t level_max_min = -13, level_max_max = 20;
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

		int8_t chrome_thick		= 1;
		int8_t chrome_corner	= 5; // in diameter of the circle.
		int8_t chrome_margin_h	= 4;
		int8_t chrome_margin_v	= 4;
		int8_t chrome_pad_h		= 6;
		int8_t chrome_pad_v		= 4;

		constexpr static int	duration_min		= 200,	duration_max		= 60'000;
		constexpr static int8_t	font_size_min		= 4,	font_size_max		= 72;
		constexpr static int8_t	chrome_thick_min	= 0,	chrome_thick_max	= 16;
		constexpr static int8_t	chrome_corner_min	= 0,	chrome_corner_max	= 32;
		constexpr static int8_t chrome_margin_h_min	= -16,	chrome_margin_h_max	= 100;
		constexpr static int8_t chrome_margin_v_min	= -16,	chrome_margin_v_max	= 100;
		constexpr static int8_t chrome_pad_h_min	= -16,	chrome_pad_h_max	= 32;
		constexpr static int8_t chrome_pad_v_min	= -16,	chrome_pad_v_max	= 32;
	} toast{};

	struct Grid {
		int8_t least_zoom_thin = 8;
		int8_t least_zoom_thick = 12;
		constexpr static int8_t
			least_zoom_thin_min	= 6, // x 3.00
			least_zoom_thin_max	= Zoom::level_max_max + 1,
			least_zoom_thick_min	= least_zoom_thin_min,
			least_zoom_thick_max	= least_zoom_thin_max;

		// 0: no grid, 1: thin grid, 2: thick grid.
		uint8_t grid_thick(int zoom_level) const {
			if (zoom_level < least_zoom_thin) return 0;
			if (zoom_level < least_zoom_thick) return 1;
			return 2;
		}
	} grid;

	struct ClickActions {
		enum Command : uint8_t {
			none = 0,
			swap_zoom_level			= 1,
			copy_color_code			= 2,
			copy_coord				= 3,
			toggle_follow_cursor	= 4,
			centralize				= 5,
			toggle_grid				= 6,
			zoom_step_down			= 7,
			zoom_step_up			= 8,
			bring_center			= 9,
			settings				= 201,
			context_menu			= 202,
		};
		struct Button {
			Command click, dblclk;
			bool cancels_drag;
		}	left{ none, swap_zoom_level, true },
			right{ context_menu, copy_color_code, true },
			middle{ toggle_follow_cursor, none, true },
			x1{ none, none, true },
			x2{ none,none, true };

		WheelZoom::Pivot swap_zoom_level_pivot = WheelZoom::cursor;

		WheelZoom::Pivot step_zoom_pivot = WheelZoom::cursor;
		uint8_t step_zoom_num_steps = 1;
		constexpr static uint8_t
			step_zoom_num_steps_min = WheelZoom::num_steps_min,
			step_zoom_num_steps_max = WheelZoom::num_steps_max;

		ColorFormat copy_color_fmt = ColorFormat::hexdec6;
		CoordFormat copy_coord_fmt = CoordFormat::origin_top_left;
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
		load_bool(section, wheel.reversed);\
		load_bool(section, wheel.num_steps);\
		load_enum(section, wheel.pivot)

		load_drag(loupe_drag);
		load_bool(loupe_drag, lattice);
		load_enum(loupe_drag, rail_mode);

		load_drag(tip_drag);
		load_enum(tip_drag, mode);
		load_enum(tip_drag, rail_mode);
		load_enum(tip_drag, color_fmt);
		load_enum(tip_drag, coord_fmt);
		load_int(tip_drag, font_size);
		load_int(tip_drag, box_inflate);
		load_int(tip_drag, box_tip_gap);
		load_int(tip_drag, chrome_thick);
		load_int(tip_drag, chrome_corner);
		load_int(tip_drag, chrome_margin_h);
		load_int(tip_drag, chrome_margin_v);
		load_int(tip_drag, chrome_pad_h);
		load_int(tip_drag, chrome_pad_v);

		load_drag(exedit_drag);
		load_enum(exedit_drag, fake_shift);
		load_enum(exedit_drag, fake_alt);

		load_bool(zoom, wheel.enabled);
		load_bool(zoom, wheel.reversed);
		load_enum(zoom, wheel.pivot);
		load_int(zoom, level_min);
		load_int(zoom, level_max);

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
		load_int(toast, chrome_corner);
		load_int(toast, chrome_margin_h);
		load_int(toast, chrome_margin_v);
		load_int(toast, chrome_pad_h);
		load_int(toast, chrome_pad_v);

		{
			char buf_ansi[3 * std::extent_v<decltype(tip_drag.font_name)>];
			if (::GetPrivateProfileStringA("tip", "font_name", "", buf_ansi, std::size(buf_ansi), ini_file) > 0)
				::MultiByteToWideChar(CP_UTF8, 0, buf_ansi, -1, tip_drag.font_name, std::size(tip_drag.font_name));

			if (::GetPrivateProfileStringA("toast", "font_name", "", buf_ansi, std::size(buf_ansi), ini_file) > 0)
				::MultiByteToWideChar(CP_UTF8, 0, buf_ansi, -1, toast.font_name, std::size(toast.font_name));
		}

		load_int(grid, least_zoom_thin);
		load_int(grid, least_zoom_thick);

		load_enum(commands, left.click);
		load_enum(commands, left.dblclk);
		load_bool(commands, left.cancels_drag);
		load_enum(commands, right.click);
		load_enum(commands, right.dblclk);
		load_bool(commands, right.cancels_drag);
		load_enum(commands, middle.click);
		load_enum(commands, middle.dblclk);
		load_bool(commands, middle.cancels_drag);
		load_enum(commands, x1.click);
		load_enum(commands, x1.dblclk);
		load_bool(commands, x1.cancels_drag);
		load_enum(commands, x2.click);
		load_enum(commands, x2.dblclk);
		load_bool(commands, x2.cancels_drag);

		load_enum(commands, swap_zoom_level_pivot);
		load_enum(commands, step_zoom_pivot);
		load_int(commands, step_zoom_num_steps);
		load_enum(commands, copy_color_fmt);
		load_enum(commands, copy_coord_fmt);

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
		save_bool(section, wheel.reversed);\
		save_bool(section, wheel.num_steps);\
		save_dec(section, wheel.pivot)

		save_drag(loupe_drag);
		save_dec(loupe_drag, lattice);
		save_dec(loupe_drag, rail_mode);

		save_drag(tip_drag);
		save_dec(tip_drag, mode);
		save_dec(tip_drag, rail_mode);
		save_dec(tip_drag, color_fmt);
		save_dec(tip_drag, coord_fmt);
		//save_dec(tip_drag, font_size);
		save_dec(tip_drag, box_inflate);
		save_dec(tip_drag, box_tip_gap);
		save_dec(tip_drag, chrome_thick);
		save_dec(tip_drag, chrome_corner);
		save_dec(tip_drag, chrome_margin_h);
		save_dec(tip_drag, chrome_margin_v);
		save_dec(tip_drag, chrome_pad_h);
		save_dec(tip_drag, chrome_pad_v);

		save_drag(exedit_drag);
		save_dec(exedit_drag, fake_shift);
		save_dec(exedit_drag, fake_alt);

		save_bool(zoom, wheel.enabled);
		save_bool(zoom, wheel.reversed);
		save_dec(zoom, wheel.pivot);
		save_dec(zoom, level_min);
		save_dec(zoom, level_max);

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
		save_dec(toast, duration);
		//save_dec(toast, font_size);
		//save_dec(toast, chrome_thick);
		//save_dec(toast, chrome_corner);
		//save_dec(toast, chrome_margin_h);
		//save_dec(toast, chrome_margin_v);
		//save_dec(toast, chrome_pad_h);
		//save_dec(toast, chrome_pad_v);

		//{
		//	char buf_ansi[3 * std::extent_v<decltype(tip.font_name)>];
		//	::WideCharToMultiByte(CP_UTF8, 0, tip.font_name, -1, buf_ansi, std::size(buf_ansi), nullptr, nullptr);
		//	::WriteProfileStringA("tip", "font_name", buf_ansi);

		//	::WideCharToMultiByte(CP_UTF8, 0, toast.font_name, -1, buf_ansi, std::size(buf_ansi), nullptr, nullptr);
		//	::WriteProfileStringA("toast", "font_name", buf_ansi);
		//}

		save_dec(grid, least_zoom_thin);
		save_dec(grid, least_zoom_thick);

		save_dec(commands, left.click);
		save_dec(commands, left.dblclk);
		save_bool(commands, left.cancels_drag);
		save_dec(commands, right.click);
		save_dec(commands, right.dblclk);
		save_bool(commands, right.cancels_drag);
		save_dec(commands, middle.click);
		save_dec(commands, middle.dblclk);
		save_bool(commands, middle.cancels_drag);
		save_dec(commands, x1.click);
		save_dec(commands, x1.dblclk);
		save_bool(commands, x1.cancels_drag);
		save_dec(commands, x2.click);
		save_dec(commands, x2.dblclk);
		save_bool(commands, x2.cancels_drag);

		save_dec(commands, swap_zoom_level_pivot);
		save_dec(commands, step_zoom_pivot);
		save_dec(commands, step_zoom_num_steps);
		save_dec(commands, copy_color_fmt);
		save_dec(commands, copy_coord_fmt);

		// lines commented out are setting items that threre're no means to change at runtime.

#undef save_drag
#undef save_bool
#undef save_color
#undef save_dec
#undef save_gen
	}
} settings;
