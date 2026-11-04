vulkan_dep = dependency('vulkan', version: '>=1.2.182', required: get_option('vulkan'))
have_vulkan = false
if not get_option('vulkan').disabled() and vulkan_dep.found()
	if not cc.check_header('vulkan/vulkan.h', dependencies: vulkan_dep)
		if get_option('vulkan').enabled()
			error('Vulkan headers are required for -Dvulkan=enabled')
		endif
		subdir_done()
	endif
	vulkan_glslang = find_program('glslang', 'glslangValidator', native: true, required: get_option('vulkan'))
	if not vulkan_glslang.found()
		subdir_done()
	endif
	glslang_version_info = run_command(vulkan_glslang, '--version', check: true).stdout()
	vulkan_glslang_version = glslang_version_info.split('\n')[0].split(':')[-1].strip()
	have_vulkan = true
	wsm_sources += files(
		'renderer.c',
		'instance.c',
		'device.c',
		'util.c',
		'rect_union.c',
		'render_timer.c',
		'dmabuf_linux.c',
	)
	subdir('shaders')
endif
