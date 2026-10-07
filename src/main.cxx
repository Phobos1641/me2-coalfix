#include "CApplication.hxx"

#include <exception>
#include <print>

// Format specification
//
// byte[4] magic: 0x1E000000
// {
//   int32_t path_size: size of path string, including NULL
//   char* path: variable sized NULL terminated string
//   int32_t data_size: size of data, including NULL
//   char* content: variable sized NULL terminated string, ASCII LF-style (0x0A) newlines
// }

int main(int argc, char *argv[])
{
    try
    {
        coalfix::CApplication application;

        return application.run(argc, argv);
    }
    catch (const std::exception &e)
    {
        std::println(stderr, "fatal exception: {}", e.what());

        return EXIT_FAILURE;
    }
}
