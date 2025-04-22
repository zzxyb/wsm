project(
	'wsm',
	'c',
	version: '0.1-dev',
	license: 'MIT',
	meson_version: '>=0.60.0',
	default_options: [
		'c_std=c11',
		'warning_level=2',
		'werror=true',
	],
)

add_project_arguments(
	[
		'-Wno-error=expansion-to-defined',
		'-DWLR_USE_UNSTABLE',
		'-D_POSIX_C_SOURCE=200809L',
		'-Wno-unused-parameter',
		'-Wno-unused-result',
		'-Wno-missing-braces',
		'-Wundef',
		'-Wvla',
	],
	language: 'c',
)

cc = meson.get_compiler('c')

is_freebsd = host_machine.system().startswith('freebsd')
datadir = get_option('datadir')
sysconfdir = get_option('sysconfdir')
prefix = get_option('prefix')

if is_freebsd
	add_project_arguments('-D_C11_SOURCE', language: 'c')
endif

wlroots_version = ['>=0.18.0', '<0.20.0']
# subproject(
# 	'wlroots',
# 	default_options: ['examples=false'],
# 	required: false,
# 	version: wlroots_version,
# )
wlroots = dependency('wlroots-0.18', version: wlroots_version, fallback: 'wlroots')
wlroots_features = {
	'xwayland': get_option('xwayland').enabled(),
	'libinput_backend': true,
	'session': true,
}
foreach name, _ : wlroots_features
	var_name = 'have_' + name.underscorify()
	have = wlroots.get_variable(pkgconfig: var_name, internal: var_name) == 'true'
	wlroots_features += { name: have }
endforeach

null_dep = dependency('', required: false)
wayland_server = dependency('wayland-server', version: '>=1.20.0')
wayland_client = dependency('wayland-client')
wayland_cursor = dependency('wayland-cursor')
wayland_egl = dependency('wayland-egl')
pixman = dependency('pixman-1')
xkbcommon = dependency('xkbcommon', version: '>=1.5.0')
libinput = wlroots_features['libinput_backend'] ? dependency('libinput', version: '>=1.26.0') : null_dep
wayland_protos = dependency('wayland-protocols', version: '>=1.24')
drm_full = dependency('libdrm') # only needed for drm_fourcc.h
drm = drm_full.partial_dependency(compile_args: true, includes: true)
threads = dependency('threads') # for pthread_setschedparam
systemd = dependency('libsystemd')
xcb = wlroots_features['xwayland'] ? dependency('xcb') : null_dep
xcb_icccm = wlroots_features['xwayland'] ? dependency('xcb-icccm') : null_dep
udev = wlroots_features['libinput_backend'] ? dependency('libudev') : null_dep
cairo = dependency('cairo')
pango = dependency('pango')
xpm = dependency('xpm')
jpeg = dependency('libjpeg')
svg = dependency('librsvg-2.0')
pangocairo = dependency('pangocairo')
math = cc.find_library('m')
rt = cc.find_library('rt')

if get_option('xwayland').enabled() and not wlroots_features['xwayland']
	error('Cannot enable Xwayland in wsm: wlroots has been built without Xwayland support')
endif
have_xwayland = xcb.found() and xcb_icccm.found() and wlroots_features['xwayland'] and get_option('xwayland').enabled() and get_option('mobile').disabled()

add_project_arguments('-DSYSCONFDIR="/@0@"'.format(join_paths(prefix, sysconfdir)), language : 'c')

version = '"@0@"'.format(meson.project_version())
add_project_arguments('-DWSM_VERSION=@0@'.format(version), language: 'c')

# Compute the relative path used by compiler invocations.
source_root = meson.current_source_dir().split('/')
build_root = meson.global_build_root().split('/')
relative_dir_parts = []
i = 0
in_prefix = true
foreach p : build_root
	if i >= source_root.length() or not in_prefix or p != source_root[i]
		in_prefix = false
		relative_dir_parts += '..'
	endif
	i += 1
endforeach
i = 0
in_prefix = true
foreach p : source_root
	if i >= build_root.length() or not in_prefix or build_root[i] != p
		in_prefix = false
		relative_dir_parts += p
	endif
	i += 1
endforeach
relative_dir = join_paths(relative_dir_parts) + '/'

# Strip relative path prefixes from the code if possible, otherwise hide them.
if cc.has_argument('-fmacro-prefix-map=/prefix/to/hide=')
	add_project_arguments(
		'-fmacro-prefix-map=@0@='.format(relative_dir),
		language: 'c',
	)
else
	add_project_arguments(
		'-DWSM_REL_SRC_DIR="@0@"'.format(relative_dir),
		language: 'c',
	)
endif

common_inc = include_directories('common')
compositor_inc = include_directories('compositor')
input_inc = include_directories('input')
xwl_inc = include_directories('xwl')
output_inc = include_directories('output')
config_inc = include_directories('config')
scene_inc = include_directories('scene')
decoration_inc = include_directories('decoration')
shell_inc = include_directories('shell')
wayland_inc = include_directories('wayland')
drm_inc = include_directories('drm')

subdir('protocols')
subdir('wayland')
subdir('common')
subdir('compositor')
subdir('scene')
subdir('input')
subdir('xwl')
subdir('output')
subdir('config')
subdir('decoration')
subdir('shell')
subdir('man')
subdir('drm')

if get_option('documentation').enabled()
	subdir('doc')
endif

wsm_sources = files(
	'main.c',
)

wsm_deps = [
	wlroots,
	drm_full,
	systemd,
	wayland_server,
	cairo,
	pango,
	pangocairo,
	xpm,
	jpeg,
	svg,
	xcb,
	xcb_icccm,
]

dir_prefix = get_option('prefix')
dir_bin = join_paths(dir_prefix, get_option('bindir'))
dir_data = join_paths(dir_prefix, get_option('datadir'))
dir_libexec = join_paths(dir_prefix, get_option('libexecdir'))
version_wsm = meson.project_version()
conf = configuration_data()
conf.set10('HAVE_XWAYLAND', have_xwayland)
conf.set10('IS_MOBILE', get_option('mobile').enabled())
conf.set_quoted('VERSION', version_wsm)
conf.set_quoted('BINDIR', dir_bin)
conf.set_quoted('DATADIR', dir_data)
conf.set_quoted('LIBEXECDIR', dir_libexec)
conf.set('datadir', join_paths(get_option('prefix'), get_option('datadir')))
conf.set('mandir', join_paths(get_option('prefix'), get_option('mandir')))
conf.set('sysconfdir', join_paths(get_option('prefix'), get_option('sysconfdir')))
configure_file(output: 'config.h', configuration: conf)

executable(
	'wsm',
	wsm_sources,
	dependencies: wsm_deps,
	link_with: [wsm_common, wsm_compositor, wsm_input, wsm_config, wsm_xwl, wsm_output, wsm_scene, wsm_decoration, wsm_shell, wsm_wayland, wsm_drm],
	include_directories:[common_inc, compositor_inc, xwl_inc, output_inc, config_inc, scene_inc, decoration_inc, wayland_inc, drm_inc],
	install: true
)
