#include "wsm_input_config.h"

#include <xkbcommon/xkbcommon.h>

bool wsm_inputs_config_load(struct wsm_inputs_config *configs) {
	return false;
}

void input_config_fill_rule_names(struct input_config *ic,
	struct xkb_rule_names *rules) {
	rules->layout = ic->xkb_layout;
	rules->model = ic->xkb_model;
	rules->options = ic->xkb_options;
	rules->rules = ic->xkb_rules;
	rules->variant = ic->xkb_variant;
}
