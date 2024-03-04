/*
The MIT License (MIT)

Copyright (c) 2024 sigma-axis

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <cmath>
#include <algorithm>
#include <bit>
#include <tuple>
#include <cwchar>
#include <concepts>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#pragma comment(lib, "imm32")
#include <CommCtrl.h>
#pragma comment(lib, "comctl32")

using byte = uint8_t;
#include <aviutl/filter.hpp>

using namespace AviUtl;

#include "resource.h"
HMODULE this_dll = nullptr;

////////////////////////////////
// Win32標準の COLORREF を拡張．
////////////////////////////////
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


////////////////////////////////
// ルーペ状態の定義
////////////////////////////////
inline constinit struct LoupeState {
	// position in the picture in pixels, relative to the top-left corner,
	// which is displayed at the center of the loupe window.
	struct {
		double x, y;
		bool follow_cursor = false;
		void clamp(int w, int h) {
			x = std::clamp<double>(x, 0, w);
			y = std::clamp<double>(y, 0, h);
		}
	} position{ 0,0 };

	// zoom --- manages the scaling ratio of zooming.
	struct {
		int scale_level, second_level;
		constexpr static int min_scale_level = -13, max_scale_level = 20;

	private:
		constexpr static int scale_denominator = 4;
		static constexpr int scale_base(int level) {
			return (4 + (level & 3)) << (level >> 2);
		}
	public:
		static constexpr double scale_ratio(int scale_level) { return scale_level >= 0 ? scale_level == 0 ? 1 : upscale(scale_level) : 1 / downscale(scale_level); }
		static constexpr double scale_ratio_inv(int scale_level) { return scale_level >= 0 ? scale_level == 0 ? 1 : 1 / upscale(scale_level) : downscale(scale_level); }
		static constexpr auto scale_ratios(int scale_level) {
			double s;
			return scale_level >= 0 ? scale_level == 0 ? std::make_pair(1.0, 1.0) :
				((s = upscale(scale_level)), std::make_pair(s, 1 / s)) :
				((s=downscale(scale_level)), std::make_pair(1 / s, s));
		}
		static constexpr auto scale_ratio_Q(int scale_level)
		{
			int n = scale_denominator, d = scale_denominator;
			if (scale_level < 0) d = scale_base(-scale_level);
			else n = scale_base(scale_level);
			// as of either the numerator or the denominator is a power of 2, so is the GCD.
			auto gcd = n | d; gcd &= -gcd; // = std::gcd(n, d);
			return std::make_pair(n / gcd, d / gcd);
		}
		static constexpr double upscale(int scale_level) { return (1.0 / scale_denominator) * scale_base(scale_level); }
		static constexpr double downscale(int scale_level) { return upscale(-scale_level); }

		constexpr double scale_ratio() const { return scale_ratio(scale_level); }
		constexpr double scale_ratio_inv() const { return scale_ratio_inv(scale_level); }
		constexpr auto scale_ratios() const { return scale_ratios(scale_level); }
		constexpr auto scale_ratio_Q() const { return scale_ratio_Q(scale_level); }
	} zoom{ 8, 0 };

	// tip --- tooltip-like info.
	struct {
		int x = 0, y = 0;
		uint8_t visible_level = 0; // 0: not visible, 1: not visible until mouse enters, >= 2: visible.
		constexpr bool is_visible() const { return visible_level > 1; }
		bool prefer_above = false;
	} tip_state;

	// toast --- notification message.
	struct {
		constexpr static int max_len_message = 32;
		wchar_t message[max_len_message]{ L"" };
		bool visible = false;

		auto timer_id() const { return  reinterpret_cast<UINT_PTR>(this); }
		template<class... TArgs>
		void set_message(HWND hwnd, int time_ms, UINT id, const TArgs&... args)
		{
			if constexpr (sizeof...(TArgs) > 0) {
				int_least64_t ptr;
				::LoadStringW(this_dll, id, reinterpret_cast<wchar_t*>(&ptr), 0);
				std::swprintf(message, std::size(message), reinterpret_cast<wchar_t*>(ptr), args...);
			}
			else ::LoadStringW(this_dll, id, message, std::size(message));
			::SetTimer(hwnd, timer_id(), time_ms, nullptr);
			visible = true;
		}
		void on_timeout(HWND hwnd)
		{
			::KillTimer(hwnd, timer_id());
			visible = false;
		}
	} toast{};

	// grid
	struct {
		bool visible = false;
	} grid;

	////////////////////////////////
	// coordinate transforms.
	////////////////////////////////

	// returned coordinates are relative to the center of the window.
	constexpr auto pic2win(double x, double y) const {
		auto s = zoom.scale_ratio();
		return std::make_pair(s * (x - position.x), s * (y - position.y));
	}
	// input coordinates are relative to the center of the window.
	constexpr auto win2pic(double x, double y) const {
		auto s = zoom.scale_ratio_inv();
		return std::make_pair(s * x + position.x, s * y + position.y);
	}

	// returns { pic2win, win2pic }-pair.
	constexpr auto transforms() const {
		auto [s1, s2] = zoom.scale_ratios();

		return std::make_pair([s1, px = position.x, py = position.y](double x, double y) {
			return std::make_pair(s1 * (x - px), s1 * (y - py));
		}, [s2, px = position.x, py = position.y](double x, double y) {
			return std::make_pair(s2 * x + px, s2 * y + py);
		});
	}

	// returns the pair of "view box" and "view port".
	// view box: the area of the picture to be on-screen.
	// view port: the area of the window where the picture will be on.
	// those are rounded so view box covers the entire pixels to be on-screen,
	// and view port may cover beyond the window corners.
	std::pair<RECT, RECT> viewbox_viewport(int picture_w, int picture_h, int client_w, int client_h) const
	{
		auto [p2w, w2p] = transforms();
		auto [pl, pt] = w2p(-0.5 * client_w, -0.5 * client_h);
		auto [pr, pb] = w2p(+0.5 * client_w, +0.5 * client_h);
		pl = std::max<double>(std::floor(pl), 0);
		pt = std::max<double>(std::floor(pt), 0);
		pr = std::min<double>(std::ceil(pr), picture_w);
		pb = std::min<double>(std::ceil(pb), picture_h);

		auto [wl, wt] = p2w(pl, pt);
		auto [wr, wb] = p2w(pr, pb);
		wl = std::floor(0.5 + wl + 0.5 * client_w);
		wt = std::floor(0.5 + wt + 0.5 * client_h);
		wr = std::floor(0.5 + wr + 0.5 * client_w);
		wb = std::floor(0.5 + wb + 0.5 * client_h);

		return {
			{ static_cast<int>(pl), static_cast<int>(pt), static_cast<int>(pr), static_cast<int>(pb) },
			{ static_cast<int>(wl), static_cast<int>(wt), static_cast<int>(wr), static_cast<int>(wb) },
		};
	}

	////////////////////////////////
	// UI helping functions.
	////////////////////////////////

	// change the scale keeping the specified position unchanged.
	void apply_zoom(int new_level, double win_ox, double win_oy, int pic_w, int pic_h) {
		auto s = zoom.scale_ratio_inv();
		zoom.scale_level = std::clamp(new_level, zoom.min_scale_level, zoom.max_scale_level);
		s -= zoom.scale_ratio_inv();

		position.x += s * win_ox; position.y += s * win_oy;
		position.clamp(pic_w, pic_h);
	}

	// resets some states that is suitable for a new sized image.
	void on_resize(int picture_w, int picture_h)
	{
		auto w2 = picture_w / 2, h2 = picture_h / 2;
		position.x = static_cast<double>(w2); position.y = static_cast<double>(h2);
		tip_state = { .x = w2, .y = h2, .visible_level = 0 };
	}
} loupe_state;


////////////////////////////////
// 設定項目．
////////////////////////////////
inline constinit struct Settings {
	enum class RailMode : uint8_t {
		none = 0, cross = 1, octagonal = 2,
	};
	enum class ZoomPivot : uint8_t {
		center = 0, cursor = 1,
	};
	enum class TipMode : uint8_t {
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
		constexpr static int8_t min_least_scale_thin = 6, max_least_scale_thin = loupe_state.zoom.max_scale_level + 1;
		constexpr static int8_t min_least_scale_thick = 6, max_least_scale_thick = loupe_state.zoom.max_scale_level + 1;

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


////////////////////////////////
// 設定ロードセーブ．
////////////////////////////////

// replacing a file extension when it's known.
template<class T, size_t len_max, size_t len_old, size_t len_new>
static void replace_tail(T(&dst)[len_max], size_t len, const T(&tail_old)[len_old], const T(&tail_new)[len_new])
{
	if (len < len_old || len - len_old + len_new > len_max) return;
	std::memcpy(dst + len - len_old, tail_new, len_new * sizeof(T));
}
inline void load_settings()
{
	char path[MAX_PATH];
	replace_tail(path, ::GetModuleFileNameA(this_dll, path, std::size(path)) + 1, "auf", "ini");

	settings.load(path);

	// additionally load the following loupe states.
	loupe_state.zoom.scale_level = std::clamp(
		static_cast<int>(::GetPrivateProfileIntA("state", "scale_level", loupe_state.zoom.scale_level, path)),
		loupe_state.zoom.min_scale_level, loupe_state.zoom.max_scale_level);
	loupe_state.zoom.second_level = std::clamp(
		static_cast<int>(::GetPrivateProfileIntA("state", "second_scale", loupe_state.zoom.second_level, path)),
		loupe_state.zoom.min_scale_level, loupe_state.zoom.max_scale_level);
	loupe_state.position.follow_cursor = ::GetPrivateProfileIntA("state", "follow_cursor",
		loupe_state.position.follow_cursor ? 1 : 0, path) != 0;
	loupe_state.grid.visible = ::GetPrivateProfileIntA("state", "show_grid",
		loupe_state.position.follow_cursor ? 1 : 0, path) != 0;
}
inline void save_settings()
{
	char path[MAX_PATH];
	replace_tail(path, ::GetModuleFileNameA(this_dll, path, std::size(path)) + 1, "auf", "ini");

	settings.save(path);

	// additionally save the following loupe states.
	char buf[std::size("+4294967296")];
	std::snprintf(buf, std::size(buf), "%d", loupe_state.zoom.scale_level);
	::WritePrivateProfileStringA("state", "scale_level", buf, path);
	std::snprintf(buf, std::size(buf), "%d", loupe_state.zoom.second_level);
	::WritePrivateProfileStringA("state", "second_scale", buf, path);
	::WritePrivateProfileStringA("state", "follow_cursor",
		loupe_state.position.follow_cursor ? "1" : "0", path);
	::WritePrivateProfileStringA("state", "show_grid",
		loupe_state.grid.visible ? "1" : "0", path);
}


////////////////////////////////
// 画像バッファ．
////////////////////////////////
inline constinit class ImageBuffer {
	BITMAPINFO bi{
		.bmiHeader = {
			.biSize = sizeof(bi.bmiHeader),
			.biPlanes = 1,
			.biBitCount = 24,
			.biCompression = BI_RGB,
		},
	};

	void* buf = nullptr;
	template<class TSelf>
	constexpr auto& wd(this TSelf& self) { return self.bi.bmiHeader.biWidth; }
	template<class TSelf>
	constexpr auto& ht(this TSelf& self) { return self.bi.bmiHeader.biHeight; }

	void reallocate(int w, int h)
	{
		int size_next = stride(w) * h, size_curr = stride() * ht();
		wd() = w; ht() = h;

		if (size_next == size_curr) return;
		if (buf != nullptr) ::VirtualFree(buf, 0, MEM_RELEASE);
		buf = size_next > 0 ? ::VirtualAlloc(nullptr, size_next, MEM_COMMIT, PAGE_READWRITE) : nullptr;
	}

public:
	constexpr const void* buffer() const { return buf; }
	constexpr const BITMAPINFO& info() const { return bi; }
	constexpr auto width() const { return wd(); }
	constexpr auto height() const { return ht(); }

	static constexpr int stride(int width) {
		// rounding upward into a multiple of 4.
		return (3 * width + 3) & (-4);
	}
	constexpr int stride() const { return stride(width()); }

	Color color_at(int x, int y) const
	{
		if (buf == nullptr ||
			x < 0 || y < 0 || x >= wd() || y >= ht()) return CLR_INVALID;
		auto ptr = reinterpret_cast<byte*>(buf) + 3 * x + stride() * (ht() - 1 - y);
		return { ptr[2], ptr[1], ptr[0] };
	}

	// returns true if the image size has been changed.
	bool update(int w, int h, void* source)
	{
		// check for size changes to the image.
		bool size_changed = false;
		if (wd() != w || ht() != h)
		{
			size_changed = true;
			reallocate(w, h);
		}

		std::memcpy(buf, source, stride() * ht());
		return size_changed;
	}

	void free()
	{
		if (buf != nullptr) {
			::VirtualFree(buf, 0, MEM_RELEASE), buf = nullptr;
			wd() = ht() = 0;
		}
	}

	bool is_valid() const { return buf != nullptr; }
} image;


////////////////////////////////
// ハンドル管理．
////////////////////////////////
template<class H, auto creator, auto deleter, class... TArgs>
class Handle {
	std::tuple<H, TArgs...> data;
	template<class TSelf>
	constexpr auto& handle(this TSelf& self) { return std::get<0>(self.data); }
public:
	constexpr Handle(TArgs... args) : data { nullptr, args... }{}
	H get()
	{
		if (handle() == nullptr) {
			handle() = [this]<size_t... I>(std::index_sequence<I...>) {
				return creator(std::get<I + 1>(data)...);
			}(std::make_index_sequence<sizeof...(TArgs)>{});
		}
		return handle();
	}
	void free() {
		deleter(handle());
		handle() = nullptr;
	}

	operator H() { return get(); }
	constexpr bool is_valid() const { return handle() != nullptr; }
};

constinit Handle<HFONT, [](wchar_t const(*name)[], int8_t const* size) {
	return ::CreateFontW(
		*size, 0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE, ANSI_CHARSET,
		OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
		DEFAULT_PITCH | FF_DONTCARE, *name);
}, ::DeleteObject, wchar_t const(*)[], int8_t const*>
	tip_font{ &settings.tip.font_name, &settings.tip.font_size },
	toast_font{ &settings.toast.font_name, &settings.toast.font_size };

constinit Handle<HMENU, []{ return ::LoadMenuW(this_dll, MAKEINTRESOURCEW(IDR_MENU_CXT)); }, ::DestroyMenu>
	cxt_menu{};


////////////////////////////////
// 外部リソース管理．
////////////////////////////////
inline constinit class {
	bool activated = false;

public:
	// global switch for allocating external resources.
	constexpr bool is_active() const { return activated; }
	void activate() { activated = true; }
	void deactivate()
	{
		activated = false;
		free();
	}
	void free()
	{
		image.free();
		tip_font.free();
		toast_font.free();
		cxt_menu.free();
	}
} ext_obj;


////////////////////////////////
// 描画関数．
////////////////////////////////

// 背景描画．
inline void draw_backplane(HDC hdc, const RECT& rc)
{
	::SetDCBrushColor(hdc, settings.color.blank);
	::FillRect(hdc, &rc, static_cast<HBRUSH>(::GetStockObject(DC_BRUSH)));
}

// 画像描画．
inline void draw_picture(HDC hdc, const RECT& vb, const RECT& vp)
{
	::SetStretchBltMode(hdc, STRETCH_DELETESCANS);
	::StretchDIBits(hdc, vp.left, vp.top, vp.right - vp.left, vp.bottom - vp.top,
		vb.left, image.height() - vb.bottom, vb.right - vb.left, vb.bottom - vb.top,
		image.buffer(), &image.info(), DIB_RGB_COLORS, SRCCOPY);
}

// グリッド描画 (thin)
inline void draw_grid_thin(HDC hdc, const RECT& vb, const RECT& vp)
{
	int w = vb.right - vb.left, W = vp.right - vp.left,
		h = vb.bottom - vb.top, H = vp.bottom - vp.top;

	auto old_mode = ::SetROP2(hdc, R2_NOT);
	for (int x = vb.left; x < vb.right; x++) {
		auto X = (x - vb.left) * W / w + vp.left;
		::MoveToEx(hdc, X, vp.top, nullptr);
		::LineTo(hdc, X, vp.bottom);
	}
	for (int y = vb.top; y < vb.bottom; y++) {
		auto Y = (y - vb.top) * H / h + vp.top;
		::MoveToEx(hdc, vp.left, Y, nullptr);
		::LineTo(hdc, vp.right, Y);
	}
	::SetROP2(hdc, old_mode);
}

// グリッド描画 (thick)
inline void draw_grid_thick(HDC hdc, const RECT& vb, const RECT& vp)
{
	int w = vb.right - vb.left, W = vp.right - vp.left,
		h = vb.bottom - vb.top, H = vp.bottom - vp.top;

	auto dc_brush = static_cast<HBRUSH>(::GetStockObject(DC_BRUSH));
	constexpr Color col0 = 0x222222, col1 = 0x444444, col2 = 0x666666,
		col3 = col2.negate(), col4 = col1.negate(), col5 = col0.negate();

	RECT rch = vp, rcv = vp;
	auto hline = [&, hdc, dc_brush](int y) { rch.top = y; rch.bottom = y + 1; ::FillRect(hdc, &rch, dc_brush); };
	auto vline = [&, hdc, dc_brush](int x) { rcv.left = x; rcv.right = x + 1; ::FillRect(hdc, &rcv, dc_brush); };

	// least significant lines.
	::SetDCBrushColor(hdc, col3);
	for (int x = vb.left; x < vb.right; x++) {
		if (x % 5 != 0) vline((x - vb.left) * W / w + vp.left);
	}
	for (int y = vb.top; y < vb.bottom; y++) {
		if (y % 5 != 0) hline((y - vb.top) * H / h + vp.top);
	}
	::SetDCBrushColor(hdc, col2);
	for (int x = vb.left + 1; x <= vb.right; x++) {
		if (x % 5 != 0) vline((x - vb.left) * W / w + vp.left - 1);
	}
	for (int y = vb.top + 1; y <= vb.bottom; y++) {
		if (y % 5 != 0) hline((y - vb.top) * H / h + vp.top - 1);
	}

	// multiple of 5, but not 10.
	::SetDCBrushColor(hdc, col4);
	for (int x = vb.left + 9 - ((vb.left + 4) % 10); x < vb.right; x += 10)
		vline((x - vb.left) * W / w + vp.left);
	for (int y = vb.top + 9 - ((vb.top + 4) % 10); y < vb.bottom; y += 10)
		hline((y - vb.top) * H / h + vp.top);
	::SetDCBrushColor(hdc, col1);
	for (int x = vb.left + 10 - ((vb.left + 5) % 10); x <= vb.right; x += 10)
		vline((x - vb.left) * W / w + vp.left - 1);
	for (int y = vb.top + 10 - ((vb.top + 5) % 10); y <= vb.bottom; y += 10)
		hline((y - vb.top) * H / h + vp.top - 1);

	// multiple of 10.
	::SetDCBrushColor(hdc, col5);
	for (int x = vb.left + 9 - ((vb.left + 9) % 10); x < vb.right; x += 10)
		vline((x - vb.left) * W / w + vp.left);
	for (int y = vb.top + 9 - ((vb.top + 9) % 10); y < vb.bottom; y += 10)
		hline((y - vb.top) * H / h + vp.top);
	::SetDCBrushColor(hdc, col0);
	for (int x = vb.left + 10 - (vb.left % 10); x <= vb.right; x += 10)
		vline((x - vb.left) * W / w + vp.left - 1);
	for (int y = vb.top + 10 - (vb.top % 10); y <= vb.bottom; y += 10)
		hline((y - vb.top) * H / h + vp.top - 1);
}

// 背景グラデーション & 枠付きの丸角矩形を描画．
inline void draw_round_rect(HDC hdc, const RECT& rc, int radius, int thick, Color back_top, Color back_btm, Color chrome)
{
	auto hrgn = ::CreateRoundRectRgn(rc.left, rc.top,
		rc.right, rc.bottom, radius, radius);
	::SelectClipRgn(hdc, hrgn);
	{
		TRIVERTEX vertices[]{
			{ rc.left,  rc.top,    static_cast<COLOR16>(back_top.R << 8), static_cast<COLOR16>(back_top.G << 8), static_cast<COLOR16>(back_top.B << 8) },
			{ rc.right, rc.bottom, static_cast<COLOR16>(back_btm.R << 8), static_cast<COLOR16>(back_btm.G << 8), static_cast<COLOR16>(back_btm.B << 8) } };
		constexpr GRADIENT_RECT rcs{ 0,1 };
		::GdiGradientFill(hdc, vertices, 2, const_cast<GRADIENT_RECT*>(&rcs), 1, GRADIENT_FILL_RECT_V);
	}
	::SelectClipRgn(hdc, nullptr);

	if (thick > 0) {
		::SetDCBrushColor(hdc, chrome);
		::FrameRgn(hdc, hrgn, static_cast<HBRUSH>(::GetStockObject(DC_BRUSH)), thick, thick);
	}
	::DeleteObject(hrgn);
}
// 色・座標表示ボックス描画．
inline void draw_tip(HDC hdc, int wd, int ht, const RECT& box, int pic_x, int pic_y, Color col, bool& prefer_above)
{
	RECT box_big = box;
	box_big.left -= settings.tip.box_inflate; box_big.right += settings.tip.box_inflate;
	box_big.top -= settings.tip.box_inflate; box_big.bottom += settings.tip.box_inflate;

	// draw the "pixel box".
	::SetDCBrushColor(hdc, col);
	::FillRect(hdc, &box_big, static_cast<HBRUSH>(::GetStockObject(DC_BRUSH)));
	::FrameRect(hdc, &box_big, static_cast<HBRUSH>(::GetStockObject(
		col.luma() <= Color::max_luma / 2 ? WHITE_BRUSH : BLACK_BRUSH)));

	// prepare text.
	constexpr int pad_x = 10, pad_y = 3, // ツールチップ矩形とテキストの間の空白．
		gap_v = 10, // ピクセルを囲む四角形とツールチップとの間の空白．
		margin = 4; // ウィンドウ端との最低保証空白．

	wchar_t tip_str[std::size(L"#RRGGBB\nX:1234, Y:1234--")];
	int tip_strlen = std::swprintf(tip_str, std::size(tip_str),
		L"#%06X\nX:%4d, Y:%4d", col.to_formattable(), pic_x, pic_y);

	// measure the text size.
	auto tmp_fon = ::SelectObject(hdc, tip_font);
	RECT rc_txt{}, rc_frm;
	::DrawTextW(hdc, tip_str, tip_strlen, &rc_txt, DT_CENTER | DT_TOP | DT_NOPREFIX | DT_NOCLIP | DT_CALCRECT);
	{
		const int w = rc_txt.right - rc_txt.left, h = rc_txt.bottom - rc_txt.top,
			x_min = margin + pad_x, x_max = wd - (w + pad_x + margin),
			y_min = margin + pad_y, y_max = ht - (h + pad_y + margin);

		int x = (box.left + box.right) / 2 - w / 2;

		// choose whether the tip should be placed above or below the box.
		const int yU = box.top - gap_v - pad_y - h, yD = box.bottom + gap_v + pad_y;
		if (auto up = y_min <= yU, dn = yD <= y_max; up || dn)
			prefer_above = prefer_above ? up : !dn;
		int y = prefer_above ? yU : yD;

		constexpr auto adjust = [](auto v, auto m, auto M) { return m > M ? (m + M) / 2 : std::clamp(v, m, M); };
		x = adjust(x, x_min, x_max);
		y = adjust(y, y_min, y_max);

		rc_txt = { x, y, x + w, y + h };
		rc_frm = { rc_txt.left - pad_x, rc_txt.top - pad_y, rc_txt.right + pad_x, rc_txt.bottom + pad_y };
	}

	// draw the round rect and its frame.
	draw_round_rect(hdc, rc_frm, settings.tip.chrome_radius, settings.tip.chrome_thick,
		settings.color.back_top, settings.color.back_bottom, settings.color.chrome);

	// then draw the text.
	::SetTextColor(hdc, settings.color.text);
	::SetBkMode(hdc, TRANSPARENT);
	::DrawTextW(hdc, tip_str, tip_strlen, &rc_txt, DT_CENTER | DT_TOP | DT_NOPREFIX | DT_NOCLIP);
	::SelectObject(hdc, tmp_fon);
}
// 通知メッセージトースト描画．
inline void draw_toast(HDC hdc, int wd, int ht)
{
	constexpr auto& toast = loupe_state.toast;
	constexpr int pad_x = 6, pad_y = 4, // トースト矩形とテキストの間の空白．
		margin = 4; // ウィンドウ端との最低保証空白．

	// measure the text size.
	auto tmp_fon = ::SelectObject(hdc, toast_font);
	RECT rc_txt{}, rc_frm{};
	::DrawTextW(hdc, toast.message, -1, &rc_txt, DT_NOPREFIX | DT_NOCLIP | DT_CALCRECT);

	// align horizontally.
	auto l = rc_txt.right - rc_txt.left;
	switch (settings.toast.placement) {
	case Settings::ToastPlacement::left_top:
	case Settings::ToastPlacement::left_bottom:
		rc_frm.left = margin;
		goto from_left;
	case Settings::ToastPlacement::right_top:
	case Settings::ToastPlacement::right_bottom:
		rc_frm.right = wd - margin;
		rc_txt.right = rc_frm.right - pad_x;
		rc_txt.left = rc_txt.right - l;
		rc_frm.left = rc_txt.left - pad_x;
		break;
	case Settings::ToastPlacement::center:
	default:
		rc_txt.left = wd / 2 - l / 2;
	from_left:
		rc_frm.left = rc_txt.left - pad_x;
		rc_txt.right = rc_txt.left + l;
		rc_frm.right = rc_txt.right + pad_x;
		break;
	}

	// then vertically.
	l = rc_txt.bottom - rc_txt.top;
	switch (settings.toast.placement) {
	case Settings::ToastPlacement::left_top:
	case Settings::ToastPlacement::right_top:
		rc_frm.top = margin;
		goto from_top;
	case Settings::ToastPlacement::right_bottom:
	case Settings::ToastPlacement::left_bottom:
		rc_frm.bottom = ht - margin;
		rc_txt.bottom = rc_frm.bottom - pad_y;
		rc_txt.top = rc_txt.bottom - l;
		rc_frm.top = rc_txt.top - pad_y;
		break;
	case Settings::ToastPlacement::center:
	default:
		rc_txt.top = ht / 2 - l / 2;
	from_top:
		rc_frm.top = rc_txt.top - pad_y;
		rc_txt.bottom = rc_txt.top + l;
		rc_frm.bottom = rc_txt.bottom + pad_y;
		break;
	}

	// draw the round rect and its frame.
	draw_round_rect(hdc, rc_frm, settings.toast.chrome_radius, settings.toast.chrome_thick,
		settings.color.back_top, settings.color.back_bottom, settings.color.chrome);

	// then draw the text.
	::SetTextColor(hdc, settings.color.text);
	::SetBkMode(hdc, TRANSPARENT);
	::DrawTextW(hdc, toast.message, -1, &rc_txt, DT_NOPREFIX | DT_NOCLIP);
	::SelectObject(hdc, tmp_fon);
}

// 未編集時などの無効状態で単色背景を描画．
inline void draw_blank(HWND hwnd)
{
	RECT rc; ::GetClientRect(hwnd, &rc);
	HDC hdc = ::GetDC(hwnd);
	draw_backplane(hdc, rc);
	::ReleaseDC(hwnd, hdc);
}
// メインの描画関数．
inline void draw(HWND hwnd)
{
	// image.is_valid() must be true here.
	_ASSERT(image.is_valid());

	int wd, ht; { RECT rc; ::GetClientRect(hwnd, &rc); wd = rc.right; ht = rc.bottom; }
	HDC window_hdc = ::GetDC(hwnd);

	// obtain the viewbox/viewport pair of the image.
	auto [vb, vp] = loupe_state.viewbox_viewport(image.width(), image.height(), wd, ht);

	// whether some part of window isn't covered by image.
	bool is_partial = vp.left > 0 || vp.right < wd ||
		vp.top > 0 || vp.bottom < ht;

	uint8_t grid_thick = loupe_state.grid.visible ?
		settings.grid.grid_thick(loupe_state.zoom.scale_level) : 0;

	// whether the tip is visible.
	constexpr auto& tip = loupe_state.tip_state;
	bool with_tip = tip.is_visible() &&
		tip.x >= 0 && tip.x < image.width() && tip.y >= 0 && tip.y < image.height();

	if (!is_partial && grid_thick == 0 && !with_tip && !loupe_state.toast.visible)
		// most cases will fall here; whole window is covered by a single image.
		draw_picture(window_hdc, vb, vp);
	else {
		// a bit complicated, so firstly draw it to the background DC and
		// blit to foreground afterwards in order to avoid flickering.
		HDC hdc = ::CreateCompatibleDC(window_hdc);
		auto tmp_bmp = ::SelectObject(hdc, ::CreateCompatibleBitmap(window_hdc, wd, ht));

		// some part of the window is exposed. fill the background.
		if (is_partial) draw_backplane(hdc, { 0, 0, wd, ht });

		// draw the main image.
		draw_picture(hdc, vb, vp);

		// draw the grid.
		if (grid_thick == 1) draw_grid_thin(hdc, vb, vp);
		else if (grid_thick >= 2) draw_grid_thick(hdc, vb, vp);

		// draw the info tip.
		if (with_tip) {
			auto [x, y] = loupe_state.pic2win(tip.x, tip.y);
			x += wd / 2.0; y += ht / 2.0;
			auto s = loupe_state.zoom.scale_ratio();
			draw_tip(hdc, wd, ht, {
				static_cast<int>(std::floor(x)), static_cast<int>(std::floor(y)),
				static_cast<int>(std::ceil(x + s)), static_cast<int>(std::ceil(y + s))
				}, tip.x, tip.y, image.color_at(tip.x, tip.y), tip.prefer_above);
		}

		// draw the toast.
		if (loupe_state.toast.visible) draw_toast(hdc, wd, ht);

		// transfer the data to the foreground DC.
		::BitBlt(window_hdc, 0, 0, wd, ht, hdc, 0, 0, SRCCOPY);
		::DeleteObject(::SelectObject(hdc, tmp_bmp));
		::DeleteDC(hdc);
	}

	::ReleaseDC(hwnd, window_hdc);
}


////////////////////////////////
// ドラッグ操作．
////////////////////////////////
class DragState {
	inline static constinit DragState* current = nullptr;

protected:
	HWND hwnd{ nullptr };
	POINT drag_start{}, last_point{}; // window coordinates.

	// should return true if ready to initiate the drag.
	virtual bool DragStart_core(bool& redraw) = 0;
	virtual void DragDelta_core(const POINT& curr, bool& redraw) {}
	virtual void DragCancel_core(bool& redraw) {}
	virtual void DragEnd_core(bool& redraw) {}
	virtual void DragAbort_core() {}

	// helper function for the "directional snapping" feature.
	static constexpr std::pair<double, double> snap_rail(double x, double y, Settings::RailMode mode, bool lattice) {
		double lx = x < 0 ? -x : x, ly = y < 0 ? -y : y; //double lx = std::abs(x), ly = std::abs(y);
		// std::abs() isn't constexpr at this moment of writing this code...
		switch (mode) {
		case Settings::RailMode::cross:
			// '+'-shape.
			(lx < ly ? x : y) = 0;
			break;
		case Settings::RailMode::octagonal:
		{
			// combination of '+' and 'X'-shape.
			constexpr double thr = 0.41421356237309504880168872420969807857; // tan(pi/8)
			// std::tan() isn't constexpr at this moment of writing this code...
			if (lx < thr * ly) x = 0;
			else if (thr * lx > ly) y = 0;
			else {
				lx = (lx + ly) / 2;
				x = x < 0 ? -lx : lx;
				y = y < 0 ? -lx : lx;
			}
			break;
		}
		case Settings::RailMode::none:
		default:
			break;
		}
		if (lattice) { x = std::round(x); y = std::round(y); }
		return { x, y };
	}

public:
	// returns true if the dragging has started.
	bool DragStart(HWND hwnd, const POINT& drag_start, bool& redraw)
	{
		if (is_dragging()) return false;

		this->hwnd = hwnd;
		this->drag_start = this->last_point = drag_start;

		if (this->DragStart_core(redraw)) {
			current = this;
			::SetCapture(hwnd);
			return true;
		}
		else {
			this->hwnd = nullptr;
			return false;
		}
	}
	// returns true if the dragging operation has been properly processed.
	static bool DragDelta(const POINT& curr, bool& redraw)
	{
		if (!is_dragging()) return false;

		current->DragDelta_core(curr, redraw);
		current->last_point = curr;
		return true;
	}
	// returns true if the dragging operation has been properly canceled.
	static bool DragCancel(bool& redraw)
	{
		if (!is_dragging()) return false;

		current->DragCancel_core(redraw);

		current->hwnd = nullptr;
		current = nullptr;

		::ReleaseCapture();
		return true;
	}
	// returns true if the dragging operation has been properly processed.
	static bool DragEnd(bool& redraw)
	{
		if (!is_dragging()) return false;

		current->DragEnd_core(redraw);

		current->hwnd = nullptr;
		current = nullptr;

		::ReleaseCapture();
		return true;
	}
	// returns true if there sure was a dragging state that is now aborted.
	static bool DragAbort()
	{
		if (current == nullptr) return false;

		current->DragAbort_core();

		auto tmp = current->hwnd;
		current->hwnd = nullptr;
		current = nullptr;

		if (tmp != nullptr && ::GetCapture() == tmp)
			::ReleaseCapture();
		return true;
	}
	// verifies the dragging status, might abort if the window is no longer captured.
	static bool is_dragging() {
		if (current != nullptr) {
			if (current->hwnd != nullptr &&
				current->hwnd == ::GetCapture()) return true;

			DragAbort();
		}
		return false;
	}
};

inline constinit class LeftDrag : public DragState {
	double revert_x{}, revert_y{}, curr_x{}, curr_y{};
	int revert_zoom{};
	constexpr static auto& pos = loupe_state.position;

protected:
	bool DragStart_core(bool& redraw) override
	{
		revert_x = curr_x = pos.x;
		revert_y = curr_y = pos.y;
		revert_zoom = loupe_state.zoom.scale_level;
		::SetCursor(::LoadCursorW(nullptr, reinterpret_cast<PCWSTR>(IDC_HAND)));
		return true;
	}
	void DragDelta_core(const POINT& curr, bool& redraw) override
	{
		double dx = curr.x - last_point.x, dy = curr.y - last_point.y;
		if (dx == 0 && dy == 0) return;

		auto s = loupe_state.zoom.scale_ratio_inv();
		curr_x -= s * dx; curr_y -= s * dy;

		std::tie(dx, dy) = snap_rail(curr_x - revert_x, curr_y - revert_y,
			::GetKeyState(VK_SHIFT) < 0 ? settings.positioning.rail_mode : Settings::RailMode::none,
			settings.positioning.lattice);
		double px = revert_x, py = revert_y;
		if (settings.positioning.lattice)
			px = std::round(2 * px) / 2, py = std::round(2 * py) / 2; // half-integral.
		px = std::clamp<double>(px + dx, 0, image.width());
		py = std::clamp<double>(py + dy, 0, image.height());

		if (px == pos.x && py == pos.y) return;
		pos.x = px; pos.y = py;
		redraw = true;
	}
	void DragCancel_core(bool& redraw) override
	{
		pos.x = revert_x; pos.y = revert_y;
		loupe_state.zoom.scale_level = revert_zoom;
		redraw = true;
	}
	void DragAbort_core() override
	{
		bool b = false;
		DragCancel_core(b);
	}
} left_drag;

inline constinit class RightDrag : public DragState {
	constexpr static auto& tip = loupe_state.tip_state;

	std::pair<double, double> win2pic(const POINT& p) {
		RECT rc; ::GetClientRect(hwnd, &rc);
		return loupe_state.win2pic(p.x - rc.right / 2.0, p.y - rc.bottom / 2.0);
	}
	int init_x{}, init_y{};

protected:
	bool DragStart_core(bool& redraw) override
	{
		switch (settings.tip.mode) {
			using enum Settings::TipMode;
		case frail:
			tip.visible_level = 2;
			redraw = true;
			break;
		case stationary:
		case sticky:
			tip.visible_level = tip.is_visible() ? 0 : 2;
			redraw = true;
			break;
		case none:
		default:
			tip.visible_level = 0;
			break;
		}
		if (!tip.is_visible()) return false;

		auto [x, y] = win2pic(drag_start);
		tip.x = init_x = static_cast<int>(std::floor(x));
		tip.y = init_y = static_cast<int>(std::floor(y));
		::SetCursor(nullptr);
		return true;
	}
	void DragDelta_core(const POINT& curr, bool& redraw) override
	{
		auto [dx, dy] = win2pic(curr);
		std::tie(dx, dy) = snap_rail(dx - (0.5 + init_x), dy - (0.5 + init_y),
			::GetKeyState(VK_SHIFT) < 0 ? settings.tip.rail_mode : Settings::RailMode::none, true);
		auto x = init_x + static_cast<int>(dx), y = init_y + static_cast<int>(dy);

		if (x == tip.x && y == tip.y) return;
		tip.x = x; tip.y = y;
		redraw = true;
	}
	void DragCancel_core(bool& redraw) override
	{
		tip.visible_level = 0;
		redraw = true;
	}
	void DragEnd_core(bool& redraw) override
	{
		switch (settings.tip.mode) {
			using enum Settings::TipMode;
		case frail:
			tip.visible_level = 0;
			redraw = true;
			break;
		case none:
		case stationary:
		case sticky:
		default:
			break;
		}
		return;
	}
	void DragAbort_core() override
	{
		bool b = false;
		DragCancel_core(b);
	}
} right_drag;


////////////////////////////////
// その他コマンド．
////////////////////////////////

// all commands below assumes image.is_valid() is true.
// returns true if the window needs redrawing.
inline bool centralize()
{
	// centralize the target point.
	loupe_state.position.x = image.width() / 2.0;
	loupe_state.position.y = image.height() / 2.0;
	return true;
}
inline bool toggle_follow_cursor(HWND hwnd)
{
	loupe_state.position.follow_cursor ^= true;

	// toast message.
	if (!settings.toast.notify_follow_cursor) return false;
	loupe_state.toast.set_message(hwnd, settings.toast.duration,
		loupe_state.position.follow_cursor ? IDS_TOAST_FOLLOW_CURSOR_ON : IDS_TOAST_FOLLOW_CURSOR_OFF);
	return true;
}
inline bool toggle_grid(HWND hwnd)
{
	loupe_state.grid.visible ^= true;

	// toast message.
	if (!settings.toast.notify_grid) return false;
	loupe_state.toast.set_message(hwnd, settings.toast.duration,
		loupe_state.grid.visible ? IDS_TOAST_GRID_ON : IDS_TOAST_GRID_OFF);
	return true;
}
inline bool apply_zoom(HWND hwnd, int new_level, double win_ox, double win_oy)
{
	loupe_state.apply_zoom(new_level, win_ox, win_oy, image.width(), image.height());

	// toast message.
	if (!settings.toast.notify_scale) return false;
	wchar_t scale[std::max(std::size(L"x 123/456"), std::max(std::size(L"x 123.00"), std::size(L"123.45%")))];
	switch (settings.toast.scale_format) {
		using enum Settings::ToastScaleFormat;
	case decimal:
		std::swprintf(scale, std::size(scale), L"x %0.2f", loupe_state.zoom.scale_ratio());
		break;
	case percent:
		std::swprintf(scale, std::size(scale), L"%d%%",
			static_cast<int>(std::round(100 * loupe_state.zoom.scale_ratio())));
		break;
	case fraction:
	default:
		auto [n, d] = loupe_state.zoom.scale_ratio_Q();
		if (d == 1) std::swprintf(scale, std::size(scale), L"x %d", n);
		else std::swprintf(scale, std::size(scale), L"x %d/%d", n, d);
		break;
	}
	loupe_state.toast.set_message(hwnd, settings.toast.duration, IDS_TOAST_SCALE_FORMAT, scale);
	return true;
}
// swap the zoom level from second.
inline bool swap_scale_level(HWND hwnd, const POINT& pt_win)
{
	auto level = loupe_state.zoom.scale_level;
	std::swap(level, loupe_state.zoom.second_level);

	double ox = 0, oy = 0;
	if (settings.zoom.pivot_swap == Settings::ZoomPivot::cursor) {
		RECT rc; ::GetClientRect(hwnd, &rc);
		ox = pt_win.x - rc.right / 2.0;
		oy = pt_win.y - rc.bottom / 2.0;
	}
	return apply_zoom(hwnd, level, ox, oy);
}
template<class... TArgs>
inline bool copy_text(size_t len, const wchar_t* fmt, const TArgs&... args)
{
	if (!::OpenClipboard(nullptr)) return false;
	bool success = false;

	::EmptyClipboard();
	if (auto h = ::GlobalAlloc(GHND, (len + 1) * sizeof(wchar_t))) {
		if (auto ptr = reinterpret_cast<wchar_t*>(::GlobalLock(h))) {
			::swprintf(ptr, len + 1, fmt, args...);
			::GlobalUnlock(h);
			::SetClipboardData(CF_UNICODETEXT, h);

			success = true;
		}
		else ::GlobalFree(h);
	}
	::CloseClipboard();
	return success;
}
// copies the color code to the clipboard.
inline bool copy_color_code(HWND hwnd, const POINT& pt_win)
{
	RECT rc; ::GetClientRect(hwnd, &rc);
	auto [x, y] = loupe_state.win2pic(pt_win.x - rc.right / 2.0, pt_win.y - rc.bottom / 2.0);
	auto color = image.color_at(static_cast<int>(std::floor(x)), static_cast<int>(std::floor(y))).to_formattable();
	if (color > 0xffffff) return false;

	if (!copy_text(6, L"%06x", color)) return false;

	// toast message.
	if (!settings.toast.notify_clipboard) return false;
	loupe_state.toast.set_message(hwnd, settings.toast.duration, IDS_TOAST_CLIPBOARD, color);
	return true;
}


////////////////////////////////
// メニュー操作．
////////////////////////////////
struct Menu {
	static void popup_menu(HWND hwnd, const POINT& pt_screen)
	{
		if (!ext_obj.is_active()) return;
		auto chk = [cxt_menu = cxt_menu.get()](UINT id, bool check) {::CheckMenuItem(cxt_menu, id, MF_BYCOMMAND | (check ? MF_CHECKED : MF_UNCHECKED)); };

		// prepare the context menu.
		chk(IDM_CXT_FOLLOW_CURSOR,					loupe_state.position.follow_cursor);
		chk(IDM_CXT_SHOW_GRID,						loupe_state.grid.visible);
		chk(IDM_CXT_REVERSE_WHEEL,					settings.zoom.reverse_wheel);
		chk(IDM_CXT_PIVOT_CENTER,					settings.zoom.pivot				== Settings::ZoomPivot::center);
		chk(IDM_CXT_PIVOT_MOUSE,					settings.zoom.pivot				== Settings::ZoomPivot::cursor);
		chk(IDM_CXT_PIVOT_DRAG_CENTER,				settings.zoom.pivot_drag		== Settings::ZoomPivot::center);
		chk(IDM_CXT_PIVOT_DRAG_MOUSE,				settings.zoom.pivot_drag		== Settings::ZoomPivot::cursor);
		chk(IDM_CXT_PIVOT_SWAP_CENTER,				settings.zoom.pivot_swap		== Settings::ZoomPivot::center);
		chk(IDM_CXT_PIVOT_SWAP_MOUSE,				settings.zoom.pivot_swap		== Settings::ZoomPivot::cursor);
		chk(IDM_CXT_LATTICE,						settings.positioning.lattice);
		chk(IDM_CXT_RAIL_NONE,						settings.positioning.rail_mode	== Settings::RailMode::none);
		chk(IDM_CXT_RAIL_CROSS,						settings.positioning.rail_mode	== Settings::RailMode::cross);
		chk(IDM_CXT_RAIL_OCTAGONAL,					settings.positioning.rail_mode	== Settings::RailMode::octagonal);
		chk(IDM_CXT_TIP_MODE_NONE,					settings.tip.mode				== Settings::TipMode::none);
		chk(IDM_CXT_TIP_MODE_FRAIL,					settings.tip.mode				== Settings::TipMode::frail);
		chk(IDM_CXT_TIP_MODE_STATIONARY,			settings.tip.mode				== Settings::TipMode::stationary);
		chk(IDM_CXT_TIP_MODE_STICKY,				settings.tip.mode				== Settings::TipMode::sticky);
		chk(IDM_CXT_TIP_RAIL_NONE,					settings.tip.rail_mode			== Settings::RailMode::none);
		chk(IDM_CXT_TIP_RAIL_CROSS,					settings.tip.rail_mode			== Settings::RailMode::cross);
		chk(IDM_CXT_TIP_RAIL_OCTAGONAL,				settings.tip.rail_mode			== Settings::RailMode::octagonal);
		chk(IDM_CXT_TOAST_NOTIFY_SCALE,				settings.toast.notify_scale);
		chk(IDM_CXT_TOAST_NOTIFY_FOLLOW_CURSOR,		settings.toast.notify_follow_cursor);
		chk(IDM_CXT_TOAST_NOTIFY_TOGGLE_GRID,		settings.toast.notify_grid);
		chk(IDM_CXT_TOAST_NOTIFY_CLIPBOARD,			settings.toast.notify_clipboard);
		chk(IDM_CXT_TOAST_PLACEMENT_CENTER,			settings.toast.placement		== Settings::ToastPlacement::center);
		chk(IDM_CXT_TOAST_PLACEMENT_LEFT_TOP,		settings.toast.placement		== Settings::ToastPlacement::left_top);
		chk(IDM_CXT_TOAST_PLACEMENT_RIGHT_TOP,		settings.toast.placement		== Settings::ToastPlacement::right_top);
		chk(IDM_CXT_TOAST_PLACEMENT_RIGHT_BOTTOM,	settings.toast.placement		== Settings::ToastPlacement::right_bottom);
		chk(IDM_CXT_TOAST_PLACEMENT_LEFT_BOTTOM,	settings.toast.placement		== Settings::ToastPlacement::left_bottom);
		chk(IDM_CXT_TOAST_SCALE_FMT_FRAC,			settings.toast.scale_format		== Settings::ToastScaleFormat::fraction);
		chk(IDM_CXT_TOAST_SCALE_FMT_DEC,			settings.toast.scale_format		== Settings::ToastScaleFormat::decimal);
		chk(IDM_CXT_TOAST_SCALE_FMT_PCT,			settings.toast.scale_format		== Settings::ToastScaleFormat::percent);
		chk(IDM_CXT_COMMAND_LDBLCLK_NONE,			settings.commands.left_dblclk	== Settings::CommonCommand::none);
		chk(IDM_CXT_COMMAND_LDBLCLK_FOLLOW_CURSOR,	settings.commands.left_dblclk	== Settings::CommonCommand::toggle_follow_cursor);
		chk(IDM_CXT_COMMAND_LDBLCLK_SWAP_ZOOM,		settings.commands.left_dblclk	== Settings::CommonCommand::swap_scale_level);
		chk(IDM_CXT_COMMAND_LDBLCLK_CENTRALIZE,		settings.commands.left_dblclk	== Settings::CommonCommand::centralize);
		chk(IDM_CXT_COMMAND_LDBLCLK_GRID,			settings.commands.left_dblclk	== Settings::CommonCommand::toggle_grid);
		chk(IDM_CXT_COMMAND_LDBLCLK_COPY_COLOR,		settings.commands.left_dblclk	== Settings::CommonCommand::copy_color_code);
		chk(IDM_CXT_COMMAND_LDBLCLK_MENU,			settings.commands.left_dblclk	== Settings::CommonCommand::context_menu);
		chk(IDM_CXT_COMMAND_RDBLCLK_NONE,			settings.commands.right_dblclk	== Settings::CommonCommand::none);
		chk(IDM_CXT_COMMAND_RDBLCLK_FOLLOW_CURSOR,	settings.commands.right_dblclk	== Settings::CommonCommand::toggle_follow_cursor);
		chk(IDM_CXT_COMMAND_RDBLCLK_SWAP_ZOOM,		settings.commands.right_dblclk	== Settings::CommonCommand::swap_scale_level);
		chk(IDM_CXT_COMMAND_RDBLCLK_CENTRALIZE,		settings.commands.right_dblclk	== Settings::CommonCommand::centralize);
		chk(IDM_CXT_COMMAND_RDBLCLK_GRID,			settings.commands.right_dblclk	== Settings::CommonCommand::toggle_grid);
		chk(IDM_CXT_COMMAND_RDBLCLK_COPY_COLOR,		settings.commands.right_dblclk	== Settings::CommonCommand::copy_color_code);
		chk(IDM_CXT_COMMAND_RDBLCLK_MENU,			settings.commands.right_dblclk	== Settings::CommonCommand::context_menu);
		chk(IDM_CXT_COMMAND_MCLICK_NONE,			settings.commands.middle_click	== Settings::CommonCommand::none);
		chk(IDM_CXT_COMMAND_MCLICK_FOLLOW_CURSOR,	settings.commands.middle_click	== Settings::CommonCommand::toggle_follow_cursor);
		chk(IDM_CXT_COMMAND_MCLICK_SWAP_ZOOM,		settings.commands.middle_click	== Settings::CommonCommand::swap_scale_level);
		chk(IDM_CXT_COMMAND_MCLICK_CENTRALIZE,		settings.commands.middle_click	== Settings::CommonCommand::centralize);
		chk(IDM_CXT_COMMAND_MCLICK_GRID,			settings.commands.middle_click	== Settings::CommonCommand::toggle_grid);
		chk(IDM_CXT_COMMAND_MCLICK_COPY_COLOR,		settings.commands.middle_click	== Settings::CommonCommand::copy_color_code);
		chk(IDM_CXT_COMMAND_MCLICK_MENU,			settings.commands.middle_click	== Settings::CommonCommand::context_menu);

		::TrackPopupMenuEx(::GetSubMenu(cxt_menu, 0), TPM_RIGHTBUTTON, pt_screen.x, pt_screen.y, hwnd, nullptr);
	}
	static bool on_menu_command(HWND hwnd, uint_fast16_t id)
	{
		// returns true if needs redraw.
		switch (id) {
		case IDM_CXT_FOLLOW_CURSOR:					return toggle_follow_cursor(hwnd);
		case IDM_CXT_CENTRALIZE:					return centralize();
		case IDM_CXT_SHOW_GRID:						return toggle_grid(hwnd);
		case IDM_CXT_REVERSE_WHEEL:					settings.zoom.reverse_wheel			^= true;										break;
		case IDM_CXT_PIVOT_CENTER:					settings.zoom.pivot					= Settings::ZoomPivot::center;					break;
		case IDM_CXT_PIVOT_MOUSE:					settings.zoom.pivot					= Settings::ZoomPivot::cursor;					break;
		case IDM_CXT_PIVOT_DRAG_CENTER:				settings.zoom.pivot_drag			= Settings::ZoomPivot::center;					break;
		case IDM_CXT_PIVOT_DRAG_MOUSE:				settings.zoom.pivot_drag			= Settings::ZoomPivot::cursor;					break;
		case IDM_CXT_PIVOT_SWAP_CENTER:				settings.zoom.pivot_swap			= Settings::ZoomPivot::center;					break;
		case IDM_CXT_PIVOT_SWAP_MOUSE:				settings.zoom.pivot_swap			= Settings::ZoomPivot::cursor;					break;
		case IDM_CXT_LATTICE:						settings.positioning.lattice		^= true;										break;
		case IDM_CXT_RAIL_NONE:						settings.positioning.rail_mode		= Settings::RailMode::none;						break;
		case IDM_CXT_RAIL_CROSS:					settings.positioning.rail_mode		= Settings::RailMode::cross;					break;
		case IDM_CXT_RAIL_OCTAGONAL:				settings.positioning.rail_mode		= Settings::RailMode::octagonal;				break;
		case IDM_CXT_TIP_MODE_NONE:					settings.tip.mode					= Settings::TipMode::none;						break;
		case IDM_CXT_TIP_MODE_FRAIL:				settings.tip.mode					= Settings::TipMode::frail;						goto hide_tip_and_redraw;
		case IDM_CXT_TIP_MODE_STATIONARY:			settings.tip.mode					= Settings::TipMode::stationary;				goto hide_tip_and_redraw;
		case IDM_CXT_TIP_MODE_STICKY:				settings.tip.mode					= Settings::TipMode::sticky;					goto hide_tip_and_redraw;
		hide_tip_and_redraw:
			loupe_state.tip_state.visible_level = 0;
			return true;
		case IDM_CXT_TIP_RAIL_NONE:					settings.tip.rail_mode				= Settings::RailMode::none;						break;
		case IDM_CXT_TIP_RAIL_CROSS:				settings.tip.rail_mode				= Settings::RailMode::cross;					break;
		case IDM_CXT_TIP_RAIL_OCTAGONAL:			settings.tip.rail_mode				= Settings::RailMode::octagonal;				break;
		case IDM_CXT_TOAST_NOTIFY_SCALE:			settings.toast.notify_scale			^= true;										break;
		case IDM_CXT_TOAST_NOTIFY_FOLLOW_CURSOR:	settings.toast.notify_follow_cursor	^= true;										break;
		case IDM_CXT_TOAST_NOTIFY_TOGGLE_GRID:		settings.toast.notify_grid			^= true;										break;
		case IDM_CXT_TOAST_NOTIFY_CLIPBOARD:		settings.toast.notify_clipboard		^= true;										break;
		case IDM_CXT_TOAST_PLACEMENT_CENTER:		settings.toast.placement			= Settings::ToastPlacement::center;				break;
		case IDM_CXT_TOAST_PLACEMENT_LEFT_TOP:		settings.toast.placement			= Settings::ToastPlacement::left_top;			break;
		case IDM_CXT_TOAST_PLACEMENT_RIGHT_TOP:		settings.toast.placement			= Settings::ToastPlacement::right_top;			break;
		case IDM_CXT_TOAST_PLACEMENT_RIGHT_BOTTOM:	settings.toast.placement			= Settings::ToastPlacement::right_bottom;		break;
		case IDM_CXT_TOAST_PLACEMENT_LEFT_BOTTOM:	settings.toast.placement			= Settings::ToastPlacement::left_bottom;		break;
		case IDM_CXT_TOAST_SCALE_FMT_FRAC:			settings.toast.scale_format			= Settings::ToastScaleFormat::fraction;			break;
		case IDM_CXT_TOAST_SCALE_FMT_DEC:			settings.toast.scale_format			= Settings::ToastScaleFormat::decimal;			break;
		case IDM_CXT_TOAST_SCALE_FMT_PCT:			settings.toast.scale_format			= Settings::ToastScaleFormat::percent;			break;
		case IDM_CXT_COMMAND_LDBLCLK_NONE:			settings.commands.left_dblclk		= Settings::CommonCommand::none;				break;
		case IDM_CXT_COMMAND_LDBLCLK_FOLLOW_CURSOR:	settings.commands.left_dblclk		= Settings::CommonCommand::toggle_follow_cursor;break;
		case IDM_CXT_COMMAND_LDBLCLK_SWAP_ZOOM:		settings.commands.left_dblclk		= Settings::CommonCommand::swap_scale_level;	break;
		case IDM_CXT_COMMAND_LDBLCLK_CENTRALIZE:	settings.commands.left_dblclk		= Settings::CommonCommand::centralize;			break;
		case IDM_CXT_COMMAND_LDBLCLK_GRID:			settings.commands.left_dblclk		= Settings::CommonCommand::toggle_grid;			break;
		case IDM_CXT_COMMAND_LDBLCLK_COPY_COLOR:	settings.commands.left_dblclk		= Settings::CommonCommand::copy_color_code;		break;
		case IDM_CXT_COMMAND_LDBLCLK_MENU:			settings.commands.left_dblclk		= Settings::CommonCommand::context_menu;		break;
		case IDM_CXT_COMMAND_RDBLCLK_NONE:			settings.commands.right_dblclk		= Settings::CommonCommand::none;				break;
		case IDM_CXT_COMMAND_RDBLCLK_FOLLOW_CURSOR:	settings.commands.right_dblclk		= Settings::CommonCommand::toggle_follow_cursor;break;
		case IDM_CXT_COMMAND_RDBLCLK_SWAP_ZOOM:		settings.commands.right_dblclk		= Settings::CommonCommand::swap_scale_level;	break;
		case IDM_CXT_COMMAND_RDBLCLK_CENTRALIZE:	settings.commands.right_dblclk		= Settings::CommonCommand::centralize;			break;
		case IDM_CXT_COMMAND_RDBLCLK_GRID:			settings.commands.right_dblclk		= Settings::CommonCommand::toggle_grid;			break;
		case IDM_CXT_COMMAND_RDBLCLK_COPY_COLOR:	settings.commands.right_dblclk		= Settings::CommonCommand::copy_color_code;		break;
		case IDM_CXT_COMMAND_RDBLCLK_MENU:			settings.commands.right_dblclk		= Settings::CommonCommand::context_menu;		break;
		case IDM_CXT_COMMAND_MCLICK_NONE:			settings.commands.middle_click		= Settings::CommonCommand::none;				break;
		case IDM_CXT_COMMAND_MCLICK_FOLLOW_CURSOR:	settings.commands.middle_click		= Settings::CommonCommand::toggle_follow_cursor;break;
		case IDM_CXT_COMMAND_MCLICK_SWAP_ZOOM:		settings.commands.middle_click		= Settings::CommonCommand::swap_scale_level;	break;
		case IDM_CXT_COMMAND_MCLICK_CENTRALIZE:		settings.commands.middle_click		= Settings::CommonCommand::centralize;			break;
		case IDM_CXT_COMMAND_MCLICK_GRID:			settings.commands.middle_click		= Settings::CommonCommand::toggle_grid;			break;
		case IDM_CXT_COMMAND_MCLICK_COPY_COLOR:		settings.commands.middle_click		= Settings::CommonCommand::copy_color_code;		break;
		case IDM_CXT_COMMAND_MCLICK_MENU:			settings.commands.middle_click		= Settings::CommonCommand::context_menu;		break;
		default: break;
		}
		return false;
	}
};


////////////////////////////////
// AviUtlに渡す関数の定義．
////////////////////////////////
inline void on_update(int w, int h, void* source)
{
	if (source != nullptr && image.update(w, h, source))
		// notify the loupe of resizing.
		loupe_state.on_resize(w, h);
}
inline bool on_command(HWND hwnd, Settings::CommonCommand cmd, POINT&& pt)
{
	if (!image.is_valid()) return false;

	switch (cmd) {
		using cc = Settings::CommonCommand;
	case cc::centralize: return centralize();
	case cc::toggle_follow_cursor: return toggle_follow_cursor(hwnd);
	case cc::swap_scale_level: return swap_scale_level(hwnd, pt);
	case cc::toggle_grid: return toggle_grid(hwnd);
	case cc::copy_color_code: return copy_color_code(hwnd, pt);
	case cc::context_menu:
		::ClientToScreen(hwnd, &pt);
		Menu::popup_menu(hwnd, pt);
		return false;

	case cc::none:
	default: return false;
	}
}

BOOL func_proc(FilterPlugin* fp, FilterProcInfo* fpip)
{
	// updates to the target image.
	if (ext_obj.is_active() &&
		fp->exfunc->is_editing(fpip->editp) && !fp->exfunc->is_saving(fpip->editp)) {

		on_update(fpip->w, fpip->h, fp->exfunc->get_disp_pixelp(fpip->editp, 0));
		draw(fp->hwnd);
	}
	return TRUE;
}

BOOL func_WndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam, EditHandle* editp, FilterPlugin* fp)
{
	using FilterMessage = FilterPlugin::WindowMessage;
	constexpr auto cursor_pos = [](LPARAM l) {
		return POINT{ .x = static_cast<int16_t>(0xffff & l), .y = static_cast<int16_t>(l >> 16) };
	};
	bool redraw = false;

	static constinit bool track_mouse_event_sent = false;
	switch (message) {
	case FilterMessage::Init:
		this_dll = fp->dll_hinst;

		// load settings.
		load_settings();

		// disable IME.
		::ImmReleaseContext(hwnd, ::ImmAssociateContext(hwnd, nullptr));
		break;

	case FilterMessage::Exit:
		if (loupe_state.toast.visible) loupe_state.toast.on_timeout(hwnd);
		ext_obj.deactivate();

		// save settings.
		save_settings();
		break;

	case FilterMessage::ChangeWindow:
		// ルーペウィンドウ表示状態の切り替え．
		if (fp->exfunc->is_filter_window_disp(fp)) {
			ext_obj.activate();

			if (fp->exfunc->is_editing(editp) && !fp->exfunc->is_saving(editp)) {
				int w, h;
				fp->exfunc->get_frame_size(editp, &w, &h);
				on_update(w, h, fp->exfunc->get_disp_pixelp(editp, 0));
			}
			redraw = true;
		}
		else ext_obj.deactivate(), DragState::DragAbort();
		break;
	case FilterMessage::FileClose:
		ext_obj.free();
		DragState::DragAbort();

		// clear the window when a file is closed.
		if (::IsWindowVisible(hwnd) != FALSE)
			draw_blank(hwnd); // can't use `redraw = true` because exfunc->is_editing() is true at this moment.
		break;

	case WM_PAINT:
		redraw = true;
		break;

	case WM_TIMER:
		// update of the toast message to dissapear.
		if (static_cast<UINT_PTR>(wparam) == loupe_state.toast.timer_id()) {
			loupe_state.toast.on_timeout(hwnd);
			redraw = image.is_valid();
		}
		break;

		// UI handlers for mouse messages.
	case WM_LBUTTONDOWN:
		if (DragState::DragCancel(redraw)) break;
		if (image.is_valid()) {
			if (::GetKeyState(VK_RBUTTON) >= 0 && ::GetKeyState(VK_MBUTTON) >= 0)
				// start dragging to move the target point.
				left_drag.DragStart(hwnd, cursor_pos(lparam), redraw);
		}
		break;
	case WM_RBUTTONDOWN:
		if (DragState::DragCancel(redraw)) break;
		if (image.is_valid()) {
			if (::GetKeyState(VK_CONTROL) < 0) {
				// show the context menu when ctrl + right click.
				auto pt = cursor_pos(lparam);
				::ClientToScreen(hwnd, &pt);
				Menu::popup_menu(hwnd, pt);
			}
			else if (::GetKeyState(VK_LBUTTON) >= 0 && ::GetKeyState(VK_MBUTTON) >= 0)
				// start dragging to show up the info tip.
				right_drag.DragStart(hwnd, cursor_pos(lparam), redraw);
		}
		break;
	case WM_MBUTTONDOWN:
		DragState::DragCancel(redraw);
		break;

	case WM_LBUTTONUP:
	case WM_RBUTTONUP:
		// finish dragging.
		DragState::DragEnd(redraw);
		break;
	case WM_CAPTURECHANGED:
		// abort dragging.
		if (DragState::DragAbort()) redraw = true;
		break;

	case WM_MOUSEMOVE:
		if (DragState::DragDelta(cursor_pos(lparam), redraw)) /* empty */;
		else if (!image.is_valid()) break;
		else if (settings.tip.mode == Settings::TipMode::sticky && loupe_state.tip_state.visible_level > 0) {
			// update to the tip of "sticky" mode.
			auto pt = cursor_pos(lparam);
			RECT rc; ::GetClientRect(hwnd, &rc);
			auto [x, y] = loupe_state.win2pic(pt.x - rc.right / 2.0, pt.y - rc.bottom / 2.0);
			auto X = static_cast<int>(std::floor(x)), Y = static_cast<int>(std::floor(y));

			if (!loupe_state.tip_state.is_visible() ||
				X != loupe_state.tip_state.x || Y != loupe_state.tip_state.y) {

				loupe_state.tip_state.visible_level = 2;
				loupe_state.tip_state.x = X;
				loupe_state.tip_state.y = Y;

				redraw = true;
			}
		}
		if (!track_mouse_event_sent &&
			settings.tip.mode == Settings::TipMode::sticky && loupe_state.tip_state.is_visible()) {
			track_mouse_event_sent = true;
			// to make WM_MOUSELEAVE message be sent.
			TRACKMOUSEEVENT tme{ .cbSize = sizeof(TRACKMOUSEEVENT), .dwFlags = TME_LEAVE, .hwndTrack = hwnd };
			::TrackMouseEvent(&tme);
		}
		break;
	case WM_MOUSELEAVE:
		track_mouse_event_sent = false;
		// hide the "sticky" tip.
		if (settings.tip.mode == Settings::TipMode::sticky && loupe_state.tip_state.is_visible()) {
			loupe_state.tip_state.visible_level = 1;
			redraw = true;
		}
		break;

	case WM_MOUSEWHEEL:
		// wheel to zoom in/out.
		if (image.is_valid()) {
			int delta = ((static_cast<int16_t>(wparam >> 16) < 0) == settings.zoom.reverse_wheel)
				? +1 : -1;

			double ox = 0, oy = 0;
			if (Settings::ZoomPivot::cursor ==
				(DragState::is_dragging() ? settings.zoom.pivot_drag : settings.zoom.pivot)) {
				auto pt = cursor_pos(lparam);
				::ScreenToClient(hwnd, &pt);
				RECT rc; ::GetClientRect(hwnd, &rc);
				ox = pt.x - rc.right / 2.0;
				oy = pt.y - rc.bottom / 2.0;
			}
			if (apply_zoom(hwnd, loupe_state.zoom.scale_level + delta, ox, oy))
				redraw = true;
		}
		break;

	// option commands.
	case WM_LBUTTONDBLCLK:
		redraw = on_command(hwnd, settings.commands.left_dblclk, cursor_pos(lparam));
		break;
	case WM_RBUTTONDBLCLK:
		redraw = on_command(hwnd, settings.commands.right_dblclk, cursor_pos(lparam));
		break;
	case WM_MBUTTONUP:
		redraw = on_command(hwnd, settings.commands.middle_click, cursor_pos(lparam));
		break;

	case FilterMessage::MainMouseMove:
		// mouse move on the main window while wheel is down moves the loupe position.
		if (image.is_valid() &&
			(loupe_state.position.follow_cursor || ::GetKeyState(VK_MBUTTON) < 0)) {
			auto pt = cursor_pos(lparam);
			loupe_state.position.x = 0.5 + static_cast<double>(pt.x);
			loupe_state.position.y = 0.5 + static_cast<double>(pt.y);
			loupe_state.position.clamp(image.width(), image.height());

			redraw = ::IsWindowVisible(hwnd) != FALSE;
		}
		break;

	case WM_COMMAND:
		// menu commands.
		if (image.is_valid()) {
			if ((wparam >> 16) == 0 && lparam == 0) // criteria for the menu commands.
				redraw = Menu::on_menu_command(hwnd, wparam & 0xffff) &&
					::IsWindowVisible(hwnd) != FALSE;
		}
		break;

	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
		// if a drag operation is being held, cancel it with ESC key.
		if (wparam == VK_ESCAPE && DragState::DragCancel(redraw)) break;

		// ショートカットキーメッセージをメインウィンドウに丸投げする．
		if (fp->hwnd_parent != nullptr)
			::SendMessageW(fp->hwnd_parent, message, wparam, lparam);
		break;
	default:
		break;
	}

	if (redraw) {
		// when redrawing is required, proccess here.
		// returning TRUE from this function may cause flickering in the main window.
		if (image.is_valid() &&
			fp->exfunc->is_editing(editp) && !fp->exfunc->is_saving(editp)) draw(hwnd);
		else draw_blank(hwnd);
	}

	return FALSE;
}


