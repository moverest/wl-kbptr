project(
  'wl-kbptr',
  'c',
  meson_version: '>=0.60.0',
  default_options: ['c_std=c11'],
)

add_project_arguments('-D_GNU_SOURCE=200809L', language: 'c')
cc = meson.get_compiler('c')

wayland_client = dependency('wayland-client')
wayland_protos = dependency('wayland-protocols')

subdir('protocol')

executable(
  'wl-kbptr',
  [
    'src/main.c',
    'src/surface-buffer.c',
    protos_src,
  ],
  dependencies: [
    wayland_client,
  ],
  install: true,
)
