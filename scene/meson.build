wsm_scene = static_library(
        'wsm_scene',
        files(
        'wsm_scene.c',
        'wsm_scene.h',
        'wsm_view_item.c',
        ),
        dependencies: [
        wlroots,
        server_protos,
        cairo,
        pango,
        pangocairo,
        math,
        xpm,
        jpeg,
        ],
        include_directories:[common_inc, xwl_inc, input_inc, output_inc, compositor_inc, node_inc]
)
