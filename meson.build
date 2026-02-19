project('ME2 CoalFix', 'cpp',
  version: '0.1',
  default_options: [
    'cpp_std=c++26',
    'warning_level=2'
  ],
)

cc = meson.get_compiler('cpp')

sources = [
  'src/main.cxx',
  'src/CApplication.cxx',
  'src/Win32.cxx',
]

inc_dirs = include_directories('.')

if cc.get_id() == 'msvc' or host_machine.system() == 'windows'
  add_project_arguments(['-DUNICODE', '-D_WIN32_WINNT=0x0601'], language: 'cpp')
endif

exe = executable(
  'ME2CoalFix',
  sources,
  include_directories: inc_dirs,
  install: true,
)

test('basic', exe)
