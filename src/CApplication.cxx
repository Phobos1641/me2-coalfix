#include "CApplication.hxx"

#include <algorithm>
#include <bit>
#include <array>
#include <locale>
#include <codecvt>
#include <print>
#include <cassert>
#include <iostream>

namespace fs = std::filesystem;

namespace coalfix
{

namespace {

// NOTE: No leading slash as that breaks relative path handling.
const fs::path sCoalescedRelPath = "BioGame/Config/PC/Cooked/Coalesced.ini";

template <typename T, std::size_t N>
constexpr bool startsWith(const std::array<T, N> &a, const std::initializer_list<uint8_t> &sig)
{
    if (a.size() < sig.size())
        return false;

    return std::equal(sig.begin(), sig.end(), a.begin());
}

template <typename T>
constexpr bool endsWith(const std::vector<T> &a, const std::initializer_list<uint8_t> &sig)
{
    if (a.size() < sig.size())
        return false;

    return std::equal(sig.begin(), sig.end(), a.end() - sig.size());
}

fs::path nextFreeBackup(const fs::path &path)
{
    auto b = path;

    b += ".bak";

    for (int i = 1; fs::exists(b); ++i)
    {
        b = path;
        b += ".bak" + std::to_string(i);
    }

    return b;
}

std::string getPathBasename(const fs::path &path)
{
    return path.filename().string();
}

}

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
            const std::string &b = getPathBasename(argv[0]);

            std::println("{} -h | {} [/path/to/Coalesced.ini]", b, b);

            return EXIT_SUCCESS;
        }

        sFinalPath = argv[1];
    }

    {
        const auto bak = nextFreeBackup(sFinalPath);

        std::error_code ec;
        fs::copy_file(sFinalPath, bak, ec);

        if (ec)
        {
            std::println(stderr, "Could not create backup of file: {}", ec.message());

            return EXIT_FAILURE;
        }

        std::println("Saved backup as: {}", bak.string());
    }

    std::println(stderr, "Opening {}...", sFinalPath.string());

    fs.open(sFinalPath, std::fstream::in | std::fstream::binary);

    if (!fs.good())
    {
        printErrorReason("open failed");

        return EXIT_FAILURE;
    }

    std::array<uint8_t, 4> magic{0};
    if (!readBuffer(magic.data(), magic.size()))
    {
        printErrorReason("Failed to read magic");

        return EXIT_FAILURE;
    }

    std::println("Read magic {:#X}{:X}{:X}{:X}", magic[0], magic[1], magic[2], magic[3]);

    // NOTE: Windows Notepad mangles, among other, the magic into 1E 20 20 20
    // NOTE: Seems like all 0x00 (NULL) are turned into 0x20 (spaces) and CR line endings gets converted to CRLF

    if (startsWith(magic, {0xEF, 0xBB, 0xBF}) || startsWith(magic, {0xFF, 0xFE}) || startsWith(magic, {0xFE, 0xFF}))
    {
        std::println(stderr, "Input was saved as Unicode (UTF-8 or UTF-16). Restore a backup, redo the edit, and re-save with ANSI encoding.");

        return EXIT_FAILURE;
    }

    if (startsWith(magic, {0x1E, 0x20, 0x20, 0x20}))
    {
        std::println(stderr, "File was saved in Notepad. Recovery may not work...");

        bNotepadMode = true;

        magic = {0x1E, 0x00, 0x00, 0x00};
    }
    else if (!startsWith(magic, {0x1E, 0x00, 0x00, 0x00}))
    {
        std::println(stderr, "Unknown magic bytes.");

        return EXIT_FAILURE;
    }

    while (true)
    {
        SINISection section;

        if (!readSignedInt32LE(section.path.size))
        {
            printErrorReason("Failed to read section path size");

            break;
        }

        // NOTE: The path specified should normally not be too long.
        if (section.path.size >= 260)
            std::println(stderr, "Path section string size reported as {}. Assuming broken record...", section.path.size);

        const auto psize = readPathString(section.path.data);
        if (psize == 0)
        {
            printErrorReason("Failed to read section path");

            break;
        }

        std::println(stderr, "Read section path '{}', of size {} (reported {})", section.path.data, psize, section.path.size);

        if (!readSignedInt32LE(section.content.size))
        {
            printErrorReason("Failed to read section content size");

            break;
        }

        const auto csize = readContentString(section.content.data);
        if (csize == 0)
        {
            printErrorReason("Failed to read section content");

            break;
        }

        std::println(stderr, "Read section content, of {} bytes (reported {})", csize, section.content.size);

        if (section.path.size != psize)
        {
            #ifdef _DEBUG
            std::println(stderr, "Mismatched header path length. Fixing...");
            #endif

            section.path.size = psize;
        }

        if (section.content.size != csize)
        {
            #ifdef _DEBUG
            std::println(stderr, "Mismatched header content length. Fixing...");
            #endif

            section.content.size = csize;
        }

        vSections.push_back(section);
    }

    std::println(stderr, "Finished reading {} sections", vSections.size());

    fs.close();

    std::println(stderr, "Re-opening file for truncation...");

    fs.open(sFinalPath, std::fstream::out | std::fstream::binary | std::fstream::trunc);

    if (!fs.good())
    {
        printErrorReason("open failed");

        return EXIT_FAILURE;
    }

    if (!writeBuffer(&magic, sizeof(magic)))
    {
        printErrorReason("Failed to write magic");

        return EXIT_FAILURE;
    }

    for (auto &it: vSections)
    {
        std::println("Writing section with path '{}' ({} chars), of size {}", it.path.data.data(), it.path.size, it.content.size);

        if (!writeSignedInt32LE(it.path.size, sizeof(it.path.size)))
        {
            printErrorReason("Failed to write section path size...");
        }

        if (!writeBuffer(it.path.data.data(), it.path.size))
        {
            printErrorReason("Failed to write section path...");
        }

        if (!writeSignedInt32LE(it.content.size, sizeof(it.content.size)))
        {
            printErrorReason("Failed to write section content size...");
        }

        std::println("Writing data...");

        if (!writeBuffer(it.content.data.data(), it.content.size))
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

    sFinalPath = fs::path(COALESCED_QUOTE(INSTALL_PATH)) / sCoalescedPath.string();

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
        sFinalPath /= sCoalescedRelPath;

        std::println(stderr, "Coalesced path: {}", sFinalPath.string());

        return;
    }

    sFinalPath = "./Coalesced.ini";
}

