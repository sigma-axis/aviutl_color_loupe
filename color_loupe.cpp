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

using byte = uint8_t;
#include <aviutl/filter.hpp>
using namespace AviUtl;

#include "color_abgr.hpp"
using namespace sigma_lib::W32::GDI;
#include "key_states.hpp"
using namespace sigma_lib::W32::UI;
#include "drag_states.hpp"
using namespace sigma_lib::W32::custom::mouse;

#include "resource.h"
HMODULE constinit this_dll = nullptr;

#include "settings.hpp"
#include "dialogs.hpp"

////////////////////////////////
// ルーペ状態の定義
////////////////////////////////
static inline constinit struct LoupeState {
	// position in the picture in pixels, relative to the top-left corner,
	// which is displayed at the center of the loupe window.
	struct Position {
		double x, y;
		bool follow_cursor = false;
		void clamp(int w, int h) {
			x = std::clamp<double>(x, 0, w);
			y = std::clamp<double>(y, 0, h);
		}
	} position{ 0,0 };

	// zoom --- manages the scaling ratio of zooming.
	struct Zoom {
		int zoom_level, second_level;
		constexpr static int zoom_level_min = -13, zoom_level_max = 20;

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
		static constexpr auto scale_ratio_Q(int zoom_level)
		{
			int n = scale_denominator, d = scale_denominator;
			if (zoom_level < 0) d = scale_base(-zoom_level);
			else n = scale_base(zoom_level);
			// as of either the numerator or the denominator is a power of 2, so is the GCD.
			auto gcd = n | d; gcd &= -gcd; // = std::gcd(n, d);
			return std::make_pair(n / gcd, d / gcd);
		}
		static constexpr double upscale(int zoom_level) { return (1.0 / scale_denominator) * scale_base(zoom_level); }
		static constexpr double downscale(int zoom_level) { return upscale(-zoom_level); }

		constexpr double scale_ratio() const { return scale_ratio(zoom_level); }
		constexpr double scale_ratio_inv() const { return scale_ratio_inv(zoom_level); }
		constexpr auto scale_ratios() const { return scale_ratios(zoom_level); }
		constexpr auto scale_ratio_Q() const { return scale_ratio_Q(zoom_level); }
	} zoom{ 8, 0 };

	// tip --- tooltip-like info.
	struct Tip {
		int x = 0, y = 0;
		uint8_t visible_level = 0; // 0: not visible, 1: not visible until mouse enters, >= 2: visible.
		constexpr bool is_visible() const { return visible_level > 1; }
		bool prefer_above = false;
	} tip;

	// toast --- notification message.
	struct Toast {
		constexpr static int max_len_message = 32;
		wchar_t message[max_len_message]{ L"" };
		bool visible = false;
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
		zoom.zoom_level = std::clamp(new_level, Zoom::zoom_level_min, Zoom::zoom_level_max);
		s -= zoom.scale_ratio_inv();

		position.x += s * win_ox; position.y += s * win_oy;
		position.clamp(pic_w, pic_h);
	}

	// resets some states that is suitable for a new sized image.
	void on_resize(int picture_w, int picture_h)
	{
		auto w2 = picture_w / 2, h2 = picture_h / 2;
		position.x = static_cast<double>(w2); position.y = static_cast<double>(h2);
		tip = { .x = w2, .y = h2, .visible_level = 0 };
	}
} loupe_state;
static_assert(LoupeState::Zoom::zoom_level_max == Settings::Zoom::zoom_level_max_max);
static_assert(LoupeState::Zoom::zoom_level_min == Settings::Zoom::zoom_level_min_min);

// define a function to export.
std::tuple<int, int> Settings::HelperFunctions::ScaleFromZoomLevel(int zoom_level) {
	return decltype(loupe_state.zoom)::scale_ratio_Q(zoom_level);
}


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
static inline void load_settings()
{
	char path[MAX_PATH];
	replace_tail(path, ::GetModuleFileNameA(this_dll, path, std::size(path)) + 1, "auf", "ini");

	settings.load(path);

	// additionally load the following loupe states.
	loupe_state.zoom.zoom_level = std::clamp(
		static_cast<int>(::GetPrivateProfileIntA("state", "zoom_level", loupe_state.zoom.zoom_level, path)),
		LoupeState::Zoom::zoom_level_min, LoupeState::Zoom::zoom_level_max);
	loupe_state.zoom.second_level = std::clamp(
		static_cast<int>(::GetPrivateProfileIntA("state", "zoom_second", loupe_state.zoom.second_level, path)),
		LoupeState::Zoom::zoom_level_min, LoupeState::Zoom::zoom_level_max);
	loupe_state.position.follow_cursor = ::GetPrivateProfileIntA("state", "follow_cursor",
		loupe_state.position.follow_cursor ? 1 : 0, path) != 0;
	loupe_state.grid.visible = ::GetPrivateProfileIntA("state", "show_grid",
		loupe_state.position.follow_cursor ? 1 : 0, path) != 0;
}
static inline void save_settings()
{
	char path[MAX_PATH];
	replace_tail(path, ::GetModuleFileNameA(this_dll, path, std::size(path)) + 1, "auf", "ini");

	settings.save(path);

	// additionally save the following loupe states.
	char buf[std::size("+4294967296")];
	std::snprintf(buf, std::size(buf), "%d", loupe_state.zoom.zoom_level);
	::WritePrivateProfileStringA("state", "zoom_level", buf, path);
	std::snprintf(buf, std::size(buf), "%d", loupe_state.zoom.second_level);
	::WritePrivateProfileStringA("state", "zoom_second", buf, path);
	::WritePrivateProfileStringA("state", "follow_cursor",
		loupe_state.position.follow_cursor ? "1" : "0", path);
	::WritePrivateProfileStringA("state", "show_grid",
		loupe_state.grid.visible ? "1" : "0", path);
}


////////////////////////////////
// 画像バッファ．
////////////////////////////////
static inline constinit class ImageBuffer {
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
		if (handle() != nullptr) {
			deleter(handle());
			handle() = nullptr;
		}
	}

	operator H() { return get(); }
	constexpr bool is_valid() const { return handle() != nullptr; }

	Handle(const Handle&) = delete;
	constexpr Handle(Handle&& that) : data{ that.data } { that.handle() = nullptr; }
	~Handle() { free(); }
};

static constinit Handle<HFONT, [](wchar_t const(*name)[], int8_t const* size) {
	return ::CreateFontW(
		*size, 0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE, ANSI_CHARSET,
		OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
		DEFAULT_PITCH | FF_DONTCARE, *name);
}, ::DeleteObject, wchar_t const(*)[], int8_t const*>
	tip_font{ &settings.tip_drag.font_name, &settings.tip_drag.font_size },
	toast_font{ &settings.toast.font_name, &settings.toast.font_size };

