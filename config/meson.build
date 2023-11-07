libxml = dependency('libxml-2.0')

wsm_config_deps = [
        libxml,
]

wsm_config = static_library(
	'wsm_config',
        files(
        'wsm_output_config.c',
        'wsm_output_manager_config.c',
        'wsm_input_config.c',
        'wsm_keyboard_shortcuts_config.c',
	),
	dependencies: [
        wsm_config_deps,
        wlroots,
        server_protos,
        ],
        include_directories:[common_inc, compositor_inc, output_inc]
)
