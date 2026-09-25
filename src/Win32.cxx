#ifdef _WIN32

#include "CApplication.hxx"

#include <string>
#include <print>

#define WIN32_LEAN_AND_MEAN
//#define WIN32_EXTRA_LEAN
//#define UNICODE

#include <windows.h>
#include <io.h>
#include <wchar.h>

#ifdef UNICODE
// NOTE: To my knowledge, Windows' WCHAR is a regular UTF-16
static_assert(sizeof(TCHAR) == sizeof(char16_t));
static_assert(sizeof(TCHAR) == sizeof(wchar_t));
#endif

namespace coalfix
{

std::string CApplication::to_utf8(const std::wstring_view &wide)
{
    if (wide.empty())
        return {};

    const int len = static_cast<int>(wide.size());
    const int bytes = ::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide.data(), len, NULL, 0, NULL, NULL);
    if (bytes == 0)
        throw std::system_error(static_cast<int>(::GetLastError()),
                                std::system_category(), "WideCharToMultiByte");

    std::string out;
    out.resize_and_overwrite(bytes, [&](char *buf, std::size_t n) {
        return ::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide.data(), len, buf, static_cast<int>(n), NULL, NULL);
    });

    return out;
}

bool CApplication::readRegString(const EREGTYPE &eType, const std::wstring_view &sRegPath, const std::wstring_view &sRegKey, std::wstring &sOutput)
{
    HKEY hKey = NULL;
    LSTATUS lRes = 0;

    REGSAM samDesired = KEY_READ;

    // NOTE: We want the 32 bit node key on 64 bit Windows
    #if defined(_WIN64) || defined(__x86_64__)
    samDesired |= KEY_WOW64_32KEY;
    #endif

    const auto hRoot = eType == EREGTYPE::HKLM ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;

    lRes = ::RegOpenKeyExW(hRoot, sRegPath.data(), 0, samDesired, &hKey);
    if (lRes != ERROR_SUCCESS)
    {
        std::fprintf(stderr, "Failed to open registry key\n");

        return false;
    }

    DWORD dwBufferSize = 0;

    // NOTE: Query the size of the value first by setting the buffer to NULL
    lRes = ::RegQueryValueExW(hKey, sRegKey.data(), 0, NULL, NULL, &dwBufferSize);
    if (lRes != ERROR_SUCCESS)
    {
        std::fprintf(stderr, "Failed to query key size\n");

        return false;
    }

    WCHAR *szBuffer = new WCHAR[dwBufferSize];

    lRes = ::RegQueryValueExW(hKey, sRegKey.data(), 0, NULL, (LPBYTE)szBuffer, &dwBufferSize);
    if (lRes != ERROR_SUCCESS)
    {
        std::fprintf(stderr, "Failed to read registry key value (%lu)\n", lRes);

        RegCloseKey(hKey);

        delete []szBuffer;

        return false;
    }

    sOutput = szBuffer;

    delete []szBuffer;

    return true;
}

std::string_view CApplication::readRegInstallPath()
{
    const auto &regRoot = EREGTYPE::HKLM;
    const WCHAR *regPath = L"Software\\Bioware\\Mass Effect 2";
    const WCHAR *regKey = L"Path";

    std::wstring regOutput;

    if (!readRegString(regRoot, regPath, regKey, regOutput))
        return {};

    const std::string &sInstallPath = to_utf8(regOutput);

    std::println(stderr, "Registry install path: {}", sInstallPath);

    return sInstallPath;
}

}

#endif // _WIN32