static constinit Handle<HMENU, []{ return ::LoadMenuW(this_dll, MAKEINTRESOURCEW(IDR_MENU_CXT)); }, ::DestroyMenu>
	cxt_menu{};


////////////////////////////////
// 通知メッセージのタイマー管理．
////////////////////////////////
static inline constinit class ToastManager {
	HWND hwnd = nullptr;
	bool active = false;
	constexpr static auto& toast = loupe_state.toast;

	auto timer_id() const { return reinterpret_cast<uintptr_t>(this); }
	static void CALLBACK timer_proc(HWND hwnd, auto, uintptr_t id, auto)
	{
		// turn the timer off.
		::KillTimer(hwnd, id);

		auto* that = reinterpret_cast<ToastManager*>(id);
		if (that == nullptr || !that->active || that->hwnd != hwnd) return; // might be a wrong call.

		// update the variable associated to the timer.
		that->active = false;

		// remove the toast.
		that->erase_core();
	}
	// hwnd must not be nullptr.
	void set_timer(int time_ms) {
		if (time_ms < USER_TIMER_MINIMUM) time_ms = USER_TIMER_MINIMUM;

		active = true;
		::SetTimer(hwnd, timer_id(), time_ms, timer_proc);
		// WM_TIMER won't be posted to the window procedure.
	}
	void kill_timer() {
		if (active && hwnd != nullptr) {
			::KillTimer(hwnd, timer_id());
			active = false;
		}
	}

	void erase_core() {
		if (toast.visible) {
			toast.visible = false;
			if (hwnd != nullptr)
				::PostMessageW(hwnd, WM_PAINT, {}, {}); // ::InvalidateRect() caused flickering.
		}
	}

public:
	bool is_active() const { return active; }
	HWND host_window() const { return hwnd; }

	// set nullptr when exitting.
	void set_host(HWND hwnd) {
		erase();
		this->hwnd = hwnd;
	}

	void set_message(int time_ms, uint32_t id, const auto&... args)
	{
		if (hwnd == nullptr) return;

		// load / format the message string from resource.
		if constexpr (sizeof...(args) > 0) {
			const wchar_t* ptr;
			::LoadStringW(this_dll, id, reinterpret_cast<wchar_t*>(&ptr), 0);
			std::swprintf(toast.message, std::size(toast.message), ptr, args...);
		}
		else ::LoadStringW(this_dll, id, toast.message, std::size(toast.message));
		toast.visible = true;

		// turn a timer on.
		set_timer(time_ms);
	}

	// removes the toast message.
	void erase()
	{
		kill_timer(); // turn a timer off if exists.
		erase_core(); // remove the toast.
	}
} toast_manager;


////////////////////////////////
// 外部リソース管理．
////////////////////////////////
static inline constinit class {
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
		toast_manager.erase();
	}
} ext_obj;


////////////////////////////////
// 描画関数．
////////////////////////////////

// 背景描画．
static inline void draw_backplane(HDC hdc, const RECT& rc)
{
	::SetDCBrushColor(hdc, settings.color.blank);
	::FillRect(hdc, &rc, static_cast<HBRUSH>(::GetStockObject(DC_BRUSH)));
}

// 画像描画．
static inline void draw_picture(HDC hdc, const RECT& vb, const RECT& vp)
{
	::SetStretchBltMode(hdc, STRETCH_DELETESCANS);
	::StretchDIBits(hdc, vp.left, vp.top, vp.right - vp.left, vp.bottom - vp.top,
		vb.left, image.height() - vb.bottom, vb.right - vb.left, vb.bottom - vb.top,
		image.buffer(), &image.info(), DIB_RGB_COLORS, SRCCOPY);
}

