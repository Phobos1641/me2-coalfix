#include "CApplication.hxx"

#include <array>
#include <locale>
#include <codecvt>
#include <cstring>
#include <cstdlib>
#include <cassert>
#include <iostream>

namespace coalfix
{

CApplication::CApplication()
{
}

CApplication::~CApplication()
{
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

            return EXIT_SUCCESS;
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
        printErrorReason("_topen failed", GetLastError());

        return 1;
    }
    #else
    fd = open(sFinalPath.c_str(), O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fd < 0)
    {
        printErrorReason("open failed");

        return EXIT_FAILURE;
    }
    #endif

    std::array<char, 4> magic = {0x00};
    if (read(fd, magic.data(), magic.size()) != magic.size())
    {
        printErrorReason("Failed to read magic");

        return EXIT_FAILURE;
    }

    std::printf("Read magic %02X%02X%02X%02X\n", magic[0], magic[1], magic[2], magic[3]);

    // NOTE: Windows Notepad mangles, among other, the magic into 1E 20 20 20
    // NOTE: Seems like all 0x00 (NULL) are turned into 0x20 (spaces)

    if (magic[0] != 0x1E)
    {
        std::fprintf(stderr, "Magic mismatch\n");

        return EXIT_FAILURE;
    }

    while (true)
    {
        SINISection section;

        if (!readInt32(section.path.size))
        {
            printErrorReason("Failed to read section path size");

            break;
        }

        // NOTE: The path specified should not be too long.
        assert(section.path.size < 260);

        const int32_t psize = readData(section.path.data);
        if (psize == 0)
        {
            printErrorReason("Failed to read section path");

            close(fd);

            break;
        }

        std::fprintf(stderr, "Read section with path '%s' with %i chars (real %i)\n", section.path.data.data(), section.path.size, psize);

        if (!readInt32(section.content.size))
        {
            printErrorReason("Failed to read section content size");

            break;
        }

        const int32_t csize = readData(section.content.data);
        if (csize == 0)
        {
            printErrorReason("Failed to read section content");

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
        printErrorReason("ftruncate failed");

        close(fd);

        return EXIT_FAILURE;
    }

    // NOTE: long __lseek(int, long, int)
    if (lseek(fd, 0, SEEK_SET) == (off_t)-1)
    {
        printErrorReason("lseek failed");

        return EXIT_FAILURE;
    }

    if (write(fd, &magic, sizeof(magic)) != sizeof(magic))
    {
        printErrorReason("Failed to write magic");

        return EXIT_FAILURE;
    }

    for (auto &it: vSections)
    {
        std::printf("Writing section with path '%s' (%i) and size %i\n", it.path.data.data(), it.path.size, it.content.size);

        if (write(fd, &it.path.size, sizeof(it.path.size)) != sizeof(it.path.size))
        {
            printErrorReason("Failed to write section path size...");
        }

        if (write(fd, it.path.data.data(), it.path.size) != it.path.size)
        {
            printErrorReason("Failed to write section path...");
        }

        if (write(fd, &it.content.size, sizeof(it.content.size)) != sizeof(it.content.size))
        {
            printErrorReason("Failed to write section content size...");
        }

        std::printf("Writing data...\n");

        if (write(fd, it.content.data.data(), it.content.size) != it.content.size)
        {
            printErrorReason("Failed to write data...");
        }
    }
#endif

    return EXIT_SUCCESS;
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

int32_t CApplication::readData(std::basic_string<char> &data)
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

    data.append(reinterpret_cast<char*>(buf), len);

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

void CApplication::printErrorReason(const std::string_view &err, int code)
{
    if (!err.empty())
    {
        std::fprintf(stderr, "%s: %i = %s\n", err.data(), code, std::strerror(code));

        return;
    }

    std::fprintf(stderr, "%i = %s\n", code, std::strerror(code));
}

}
