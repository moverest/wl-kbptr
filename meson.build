project(
  'wl-kbptr',
  'c',
  meson_version: '>=1.1',
  default_options: ['c_std=c11'],
  license: 'GPL-3.0',
  version: '0.4.1',
)

use_opencv = get_option('opencv').enabled()
use_kde = get_option('kde').enabled()
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

if use_kde
  add_project_arguments('-DKDE_ENABLED=1', language: ['c'])
  libei = dependency('libei-1.0')
  sdbus = dependency('libsystemd', required: false)
  if not sdbus.found()
    sdbus = dependency(
      'basu',
      not_found_message: 'KDE backend requires libsystemd or basu (sd-bus).',
    )
    # basu ships its sd-bus header under <basu/sd-bus.h>, not <systemd/sd-bus.h>.
    add_project_arguments('-DUSE_BASU_SDBUS=1', language: ['c'])
  endif
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
  'src/pointer.c',
  'src/pointer_wlr.c',
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

if use_kde
  sources += [
    'src/pointer_kde.c',
    'src/pointer_kde_coords.c',
    'src/eis_dbus.c',
  ]
  dependencies += [libei, sdbus]
endif

if use_kde and use_opencv
  sources += ['src/screenshot_kde.c']
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

pointer_test_exec = executable(
  'test_pointer',
  [
    'src/test_pointer.c',
    'src/pointer_kde_coords.c',
  ],
)

test('test_pointer', pointer_test_exec)

install_data(
  'share/wl-kbptr.desktop',
  rename: 'wl-kbptr.desktop',
  install_dir: 'share/applications',
)
