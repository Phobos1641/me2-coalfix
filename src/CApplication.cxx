#include "CApplication.hxx"

#include <algorithm>
#include <bit>
#include <array>
#include <locale>
#include <codecvt>
#include <print>
#include <cassert>
#include <iostream>
#include <cerrno>

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

template <typename T>
constexpr bool compare_ustring_size(const SUnrealString &ustring, const T &sz)
{
    static_assert(std::is_integral_v<T>);

    // NOTE: The expected size passed will include NULL
    const auto &i = ustring.data.size() + 1;

    if (std::numeric_limits<T>::is_signed && sz < 0)
        return false;

    if (i > std::numeric_limits<int32_t>::max() || sz > std::numeric_limits<int32_t>::max())
        return false;

    return i == static_cast<std::size_t>(sz);
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

    errno = 0;

    return b;
}

std::string getPathBasename(const fs::path &path)
{
    return path.filename().string();
}

inline void printErrorReason(const std::string_view &err, int code)
{
    if (!err.empty())
    {
        std::println(stderr, "{}: {} = {}", err, code, std::strerror(code));

        return;
    }

    std::println(stderr, "{} = {}", code, std::strerror(code));
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
        printErrorReason("open failed", errno);

        return EXIT_FAILURE;
    }

    std::array<uint8_t, 4> magic{0};
    if (!readBuffer(magic.data(), magic.size()))
    {
        throw std::runtime_error("Failed to read magic");
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

        // Read section path, size and data
        if (!readSignedInt32LE(section.path.size))
        {
            printErrorReason("Failed to read section path size", errno);

            break;
        }

        const auto psize = section.path.size;

        // NOTE: The path specified should normally not be too long.
        if (section.path.size >= 260)
            std::println(stderr, "Path section string size reported as {}. Assuming broken record...", section.path.size);

        if (!readPathString(section.path))
        {
            std::runtime_error("Failed to read section path");
        }

        std::println(stderr, "Read section path '{}', sized {} byte(s) of reported {} byte(s)", section.path.data, section.path.size, psize);

        // Read section content, size and data
        if (!readSignedInt32LE(section.content.size))
        {
            std::runtime_error("Failed to read section content size");
        }

        const auto csize = section.content.size;

        if (!readContentString(section.content))
        {
            throw std::runtime_error("Failed to read section content");
        }

        std::println(stderr, "Read section content, sized {} byte(s) of reported {} byte(s)", section.content.size, csize);

        vSections.push_back(section);
    }

    std::println(stderr, "Finished reading {} sections", vSections.size());

    fs.close();

    std::println(stderr, "Re-opening file for truncation...");

    fs.open(sFinalPath, std::fstream::out | std::fstream::binary | std::fstream::trunc);

    if (!fs.good())
    {
        printErrorReason("open failed", errno);

        return EXIT_FAILURE;
    }

    if (!writeArray(magic))
    {
        printErrorReason("Failed to write magic", errno);

        return EXIT_FAILURE;
    }

    for (auto &it: vSections)
    {
        std::println("Writing section with path '{}' ({} chars), of size {}", it.path.data.data(), it.path.size, it.content.size);

        writeUString(it.path);
        writeUString(it.content);
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

bool CApplication::readPathString(SUnrealString &ustring)
{
    std::vector<char> buf;
    buf.reserve(256);

    const auto &sz = ustring.size;

    #ifndef NDEBUG
    std::println("Reading path string...");
    #endif

    while (true)
    {
        char c{0};

        fs.read(&c, 1);
        if (fs.gcount() != 1)
        {
            throw std::runtime_error("Failed to read path data");
        }

        if (c == 0x00)
        {
            #ifndef NDEBUG
            std::println("Read NULL byte. Breaking...");
            #endif

            break;
        }

        buf.insert(buf.end(), c);

        // Hex compare .ini || .INI + SPACE, Notepad transforms 0x00 to 0x20

        if (bNotepadMode && (endsWith(buf, {0x2E, 0x69, 0x6E, 0x69, 0x20}) ||
                             endsWith(buf, {0x2E, 0x49, 0x4E, 0x49, 0x20})))
        {
            std::println(stderr, "Found end of likely broken path record.");

            buf.pop_back();

            break;
        }
    }

    ustring.data.clear();
    ustring.data.assign(buf.cbegin(), buf.cend());

    if (bNotepadMode)
        ustring.size = buf.size() + 1;

    return bNotepadMode || compare_ustring_size(ustring, sz);
}

bool CApplication::readContentString(SUnrealString &ustring)
{
    std::vector<char> buf;
    buf.reserve(4096);

    const auto &sz = ustring.size;

    #ifndef NDEBUG
    std::println("Reading content string...");
    #endif

    while (true)
    {
        char c{0};

        fs.read(&c, 1);
        if (fs.gcount() != 1)
        {
            if (bNotepadMode && fs.eof() && buf.size() > 0)
            {
                std::println(stderr, "End of file read with non-empty buffer, breaking...");

                break;
            }

            throw std::runtime_error("Failed to read content data");
        }

        if (c == 0x00)
        {
            #ifndef NDEBUG
            std::println("Read NULL byte. Breaking...");
            #endif

            break;
        }

        buf.insert(buf.end(), c);

        // NOTE: Strip Windows newlines
        if (endsWith(buf, {0x0D, 0x0A}))
        {
            std::println(stderr, "Found CRLF line ending. Stripping...");

            buf[buf.size() - 2] = std::move(buf.back());
            buf.pop_back();
        }

        if (bNotepadMode && endsWith(buf, {0x0A, 0x20}))
        {
            std::println(stderr, "Found likely end of broken content record.");

            buf.pop_back();

            break;
        }
    }

    ustring.data.clear();
    ustring.data.assign(buf.cbegin(), buf.cend());

    if (bNotepadMode)
        ustring.size = buf.size() + 1;

    return bNotepadMode || compare_ustring_size(ustring, sz);
}

bool CApplication::readSignedInt32LE(int32_t &i)
{
    static_assert(sizeof(int32_t) == 4);

    std::array<uint8_t, 4> buf{0};

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

    i = std::bit_cast<int32_t>(u);

    return true;
}

template <typename T, std::size_t N>
bool CApplication::writeArray(const std::array<T, N> &arr)
{
    fs.write(reinterpret_cast<const char*>(arr.data()), arr.size());

    return fs.good();
}

bool CApplication::writeString(const std::basic_string<char> &str)
{
    fs.write(reinterpret_cast<const char*>(str.c_str()), str.length() + 1);

    return fs.good();
}

void CApplication::writeUString(const SUnrealString &ustring)
{
    if (!writeSignedInt32LE(ustring.size))
    {
        throw std::runtime_error("Failed to write UString size...");
    }

    if (!writeString(ustring.data))
    {
        throw std::runtime_error("Failed to write UString data...");
    }
}

bool CApplication::writeSignedInt32LE(const int32_t &i)
{
    if constexpr(std::endian::native == std::endian::little) {
        fs.write(reinterpret_cast<const char*>(&i), sizeof(i));

        return fs.good();
    }

    const auto &tmp = std::byteswap(i);

    fs.write(reinterpret_cast<const char*>(&tmp), sizeof(i));

    return fs.good();
}

}
