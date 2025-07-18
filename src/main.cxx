#include "CApplication.hxx"

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
    CApplication app;

    return app.run(argc, argv);
}
