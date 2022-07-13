project(
	'tmp',
	'c',
	version: '0.1.0',
	license: 'GPL',
)

wayland_client = dependency('wayland-client')
wayland_protos = dependency('wayland-protocols', version: '>=1.14')
xkbcommon = dependency('xkbcommon')
egl = dependency('egl')
glesv2 = dependency('glesv2')

subdir('protocol')

executable(
	'tmp',
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
