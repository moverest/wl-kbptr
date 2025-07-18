project(
  'wl-kbptr',
  'c',
  meson_version: '>=1.1',
  default_options: ['c_std=c11'],
  license: 'GPL-3.0',
  version: '0.4.0',
)

use_opencv = get_option('opencv').enabled()
version_suffix = ''

if get_option('buildtype') == 'debug'
  add_project_arguments('-DDEBUG', language: ['c', 'cpp'])
  version_suffix = '-debug'
endif

add_project_arguments(
  '-D_GNU_SOURCE=200809L',
  '-DVERSION="@0@@1@"'.format(meson.project_version(), version_suffix),
  language: 'c',
)

cc = meson.get_compiler('c')

if use_opencv
  add_project_arguments('-DOPENCV_ENABLED=1', language: ['c', 'cpp'])
  add_languages('cpp', native: false)
  opencv = dependency('opencv4')
  pixman = dependency('pixman-1')
endif

wayland_client = dependency('wayland-client')
wayland_protos = dependency('wayland-protocols')
xkbcommon = dependency('xkbcommon')
cairo = dependency('cairo')
math = cc.find_library('m')

subdir('protocol')

dependencies = [
  wayland_client,
  xkbcommon,
  cairo,
  math,
]

sources = [
  'src/main.c',
  'src/surface_buffer.c',
  'src/mode.c',
  'src/mode_tile.c',
  'src/mode_floating.c',
  'src/mode_bisect.c',
  'src/mode_split.c',
  'src/mode_click.c',
  'src/utils.c',
  'src/utils_cairo.c',
  'src/utils_wayland.c',
  'src/config.c',
  'src/label.c',
  protos_src,
]

if use_opencv
  sources += [
    'src/screencopy.c',
    'src/target_detection.cpp',
  ]
  dependencies += [opencv, pixman]
endif

executable(
  'wl-kbptr',
  sources,
  dependencies: dependencies,
  install: true,
)

label_test_exec = executable(
  'test_label',
  [
    'src/test_label.c',
    'src/label.c',
    'src/utils.c',
  ],
)

test('test_label', label_test_exec)

install_data(
  'share/wl-kbptr.desktop',
  rename: 'wl-kbptr.desktop',
  install_dir: 'share/applications',
)
