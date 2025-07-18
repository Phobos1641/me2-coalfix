#ifndef _CAPPLICATION_HXX_
#define _CAPPLICATION_HXX_

#include <iostream>
#include <vector>
#include <locale>
#include <string>
#include <codecvt>
#include <filesystem>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <cstring>

#ifdef _WIN32
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
#endif // _WIN32

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

struct SUnrealString
{
    int32_t size = 0;
    char *data = nullptr;
};

struct SINISection
{
    SUnrealString path;
    SUnrealString content;
};

#ifdef _WIN32
typedef std::basic_string<TCHAR> tstring;
#endif // _WIN32

class CApplication
{
public:
    CApplication();
    virtual ~CApplication();

    int run(int argc, char **argv);

protected:
    void setupDefaultPath();
    void freeSections();

#ifdef _WIN32
    bool readRegString(const HKEY hRoot, const tstring &sRegPath, const tstring &sRegKey, tstring &sOutput);
#endif // _WIN32

    void printFileErrorReason();
    std::string getBasename(const std::filesystem::path &path);

    int32_t readData(void *&data);
    bool readInt32(int32_t &i);

private:
    const std::filesystem::path sCoalescedPath = "/BioGame/Config/PC/Cooked/Coalesced.ini";
    std::filesystem::path sFinalPath;

    int fd = -1;

    std::vector<SINISection> vSections;

};

#endif // _CAPPLICATION_HXX_