// グリッド描画 (thin)
static inline void draw_grid_thin(HDC hdc, const RECT& vb, const RECT& vp)
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
static inline void draw_grid_thick(HDC hdc, const RECT& vb, const RECT& vp)
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
static inline void draw_round_rect(HDC hdc, const RECT& rc, int radius, int thick, Color back_top, Color back_btm, Color chrome)
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
static inline void draw_tip(HDC hdc, int wd, int ht, const RECT& box,
	int pic_x, int pic_y, int pic_w, int pic_h, Color col, bool& prefer_above)
{
	RECT box_big = box;
	box_big.left -= settings.tip_drag.box_inflate; box_big.right += settings.tip_drag.box_inflate;
	box_big.top -= settings.tip_drag.box_inflate; box_big.bottom += settings.tip_drag.box_inflate;

	// draw the "pixel box".
	::SetDCBrushColor(hdc, col);
	::FillRect(hdc, &box_big, static_cast<HBRUSH>(::GetStockObject(DC_BRUSH)));
	::FrameRect(hdc, &box_big, static_cast<HBRUSH>(::GetStockObject(
		col.luma() <= Color::max_luma / 2 ? WHITE_BRUSH : BLACK_BRUSH)));

	// prepare text.
	constexpr int pad_x = 10, pad_y = 3, // ツールチップ矩形とテキストの間の空白．
		gap_v = 10, // ピクセルを囲む四角形とツールチップとの間の空白．
		margin = 4; // ウィンドウ端との最低保証空白．

	// prepare the string to place in.
	wchar_t tip_str[std::bit_ceil(
		std::max(std::size(L"#RRGGBB"), std::size(L"RGB(000,000,000)")) + 1 +
		std::max(std::size(L"X:1234, Y:1234"), std::size(L"X:-1234.5, Y:-1234.5")))];
	int tip_strlen = 0;
	switch (settings.tip_drag.color_fmt) {
		using enum Settings::ColorFormat;
	case dec3x3:
		tip_strlen += std::swprintf(tip_str, std::size(tip_str), L"RGB(%3u,%3u,%3u)\n", col.R, col.G, col.B);
		break;
	case hexdec6:
	default:
		tip_strlen += std::swprintf(tip_str, std::size(tip_str), L"#%06X\n", col.to_formattable());
		break;
	}
	switch (settings.tip_drag.coord_fmt) {
		using enum Settings::CoordFormat;
	case origin_center:
	{
		wchar_t fmt[] = L"X:%7.1f, Y:%7.1f";
		double x = pic_x - pic_w / 2.0, y = pic_y - pic_h / 2.0;
		if ((pic_w & 1) == 0) fmt[3] = L'5', fmt[5] = L'0'; // no half-integer.
		if (pic_w < 2000) fmt[3]--; // trim by 1 digit, it's rarely necessary.
		if ((pic_h & 1) == 0) fmt[12] = L'5', fmt[14] = L'0';
		if (pic_h < 2000) fmt[12]--;
		tip_strlen += std::swprintf(&tip_str[tip_strlen], std::size(tip_str) - tip_strlen, fmt, x, y);
		break;
	}
	case origin_top_left:
	default:
		tip_strlen += std::swprintf(&tip_str[tip_strlen], std::size(tip_str) - tip_strlen, L"X:%4d, Y:%4d", pic_x, pic_y);
		break;
	}

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
	draw_round_rect(hdc, rc_frm, settings.tip_drag.chrome_radius, settings.tip_drag.chrome_thick,
		settings.color.back_top, settings.color.back_bottom, settings.color.chrome);

	// then draw the text.
	::SetTextColor(hdc, settings.color.text);
	::SetBkMode(hdc, TRANSPARENT);
	::DrawTextW(hdc, tip_str, tip_strlen, &rc_txt, DT_CENTER | DT_TOP | DT_NOPREFIX | DT_NOCLIP);
	::SelectObject(hdc, tmp_fon);
}
// 通知メッセージトースト描画．
static inline void draw_toast(HDC hdc, int wd, int ht)
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
		using enum Settings::Toast::Placement;
	case top_left:
	case left:
	case bottom_left:
		rc_frm.left = margin;
		goto from_left;
	case top_right:
	case right:
	case bottom_right:
		rc_frm.right = wd - margin;
		rc_txt.right = rc_frm.right - pad_x;
		rc_txt.left = rc_txt.right - l;
		rc_frm.left = rc_txt.left - pad_x;
		break;
	default:
		rc_frm.left = wd / 2 - l / 2 - pad_x;
	from_left:
		rc_txt.left = rc_frm.left + pad_x;
		rc_txt.right = rc_txt.left + l;
		rc_frm.right = rc_txt.right + pad_x;
		break;
	}

	// then vertically.
	l = rc_txt.bottom - rc_txt.top;
	switch (settings.toast.placement) {
		using enum Settings::Toast::Placement;
	case top_left:
	case top:
	case top_right:
		rc_frm.top = margin;
		goto from_top;
	case bottom_left:
	case bottom:
	case bottom_right:
		rc_frm.bottom = ht - margin;
		rc_txt.bottom = rc_frm.bottom - pad_y;
		rc_txt.top = rc_txt.bottom - l;
		rc_frm.top = rc_txt.top - pad_y;
		break;
	default:
		rc_frm.top = ht / 2 - l / 2 - pad_y;
	from_top:
		rc_txt.top = rc_frm.top + pad_y;
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
static inline void draw_blank(HWND hwnd)
{
	RECT rc; ::GetClientRect(hwnd, &rc);
	HDC window_hdc = ::GetDC(hwnd);

	if (ext_obj.is_active() && loupe_state.toast.visible) {
		// needs to draw a toast. prepare a back buffer.
		HDC hdc = ::CreateCompatibleDC(window_hdc);
		auto tmp_bmp = ::SelectObject(hdc, ::CreateCompatibleBitmap(window_hdc, rc.right, rc.bottom));

		draw_backplane(hdc, rc);
		draw_toast(hdc, rc.right, rc.bottom);

		// copy to the front buffer and delete the back buffer.
		::BitBlt(window_hdc, 0, 0, rc.right, rc.bottom, hdc, 0, 0, SRCCOPY);
		::DeleteObject(::SelectObject(hdc, tmp_bmp));
		::DeleteDC(hdc);
	}
	else draw_backplane(window_hdc, rc);
	::ReleaseDC(hwnd, window_hdc);
}
// メインの描画関数．
static inline void draw(HWND hwnd)
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
		settings.grid.grid_thick(loupe_state.zoom.zoom_level) : 0;

	// whether the tip is visible.
	constexpr auto& tip = loupe_state.tip;
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
				}, tip.x, tip.y,
				image.width(), image.height(), image.color_at(tip.x, tip.y), tip.prefer_above);
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
// ドラッグ操作実装．
////////////////////////////////
struct DragCxt {
	EditHandle* editp;
	WPARAM wparam;
	bool redraw_loupe;
	bool redraw_main;
};
class DragState : public DragStateBase<DragCxt> {
	friend class VoidDragState;
protected:
	static std::pair<double, double> win2pic(const POINT& p) {
		RECT rc; ::GetClientRect(hwnd, &rc);
		return loupe_state.win2pic(p.x - rc.right / 2.0, p.y - rc.bottom / 2.0);
	}
	static POINT win2pic_i(const POINT& p) {
		auto [x, y] = win2pic(p);
		return { .x = static_cast<int>(std::floor(x)), .y = static_cast<int>(std::floor(y)) };
	}

	void Abort_core(context& cxt) override { Cancel_core(cxt); }

	virtual Settings::KeysActivate KeysActivate() { return {}; }
	virtual Settings::WheelZoom WheelZoom() { return settings.zoom.wheel; }

public:
	static void InitiateDrag(HWND hwnd, const POINT& drag_start, context& cxt);
	static Settings::WheelZoom CurrentWheelZoom() {
		auto* drag = current_drag<DragState>();
		if (drag == nullptr) return settings.zoom.wheel;
		return drag->WheelZoom();
	}
};

static inline constinit class LoupeDrag : public DragState {
	double revert_x{}, revert_y{}, curr_x{}, curr_y{};
	int revert_zoom{};
	constexpr static auto& pos = loupe_state.position;

protected:
	bool Ready_core(context& cxt) override
	{
		if (!image.is_valid()) return false;

		revert_x = curr_x = pos.x;
		revert_y = curr_y = pos.y;
		revert_zoom = loupe_state.zoom.zoom_level;
		::SetCursor(::LoadCursorW(nullptr, reinterpret_cast<PCWSTR>(IDC_HAND)));
		return true;
	}
	void Delta_core(const POINT& curr, context& cxt) override
	{
		double dx = curr.x - last_point.x, dy = curr.y - last_point.y;
		if (dx == 0 && dy == 0) return;

		auto s = loupe_state.zoom.scale_ratio_inv();
		curr_x -= s * dx; curr_y -= s * dy;

		std::tie(dx, dy) = snap_rail(curr_x - revert_x, curr_y - revert_y,
			(cxt.wparam & MK_SHIFT) != 0 ? settings.loupe_drag.rail_mode : RailMode::none,
			settings.loupe_drag.lattice);
		double px = revert_x, py = revert_y;
		if (settings.loupe_drag.lattice)
			px = std::round(2 * px) / 2, py = std::round(2 * py) / 2; // half-integral.
		px = std::clamp<double>(px + dx, 0, image.width());
		py = std::clamp<double>(py + dy, 0, image.height());

		if (px == pos.x && py == pos.y) return;
		pos.x = px; pos.y = py;
		cxt.redraw_loupe = true;
	}
	void Cancel_core(context& cxt) override
	{
		pos.x = revert_x; pos.y = revert_y;
		loupe_state.zoom.zoom_level = revert_zoom;
		cxt.redraw_loupe = true;
	}

	DragInvalidRange InvalidRange() override { return settings.loupe_drag.range; }
	Settings::KeysActivate KeysActivate() override { return settings.loupe_drag.keys; }
	Settings::WheelZoom WheelZoom() override { return settings.loupe_drag.wheel; }
} loupe_drag;

