project(
  'ME2CoalFix',
  'cpp',
  version: '0.1',
  default_options: [
    'warning_level=3',
    'cpp_std=c++17'
  ],
)

exe = executable(
  'ME2CoalFix',
  'src/main.cxx',
  'src/CApplication.cxx',
  'src/Win32.cxx',
  cpp_args: [
    '-D_UNICODE=1',
    '-DUNICODE=1',
  ],
  install: true,
)

test('basic', exe)
