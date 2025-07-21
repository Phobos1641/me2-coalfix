#ifndef _CAPPLICATION_HXX_
#define _CAPPLICATION_HXX_

#include <vector>
#include <string>
#include <filesystem>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <cstring>

#include "Win32.hxx"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace coalfix
{

struct SUnrealString
{
    int32_t size = 0;
    std::basic_string<char> data{};
};

struct SINISection
{
    SUnrealString path;
    SUnrealString content;
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

    int32_t readData(std::basic_string<char> &data);
    bool readInt32(int32_t &i);

private:
    const std::filesystem::path sCoalescedPath = "/BioGame/Config/PC/Cooked/Coalesced.ini";
    std::filesystem::path sFinalPath;

    int fd = -1;

    std::vector<SINISection> vSections;

};

}

#endif // _CAPPLICATION_HXX_
