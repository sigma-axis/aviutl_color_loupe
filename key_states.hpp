/*
The MIT License (MIT)

Copyright (c) 2024 sigma-axis

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#pragma once

#include <cstdint>
#include <tuple>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

////////////////////////////////
// Windows API 利用の補助関数．
////////////////////////////////
namespace sigma_lib::W32::UI
{
	enum class flag_map : uint8_t {
		id	= 0,
		off	= 1,
		on	= 2,
		inv	= 3,
	};

	// キー入力認識をちょろまかす補助クラス．
	template<size_t N>
	class ForceKeyState {
		template<size_t i, class... TArgs>
		using arg_at = std::decay_t<std::tuple_element_t<i, std::tuple<TArgs...>>>;

		template<class... TArgs>
		static consteval bool check_requirements() {
			constexpr auto check_arg = []<size_t i>() {
				return std::convertible_to<arg_at<i, TArgs...>, int>;
			};
			constexpr auto check_result = []<size_t i>() {
				using result = arg_at<i, TArgs...>;
				return std::same_as<result, bool>
					|| std::same_as<result, flag_map>
					|| std::convertible_to<result, uint8_t>;
			};

			if (!([]<size_t... I>(std::index_sequence<I...>) {
				if constexpr (!(check_arg.operator() < 2 * I > () && ...)) return false;
				if constexpr (!(check_result.operator() < 2 * I + 1 > () && ...)) return false;
				return true;
			}(std::make_index_sequence<N>{}))) return false;

			return true;
		}

		constexpr static size_t num_keys = 256;
		uint8_t key[N], rew[N];

		static constexpr uint8_t coerce_key(int val) {
			if (val < 0 || val >= num_keys) return 0;
			return static_cast<uint8_t>(val);
		}
		struct modifier {
			uint8_t a, b;
			constexpr modifier(bool mod) : modifier{ mod ? flag_map::on : flag_map::off } {}
			constexpr modifier(flag_map mod) {
				using enum flag_map;
				switch (mod) {
				case off:	a = 0x80, b = 0x80; break;
				case on:	a = 0x80, b = 0x00; break;
				case inv:	a = 0x00, b = 0x80; break;
				case id: default: a = b = 0x00; break;
				}
			}
			constexpr modifier(uint8_t mod) : a{ 0xff }, b{ ~mod } {}

			constexpr uint8_t operator()(uint8_t& state) const {
				auto ret = state;
				state = (state | a) ^ b;
				return ret;
			}
			constexpr operator bool() const { return a != 0 || b != 0; }
		};

		template<size_t... I>
		ForceKeyState(std::index_sequence<I...>, const auto&& data) noexcept
			: key{ coerce_key(std::get<2 * I>(data))... }
		{
			modifier m[]{ { std::get<2 * I + 1>(data) }... };
			if (((key[I] > 0 && m) || ...)) {
				// overwrite the state.
				uint8_t state[num_keys]; std::ignore = ::GetKeyboardState(state);
				((key[I] > 0 && m ? (rew[I] = m[I](state[key[I]])) : (key[I] = 0)), ...);
				::SetKeyboardState(state);
			}
		}

	public:
		template<class... TArgs>
			requires(sizeof...(TArgs) == 2 * N && check_requirements<TArgs...>())
		ForceKeyState(TArgs const... args) noexcept
			: ForceKeyState{ std::make_index_sequence<N>{}, std::make_tuple(args...) } {}

		ForceKeyState(const ForceKeyState&) = delete;
		ForceKeyState(ForceKeyState&&) = delete;

		~ForceKeyState() noexcept {
			size_t i = 0;
			for (; i < N; i++) { if (key[i] > 0) goto rewind; }
			return;

		rewind:
			// rewind the state.
			uint8_t state[num_keys]; std::ignore = ::GetKeyboardState(state);
			do {
				if (key[i] > 0) state[key[i]] = rew[i];
			} while (++i < N);
			::SetKeyboardState(state);
		}
	};
	template<class... TArgs>
	ForceKeyState(TArgs...)->ForceKeyState<sizeof...(TArgs) / 2>;
	template<> struct ForceKeyState<0> {};
}
