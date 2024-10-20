man_conf = configuration_data()
man_conf.set('version', meson.project_version())

dir_prefix = get_option('prefix')
dir_man = join_paths(dir_prefix, get_option('mandir'))

configure_file(
	input: 'wsm.man',
	output: 'wsm.1',
	install_dir: join_paths(dir_man, 'man1'),
	configuration: man_conf
)
