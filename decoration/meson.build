wsm_decoration = static_library(
        'wsm_decoration',
        files(
            'wsm_server_decoration_manager.c',
            'wsm_xdg_decoration_manager.c',
            'wsm_xdg_decoration.c',
            'wsm_server_decoration.c',
            'wsm_titlebar.c',
        ),
        dependencies: [
        wlroots,
        server_protos,
        ],
        include_directories:[compositor_inc, common_inc, xwl_inc]
)
