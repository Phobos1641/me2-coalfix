project(
  'ME2CoalescedFixer',
  'cpp',
  version: '0.1',
  default_options: [
    'warning_level=3',
    'cpp_std=c++17'
  ],
)

exe = executable(
  'ME2CoalescedFixer',
  'src/main.cxx',
  cpp_args: [
    '-D_UNICODE=1',
    '-DUNICODE=1',
  ],
  install: true,
)

test('basic', exe)
