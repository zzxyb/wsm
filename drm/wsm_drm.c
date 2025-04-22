#include "wsm_drm.h"
#include "wsm_output.h"

#include <assert.h>

#include <xf86drm.h>
#include <xf86drmMode.h>

#include <wlr/backend.h>
#include <wlr/backend/drm.h>
#include <wlr/types/wlr_output.h>

drmModeConnector *wsm_drm_get_connector_from_output(struct wsm_output *output,
		struct wlr_backend *backend) {
	assert(wlr_output_is_drm(output->wlr_output));
	int fd = wlr_backend_get_drm_fd(backend);
	uint32_t connector_id = wlr_drm_connector_get_id(output->wlr_output);

	return drmModeGetConnectorCurrent(fd, connector_id);
}