static inline constinit class TipDrag : public DragState {
	constexpr static auto& tip = loupe_state.tip;
	POINT init{};
	LoupeState::Tip revert{};

protected:
	bool Ready_core(context& cxt) override
	{
		if (!image.is_valid()) return false;

		revert = tip;

		bool hide_cursor = true;
		switch (settings.tip_drag.mode) {
			using enum Settings::TipDrag::Mode;
		case frail:
			tip.visible_level = 2;
			break;
		case stationary:
		case sticky:
		default:
			if (tip.is_visible()) {
				tip.visible_level = 0;
				hide_cursor = false;
			}
			else tip.visible_level = 2;
			break;
		}

		init = win2pic_i(drag_start);
		tip.x = init.x; tip.y = init.y;

		cxt.redraw_loupe = true;
		if (hide_cursor) ::SetCursor(nullptr);

		return true;
	}
	void Unready_core(context& cxt, bool was_valid) override
	{
		if (was_valid) return;
		tip = revert;
		cxt.redraw_loupe = true;
	}
	void Delta_core(const POINT& curr, context& cxt) override
	{
		if (!tip.is_visible()) return;

		auto [dx, dy] = win2pic(curr);
		std::tie(dx, dy) = snap_rail(dx - (0.5 + init.x), dy - (0.5 + init.y),
			(cxt.wparam & MK_SHIFT) != 0 ? settings.tip_drag.rail_mode : RailMode::none, true);
		auto x = init.x + static_cast<int>(dx), y = init.y + static_cast<int>(dy);

		if (x == tip.x && y == tip.y) return;
		tip.x = x; tip.y = y;
		cxt.redraw_loupe = true;
	}
	void End_core(context& cxt) override
	{
		if (!tip.is_visible()) return;

		switch (settings.tip_drag.mode) {
			using enum Settings::TipDrag::Mode;
		case frail:
			tip.visible_level = 0;
			cxt.redraw_loupe = true;
			break;
		}
		return;
	}
	void Cancel_core(context& cxt) override
	{
		tip.visible_level = 0;
		cxt.redraw_loupe = true;
	}

	DragInvalidRange InvalidRange() override { return settings.tip_drag.range; }
	Settings::KeysActivate KeysActivate() override { return settings.tip_drag.keys; }
	Settings::WheelZoom WheelZoom() override { return settings.tip_drag.wheel; }
} tip_drag;

static inline constinit class ExEditDrag : public DragState {
	POINT revert{}, last{};

	using FilterMessage = FilterPlugin::WindowMessage;
	constexpr static auto name_exedit = "拡張編集";

	static inline constinit FilterPlugin* exedit_fp = nullptr;
	static void send_message(context& cxt, UINT message, const POINT& pos, WPARAM btn)
	{
		constexpr auto force_btn = [](WPARAM wparam, WPARAM btn) {
			constexpr WPARAM all_btns = MK_LBUTTON | MK_MBUTTON | MK_RBUTTON | MK_XBUTTON1 | MK_XBUTTON2;
			return (wparam & ~all_btns) | btn;
		};
		constexpr auto code_pos = [](auto x, auto y) {
			return static_cast<LPARAM>((x & 0xffff) | (y << 16));
		};
		if (exedit_fp->func_WndProc != nullptr && exedit_fp->hwnd != nullptr)
			cxt.redraw_main |= exedit_fp->func_WndProc(exedit_fp->hwnd, message,
				force_btn(cxt.wparam, btn), code_pos(pos.x, pos.y), cxt.editp, exedit_fp) != FALSE;
	}
	static ForceKeyState key_fake(short vkey, Settings::ExEditDrag::KeyFake fake, auto&& curr) {
		switch (fake) {
			using enum Settings::ExEditDrag::KeyFake;
		case flat: return { ForceKeyState::vkey_invalid, false };
		case off: return { vkey, false };
		case on: return { vkey, true };
		case invert:
		default: return { vkey, !curr() };
		}
	}
	static void fake_wparam(const Settings::ExEditDrag& op, WPARAM& wp) {
		switch (op.fake_shift) {
			using enum Settings::ExEditDrag::KeyFake;
		case off:		wp &= ~MK_SHIFT; break;
		case on:		wp |= MK_SHIFT; break;
		case invert:	wp ^= MK_SHIFT; break;
		case flat: default: break;
		}
	}

public:
	static void init(FilterPlugin* this_fp) {
			if (exedit_fp != nullptr) return;

			SysInfo si; this_fp->exfunc->get_sys_info(nullptr, &si);
			for (int i = 0; i < si.filter_n; i++) {
				auto that_fp = this_fp->exfunc->get_filterp(i);
				if (that_fp->name != nullptr &&
					0 == std::strcmp(that_fp->name, name_exedit)) {
					exedit_fp = that_fp;
					return;
				}
			}
	}
	static bool is_valid() { return exedit_fp != nullptr; }

protected:
	bool Ready_core(context& cxt) override
	{
		if (!is_valid() || !image.is_valid()) return false;

		last = win2pic_i(last_point);
		if (0 <= last.x && last.x < image.width() &&
			0 <= last.y && last.y < image.height()) {
			revert = last;
			::SetCursor(::LoadCursorW(nullptr, reinterpret_cast<PCWSTR>(IDC_SIZEALL)));
			return true;
		}
		return false;
	}
	void Start_core(context& cxt) override
	{
		const auto& op = settings.exedit_drag;
		auto shift = key_fake(VK_SHIFT, op.fake_shift, [&]{ return (cxt.wparam & MK_SHIFT) != 0; }),
			alt = key_fake(VK_MENU, op.fake_alt, []{ return ::GetKeyState(VK_MENU) < 0; });
		fake_wparam(op, cxt.wparam);
		send_message(cxt, FilterMessage::MainMouseDown, last, MK_LBUTTON);
	}
	void Delta_core(const POINT& curr, context& cxt) override
	{
		auto pt = win2pic_i(curr);
		if (pt.x == last.x && pt.y == last.y) return;
		last = pt;

		const auto& op = settings.exedit_drag;
		auto shift = key_fake(VK_SHIFT, op.fake_shift, [&] { return (cxt.wparam & MK_SHIFT) != 0; }),
			alt = key_fake(VK_MENU, op.fake_alt, [] { return ::GetKeyState(VK_MENU) < 0; });
		fake_wparam(op, cxt.wparam);
		send_message(cxt, FilterMessage::MainMouseMove, last, MK_LBUTTON);
	}
	void End_core(context& cxt) override
	{
		const auto& op = settings.exedit_drag;
		auto shift = key_fake(VK_SHIFT, op.fake_shift, [&] { return (cxt.wparam & MK_SHIFT) != 0; }),
			alt = key_fake(VK_MENU, op.fake_alt, []{ return ::GetKeyState(VK_MENU) < 0; });
		fake_wparam(op, cxt.wparam);
		send_message(cxt, FilterMessage::MainMouseUp, last, 0);
	}
	void Cancel_core(context& cxt) override
	{
		cxt.wparam = 0;
		ForceKeyState shift{ VK_SHIFT, false }, alt{ VK_MENU, false };
		send_message(cxt, FilterMessage::MainMouseMove, revert, MK_LBUTTON);
		send_message(cxt, FilterMessage::MainMouseUp, revert, 0);
	}

	DragInvalidRange InvalidRange() override { return settings.exedit_drag.range; }
	Settings::KeysActivate KeysActivate() override { return settings.exedit_drag.keys; }
	Settings::WheelZoom WheelZoom() override { return settings.exedit_drag.wheel; }
} exedit_drag;