////////////////////////////////
// Entry point.
////////////////////////////////
BOOL WINAPI DllMain(HINSTANCE hinst, DWORD fdwReason, LPVOID lpvReserved)
{
	switch (fdwReason) {
	case DLL_PROCESS_ATTACH:
		::DisableThreadLibraryCalls(hinst);
		break;
	}
	return TRUE;
}


////////////////////////////////
// 看板．
////////////////////////////////
#define PLUGIN_NAME		"色ルーペ"
#define PLUGIN_VERSION	"v1.13-beta2"
#define PLUGIN_AUTHOR	"sigma-axis"
#define PLUGIN_INFO_FMT(name, ver, author)	(name##" "##ver##" by "##author)
#define PLUGIN_INFO		PLUGIN_INFO_FMT(PLUGIN_NAME, PLUGIN_VERSION, PLUGIN_AUTHOR)

extern "C" __declspec(dllexport) FilterPluginDLL* __stdcall GetFilterTable(void)
{
	constexpr int initial_width = 256, initial_height = 256;

	// （フィルタとは名ばかりの）看板．
	using Flag = FilterPlugin::Flag;
	static constinit FilterPluginDLL filter{
		.flag = Flag::DispFilter | Flag::AlwaysActive | Flag::ExInformation |
			Flag::MainMessage | Flag::NoInitData |
			Flag::WindowThickFrame | Flag::WindowSize,
		.x = initial_width, .y = initial_height,
		.name = PLUGIN_NAME,

		.func_proc = func_proc,
		.func_WndProc = func_WndProc,
		.information = PLUGIN_INFO,
	};
	return &filter;
}
