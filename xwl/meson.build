wsm_xwl = static_library(
	'wsm_xwl',
	files(
		'wsm_xwayland.c',
		'wsm_xwayland_unmanaged.c',
	),
	dependencies: [
		xcb,
		wlroots,
		wayland_server,
		server_protos,
		xcb_icccm,
		cairo,
		pango,
		pangocairo,
	],
	include_directories:[common_inc, compositor_inc, output_inc, scene_inc, input_inc, decoration_inc, config_inc, shell_inc]
)
