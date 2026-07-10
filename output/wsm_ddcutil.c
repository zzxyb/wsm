#include "../config.h"

#include "wsm_ddcutil.h"

#include "wsm_common.h"
#include "wsm_log.h"
#include "wsm_output.h"

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#if HAVE_DDCUTIL
#include <ddcutil_c_api.h>
#include <pthread.h>
#include <wlr/types/wlr_output.h>
#endif

#define DDC_BRIGHTNESS_VCP_FEATURE_CODE 0x10

#if HAVE_DDCUTIL
struct wsm_ddcutil {
	struct wsm_brightness base;
	DDCA_Display_Ref display_ref;
	char *label;
	char *id;
};

static pthread_mutex_t ddcutil_mutex = PTHREAD_MUTEX_INITIALIZER;
static bool ddcutil_initialized = false;

static struct wsm_ddcutil *ddcutil_from_brightness(
		struct wsm_brightness *brightness) {
	return wl_container_of(brightness, (struct wsm_ddcutil *)0, base);
}

static bool ensure_ddcutil_initialized(void) {
	if (ddcutil_initialized) {
		return true;
	}

	DDCA_Status status = ddca_init(NULL, DDCA_SYSLOG_NOTICE,
		DDCA_INIT_OPTIONS_CLIENT_OPENED_SYSLOG);
	if (status < 0) {
		wsm_log(WSM_ERROR, "Could not initialize ddcutil API: %d", status);
		return false;
	}
	ddcutil_initialized = true;
	return true;
}

static bool ddcutil_status_ok(DDCA_Status status) {
	return status == 0;
}

static char *ddcutil_path_id(DDCA_IO_Path path) {
	char *id = NULL;
	switch (path.io_mode) {
	case DDCA_IO_I2C:
		str_printf(&id, "i2c:%d", path.path.i2c_busno);
		break;
	case DDCA_IO_USB:
		str_printf(&id, "usb:%d", path.path.hiddev_devno);
		break;
	}
	return id;
}

static bool ddcutil_read_brightness(struct wsm_ddcutil *ddc,
		long *brightness, long *max_brightness) {
	bool ok = false;
	DDCA_Display_Handle handle = NULL;

	pthread_mutex_lock(&ddcutil_mutex);
	DDCA_Status status = ddca_open_display2(ddc->display_ref, true, &handle);
	if (!ddcutil_status_ok(status)) {
		wsm_log(WSM_ERROR, "ddca_open_display2 failed: %d", status);
		goto out;
	}

	DDCA_Non_Table_Vcp_Value value;
	status = ddca_get_non_table_vcp_value(handle,
		DDC_BRIGHTNESS_VCP_FEATURE_CODE, &value);
	if (!ddcutil_status_ok(status)) {
		wsm_log(WSM_ERROR, "ddca_get_non_table_vcp_value failed: %d", status);
		goto out;
	}

	*brightness = value.sh << 8 | value.sl;
	*max_brightness = value.mh << 8 | value.ml;
	ok = true;

out:
	if (handle) {
		DDCA_Status close_status = ddca_close_display(handle);
		if (!ddcutil_status_ok(close_status)) {
			ok = false;
			wsm_log(WSM_ERROR, "ddca_close_display failed: %d", close_status);
		}
	}
	pthread_mutex_unlock(&ddcutil_mutex);
	return ok;
}

