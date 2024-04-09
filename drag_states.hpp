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
#include <chrono>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

////////////////////////////////
// ドラッグ操作の基底クラス．
////////////////////////////////
namespace sigma_lib::W32::custom::mouse
{
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

		constexpr bool is_valid(int dist, int time) const {
			return cmp_d(dist, distance) || cmp_t(time, timespan);
		}
		constexpr bool is_valid(int dx, int dy, int time) const {
			return cmp_d(dx, distance) || cmp_d(dy, distance) || cmp_t(time, timespan);
		}
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

	template<class DragContext>
	class DragStateBase {
		inline static constinit DragStateBase* current = nullptr;

		using clock = std::chrono::steady_clock;
		static inline constinit clock::time_point start_time{};
		static inline constinit auto invalid_range = DragInvalidRange::AlwaysInvalid();
		static inline bool was_validated = false;
		static bool check_is_valid(const POINT& curr) {
			return invalid_range.is_valid(
				curr.x - drag_start.x, curr.y - drag_start.y,
				static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - start_time).count()));
		}

	public:
		using context = DragContext;

	protected:
		static inline constinit HWND hwnd{ nullptr };
		static inline constinit POINT drag_start{}, last_point{}; // window coordinates.

		// should return true if ready to initiate the drag.
		virtual bool DragReady_core(context& cxt) = 0;
		virtual void DragUnready_core(context& cxt) {}

		virtual void DragStart_core(context& cxt) {};
		virtual void DragDelta_core(const POINT& curr, context& cxt) {}
		virtual void DragEnd_core(context& cxt) {}
		virtual void DragCancel_core(context& cxt) {}
		virtual void DragAbort_core(context& cxt) {}

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
		// returns true if the dragging is made ready.
		bool DragStart(HWND hwnd, const POINT& drag_start, context& cxt)
		{
			if (is_dragging(cxt)) return false;

			DragStateBase::hwnd = hwnd;
			DragStateBase::drag_start = last_point = drag_start;

			if (DragReady_core(cxt)) {
				start_time = clock::now();
				invalid_range = InvalidRange();
				was_validated = false;

				current = this;
				::SetCapture(hwnd);

				// instantly start drag operation if the range insisits so.
				if (invalid_range.is_valid(0, 0)) {
					was_validated = true;
					current->DragStart_core(cxt);
				}
				return true;
			}
			else {
				DragStateBase::hwnd = nullptr;
				return false;
			}
		}
		// returns true if the dragging operation has been properly processed.
		static bool DragDelta(const POINT& curr, context& cxt, bool force = false)
		{
			if (!is_dragging(cxt)) return false;

			// ignore the input until it exceeds a certain range.
			if (!was_validated && (force || check_is_valid(curr))) {
				was_validated = true;
				current->DragStart_core(cxt);
			}
			if (was_validated) {
				current->DragDelta_core(curr, cxt);
				last_point = curr;
			}
			return true;
		}
		// returns true if the dragging operation has been properly canceled.
		static bool DragCancel(context& cxt)
		{
			if (!is_dragging(cxt)) return false;

			if (was_validated) current->DragCancel_core(cxt);
			current->DragUnready_core(cxt);

			hwnd = nullptr;
			current = nullptr;

			::ReleaseCapture();
			return true;
		}
		// returns false if the dragging operation should be recognized as a single click.
		static bool DragEnd(context& cxt)
		{
			if (!is_dragging(cxt)) return true; // dragging must have been canceled.

			if (was_validated) current->DragEnd_core(cxt);
			else if (check_is_valid(drag_start))
				// the mouse stayed so long it can't be recognized as a click.
				was_validated = true;
			current->DragUnready_core(cxt);

			hwnd = nullptr;
			current = nullptr;

			::ReleaseCapture();
			return was_validated;
		}
		// returns true if there sure was a dragging state that is now aborted.
		static bool DragAbort(context& cxt)
		{
			if (current == nullptr) return false;

			if (was_validated) current->DragAbort_core(cxt);
			current->DragUnready_core(cxt);

			auto tmp = hwnd;
			hwnd = nullptr;
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

				DragAbort(cxt);
			}
			return false;
		}

		template<class DragState>
		static DragState* current_drag() { return dynamic_cast<DragState*>(current); }

		// make sure derived classes finalize their fields.
		virtual ~DragStateBase() {}
	};

	template<class DragBase>
	class VoidDrag : public DragBase {
		using context = DragBase::context;
	protected:
		bool DragReady_core(context& cxt) override { return true; }
		void DragUnready_core(context& cxt) override {}

		void DragStart_core(context& cxt) override {}
		void DragDelta_core(const POINT& curr, context& cxt) override {}
		void DragEnd_core(context& cxt) override {}
		void DragCancel_core(context& cxt) override {}
		void DragAbort_core(context& cxt) override {}

		DragInvalidRange InvalidRange() override { return DragInvalidRange::AlwaysInvalid(); }
	};
}
