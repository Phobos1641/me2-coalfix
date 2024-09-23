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

// Format specification
//
// byte[4] magic: 0x1E000000
// {
//   int32_t path_size: size of path string, including NULL
//   char* path: variable sized NULL terminated string
//   int32_t data_size: size of data, including NULL
//   char* content: variable sized NULL terminated string, ASCII LF-style (0x0A) newlines
// }

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

CApplication::CApplication()
{
}

CApplication::~CApplication()
{
    freeSections();

    if (fd >= 0)
    {
        std::fprintf(stderr, "Closing file handle...\n");

#ifdef _WIN32
        _close(fd);
#else
        close(fd);
#endif
    }
}

int CApplication::run(int argc, char **argv)
{
    setupDefaultPath();

    if (argc == 2)
    {
        const std::string &arg = argv[1];
        if (arg == "-h" || arg == "--help")
        {
            const std::string &b = getBasename(argv[0]);

            std::printf("%s -h | %s [/path/to/Coalesced.ini]\n", b.c_str(), b.c_str());

            return 0;
        }

        sFinalPath = argv[1];
    }

    #if defined(_WIN32) && defined(UNICODE)
    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>,char16_t> conv;

    const std::string &sFinalPathUTF8 = conv.to_bytes(reinterpret_cast<const char16_t *>(sFinalPath.c_str()));
    #else
    const std::string &sFinalPathUTF8 = sFinalPath.string();
    #endif

    std::fprintf(stderr, "Opening %s...\n", sFinalPathUTF8.c_str());

    #ifdef _WIN32
    const int mode = _O_RDWR | _O_BINARY;
    const int perm = _S_IREAD | _S_IWRITE;

    #ifdef UNICODE
    fd = _wopen(sFinalPath.c_str(), mode, perm);
    #else
    fd = _open(sFinalPathUTF8.c_str(), mode, perm);
    #endif
    if (fd == -1)
    {
        std::fprintf(stderr, "_topen failed: %lu\n", GetLastError());

        printFileErrorReason();

        return 1;
    }
    #else
    fd = open(sFinalPath.c_str(), O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fd < 0)
    {
        std::fprintf(stderr, "open failed: %i\n", errno);

        printFileErrorReason();

        return 1;
    }
    #endif

    char magic[4] = { 0x00 };
    if (read(fd, &magic, sizeof(magic)) != sizeof(magic))
    {
        std::fprintf(stderr, "Failed to read magic\n");

        return 1;
    }

    std::printf("Read magic %02X%02X%02X%02X\n", magic[0], magic[1], magic[2], magic[3]);

    // NOTE: Windows Notepad mangles, among other, the magic into 1E 20 20 20
    // NOTE: Seems like all 0x00 (NULL) are turned into 0x20 (spaces)

    if (*reinterpret_cast<uint32_t*>(magic) != 0x1E)
    {
        std::fprintf(stderr, "Magic mismatch\n");

        return 1;
    }

    while (true)
    {
        SINISection section;

        if (!readInt32(section.path.size))
        {
            std::fprintf(stderr, "Failed to read section path size\n");

            break;
        }

        // NOTE: The path specified should not be too long.
        assert(section.path.size < 260);

        const int32_t psize = readData((void*&)section.path.data);
        if (psize == 0)
        {
            std::fprintf(stderr, "Failed to read section path\n");

            close(fd);

            break;
        }

        std::fprintf(stderr, "Read section with path '%s' with %i chars (real %i)\n", section.path.data, section.path.size, psize);

        if (!readInt32(section.content.size))
        {
            std::fprintf(stderr, "Failed to read section content size\n");

            std::free(section.path.data);

            break;
        }

        const int32_t csize = readData((void*&)section.content.data);
        if (csize == 0)
        {
            std::fprintf(stderr, "Failed to read section content\n");

            std::free(section.path.data);

            break;
        }

        std::fprintf(stderr, "Read section content of %i bytes (real %i)\n", section.content.size, csize);

        if (section.path.size != psize)
        {
            std::printf("Mismatched header path length. (%u != %u) Fixing...\n", section.path.size, psize);

            section.content.size = psize;
        }

        if (section.content.size != csize)
        {
            std::printf("Mismatched header content length. (%u != %u) Fixing...\n", section.content.size, csize);

            section.content.size = csize;
        }

        vSections.push_back(section);
    }

    std::printf("Finished reading sections\n");

#if 1
    std::fprintf(stderr, "Truncating file...\n");

    if (ftruncate(fd, 0) == -1)
    {
        std::fprintf(stderr, "ftruncate failed: %i\n", errno);

        close(fd);

        return 1;
    }

    // NOTE: long __lseek(int, long, int)
    if (lseek(fd, 0, SEEK_SET) == (off_t)-1)
    {
        std::fprintf(stderr, "lseek failed: %i\n", errno);

        return 1;
    }

    if (write(fd, &magic, sizeof(magic)) != sizeof(magic))
    {
        std::fprintf(stderr, "Failed to write magic\n");

        return 1;
    }

    for (auto &it: vSections)
    {
        std::printf("Writing section with path '%s' (%i) and size %i\n", it.path.data, it.path.size, it.content.size);

        if (write(fd, &it.path.size, sizeof(it.path.size)) != sizeof(it.path.size))
        {
            std::fprintf(stderr, "Failed to write section path size...\n");
        }

        if (write(fd, it.path.data, it.path.size) != it.path.size)
        {
            std::fprintf(stderr, "Failed to write section path...\n");
        }

        if (write(fd, &it.content.size, sizeof(it.content.size)) != sizeof(it.content.size))
        {
            std::fprintf(stderr, "Failed to write section content size...\n");
        }

        std::printf("Writing data...\n");

        if (write(fd, it.content.data, it.content.size) != it.content.size)
        {
            std::fprintf(stderr, "Failed to write data...\n");
        }
    }
#endif

    return 0;
}

