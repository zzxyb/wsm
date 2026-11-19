wsm_sources += files(
	'gles/egl.c',
	'gles/renderer.c',
	'gles/render_timer.c',

	'pixman/renderer.c',

	'wsm_renderer.c',
)

glslang = find_program('glslang', 'glslangValidator', native: true, required: false, disabler: true)

subdir('gles/shaders')