static bool ddcutil_set_brightness(struct wsm_brightness *brightness,
		long value) {
	struct wsm_ddcutil *ddc = ddcutil_from_brightness(brightness);
	bool ok = false;
	DDCA_Display_Handle handle = NULL;

	pthread_mutex_lock(&ddcutil_mutex);
	DDCA_Status status = ddca_open_display2(ddc->display_ref, true, &handle);
	if (!ddcutil_status_ok(status)) {
		wsm_log(WSM_ERROR, "ddca_open_display2 failed: %d", status);
		goto out;
	}

	long current = -1;
	DDCA_Non_Table_Vcp_Value vcp_value;
	status = ddca_get_non_table_vcp_value(handle,
		DDC_BRIGHTNESS_VCP_FEATURE_CODE, &vcp_value);
	if (ddcutil_status_ok(status)) {
		current = vcp_value.sh << 8 | vcp_value.sl;
	} else {
		wsm_log(WSM_ERROR, "ddca_get_non_table_vcp_value failed: %d", status);
	}

	if (current != value) {
		uint8_t sh = value >> 8 & 0xff;
		uint8_t sl = value & 0xff;
		status = ddca_set_non_table_vcp_value(handle,
			DDC_BRIGHTNESS_VCP_FEATURE_CODE, sh, sl);
		if (!ddcutil_status_ok(status)) {
			wsm_log(WSM_ERROR, "ddca_set_non_table_vcp_value failed: %d", status);
			goto out;
		}
	}

	brightness->brightness = value;
	ok = true;

out:
	if (handle) {
		DDCA_Status close_status = ddca_close_display(handle);
		if (!ddcutil_status_ok(close_status)) {
			ok = false;
			wsm_log(WSM_ERROR, "ddca_close_display failed: %d", close_status);
		}
	}
	pthread_mutex_unlock(&ddcutil_mutex);
	return ok;
}

static void ddcutil_destroy(struct wsm_brightness *brightness) {
	struct wsm_ddcutil *ddc = ddcutil_from_brightness(brightness);
	free(ddc->label);
	free(ddc->id);
	free(ddc);
}

static const struct wsm_brightness_impl ddcutil_impl = {
	.destroy = ddcutil_destroy,
	.set_brightness = ddcutil_set_brightness,
};

static bool display_info_matches_output(DDCA_Display_Info *info,
		struct wsm_output *output) {
	const char *model = output->wlr_output->model;
	const char *serial = output->wlr_output->serial;
	if (serial && serial[0] && info->sn[0]) {
		return strcmp(serial, info->sn) == 0;
	}
	if (!model || !model[0] || !info->model_name[0]) {
		return false;
	}
	return strcmp(model, info->model_name) == 0;
}
#endif

struct wsm_brightness *wsm_ddcutil_create(struct wsm_output *output) {
#if HAVE_DDCUTIL
	if (!ensure_ddcutil_initialized()) {
		return NULL;
	}

	DDCA_Display_Ref *display_refs = NULL;
	DDCA_Status status = ddca_get_display_refs(false, &display_refs);
	if (!ddcutil_status_ok(status) || !display_refs) {
		return NULL;
	}

	struct wsm_ddcutil *matched = NULL;
	for (int i = 0; display_refs[i] != NULL; ++i) {
		DDCA_Display_Info *info = NULL;
		if (!ddcutil_status_ok(ddca_get_display_info(display_refs[i], &info))) {
			continue;
		}

		bool matches = display_info_matches_output(info, output);
		if (matches) {
			matched = calloc(1, sizeof(struct wsm_ddcutil));
			if (matched) {
				wsm_brightness_init(&matched->base, &ddcutil_impl,
					output, WSM_BRIGHTNESS_METHOD_DDCUTIL);
				matched->display_ref = display_refs[i];
				matched->label = strdup(info->model_name);
				matched->id = ddcutil_path_id(info->path);
			}
		}
		ddca_free_display_info(info);
		if (matched) {
			break;
		}
	}

	if (!matched) {
		return NULL;
	}

	if (!ddcutil_read_brightness(matched, &matched->base.brightness,
			&matched->base.max_brightness) ||
			matched->base.max_brightness <= 0) {
		wsm_brightness_destroy(&matched->base);
		return NULL;
	}
	matched->base.min_brightness = 0;

	wsm_log(WSM_DEBUG, "Using ddcutil display %s for %s",
		matched->label, output->wlr_output->name);
	return &matched->base;
#else
	return NULL;
#endif
}
