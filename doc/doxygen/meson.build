# Here be dragons
# This is a workaround for Meson's custom_target() directive, which
# currently does not support outputs pointing to a sub-directory
# XXX: try turning these into maps, so they can be indexed with picture name
dot_png = []
dot_map = []

doxygen_conf = configuration_data()
doxygen_conf.set('VERSION', meson.project_version())
doxygen_conf.set('top_builddir', meson.project_build_root())
wsm_doxygen = configure_file(
		input: 'wsm.doxygen.in',
		output: 'wsm.doxygen',
	configuration: doxygen_conf,
)

src_files = files([
		'../../common/wsm_cairo.h',
		'../../common/wsm_common.h',
		'../../common/wsm_desktop.h',
		'../../common/wsm_list.h',
		'../../common/wsm_log.h',
		'../../common/wsm_pango.h',
		'../../common/wsm_parser.h',
		'../../compositor/wsm_container.h',
		'../../compositor/wsm_idle_inhibit_v1.h',
		'../../compositor/wsm_server.h',
		'../../compositor/wsm_session_lock.h',
		'../../compositor/wsm_transaction.h',
		'../../compositor/wsm_view.h',
		'../../compositor/wsm_workspace.h',
		'../../compositor/wsm_workspace_manager.h',
		'../../config/wsm_config.h',
		'../../config/wsm_input_config.h',
		'../../config/wsm_keyboard_shortcuts_config.h',
		'../../config/wsm_output_config.h',
		'../../decoration/wsm_titlebar.h',
		'../../decoration/wsm_server_decoration.h',
		'../../decoration/wsm_server_decoration_manager.h',
		'../../decoration/wsm_xdg_decoration_manager.h',
		'../../decoration/wsm_xdg_decoration.h',
		'../../input/wsm_input_manager.h',
		'../../input/wsm_seat.h',
		'../../input/wsm_seatop_move_floating.h',
		'../../input/wsm_tablet.h',
		'../../input/wsm_cursor.h',
		'../../input/wsm_keyboard.h',
		'../../input/wsm_seatop_resize_floating.h',
		'../../input/wsm_text_input.h',
		'../../input/wsm_input.h',
		'../../input/wsm_pointer.h',
		'../../input/wsm_seatop_down.h',
		'../../input/wsm_switch.h',
		'../../output/wsm_backlight.h',
		'../../output/wsm_output.h',
		'../../output/wsm_output_manager.h',
		'../../scene/wsm_scene.h',
		'../../scene/node/wsm_image_node.h',
		'../../scene/node/wsm_node_descriptor.h',
		'../../scene/node/wsm_text_node.h',
		'../../shell/wsm_arrange.h',
		'../../shell/wsm_layer_popup.h',
		'../../shell/wsm_layer_shell.h',
		'../../shell/wsm_xdg_popup.h',
		'../../shell/wsm_xdg_shell.h',
		'../../xwl/wsm_xwayland.h',
])

gen_doxygen = find_program('gen-doxygen.py')

subdir('xml')

formats = {
	'html': {
		'wsm': src_files,
	},
}

foreach f_name, sections: formats
	foreach s_name, s_files: sections
		t_name = '@0@-@1@-doc'.format(f_name, s_name)

		# We do not really need an output file, but Meson
		# will complain if one is not set, so we use a
		# dummy 'stamp' file
		stamp = join_paths(meson.current_build_dir(), '@0@.stamp'.format(t_name))
		custom_target(
			t_name,
			command: [
				gen_doxygen,
				# XXX pass doxygen path as argument
				'--builddir=@OUTDIR@',
				'--section=@0@'.format(s_name),
				'--output-format=@0@'.format(f_name),
				'--stamp=@0@'.format(stamp),
					wsm_doxygen,
				'@INPUT@',
			],
			input: s_files,
			output: '@0@.stamp'.format(t_name),
			depends: [dot_png, dot_map],
			build_by_default: true,
		)
	endforeach
endforeach

man_files = src_files
stamp = join_paths(meson.current_build_dir(), 'man3.stamp')
custom_target(
	'man-pages-3',
	command: [
		gen_doxygen,
		'--builddir=@OUTDIR@',
		'--output-format=man3',
		'--stamp=@0@'.format(stamp),
			wsm_doxygen,
		'@INPUT@',
	],
	input: man_files,
	output: 'man3',
	build_by_default: true,
	install: true,
	install_dir: join_paths(get_option('prefix'), get_option('mandir')),
)
