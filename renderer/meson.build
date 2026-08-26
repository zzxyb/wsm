wsm_sources += files(
	'gles/egl.c',
	'gles/renderer.c',
	'gles/pixel_format.c',
	'gles/render_timer.c',

	'pixman/pixel_format.c',
	'pixman/renderer.c',

	'wsm_renderer.c',
)

glslang = find_program('glslang', 'glslangValidator', native: true, required: false, disabler: true)

subdir('gles/shaders')
