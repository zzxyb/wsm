brightness_protocol = '../protocols/wsm-brightness-management-unstable-v1.xml'

brightness_client_header = custom_target(
	'wsm_brightness_management_client_header',
	input: brightness_protocol,
	output: 'wsm-brightness-management-unstable-v1-client-protocol.h',
	command: [wayland_scanner, 'client-header', '@INPUT@', '@OUTPUT@'],
)

brightness_client_code = custom_target(
	'wsm_brightness_management_client_code',
	input: brightness_protocol,
	output: 'wsm-brightness-management-unstable-v1-client-protocol.c',
	command: [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@'],
)

executable(
	'wsm-brightness-client',
	files('wsm_brightness_client.c'),
	brightness_client_header,
	brightness_client_code,
	dependencies: wayland_client,
	install: false,
)
