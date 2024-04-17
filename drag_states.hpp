/*
The MIT License (MIT)

Copyright (c) 2024 sigma-axis

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#pragma once

#include <cstdint>
#include <cmath>
#include <concepts>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

////////////////////////////////
// ドラッグ操作の基底クラス．
////////////////////////////////
namespace sigma_lib::W32::custom::mouse
{
	enum class MouseButton : uint8_t {
		none = 0,
		left = 1,
		right = 2,
		middle = 3,
		x1 = 4,
		x2 = 5,
	};
	enum class RailMode : uint8_t {
		none = 0, cross = 1, octagonal = 2,
	};
	struct DragInvalidRange {
		// distance of locations where the mouse button was
		// pressed and released, measured in pixels.
		int16_t distance;
		// time taken from the mouse button was pressed
		// until it's released, measured in milli seconds.
		int16_t timespan;

		constexpr bool is_distance_valid(int dist) const {
			return cmp_d(dist, distance);
		}
		constexpr bool is_distance_valid(int dx, int dy) const {
			return cmp_d(dx, distance) || cmp_d(dy, distance);
		}
		constexpr bool distance_always_invalid() const { return distance == invalid_val; }

		constexpr bool is_timespan_valid(int time) const {
			return cmp_t(time, timespan);
		}
		constexpr bool timespan_always_invalid() const { return timespan == invalid_val; }

		constexpr bool is_valid(int dist, int time) const {
			return is_distance_valid(dist) || is_timespan_valid(time);
		}
		constexpr bool is_valid(int dx, int dy, int time) const {
			return is_distance_valid(dx, dy) || is_timespan_valid(time);
		}
		constexpr bool is_always_invalid() const { return distance_always_invalid() && timespan_always_invalid(); }

		consteval static DragInvalidRange AlwaysValid() { return { -1, 0 }; }
		consteval static DragInvalidRange AlwaysInvalid() { return { invalid_val, invalid_val }; }

	private:
		constexpr static int16_t invalid_val = 0x7fff;
		constexpr static bool cmp_d(int val, int16_t threshold) {
			return threshold != invalid_val && (val < 0 ? -val : val) > threshold;
		}
		constexpr static bool cmp_t(int val, int16_t threshold) {
			return threshold != invalid_val && val > threshold;
		}

	public:
		constexpr static int16_t distance_min = -1, distance_max = 64;
		constexpr static int16_t timespan_min = 0, timespan_max = invalid_val;
	};

	template<class Context>
	class DragStateBase {
		inline static constinit DragStateBase* current = nullptr;

		static inline constinit auto invalid_range = DragInvalidRange::AlwaysInvalid();
		static inline bool was_validated = false;
		static bool is_distance_valid(const POINT& curr) {
			return invalid_range.is_distance_valid(
				curr.x - drag_start.x, curr.y - drag_start.y);
		}

		static inline uintptr_t timer_id = 0;
		static void CALLBACK timer_proc(HWND hwnd, auto, uintptr_t id, auto)
		{
			// turn the timer off.
			::KillTimer(hwnd, id);

			if (hwnd != nullptr || id == 0 || id != timer_id) return; // might be a wrong call.
			timer_id = 0; // the timer is no longer alive.

			if (DragStateBase::hwnd == nullptr || current == nullptr) return; // wrong states. ignore.

			// prepare and send a "fake" message so the drag will turn into the "validated" state.
			constexpr auto make_lparam = [] {
				POINT pt;
				::GetCursorPos(&pt); ::ScreenToClient(DragStateBase::hwnd, &pt);
				return static_cast<LPARAM>((0xffff & pt.x) | (pt.y << 16));
			};
			constexpr auto make_wparam = [] {
				int8_t ks[256];
				// one of the flaws in windows.h.
				std::ignore = ::GetKeyboardState(reinterpret_cast<uint8_t*>(ks));
				WPARAM ret = 0;
				if (ks[VK_LBUTTON]	< 0) ret |= MK_LBUTTON;
				if (ks[VK_RBUTTON]	< 0) ret |= MK_RBUTTON;
				if (ks[VK_MBUTTON]	< 0) ret |= MK_MBUTTON;
				if (ks[VK_XBUTTON1]	< 0) ret |= MK_XBUTTON1;
				if (ks[VK_XBUTTON2]	< 0) ret |= MK_XBUTTON2;
				if (ks[VK_CONTROL]	< 0) ret |= MK_CONTROL;
				if (ks[VK_SHIFT]	< 0) ret |= MK_SHIFT;
				return ret;
			};
			// make sure to be recognized as "valid".
			invalid_range = DragInvalidRange::AlwaysValid();
			::PostMessageW(DragStateBase::hwnd, WM_MOUSEMOVE, make_wparam(), make_lparam());
		}
		static void set_timer(int time_ms) {
			if (time_ms < USER_TIMER_MINIMUM) time_ms = USER_TIMER_MINIMUM;
			timer_id = ::SetTimer(nullptr, timer_id, time_ms, timer_proc);
		}
		static void kill_timer() {
			if (timer_id != 0) {
				::KillTimer(nullptr, timer_id);
				timer_id = 0;
			}
		}

	public:
		using context = Context;

	protected:
		static inline constinit HWND hwnd{ nullptr };
		static inline constinit POINT drag_start{}, last_point{}; // window coordinates.
		static inline constinit MouseButton button = MouseButton::none;

		// should return true if ready to initiate the drag.
		virtual bool Ready_core(context& cxt) = 0;
		virtual void Unready_core(context& cxt, bool was_valid) {}

		virtual void Start_core(context& cxt) {};
		virtual void Delta_core(const POINT& curr, context& cxt) {}
		virtual void End_core(context& cxt) {}
		virtual void Cancel_core(context& cxt) {}
		virtual void Abort_core(context& cxt) {}

		// determines whether a short drag should be recognized as a single click.
		virtual DragInvalidRange InvalidRange() { return DragInvalidRange::AlwaysValid(); }

		// helper function for the "directional snapping" feature.
		static constexpr std::pair<double, double> snap_rail(double x, double y, RailMode mode, bool lattice) {
			double lx = x < 0 ? -x : x, ly = y < 0 ? -y : y; //double lx = std::abs(x), ly = std::abs(y);
			// std::abs() isn't constexpr at this moment of writing this code...
			switch (mode) {
			case RailMode::cross:
				// '+'-shape.
				(lx < ly ? x : y) = 0;
				break;
			case RailMode::octagonal:
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
			case RailMode::none:
			default:
				break;
			}
			if (lattice) { x = std::round(x); y = std::round(y); }
			return { x, y };
		}

	public:
		// returns true if the dragging is made ready. button must not be MouseButton::none.
		bool Start(HWND hwnd, MouseButton button, const POINT& drag_start, context& cxt)
		{
			if (is_dragging(cxt)) return false;

			DragStateBase::hwnd = hwnd;
			DragStateBase::drag_start = last_point = drag_start;
			DragStateBase::button = button;

			if (Ready_core(cxt)) {
				invalid_range = InvalidRange();
				was_validated = false;

				current = this;
				::SetCapture(hwnd);

				// instantly start drag operation if the range insisits so.
				if (invalid_range.is_valid(0, 0)) {
					was_validated = true;
					current->Start_core(cxt);
				}
				else if (!invalid_range.timespan_always_invalid())
					set_timer(invalid_range.timespan);
				return true;
			}
			else {
				DragStateBase::hwnd = nullptr;
				DragStateBase::button = MouseButton::none;
				return false;
			}
		}
		static void Validate(context& cxt)
		{
			if (was_validated || !is_dragging(cxt)) return;

			was_validated = true;
			kill_timer();
			current->Start_core(cxt);
		}
		// returns true if the dragging operation has been properly processed.
		static bool Delta(const POINT& curr, context& cxt)
		{
			if (!is_dragging(cxt)) return false;

			// ignore the input until it exceeds a certain range.
			if (!was_validated && is_distance_valid(curr)) {
				was_validated = true;
				kill_timer();
				current->Start_core(cxt);
			}
			if (was_validated) {
				current->Delta_core(curr, cxt);
				last_point = curr;
			}
			return true;
		}
		// returns true if the dragging operation has been properly canceled.
		static bool Cancel(context& cxt)
		{
			if (!is_dragging(cxt)) return false;

			kill_timer();
			if (was_validated) current->Cancel_core(cxt);
			current->Unready_core(cxt, was_validated);

			hwnd = nullptr;
			button = MouseButton::none;
			current = nullptr;

			::ReleaseCapture();
			return true;
		}
		// returns whether the button-up should be recognized as a single click.
		// actually ends dragging only if the button used for dragging matches up_button.
		static struct { bool is_click; } End(MouseButton up_button, context& cxt)
		{
			if (!is_dragging(cxt)) return { false }; // dragging must have been canceled.
			if (up_button != button) return { true }; // different button, maybe a single click.

			kill_timer();
			if (was_validated) current->End_core(cxt);
			current->Unready_core(cxt, was_validated);

			hwnd = nullptr;
			button = MouseButton::none;
			current = nullptr;

			::ReleaseCapture();
			return { !was_validated };
		}
		// returns true if there sure was a dragging state that is now aborted.
		static bool Abort(context& cxt)
		{
			if (current == nullptr) return false;

			kill_timer();
			if (was_validated) current->Abort_core(cxt);
			current->Unready_core(cxt, was_validated);

			auto tmp = hwnd;
			hwnd = nullptr;
			button = MouseButton::none;
			current = nullptr;

			if (tmp != nullptr && ::GetCapture() == tmp)
				::ReleaseCapture();
			return true;
		}
		// verifies the dragging status, might abort if the window is no longer captured.
		static bool is_dragging(context& cxt) {
			if (current != nullptr) {
				if (hwnd != nullptr &&
					hwnd == ::GetCapture()) return true;

				Abort(cxt);
			}
			return false;
		}

		template<std::derived_from<DragStateBase> DragState = DragStateBase>
		static DragState* current_drag() { return dynamic_cast<DragState*>(current); }

		// make sure derived classes finalize their fields.
		virtual ~DragStateBase() {}
	};

	template<class DragBase>
		requires(std::derived_from<DragBase, DragStateBase<class DragBase::context>>)
	class VoidDrag : public DragBase {
		using context = DragBase::context;
	protected:
		bool Ready_core(context& cxt) override { return true; }
		void Unready_core(context& cxt, bool was_valid) override {}

		void Start_core(context& cxt) override {}
		void Delta_core(const POINT& curr, context& cxt) override {}
		void End_core(context& cxt) override {}
		void Cancel_core(context& cxt) override {}
		void Abort_core(context& cxt) override {}

		DragInvalidRange InvalidRange() override { return DragInvalidRange::AlwaysInvalid(); }
	};
}
