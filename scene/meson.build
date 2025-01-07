wsm_scene = static_library(
	'wsm_scene',
	files(
		'wsm_scene.c',
		'wsm_scene.h',
		'node/wsm_node.c',
		'node/wsm_text_node.c',
		'node/wsm_image_node.c',
		'node/wsm_node_descriptor.c',
		'effects/wsm_effect.c',
		'effects/blur/wsm_gl_blur_base.c',
		'effects/blur/wsm_gl_blur.c',
		'effects/scissors/wsm_gl_scissors_base.c',
		'effects/scissors/wsm_gl_scissors.c',
	),
	dependencies: [
		wlroots,
		server_protos,
		cairo,
		pango,
		pangocairo,
		math,
		xpm,
		jpeg,
		svg,
		drm_full,
	],
	include_directories:[common_inc, xwl_inc, input_inc, output_inc, compositor_inc, decoration_inc, shell_inc]
)
