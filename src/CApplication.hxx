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

    bool readBuffer(void *buf, const int32_t &sz);
    bool readPathString(SUnrealString &ustring);
    bool readContentString(SUnrealString &ustring);

    bool readSignedInt32LE(int32_t &i);

    template <typename T, std::size_t N>
    bool writeArray(const std::array<T, N> &arr);

    bool writeString(const std::basic_string<char> &str);

    void writeUString(const SUnrealString &ustring);

    bool writeSignedInt32LE(const int32_t &i);

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
