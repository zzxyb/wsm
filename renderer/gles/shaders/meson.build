check = find_program('./check.sh', native: true)
embed = find_program('./embed.sh', native: true)

shaders = [
	'blur_quad.vert',
	'round_tex.vert',
	'blur_down_rgba.frag',
	'blur_down_external.frag',
	'blur_up.frag',
	'blur_round.frag',

	'common.vert',
	'quad.frag',
	'round_rect.vert',
	'round_rect.frag',
	'round_border.frag',
	'shadow_cache.vert',
	'shadow_cache.frag',
	'tex_rgba.frag',
	'tex_rgbx.frag',
	'tex_external.frag',
]

foreach name : shaders
	custom_target(
		'gles2-' + name,
		input: name,
		output: name + '_check',
		command: [check, glslang, '@INPUT@'],
		capture: true,
		build_by_default: true,
	)

	output = name.underscorify() + '_src.h'
	var = name.underscorify() + '_src'
	wsm_sources += custom_target(
		output,
		command: [embed, var],
		input: name,
		output: output,
		feed: true,
		capture: true,
	)
endforeach
