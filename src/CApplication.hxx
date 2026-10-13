#ifndef CAPPLICATION_HXX
#define CAPPLICATION_HXX

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

enum class EREGTYPE : uint8_t {
    HKLM,
    HKCU,
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

    bool readBuffer(void *buf, const int32_t &sz);
    bool readPathString(std::basic_string<char> &data, const std::size_t nExpectedSize);
    bool readContentString(std::basic_string<char> &data, const std::size_t nExpectedSize);

    bool readSignedInt32LE(int32_t &i);

    bool writeBuffer(const void *buf, const int32_t &sz);

    bool writeSignedInt32LE(const int32_t &i, const int32_t &sz);

private:
    bool readRegString(const EREGTYPE &eType, const std::wstring &sRegPath, const std::wstring &sRegKey, std::wstring &sOutput);
    std::string_view readRegInstallPath();

    std::filesystem::path sFinalPath{};

    std::fstream fs{};

    std::vector<SINISection> vSections{};

    bool bNotepadMode{false};

};

}

#endif // CAPPLICATION_HXX
