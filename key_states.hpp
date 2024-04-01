/*
The MIT License (MIT)

Copyright (c) 2024 sigma-axis

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#pragma once

#include <cstdint>
#include <algorithm>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

////////////////////////////////
// Windows API 利用の補助関数．
////////////////////////////////
namespace sigma_lib::W32::UI
{
	// キー入力認識をちょろまかす補助クラス．
	class ForceKeyState {
		static auto force_key_state(short vkey, uint8_t state)
		{
			uint8_t states[256]; std::ignore = ::GetKeyboardState(states);
			std::swap(state, states[vkey]); ::SetKeyboardState(states);
			return state;
		}

		constexpr static auto state(bool pressed) { return pressed ? key_pressed : key_released; }

		const short vkey;
		const uint8_t prev;

	public:
		constexpr static uint8_t key_released = 0, key_pressed = 0x80;
		constexpr static int vkey_invalid = -1;

		ForceKeyState(short vkey, bool pressed) : ForceKeyState(vkey, state(pressed)) {}
		ForceKeyState(short vkey, uint8_t state) :
			vkey{ 0 <= vkey && vkey < 256 ? vkey : vkey_invalid },
			prev{ this->vkey != vkey_invalid ? force_key_state(this->vkey, state) : uint8_t{} } {}
		~ForceKeyState() { if (vkey != vkey_invalid) force_key_state(vkey, prev); }

		ForceKeyState(const ForceKeyState&) = delete;
		ForceKeyState& operator=(const ForceKeyState&) = delete;
	};
}
