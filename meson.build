project(
	'wltypometer',
	'c',
	version: '0.1.0',
	license: 'GPL',
	default_options: [
		'c_std=c89',
	],
)

wl_client = dependency('wayland-client')
egl = dependency('egl')
gles = dependency('glesv2')

wl_scanner = find_program('wayland-scanner')

scanner_private_code = generator(
	wl_scanner,
	output: '@BASENAME@-protocol.c',
	arguments: ['private-code', '@INPUT@', '@OUTPUT@'],
)

scanner_client_header = generator(
	wl_scanner,
	output: '@BASENAME@-client-protocol.h',
	arguments: ['client-header', '@INPUT@', '@OUTPUT@'],
)

protos_src = []
protos_headers = []

protos = [
	'wlr-export-dmabuf-unstable-v1.xml',
	'virtual-keyboard-unstable-v1.xml',
]

foreach protocol : protos
	protos_src += scanner_private_code.process(protocol)
	protos_headers += scanner_client_header.process(protocol)
endforeach

protos_lib = static_library(
	'protocols',
	protos_src + protos_headers,
	dependencies: [wl_client]
)

protos_dep = declare_dependency(
	link_with: protos_lib,
	sources: protos_headers,
)

executable(
	'wltypometer',
	files([
		'main.c',
		'args.c',
		'ev_loop.c',
	]),
	dependencies: [
		protos_dep,
		wl_client,
		egl,
		gles,
	],
	install: true,
)