// placeholder drag. might be used as a fallback.
static inline constinit class VoidDragState : public VoidDrag<DragState> {
	DragState* surrogate = nullptr;

protected:
	// let resemble the "click-or-not" behavior to the origin of the fallback.
	DragInvalidRange InvalidRange() override {
		return surrogate != nullptr ? surrogate->InvalidRange() : VoidDrag<DragState>::InvalidRange();
	}

public:
	using context = DragState::context;
	using DragState::Start;
	bool Start(HWND hwnd, const POINT& drag_start, context& cxt, DragState* surrogate) {
		this->surrogate = surrogate;
		auto ret = Start(hwnd, drag_start, cxt);
		this->surrogate = nullptr;
		return ret;
	}
} void_drag;

inline void DragState::InitiateDrag(HWND hwnd, const POINT& drag_start, context& cxt)
{
	if (!ext_obj.is_active()) return;

	constexpr WPARAM btns = MK_LBUTTON | MK_RBUTTON | MK_MBUTTON | MK_XBUTTON1 | MK_XBUTTON2;
	auto btn = [&] {
		switch (cxt.wparam & btns) {
			using enum MouseButton;
		case MK_LBUTTON:	return left;
		case MK_RBUTTON:	return right;
		case MK_MBUTTON:	return middle;
		case MK_XBUTTON1:	return x1;
		case MK_XBUTTON2:	return x2;
		default:			return none; // no button or multiple buttons.
		}
	}();
	if (btn == MouseButton::none) return;

	bool ctrl = (cxt.wparam & MK_CONTROL) != 0,
		shift = (cxt.wparam & MK_SHIFT) != 0,
		alt = ::GetKeyState(VK_MENU) < 0;

	constexpr DragState* drags[] = { &loupe_drag, &tip_drag, &exedit_drag };
	DragState* chosen = nullptr;
	for (auto* drag : drags) {
		// check the button/key-combination condition.
		if (drag->KeysActivate().match(btn, ctrl, shift, alt)) {
			// then type-specific condition.
			if (drag->Start(hwnd, drag_start, cxt)) return;
			chosen = drag;
			break;
		}
	}

	// initiate the "placeholder drag".
	void_drag.Start(hwnd, drag_start, cxt, chosen);
}