void CApplication::setupDefaultPath()
{
#ifdef _WIN32
    setlocale(LC_ALL, "en_US.UTF8");

    const HKEY regRoot = HKEY_LOCAL_MACHINE;
    const TCHAR *regPath = TEXT("Software\\Bioware\\Mass Effect 2");
    const TCHAR *regKey = TEXT("Path");

    tstring regOutput;

    if (readRegString(regRoot, regPath, regKey, regOutput))
    {
        #ifdef UNICODE
        std::wstring_convert<std::codecvt_utf8_utf16<char16_t>,char16_t> conv;

        sFinalPath = conv.to_bytes(reinterpret_cast<const char16_t *>(regOutput.data()));

        std::fwprintf(stderr, L"Install path: %s\n", sFinalPath.c_str());
        #else
        sFinalPath = regOutput;

        std::fprintf(stderr, "Install path: %s\n", sFinalPath.string().c_str());
        #endif

        sFinalPath /= sCoalescedPath.string();
    }
    else
    {
        std::fprintf(stderr, "Failed to read registry key");
    }
#else
    #ifdef COALESCED_PATH
        #define COALESCED_STRING(x) #x
        #define COALESCED_QUOTE(x) COALESCED_STRING(x)

    sFinalPath = std::string(COALESCED_QUOTE(COALESCED_PATH)) + sCoalescedPath.string();
    #else
    char *cPathEnv = std::getenv("ME2_PATH");
    if (cPathEnv)
        sFinalPath = cPathEnv;
    #endif
#endif

    if (sFinalPath.empty())
        sFinalPath = "./Coalesced.ini";
}

void CApplication::freeSections()
{
    std::fprintf(stderr, "Freeing section data...\n");

    for (auto &it: vSections)
    {
        assert(it.path.data != NULL);
        if (it.path.data)
        {
            std::free(it.path.data);
            it.path.data = nullptr;
        }

        assert(it.content.data != NULL);
        if (it.content.data)
        {
            std::free(it.content.data);
            it.content.data = nullptr;
        }
    }
}

int32_t CApplication::readData(void *&data)
{
    int32_t len = 0, size = BUFSIZ;
    void *buf = nullptr;
    char *c = nullptr;

    buf = std::malloc(size);
    assert(buf != NULL);

    for (; ; ++len)
    {
        if (len == size)
        {
            size *= 2;
            buf = std::realloc(buf, size);
            assert(buf != NULL);
        }

        c = reinterpret_cast<char*>(buf) + len;
        if (read(fd, c, 1) != 1)
        {
            std::free(buf);

            return 0;
        }

        // NOTE: Strip Windows newlines
        if (*c == 0x0D)
        {
            --len;
            continue;
        }

        if (*c == 0x00)
            break;
    }

    data = buf;

    return len + 1;
}

bool CApplication::readInt32(int32_t &i)
{
    if (read(fd, &i, sizeof(int32_t)) != sizeof(int32_t))
    {
        return false;
    }

    return true;
}

std::string CApplication::getBasename(const std::filesystem::path &path)
{
    return path.filename().string();
}

void CApplication::printFileErrorReason()
{
    if (errno == ENOENT)
        std::fprintf(stderr, "Unable to open file. File does not exist\n");
    else if (errno == EACCES)
        std::fprintf(stderr, "Unable to open file. Access denied\n");
}

#ifdef _WIN32
bool CApplication::readRegString(const HKEY hRoot, const tstring &sRegPath, const tstring &sRegKey, tstring &sOutput)
{
    HKEY hKey = NULL;
    LSTATUS lRes = 0;

    REGSAM samDesired = KEY_READ;

    // NOTE: We want the 32 bit node key on 64 bit Windows
    #if defined(_WIN64) || defined(__x86_64__)
    samDesired |= KEY_WOW64_32KEY;
    #endif

    lRes = RegOpenKeyEx(hRoot, sRegPath.c_str(), 0, samDesired, &hKey);
    if (lRes != ERROR_SUCCESS)
    {
        std::fprintf(stderr, "Failed to open registry key\n");

        return false;
    }

    DWORD dwBufferSize = 0;

    // NOTE: Query the size of the value first by setting the buffer to NULL
    lRes = RegQueryValueEx(hKey, sRegKey.c_str(), 0, NULL, NULL, &dwBufferSize);
    if (lRes != ERROR_SUCCESS)
    {
        std::fprintf(stderr, "Failed to query key size\n");

        return false;
    }

    TCHAR *szBuffer = new TCHAR[dwBufferSize];

    lRes = RegQueryValueEx(hKey, sRegKey.c_str(), 0, NULL, (LPBYTE)szBuffer, &dwBufferSize);
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
#endif

int main(int argc, char *argv[])
{
    CApplication app;

    return app.run(argc, argv);
}
