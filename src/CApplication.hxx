#ifndef _CAPPLICATION_HXX_
#define _CAPPLICATION_HXX_

#include <vector>
#include <string>
#include <fstream>
#include <filesystem>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <cstring>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace coalfix
{

struct SUnrealString
{
    int32_t size{0};
    std::basic_string<char> data{};
};

struct SINISection
{
    SUnrealString path{};
    SUnrealString content{};
};

class CApplication
{
public:
    CApplication();
    virtual ~CApplication();

    int run(int argc, char **argv);

protected:
    void setupDefaultPath();

    void printErrorReason(const std::string_view &err = {}, int code = errno);
    std::string getBasename(const std::filesystem::path &path);

    int32_t readBuffer(void *buf, const int32_t &sz);
    int32_t readString(std::basic_string<char> &data);
    bool readInt32(int32_t &i);

    int32_t writeBuffer(const void *buf, const int32_t &sz);

private:
    bool readRegString(const HKEY hRoot, const tstring &sRegPath, const tstring &sRegKey, tstring &sOutput);

    const std::filesystem::path sCoalescedPath = "/BioGame/Config/PC/Cooked/Coalesced.ini";
    std::filesystem::path sFinalPath;

    std::fstream fs;

    std::vector<SINISection> vSections;

};

}

#endif // _CAPPLICATION_HXX_
