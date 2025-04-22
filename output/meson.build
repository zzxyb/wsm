wsm_output = static_library(
	'wsm_output',
	files(
		'wsm_output.c',
		'wsm_backlight_device.c',
		'wsm_output_manager.c',
	),
	dependencies: [
		wlroots,
		server_protos,
		drm,
		systemd,
	],
	include_directories:[common_inc, xwl_inc, compositor_inc, scene_inc, input_inc, config_inc, decoration_inc, shell_inc, drm_inc]
)
