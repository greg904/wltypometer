project(
	'wltypometer',
	'c',
	version: '0.1.0',
	license: 'GPL',
)

wayland_client = dependency('wayland-client')
xkbcommon = dependency('xkbcommon')
egl = dependency('egl')
glesv2 = dependency('glesv2')

subdir('protocol')

executable(
	'wltypometer',
	files([
		'main.c',
	]),
	dependencies: [
		client_protos,
		wayland_client,
        egl,
        glesv2,
	],
	install: true,
)
