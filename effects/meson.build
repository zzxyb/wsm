wsm_effect = static_library(
        'wsm_effect',
        files(
        'wsm_effect.c',
        'blur/wsm_gl_blur_base.c',
        'blur/wsm_gl_blur.c',
        'scissors/wsm_gl_scissors_base.c',
        'scissors/wsm_gl_scissors.c',
        ),
        dependencies: [
        wlroots,
        server_protos,
        ],
        include_directories:[common_inc]
)
