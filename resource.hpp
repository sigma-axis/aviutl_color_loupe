/*
The MIT License (MIT)

Copyright (c) 2024 sigma-axis

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#pragma once

#include <cstdint>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "resource.h"
inline HMODULE constinit this_dll = nullptr;

namespace sigma_lib::W32::resources
{
	namespace string
	{
		inline const wchar_t* get(uint32_t id) {
			const wchar_t* ret;
			::LoadStringW(this_dll, id, reinterpret_cast<wchar_t*>(&ret), 0);
			return ret;
		}

		inline int copy(uint32_t id, wchar_t* buf, size_t len) {
			return ::LoadStringW(this_dll, id, buf, len);
		}
		template<size_t len>
		inline int copy(uint32_t id, wchar_t(&buf)[len]) { return copy(id, buf, len); }
	}
	namespace cursor
	{
		enum ID : uint32_t {
			arrow		= reinterpret_cast<uint32_t>(IDC_ARROW),
			ibeam		= reinterpret_cast<uint32_t>(IDC_IBEAM),
			wait		= reinterpret_cast<uint32_t>(IDC_WAIT),
			cross		= reinterpret_cast<uint32_t>(IDC_CROSS),
			uparrow		= reinterpret_cast<uint32_t>(IDC_UPARROW),
			size		= reinterpret_cast<uint32_t>(IDC_SIZE),
			icon		= reinterpret_cast<uint32_t>(IDC_ICON),
			sizenwse	= reinterpret_cast<uint32_t>(IDC_SIZENWSE),
			sizenesw	= reinterpret_cast<uint32_t>(IDC_SIZENESW),
			sizewe		= reinterpret_cast<uint32_t>(IDC_SIZEWE),
			sizens		= reinterpret_cast<uint32_t>(IDC_SIZENS),
			sizeall		= reinterpret_cast<uint32_t>(IDC_SIZEALL),
			no			= reinterpret_cast<uint32_t>(IDC_NO),
			hand		= reinterpret_cast<uint32_t>(IDC_HAND),
			appstarting	= reinterpret_cast<uint32_t>(IDC_APPSTARTING),
			help		= reinterpret_cast<uint32_t>(IDC_HELP),
			pin			= reinterpret_cast<uint32_t>(IDC_PIN),
			person		= reinterpret_cast<uint32_t>(IDC_PERSON),
		};
		inline HCURSOR get(ID id) {
			return reinterpret_cast<HCURSOR>(::LoadImageW(nullptr,
				reinterpret_cast<PCWSTR>(id), IMAGE_CURSOR, 0, 0, LR_DEFAULTSIZE | LR_SHARED));
		}
		inline HCURSOR load(uint32_t resource_id) {
			return reinterpret_cast<HCURSOR>(::LoadImageW(this_dll,
				MAKEINTRESOURCEW(resource_id), IMAGE_CURSOR, 0, 0, LR_DEFAULTSIZE | LR_SHARED));
		}
	}
}
