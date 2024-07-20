wsm_input = static_library(
	'wsm_input',
        files(
        'gesture/wsm_gesture_drag.c',
        'gesture/wsm_gesture_single.c',
        'gesture/wsm_gesture_swipe.c',
        'gesture/wsm_gesture_zoom.c',
        'gesture/wsm_gesture.c',
        'wsm_input_manager.c',
        'wsm_seat.c',
        'wsm_cursor.c',
        'wsm_keyboard.c',
        'wsm_tablet.c',
        'wsm_switch.c',
        'wsm_input.c',
        'wsm_text_input.c',
        'wsm_seatop_default.c',
        'wsm_seatop_move_floating.c',
        'wsm_seatop_resize_floating.c',
        'wsm_seatop_down.c',
        'wsm_text_input.c'
	),
	dependencies: [
        wayland_server,
        wlroots,
        server_protos,
        libinput,
        xkbcommon,
        xcb,
        xcb_icccm,
        ],
        include_directories:[common_inc, xwl_inc, compositor_inc, scene_inc, output_inc, shell_inc, decoration_inc, config_inc]
)
