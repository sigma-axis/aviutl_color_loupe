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

	template<class DragContext>
	class DragStateBase {
		inline static constinit DragStateBase* current = nullptr;

	public:
		using context = DragContext;

	protected:
		HWND hwnd{ nullptr };
		POINT drag_start{}, last_point{}; // window coordinates.

		// should return true if ready to initiate the drag.
		virtual bool DragStart_core(context& cxt) = 0;
		virtual void DragDelta_core(const POINT& curr, context& cxt) {}
		virtual void DragCancel_core(context& cxt) {}
		virtual void DragEnd_core(context& cxt) {}
		virtual void DragAbort_core(context& cxt) {}

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
		// returns true if the dragging has started.
		bool DragStart(HWND hwnd, const POINT& drag_start, context& cxt)
		{
			if (is_dragging(cxt)) return false;

			this->hwnd = hwnd;
			this->drag_start = this->last_point = drag_start;

			if (this->DragStart_core(cxt)) {
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
		static bool DragDelta(const POINT& curr, context& cxt)
		{
			if (!is_dragging(cxt)) return false;

			current->DragDelta_core(curr, cxt);
			current->last_point = curr;
			return true;
		}
		// returns true if the dragging operation has been properly canceled.
		static bool DragCancel(context& cxt)
		{
			if (!is_dragging(cxt)) return false;

			current->DragCancel_core(cxt);

			current->hwnd = nullptr;
			current = nullptr;

			::ReleaseCapture();
			return true;
		}
		// returns true if the dragging operation has been properly processed.
		static bool DragEnd(context& cxt)
		{
			if (!is_dragging(cxt)) return false;

			current->DragEnd_core(cxt);

			current->hwnd = nullptr;
			current = nullptr;

			::ReleaseCapture();
			return true;
		}
		// returns true if there sure was a dragging state that is now aborted.
		static bool DragAbort(context& cxt)
		{
			if (current == nullptr) return false;

			current->DragAbort_core(cxt);

			auto tmp = current->hwnd;
			current->hwnd = nullptr;
			current = nullptr;

			if (tmp != nullptr && ::GetCapture() == tmp)
				::ReleaseCapture();
			return true;
		}
		// verifies the dragging status, might abort if the window is no longer captured.
		static bool is_dragging(context& cxt) {
			if (current != nullptr) {
				if (current->hwnd != nullptr &&
					current->hwnd == ::GetCapture()) return true;

				DragAbort(cxt);
			}
			return false;
		}

		// make sure derived classes finalize their fields.
		virtual ~DragStateBase() {}
	};
}
