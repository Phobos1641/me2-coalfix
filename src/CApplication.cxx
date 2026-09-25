#include "CApplication.hxx"

#include <array>
#include <locale>
#include <codecvt>
#include <print>
#include <cassert>
#include <iostream>

namespace coalfix
{

CApplication::CApplication()
{
}

CApplication::~CApplication()
{
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

            std::println("{} -h | {} [/path/to/Coalesced.ini]\n", b, b);

            return EXIT_SUCCESS;
        }

        sFinalPath = argv[1];
    }

    std::println(stderr, "Opening {}...", sFinalPath.string());

    fs.open(sFinalPath, std::fstream::in | std::fstream::binary);

    if (!fs.good())
    {
        printErrorReason("open failed");

        return EXIT_FAILURE;
    }

    std::array<char, 4> magic = {0x00};
    if (readBuffer(magic.data(), magic.size()) != magic.size())
    {
        printErrorReason("Failed to read magic");

        return EXIT_FAILURE;
    }

    std::printf("Read magic %02X%02X%02X%02X\n", magic[0], magic[1], magic[2], magic[3]);

    // NOTE: Windows Notepad mangles, among other, the magic into 1E 20 20 20
    // NOTE: Seems like all 0x00 (NULL) are turned into 0x20 (spaces)

    if (magic[0] != 0x1E)
    {
        std::println(stderr, "Magic mismatch");

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

        const int32_t psize = readString(section.path.data);
        if (psize == 0)
        {
            printErrorReason("Failed to read section path");

            break;
        }

        std::println(stderr, "Read section with path '{}' with {} chars (real {})", section.path.data, section.path.size, psize);

        if (!readInt32(section.content.size))
        {
            printErrorReason("Failed to read section content size");

            break;
        }

        const int32_t csize = readString(section.content.data);
        if (csize == 0)
        {
            printErrorReason("Failed to read section content");

            break;
        }

        std::println(stderr, "Read section content of {} bytes (real {})", section.content.size, csize);

        if (section.path.size != psize)
        {
            std::println("Mismatched header path length. ({} != {}) Fixing...", section.path.size, psize);

            section.content.size = psize;
        }

        if (section.content.size != csize)
        {
            std::println("Mismatched header content length. ({} != {}) Fixing...", section.content.size, csize);

            section.content.size = csize;
        }

        vSections.push_back(section);
    }

    std::println(stderr, "Finished reading sections\n");

    std::println(stderr, "Re-opening file for truncation...");

    fs.close();

    fs.open(sFinalPath, std::fstream::out | std::fstream::binary | std::fstream::trunc);

    if (!fs.good())
    {
        printErrorReason("open failed");

        return EXIT_FAILURE;
    }

    if (writeBuffer(&magic, sizeof(magic)) != sizeof(magic))
    {
        printErrorReason("Failed to write magic");

        return EXIT_FAILURE;
    }

    for (auto &it: vSections)
    {
        std::printf("Writing section with path '%s' (%i) and size %i\n", it.path.data.data(), it.path.size, it.content.size);

        if (writeBuffer(&it.path.size, sizeof(it.path.size)) != sizeof(it.path.size))
        {
            printErrorReason("Failed to write section path size...");
        }

        if (writeBuffer(it.path.data.data(), it.path.size) != it.path.size)
        {
            printErrorReason("Failed to write section path...");
        }

        if (writeBuffer(&it.content.size, sizeof(it.content.size)) != sizeof(it.content.size))
        {
            printErrorReason("Failed to write section content size...");
        }

        std::printf("Writing data...\n");

        if (writeBuffer(it.content.data.data(), it.content.size) != it.content.size)
        {
            printErrorReason("Failed to write data...");
        }
    }

    return EXIT_SUCCESS;
}

void CApplication::setupDefaultPath()
{
#ifdef INSTALL_PATH
    #define COALESCED_STRING(x) #x
    #define COALESCED_QUOTE(x) COALESCED_STRING(x)

    sFinalPath = std::string(COALESCED_QUOTE(INSTALL_PATH)) + sCoalescedPath.string();

    return;
#endif

    char *cPathEnv = std::getenv("ME2_PATH");
    if (cPathEnv)
        sFinalPath = cPathEnv;
    #ifdef _WIN32
    else
        sFinalPath = readRegInstallPath();
    #endif

    if (!sFinalPath.empty())
    {
        sFinalPath /= sCoalescedPath.string();

        std::println(stderr, "Coalesced path: {}", sFinalPath.string());

        return;
    }

    sFinalPath = "./Coalesced.ini";
}

int32_t CApplication::readBuffer(void *buf, const std::int32_t &sz)
{
    try
    {
        fs.read(reinterpret_cast<char*>(buf), sz);

        return fs.gcount();
    }
    catch (...)
    {
        throw;
    }
}

int32_t CApplication::readString(std::basic_string<char> &data)
{
    int32_t len = 0;

    // NOTE: Yes, this is absurdly inefficient.

    try
    {
        char c = 0x00;

        for (; ; ++len)
        {
            fs.read(&c, 1);
            if (fs.gcount() != 1)
            {
                return 0;
            }

            // NOTE: Strip Windows newlines
            if (c == 0x0D)
            {
                --len;
                continue;
            }

            if (c == 0x00)
                break;

            data.insert(data.end(), c);
        }
    }
    catch (...)
    {
        throw;
    }

    return len + 1;
}

bool CApplication::readInt32(int32_t &i)
{
    try
    {
        fs.read(reinterpret_cast<char*>(&i), sizeof(int32_t));

        const std::size_t &n = fs.gcount();

        std::println(stderr, "Read {} bytes on an int32_t ({}) operation", n, sizeof(int32_t));

        if (fs.gcount() != sizeof(int32_t))
        {
            return false;
        }

        return true;
    }
    catch (...)
    {
        throw;
    }
}

int32_t CApplication::writeBuffer(const void *buf, const int32_t &sz)
{
    try
    {
        fs.write(reinterpret_cast<const char*>(buf), sz);

        return fs.good() ? sz : -1;
    }
    catch (...)
    {
        throw;
    }
}

std::string CApplication::getBasename(const std::filesystem::path &path)
{
    return path.filename().string();
}

void CApplication::printErrorReason(const std::string_view &err, int code)
{
    if (!err.empty())
    {
        std::println(stderr, "{}: {} = {}", err, code, std::strerror(code));

        return;
    }

    std::println(stderr, "{} = {}", code, std::strerror(code));
}

}
