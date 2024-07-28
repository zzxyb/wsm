wsm_shell = static_library(
        'wsm_shell',
        files(
        'wsm_xdg_shell.c',
        'wsm_layer_shell.c',
        'wsm_arrange.c',
        'wsm_layer_popup.c',
        'wsm_xdg_popup.c',
	),
	dependencies: [
        xcb,
        wlroots,
        server_protos,
        ],
        include_directories:[common_inc, compositor_inc, output_inc, scene_inc, input_inc, xwl_inc, decoration_inc, config_inc]
)