bool CApplication::readBuffer(void *buf, const std::int32_t &sz)
{
    try
    {
        fs.read(reinterpret_cast<char*>(buf), sz);

        return fs.gcount() == sz;
    }
    catch (...)
    {
        throw;
    }
}

bool CApplication::readPathString(std::basic_string<char> &data)
{
    std::size_t len = 0;

    std::vector<char> buf;
    buf.reserve(256);

    while (true)
    {
        char c{0};

        fs.read(&c, 1);
        if (fs.gcount() != 1)
        {
            throw std::runtime_error("Failed to read data");
        }

        if (c == 0x00)
            break;

        buf.insert(buf.end(), c);

        // Hex compare .ini || .INI + SPACE, Notepad transforms 0x00 to 0x20

        if (bNotepadMode && (endsWith(buf, {0x2E, 0x69, 0x6E, 0x69, 0x20}) ||
                             endsWith(buf, {0x2E, 0x49, 0x4E, 0x49, 0x20})))
        {
            std::println(stderr, "Found end of likely broken path record.");

            buf.pop_back();

            ++len;

            break;
        }

        ++len;
    }

    data.assign(buf.cbegin(), buf.cend());

    return data.size() == len + 1;
}

bool CApplication::readContentString(std::basic_string<char> &data)
{
    std::size_t len = 0;

    std::vector<char> buf;
    buf.reserve(256);

    while (true)
    {
        char c{0};

        fs.read(&c, 1);
        if (fs.gcount() != 1)
        {
            throw std::runtime_error("Failed to read data");
        }

        // NOTE: Strip Windows newlines
        if (endsWith(buf, {0x0D, 0x0A}))
        {
            std::println(stderr, "Found CRLF line ending. Stripping...");

            buf[buf.size() - 2] = std::move(buf.back());
            buf.pop_back();
        }

        if (c == 0x00)
            break;

        buf.insert(buf.end(), c);

        if (bNotepadMode && endsWith(buf, {0x0A, 0x20}))
        {
            std::println(stderr, "Found likely end of broken content record.");

            buf.pop_back();

            ++len;

            break;
        }

        ++len;
    }

    data.assign(buf.cbegin(), buf.cend());

    return data.size() == len + 1;
}

bool CApplication::readSignedInt32LE(int32_t &i)
{
    static_assert(sizeof(int32_t) == 4);

    std::array<uint8_t, 4> buf;

    fs.read(reinterpret_cast<char*>(buf.data()), buf.size());

    const std::size_t &n = fs.gcount();

    if (fs.gcount() != buf.size())
    {
        std::println(stderr, "Read {} byte(s) on a {}-byte(s) operation", n, buf.size());

        return false;
    }

    const uint32_t u =
        static_cast<uint32_t>(buf[0]) |
        static_cast<uint32_t>(buf[1]) << 8 |
        static_cast<uint32_t>(buf[2]) << 16 |
        static_cast<uint32_t>(buf[3]) << 24;

    i = std::bit_cast<std::int32_t>(u);

    return true;
}

bool CApplication::writeBuffer(const void *buf, const int32_t &sz)
{
    fs.write(reinterpret_cast<const char*>(buf), sz);

    return fs.good();
}

bool CApplication::writeSignedInt32LE(const int32_t &i, const int32_t &sz)
{
    if constexpr(std::endian::native == std::endian::little) {
        fs.write(reinterpret_cast<const char*>(&i), sz);

        return fs.good();
    }

    const auto &tmp = std::byteswap(i);

    fs.write(reinterpret_cast<const char*>(&tmp), sz);

    return fs.good();
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