////////////////////////////////
// その他コマンド．
////////////////////////////////
// returns true if the window needs redrawing.
static inline bool centralize()
{
	if (!image.is_valid()) return false;

	// centralize the target point.
	loupe_state.position.x = image.width() / 2.0;
	loupe_state.position.y = image.height() / 2.0;
	return true;
}
static inline bool centralize_point(double win_ox, double win_oy)
{
	if (!image.is_valid()) return false;

	// move the target point to the specified position.
	auto [x, y] = loupe_state.win2pic(win_ox, win_oy);
	loupe_state.position.x = std::clamp<double>(x, 0, image.width());
	loupe_state.position.y = std::clamp<double>(y, 0, image.height());
	return true;
}
static inline bool toggle_follow_cursor()
{
	loupe_state.position.follow_cursor ^= true;

	// toast message.
	if (!settings.toast.notify_follow_cursor) return false;
	toast_manager.set_message(settings.toast.duration,
		loupe_state.position.follow_cursor ? IDS_TOAST_FOLLOW_CURSOR_ON : IDS_TOAST_FOLLOW_CURSOR_OFF);
	return true;
}
static inline bool toggle_grid()
{
	loupe_state.grid.visible ^= true;

	// toast message.
	if (!settings.toast.notify_grid) return true;
	toast_manager.set_message(settings.toast.duration,
		loupe_state.grid.visible ? IDS_TOAST_GRID_ON : IDS_TOAST_GRID_OFF);
	return true;
}
static inline bool apply_zoom(int new_level, double win_ox, double win_oy)
{
	// ignore if the zoom level is identical.
	new_level = std::clamp(new_level,
		std::max<int>(settings.zoom.zoom_level_min, LoupeState::Zoom::zoom_level_min),
		std::min<int>(settings.zoom.zoom_level_max, LoupeState::Zoom::zoom_level_max));
	if (new_level == loupe_state.zoom.zoom_level) return false;

	// apply.
	if (image.is_valid())
		loupe_state.apply_zoom(new_level, win_ox, win_oy, image.width(), image.height());
	else loupe_state.zoom.zoom_level = new_level;

	// toast message.
	if (!settings.toast.notify_scale) return true;
	wchar_t scale[std::max(std::size(L"x 123/456"), std::max(std::size(L"x 123.45"), std::size(L"123.45%")))];
	switch (settings.toast.scale_format) {
		using enum Settings::Toast::ScaleFormat;
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
	toast_manager.set_message(settings.toast.duration, IDS_TOAST_SCALE_FORMAT, scale);
	return true;
}
static inline bool swap_zoom_level(double win_ox, double win_oy, Settings::WheelZoom::Pivot pivot)
{
	auto level = loupe_state.zoom.zoom_level;
	std::swap(level, loupe_state.zoom.second_level);

	if (pivot == Settings::WheelZoom::center) win_ox = win_oy = 0;
	return apply_zoom(level, win_ox, win_oy);
}
static bool copy_text(const wchar_t* text)
{
	if (!::OpenClipboard(nullptr)) return false;
	bool success = false;

	::EmptyClipboard();
	auto len = std::wcslen(text);
	if (auto h = ::GlobalAlloc(GHND, (len + 1) * sizeof(wchar_t))) {
		if (auto ptr = reinterpret_cast<wchar_t*>(::GlobalLock(h))) {
			std::memcpy(ptr, text, (len + 1) * sizeof(wchar_t));
			::GlobalUnlock(h);
			::SetClipboardData(CF_UNICODETEXT, h);

			success = true;
		}
		else ::GlobalFree(h);
	}
	::CloseClipboard();
	return success;
}
static inline bool copy_color_code(double win_ox, double win_oy)
{
	if (!image.is_valid()) return false;

	auto [x, y] = loupe_state.win2pic(win_ox, win_oy);
	auto color = image.color_at(static_cast<int>(std::floor(x)), static_cast<int>(std::floor(y)));
	if (color.A != 0) return false;

	wchar_t buf[std::max(std::size(L"rrggbb"), std::size(L"RGB(255,255,255)"))];
	switch (settings.commands.copy_color_fmt) {
		using enum Settings::ColorFormat;
	case dec3x3:
		std::swprintf(buf, std::size(buf), L"RGB(%u,%u,%u)", color.R, color.G, color.B);
		break;
	case hexdec6:
	default:
		std::swprintf(buf, std::size(buf), L"%06x", color.to_formattable());
		break;
	}
	if (!copy_text(buf)) return false;

	// toast message.
	if (!settings.toast.notify_clipboard) return false;
	toast_manager.set_message(settings.toast.duration, IDS_TOAST_CLIPBOARD, buf);
	return true;
}
static inline bool copy_coordinate(double win_ox, double win_oy)
{
	if (!image.is_valid()) return false;

	auto [x, y] = loupe_state.win2pic(win_ox, win_oy);
	int img_x = static_cast<int>(std::floor(x)),
		img_y = static_cast<int>(std::floor(y)),
		w = image.width(), h = image.height();
	if (!(0 <= img_x && img_x < w && 0 <= img_y && img_y < h)) return false;

	wchar_t buf[std::max(std::size(L"1234,1234"), std::size(L"-1234.5,-1234.5"))];
	switch (settings.commands.copy_coord_fmt) {
		using enum Settings::CoordFormat;
	case origin_center:
	{
		x = img_x - w / 2.0; y = img_y - h / 2.0;
		if (!(std::abs(x) < 10'000 && std::abs(y) < 10'000)) return false;

		wchar_t fmt[] = L"%.1f,%.1f";
		if ((w & 1) == 0) fmt[2] = L'0';
		if ((h & 1) == 0) fmt[7] = L'0';
		std::swprintf(buf, std::size(buf), fmt, x, y);
		break;
	}
	case origin_top_left:
	default:
		if (!(img_x < 10'000 && img_y < 10'000)) return false;
		std::swprintf(buf, std::size(buf), L"%d,%d", img_x, img_y);
		break;
	}
	if (!copy_text(buf)) return false;

	// toast message.
	if (!settings.toast.notify_clipboard) return false;
	toast_manager.set_message(settings.toast.duration, IDS_TOAST_CLIPBOARD, buf);
	return true;
}
// update the tip position when it's following the mouse cursor. returns true for all cases.
static inline bool tip_to_cursor(HWND hwnd)
{
	if (!loupe_state.tip.is_visible()
		|| (settings.tip_drag.mode != Settings::TipDrag::sticky &&
			DragState::current_drag<DragState>() != &tip_drag)) return true;

	POINT pt; ::GetCursorPos(&pt); ::ScreenToClient(hwnd, &pt);
	RECT rc; ::GetClientRect(hwnd, &rc);

	auto [x, y] = loupe_state.win2pic(pt.x - rc.right / 2.0, pt.y - rc.bottom / 2.0);
	loupe_state.tip.x = static_cast<int>(std::floor(x));
	loupe_state.tip.y = static_cast<int>(std::floor(y));
	return true;
}
static inline bool open_settings(HWND hwnd)
{
	if (dialogs::open_settings(hwnd)) {
		// hide the toast as its duration/visibility might have changed.
		toast_manager.erase();

		// hide the tip as settings for the tip might have changed.
		loupe_state.tip.visible_level = 0;

		// re-apply the zoom level, as its valid range might have changed.
		apply_zoom(loupe_state.zoom.zoom_level, 0, 0);

		// no need to discard font handles for new font settings,
		// as there's no option at this point yet.
		return true;
	}
	return false;
}


////////////////////////////////
// メニュー操作．
////////////////////////////////
struct Menu {
	static bool popup_menu(HWND hwnd, bool by_mouse)
	{
		if (!ext_obj.is_active()) return false;

		HMENU menu = cxt_menu;
		auto ena = [&](uint32_t id, bool enabled) { ::EnableMenuItem(menu, id, MF_BYCOMMAND | (enabled ? MF_ENABLED : MF_GRAYED)); };
		auto chk = [&](uint32_t id, bool check) { ::CheckMenuItem(menu, id, MF_BYCOMMAND | (check ? MF_CHECKED : MF_UNCHECKED)); };

		// prepare the context menu.
		ena(IDM_CXT_PT_COPY_COLOR,			by_mouse && image.is_valid());
		ena(IDM_CXT_PT_COPY_COORD,			by_mouse && image.is_valid());
		ena(IDM_CXT_PT_MOVE_CENTER,			by_mouse && image.is_valid());
		chk(IDM_CXT_FOLLOW_CURSOR,			loupe_state.position.follow_cursor);
		chk(IDM_CXT_SHOW_GRID,				loupe_state.grid.visible);
		ena(IDM_CXT_SWAP_ZOOM,				image.is_valid());
		ena(IDM_CXT_CENTRALIZE,				image.is_valid());
		chk(IDM_CXT_TIP_MODE_FRAIL,			settings.tip_drag.mode == Settings::TipDrag::frail);
		chk(IDM_CXT_TIP_MODE_STATIONARY,	settings.tip_drag.mode == Settings::TipDrag::stationary);
		chk(IDM_CXT_TIP_MODE_STICKY,		settings.tip_drag.mode == Settings::TipDrag::sticky);
		chk(IDM_CXT_REVERSE_WHEEL,			settings.zoom.wheel.reversed);

		POINT pt; ::GetCursorPos(&pt);
		auto id = ::TrackPopupMenuEx(::GetSubMenu(menu, 0),
			TPM_RIGHTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD, pt.x, pt.y, hwnd, nullptr);

		return id != 0 && on_menu_command(hwnd, id, by_mouse, pt);
	}
	// returns true if needs redraw.
	static bool on_menu_command(HWND hwnd, uint32_t id, bool by_mouse, const POINT& pt_screen = {0, 0})
	{
		if (!ext_obj.is_active()) return false;

		auto rel_win_center = [&] {
			POINT pt = pt_screen; ::ScreenToClient(hwnd, &pt);
			RECT rc; ::GetClientRect(hwnd, &rc);
			return std::make_pair(pt.x - rc.right / 2.0, pt.y - rc.bottom / 2.0);
		};

		switch (id) {
		case IDM_CXT_PT_COPY_COLOR:
			if (by_mouse) {
				auto [x, y] = rel_win_center();
				return copy_color_code(x, y);
			}
			break;
		case IDM_CXT_PT_COPY_COORD:
			if (by_mouse) {
				auto [x, y] = rel_win_center();
				return copy_coordinate(x, y);
			}
			break;
		case IDM_CXT_PT_MOVE_CENTER:
			if (by_mouse) {
				auto [x, y] = rel_win_center();
				return centralize_point(x, y);
			}
			break;

		case IDM_CXT_FOLLOW_CURSOR:	return toggle_follow_cursor();
		case IDM_CXT_SHOW_GRID:		return toggle_grid();
		case IDM_CXT_SWAP_ZOOM:
		{
			double x = 0, y = 0;
			auto pivot = Settings::WheelZoom::center;
			if (by_mouse) {
				std::tie(x, y) = rel_win_center();
				pivot = settings.commands.swap_zoom_level_pivot;
			}
			return swap_zoom_level(x, y, pivot) && tip_to_cursor(hwnd);
		}
		case IDM_CXT_CENTRALIZE:	return centralize();

		case IDM_CXT_REVERSE_WHEEL:
			settings.loupe_drag.wheel.reversed	^= true;
			settings.tip_drag.wheel.reversed	^= true;
			settings.exedit_drag.wheel.reversed	^= true;
			settings.zoom.wheel.reversed		^= true;
			break;

		case IDM_CXT_TIP_MODE_FRAIL:		settings.tip_drag.mode = Settings::TipDrag::frail;		goto hide_tip_and_redraw;
		case IDM_CXT_TIP_MODE_STATIONARY:	settings.tip_drag.mode = Settings::TipDrag::stationary;	goto hide_tip_and_redraw;
		case IDM_CXT_TIP_MODE_STICKY:		settings.tip_drag.mode = Settings::TipDrag::sticky;		goto hide_tip_and_redraw;
		hide_tip_and_redraw:
			loupe_state.tip.visible_level = 0;
			return true;

		case IDM_CXT_SETTINGS:
			return open_settings(hwnd);

		default: break;
		}
		return false;
	}
};


////////////////////////////////
// AviUtlに渡す関数の定義．
////////////////////////////////
static inline void on_update(int w, int h, void* source)
{
	if (source != nullptr && image.update(w, h, source))
		// notify the loupe of resizing.
		loupe_state.on_resize(w, h);
}
static inline void on_command(bool& redraw_loupe, HWND hwnd, Settings::ClickActions::Command cmd, const POINT& pt)
{
	if (!ext_obj.is_active()) return;

	auto rel_win_center = [&] {
		RECT rc; ::GetClientRect(hwnd, &rc);
		return std::make_pair(pt.x - rc.right / 2.0, pt.y - rc.bottom / 2.0);
	};

	switch (cmd) {
		using ca = Settings::ClickActions;
	case ca::swap_zoom_level:
	{
		auto [x, y] = rel_win_center();
		redraw_loupe |= swap_zoom_level(x, y, settings.commands.swap_zoom_level_pivot) && tip_to_cursor(hwnd);
		break;
	}
	case ca::copy_color_code:
	{
		auto [x, y] = rel_win_center();
		redraw_loupe |= copy_color_code(x, y);
		break;
	}
	case ca::toggle_follow_cursor:	redraw_loupe |= toggle_follow_cursor();	break;
	case ca::centralize:			redraw_loupe |= centralize();			break;
	case ca::toggle_grid:			redraw_loupe |= toggle_grid();			break;
	case ca::zoom_step_down:
	case ca::zoom_step_up:
	{
		int steps = settings.commands.step_zoom_num_steps;
		if (cmd == ca::zoom_step_down) steps = -steps;
		double x = 0, y = 0;
		if (settings.commands.step_zoom_pivot != Settings::WheelZoom::center)
			std::tie(x, y) = rel_win_center();
		redraw_loupe |= apply_zoom(loupe_state.zoom.zoom_level + steps, x, y);
		break;
	}
	case ca::copy_coord:
	{
		auto [x, y] = rel_win_center();
		redraw_loupe |= copy_coordinate(x, y);
		break;
	}

	case ca::settings:
		redraw_loupe |= open_settings(hwnd);
		break;
	case ca::context_menu:
		// should redraw the window before the context menu popups.
		if (redraw_loupe) ::InvalidateRect(hwnd, nullptr, FALSE);
		redraw_loupe = Menu::popup_menu(hwnd, true);
		break;
	}
}

static BOOL func_proc(FilterPlugin* fp, FilterProcInfo* fpip)
{
	// updates to the target image.
	if (ext_obj.is_active() &&
		fp->exfunc->is_editing(fpip->editp) && !fp->exfunc->is_saving(fpip->editp)) {

		on_update(fpip->w, fpip->h, fp->exfunc->get_disp_pixelp(fpip->editp, 0));
		draw(fp->hwnd);
	}
	return TRUE;
}

static BOOL func_WndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam, EditHandle* editp, FilterPlugin* fp)
{
	using FilterMessage = FilterPlugin::WindowMessage;
	constexpr auto cursor_pos = [](LPARAM l) {
		return POINT{ .x = static_cast<int16_t>(0xffff & l), .y = static_cast<int16_t>(l >> 16) };
	};
	DragState::context cxt{ .editp = editp, .wparam = wparam, .redraw_loupe = false, .redraw_main = false };

	static constinit bool track_mouse_event_sent = false;
	switch (message) {
	case FilterMessage::Init:
		this_dll = fp->dll_hinst;

		// load settings.
		load_settings();

		// disable IME.
		::ImmReleaseContext(hwnd, ::ImmAssociateContext(hwnd, nullptr));

		// find exedit.
		exedit_drag.init(fp);

		// activate the toast manager.
		toast_manager.set_host(hwnd);
		break;

	case FilterMessage::Exit:
		// deactivate the toast manager.
		toast_manager.set_host(nullptr);

		// make sure new allocation would no longer occur.
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
			cxt.redraw_loupe = true;
		}
		else ext_obj.deactivate(), DragState::Abort(cxt);
		break;
	case FilterMessage::FileClose:
		ext_obj.free();
		DragState::Abort(cxt);

		// clear the window when a file is closed.
		if (::IsWindowVisible(hwnd) != FALSE)
			draw_blank(hwnd); // can't use `cxt.redraw_loupe = true` because exfunc->is_editing() is true at this moment.
		break;

	case WM_PAINT:
		cxt.redraw_loupe = true;
		break;

		// UI handlers for mouse messages.
	case WM_LBUTTONDOWN:
	case WM_RBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_XBUTTONDOWN:
		// cancel an existing drag operation.
		if (!DragState::Cancel(cxt))
			// if nothing is canceled, try to initiate a new.
			DragState::InitiateDrag(hwnd, cursor_pos(lparam), cxt);
		break;

	case WM_LBUTTONUP:
	case WM_RBUTTONUP:
	case WM_MBUTTONUP:
	case WM_XBUTTONUP:
		// finish dragging.
		if (!DragState::End(cxt)) {
			// now recognized as a single click.

			// find the assinged command.
			auto& btn = [&]() -> Settings::ClickActions::Button& {
				switch (message) {
				case WM_LBUTTONUP: return settings.commands.left;
				case WM_RBUTTONUP: return settings.commands.right;
				case WM_MBUTTONUP: return settings.commands.middle;
				case WM_XBUTTONUP:
				default:
					if ((wparam >> 16) == XBUTTON1)
						return settings.commands.x1;
					else
						return settings.commands.x2;
				}
			}();

			// handle that command.
			on_command(cxt.redraw_loupe, hwnd, btn.click, cursor_pos(lparam));
		}
		break;
	case WM_CAPTURECHANGED:
		// abort dragging.
		DragState::Abort(cxt);
		break;

	case WM_MOUSEMOVE:
		if (DragState::Delta(cursor_pos(lparam), cxt)) /* empty */;
		else if (!image.is_valid()) break;
		else if (settings.tip_drag.mode == Settings::TipDrag::sticky && loupe_state.tip.visible_level > 0) {
			// update to the tip of "sticky" mode.
			auto pt = cursor_pos(lparam);
			RECT rc; ::GetClientRect(hwnd, &rc);
			auto [x, y] = loupe_state.win2pic(pt.x - rc.right / 2.0, pt.y - rc.bottom / 2.0);
			auto X = static_cast<int>(std::floor(x)), Y = static_cast<int>(std::floor(y));

			if (!loupe_state.tip.is_visible() ||
				X != loupe_state.tip.x || Y != loupe_state.tip.y) {

				loupe_state.tip.visible_level = 2;
				loupe_state.tip.x = X;
				loupe_state.tip.y = Y;

				cxt.redraw_loupe = true;
			}
		}
		if (!track_mouse_event_sent &&
			settings.tip_drag.mode == Settings::TipDrag::sticky && loupe_state.tip.is_visible()) {
			track_mouse_event_sent = true;
			// to make WM_MOUSELEAVE message be sent.
			TRACKMOUSEEVENT tme{ .cbSize = sizeof(TRACKMOUSEEVENT), .dwFlags = TME_LEAVE, .hwndTrack = hwnd };
			::TrackMouseEvent(&tme);
		}
		break;
	case WM_MOUSELEAVE:
		track_mouse_event_sent = false;
		// hide the "sticky" tip.
		if (settings.tip_drag.mode == Settings::TipDrag::sticky && loupe_state.tip.is_visible()) {
			loupe_state.tip.visible_level = 1;
			cxt.redraw_loupe = true;
		}
		break;

	case WM_MOUSEWHEEL:
		// wheel to zoom in/out.
		if (ext_obj.is_active()){
			auto zoom = DragState::CurrentWheelZoom();
			if (!zoom.enabled) break;

			// if dragging, it's no longer recognized as a click.
			DragState::Validate(cxt);

			// find the number of steps.
			int delta = zoom.num_steps;

			// reverse the wheel if necessary.
			if ((static_cast<int16_t>(wparam >> 16) < 0) ^ zoom.reversed) delta = -delta;

			// take the pivot into account.
			double ox = 0, oy = 0;
			if (zoom.pivot != Settings::WheelZoom::center) {
				auto pt = cursor_pos(lparam);
				::ScreenToClient(hwnd, &pt);
				RECT rc; ::GetClientRect(hwnd, &rc);
				ox = pt.x - rc.right / 2.0;
				oy = pt.y - rc.bottom / 2.0;
			}

			// then apply the zoom.
			cxt.redraw_loupe |= apply_zoom(loupe_state.zoom.zoom_level + delta, ox, oy) && tip_to_cursor(hwnd);
		}
		break;

	// option commands.
	case WM_LBUTTONDBLCLK:
		on_command(cxt.redraw_loupe, hwnd, settings.commands.left.dblclk, cursor_pos(lparam));
		break;
	case WM_RBUTTONDBLCLK:
		on_command(cxt.redraw_loupe, hwnd, settings.commands.right.dblclk, cursor_pos(lparam));
		break;
	case WM_MBUTTONDBLCLK:
		on_command(cxt.redraw_loupe, hwnd, settings.commands.middle.dblclk, cursor_pos(lparam));
		break;
	case WM_XBUTTONDBLCLK:
		on_command(cxt.redraw_loupe, hwnd, ((wparam >> 16) == XBUTTON1 ?
			settings.commands.x1 : settings.commands.x2).dblclk, cursor_pos(lparam));
		break;

	case FilterMessage::MainMouseMove:
		// mouse move on the main window while wheel is down moves the loupe position.
		if (image.is_valid() &&
			(loupe_state.position.follow_cursor || (wparam & MK_MBUTTON) != 0)) {
			auto pt = cursor_pos(lparam);
			loupe_state.position.x = 0.5 + static_cast<double>(pt.x);
			loupe_state.position.y = 0.5 + static_cast<double>(pt.y);
			loupe_state.position.clamp(image.width(), image.height());

			cxt.redraw_loupe = ::IsWindowVisible(hwnd) != FALSE;
		}
		break;

	case WM_COMMAND:
		// menu commands.
		if ((wparam >> 16) == 0 && lparam == 0) // criteria for the menu commands.
			cxt.redraw_loupe = Menu::on_menu_command(hwnd, wparam & 0xffff, false);
		break;

	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
		if (wparam == VK_ESCAPE) {
			// if a drag operation is being held, cancel it with ESC key.
			if (DragState::Cancel(cxt)) break;
		}
		else if (wparam == VK_F10 && (lparam & KF_ALTDOWN) == 0 && message == WM_SYSKEYDOWN
			&& ::GetKeyState(VK_SHIFT) < 0 && ::GetKeyState(VK_CONTROL) >= 0) {
			// Shift + F10:
			// hard-coded shortcut key that shows up the context menu.
			cxt.redraw_loupe = Menu::popup_menu(hwnd, false);
			break;
		}
		[[fallthrough]];
	case WM_KEYUP:
	case WM_SYSKEYUP:
		// ショートカットキーメッセージをメインウィンドウに丸投げする．
		if (fp->hwnd_parent != nullptr)
			::SendMessageW(fp->hwnd_parent, message, wparam, lparam);
		break;
	default:
		break;
	}

	if (cxt.redraw_loupe) {
		// when redrawing is required, proccess here.
		// returning TRUE from this function may cause flickering in the main window.
		if (image.is_valid() &&
			fp->exfunc->is_editing(editp) && !fp->exfunc->is_saving(editp)) draw(hwnd);
		else draw_blank(hwnd);
	}

	return cxt.redraw_main ? TRUE : FALSE;
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
#define PLUGIN_VERSION	"v2.00-beta2"
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
