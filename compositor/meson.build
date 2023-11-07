wsm_compositor = static_library(
        'wsm_compositor',
        files(
        'wsm_view.c',
        'wsm_xdg_shell.c',
        'wsm_workspace_manager.c',
        'wsm_workspace.c',
        'wsm_server.c',
        'wsm_idle_inhibit_v1.c',
        'wsm_layer_shell.c',
        'wsm_container.c',
        'wsm_arrange.c',
        'wsm_xdg_popup.c',
        'wsm_foreign_toplevel.c',
	),
	dependencies: [
        wayland_server,
        server_protos,
        pixman,
        drm_full,
        cairo,
        pango,
        pangocairo,
        ],
        include_directories:[common_inc, xwl_inc, input_inc, output_inc, scene_inc, decoration_inc]
)
