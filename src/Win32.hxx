#ifndef _WIN32_HXX_
#define _WIN32_HXX_

#ifdef _WIN32

#include <string>

#define WIN32_LEAN_AND_MEAN
//#define WIN32_EXTRA_LEAN
//#define UNICODE

#include <windows.h>
#include <io.h>
#include <wchar.h>

#ifdef UNICODE
// NOTE: To my knowledge, Windows' WCHAR is a regular UTF-16
static_assert(sizeof(TCHAR) == sizeof(char16_t));
#endif

namespace coalfix
{

typedef std::basic_string_view<TCHAR> tstring;

bool readRegString(const HKEY hRoot, const tstring &sRegPath, const tstring &sRegKey, tstring &sOutput);

}

#endif // _WIN32

#endif // _WIN32_HXX_
