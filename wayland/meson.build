wsm_wayland = static_library(
	'wsm_wayland',
	files(
		'wsm_brightness_control_v1.c',
	),
	dependencies: [
		wlroots,
		wayland_server,
		server_protos,
	],
	include_directories:[common_inc, config_inc, output_inc, scene_inc]
)
