wsm_node = static_library(
        'wsm_node',
        files(
        'wsm_text_node.c',
        'wsm_image_node.c',
        'wsm_node_descriptor.c',
	),
	dependencies: [
        wlroots,
        wayland_server,
        cairo,
        pango,
        pangocairo,
        math,
        xpm,
        jpeg,
        svg,
        ],
        include_directories:[common_inc, compositor_inc, xwl_inc]
)
