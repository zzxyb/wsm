#include "util/wsm_env.h"
#include "util/wsm_log.h"

#include <stdlib.h>
#include <string.h>

bool env_parse_bool(const char *option) {
	const char *env = getenv(option);
	if (env) {
		wsm_log(WSM_INFO, "Loading %s option: %s", option, env);
	}

	if (!env || strcmp(env, "0") == 0) {
		return false;
	} else if (strcmp(env, "1") == 0) {
		return true;
	}

	wsm_log(WSM_ERROR, "Unknown %s option: %s", option, env);
	return false;
}

size_t env_parse_switch(const char *option, const char **switches) {
	const char *env = getenv(option);
	if (env) {
		wsm_log(WSM_INFO, "Loading %s option: %s", option, env);
	} else {
		return 0;
	}

	for (ssize_t i = 0; switches[i]; i++) {
		if (strcmp(env, switches[i]) == 0) {
			return i;
		}
	}

	wsm_log(WSM_ERROR, "Unknown %s option: %s", option, env);
	return 0;
}
