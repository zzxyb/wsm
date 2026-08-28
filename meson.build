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

little_endian = target_machine.endian() == 'little'
big_endian = target_machine.endian() == 'big'

add_project_arguments([
	'-DWSM_PRIVATE=',
	'-Wno-error=expansion-to-defined',
	'-DWLR_USE_UNSTABLE',
	'-DWLR_PRIVATE=',
	'-D_POSIX_C_SOURCE=200809L',
	'-Wno-unused-parameter',
	'-Wno-unused-result',
	'-Wno-missing-braces',
	'-Wundef',
	'-Wvla',
	'-DWSM_LITTLE_ENDIAN=@0@'.format(little_endian.to_int()),
	'-DWSM_BIG_ENDIAN=@0@'.format(big_endian.to_int()),
],language: 'c',
)

cc = meson.get_compiler('c')

is_freebsd = host_machine.system().startswith('freebsd')
datadir = get_option('datadir')
sysconfdir = get_option('sysconfdir')
prefix = get_option('prefix')

if is_freebsd
	add_project_arguments('-D_C11_SOURCE', language: 'c')
endif

wlroots = dependency('wlroots-0.21', version: '>=0.21.0', fallback: 'wlroots')
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
threads = dependency('threads')
wayland_client = dependency('wayland-client', required: false, disabler: true)

wayland_kwargs = {
	'version': '>=1.24.0',
	'fallback': 'wayland',
	'default_options': [
		'tests=false',
		'documentation=false',
	],
}
wayland_server = dependency('wayland-server',
	kwargs: wayland_kwargs,
)

msg = []
if wlroots_features['libinput_backend']
  msg += ['wlroots: libinput backend enabled.']
  libinput = dependency(
    'libinput',
    version: '>=1.19.0',
    required: true,
    not_found_message: '\n'.join(msg),
  )
else
  msg += ['wlroots: libinput backend disabled (wlroots_features["libinput_backend"] = false).']
  libinput = null_dep
endif

wayland_protos = dependency('wayland-protocols',
	version: '>=1.44',
	fallback: 'wayland-protocols',
	default_options: ['tests=false'],
)

xcb = wlroots_features['xwayland'] ? dependency('xcb') : null_dep
xcb_icccm = wlroots_features['xwayland'] ? dependency('xcb-icccm') : null_dep
cairo = dependency('cairo')
pangocairo = dependency('pangocairo')
pango = dependency('pango')
gio = dependency('gio-2.0')
drm = dependency('libdrm',
	version: '>=2.4.122',
	fallback: 'libdrm',
	default_options: [
		'auto_features=disabled',
		'tests=false',
	],
)
egl = dependency('egl', version: '>= 1.5', required: false, disabler: true)
glesv2 = dependency('glesv2', required: false, disabler: true)
gbm = dependency('gbm', required: false, disabler: true)
pixman = dependency('pixman-1',
	version: '>=0.43.0',
	fallback: 'pixman',
	default_options: ['werror=false'],
)
lcms2 = dependency('lcms2', required: true)
xkbcommon = dependency('xkbcommon',
	version: '>=1.8.0',
	fallback: 'libxkbcommon',
	default_options: [
		'enable-tools=false',
		'enable-x11=false',
		'enable-docs=false',
		'enable-xkbregistry=false',
	],
)
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

wsm_inc = include_directories('include')

if get_option('documentation').enabled()
	subdir('doc')
endif

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

wsm_sources = files(
)

wsm_sources += files(
	'server/wsm_server.c',
)

subdir('protocols')
subdir('util')
subdir('renderer')
subdir('buffer')
subdir('texture')
subdir('pass')
subdir('types')
subdir('node')
subdir('scene')

wsm_deps = [
	server_protos,
	math,
	threads,
	wayland_server,
	wlroots,
	cairo,
	pangocairo,
	pango,
	gio,
	drm,
	egl,
	glesv2,
	gbm,
	pixman,
	lcms2,
	xkbcommon,
]

if have_xwayland
endif

if wlroots_features['libinput_backend']
endif

executable(
	'wsm',
	wsm_sources,
	include_directories: [wsm_inc],
	dependencies: wsm_deps,
	link_with: [],
	install: true
)
