project(
  'wl-kbptr',
  'c',
  meson_version: '>=0.60.0',
  default_options: ['c_std=c11'],
  license: 'GPL-3.0',
  version: '0.2.3',
)

add_project_arguments('-D_GNU_SOURCE=200809L', language: 'c')
cc = meson.get_compiler('c')

wayland_client = dependency('wayland-client')
wayland_protos = dependency('wayland-protocols')
xkbcommon = dependency('xkbcommon')
cairo = dependency('cairo')
math = cc.find_library('m')

subdir('protocol')

executable(
  'wl-kbptr',
  [
    'src/main.c',
    'src/surface_buffer.c',
    'src/mode_tile.c',
    'src/mode_bisect.c',
    'src/utils.c',
    'src/utils_cairo.c',
    'src/config.c',
    'src/label.c',
    protos_src,
  ],
  dependencies: [
    wayland_client,
    xkbcommon,
    cairo,
    math,
    wayland_client,
  ],
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
