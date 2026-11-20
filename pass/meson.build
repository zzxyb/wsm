wsm_sources += files(
	'blit.c',
	'gles/blit.c',
	'pixman/blit.c',
	'copy.c',
	'gles/copy.c',
	'pixman/copy.c',
	'texture.c',
	'gles/texture.c',
	'pixman/texture.c',
	'blur.c',
	'gles/blur.c',
	'pixman/blur.c',
	'rect.c',
	'border.c',
	'shadow.c',
	'gles/rect.c',
	'gles/border.c',
	'gles/shadow.c',
	'pixman/rect.c',
	'pixman/border.c',
	'pixman/shadow.c',
	'gles/pass.c',
	'pixman/pass.c',
)

if have_vulkan
	wsm_sources += files(
		'vulkan/pass.c',
		'vulkan/rect.c',
		'vulkan/texture.c',
		'vulkan/border.c',
		'vulkan/shadow.c',
		'vulkan/blur.c',
		'vulkan/copy.c',
		'vulkan/blit.c',
	)
endif
