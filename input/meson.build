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
        'wsm_seatop.c'
	),
	dependencies: [
        wlroots,
        server_protos,
        libinput,
        xkbcommon,
        ],
        include_directories:[common_inc, xwl_inc, compositor_inc, scene_inc, output_inc, node_inc]
)
