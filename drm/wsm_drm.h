#ifndef WSM_DRM_h
#define WSM_DRM_h

typedef struct _drmModeConnector drmModeConnector;

struct wlr_backend;

struct wsm_output;

drmModeConnector *wsm_drm_get_connector_from_output(struct wsm_output *output,
	struct wlr_backend *backend);

#endif