wayland_scanner_dep = dependency('wayland-scanner', native: true)
wayland_scanner = find_program(
  wayland_scanner_dep.get_variable('wayland_scanner'), native: true,
)

wl_protocol_dir = wayland_protos.get_variable('pkgdatadir')

wayland_scanner_code = generator(
  wayland_scanner, output: '@BASENAME@-protocol.c',
  arguments: ['private-code', '@INPUT@', '@OUTPUT@'],
)

wayland_scanner_header = generator(
  wayland_scanner, output: '@BASENAME@-client-protocol.h',
  arguments: ['client-header', '@INPUT@', '@OUTPUT@'],
)

client_protocols = [
  wl_protocol_dir / 'stable/xdg-shell/xdg-shell.xml',
  wl_protocol_dir / 'unstable/xdg-output/xdg-output-unstable-v1.xml',
  wl_protocol_dir / 'stable/viewporter/viewporter.xml',
  'wlr-layer-shell-unstable-v1.xml',
  'wlr-virtual-pointer-unstable-v1.xml',
  'fractional-scale-v1.xml',
]

protos_src = []
foreach xml : client_protocols
	protos_src += wayland_scanner_code.process(xml)
	protos_src += wayland_scanner_header.process(xml)
endforeach
