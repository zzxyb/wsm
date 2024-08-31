wsm_compositor = static_library(
	'wsm_compositor',
	files(
		'wsm_view.c',
		'wsm_workspace_manager.c',
		'wsm_workspace.c',
		'wsm_server.c',
		'wsm_idle_inhibit_v1.c',
		'wsm_container.c',
		'wsm_transaction.c',
		'wsm_session_lock.c',
	),
	dependencies: [
		wlroots,
		wayland_server,
		server_protos,
		pixman,
		drm_full,
		cairo,
		pango,
		pangocairo,
		xcb_icccm,
	],
	include_directories:[common_inc, xwl_inc, input_inc, output_inc, scene_inc, decoration_inc, shell_inc, config_inc]
)
