wsm_data = static_library(
	'wsm_data',
	files('wsm_output_memory.c'),
	dependencies: [wlroots, drm],
	include_directories: [common_inc, compositor_inc, output_inc, config_inc,
		scene_inc, data_inc, xwl_inc],
)

home = run_command('sh', '-c', 'printf %s "$HOME"', check: true).stdout()
meson.add_install_script(
	'sh',
	join_paths(meson.current_source_dir(), 'install-output-memory.sh'),
	home,
	join_paths(meson.current_source_dir(), 'outputs.toml'),
)
