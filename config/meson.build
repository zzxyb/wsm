libxml = dependency('libxml-2.0')

wsm_config_deps = [
	libxml,
]

wsm_config = static_library(
	'wsm_config',
	files(
		'wsm_config.c',
		'wsm_output_config.c',
		'wsm_output_manager_config.c',
		'wsm_input_config.c',
		'wsm_keyboard_shortcuts_config.c',
	),
	dependencies: [
		wsm_config_deps,
		wlroots,
		server_protos,
		xcb_icccm,
		cairo,
		pango,
		pangocairo,
		drm_full,
	],
	include_directories:[common_inc, compositor_inc, scene_inc, output_inc, xwl_inc, input_inc, decoration_inc]
)
