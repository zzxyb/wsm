wsm_xwl = static_library(
        'wsm_xwl',
        files(
        'wsm_xwayland.c',
        'wsm_xwayland_unmanaged.c',
	),
	dependencies: [
        wlroots,
        server_protos,
        ],
        include_directories:[common_inc, compositor_inc, output_inc]
)
