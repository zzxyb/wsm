#include "wsm_task_session.h"

#include "wsm_container.h"
#include "wsm_seat.h"
#include "wsm_transaction.h"
#include "wsm_view.h"

static bool select_valid(struct wsm_task_session *session, bool reverse) {
	if (session->model.length == 0) {
		return false;
	}
	for (size_t i = 0; i < session->model.length; ++i) {
		session->selected = reverse
			? (session->selected + session->model.length - 1) %
				session->model.length
			: (session->selected + 1) % session->model.length;
		if (wsm_task_model_get(&session->model, session->selected) != NULL) {
			return true;
		}
	}
	return false;
}

void wsm_task_session_init(struct wsm_task_session *session,
		struct wsm_seat *seat, const struct wsm_task_view *view) {
	*session = (struct wsm_task_session){.seat = seat, .view = *view};
	wsm_task_model_init(&session->model);
}

void wsm_task_session_finish(struct wsm_task_session *session) {
	wsm_task_session_cancel(session);
	wsm_task_model_finish(&session->model);
}

bool wsm_task_session_begin(struct wsm_task_session *session, bool reverse) {
	if (session->active ||
			!wsm_task_model_collect_mru(&session->model, session->seat) ||
			session->model.length == 0) {
		return false;
	}
	session->selected = 0;
	if (session->model.length > 1) {
		select_valid(session, reverse);
	}
	if (!session->view.impl->show(
			session->view.data, &session->model, session->selected)) {
		wsm_task_model_finish(&session->model);
		return false;
	}
	session->active = true;
	return true;
}

void wsm_task_session_step(struct wsm_task_session *session, bool reverse) {
	if (session->active && select_valid(session, reverse)) {
		session->view.impl->update(
			session->view.data, &session->model, session->selected);
	}
}

static void end_session(struct wsm_task_session *session) {
	if (session->active) {
		session->view.impl->hide(session->view.data);
		session->active = false;
	}
	wsm_task_model_finish(&session->model);
}

void wsm_task_session_commit(struct wsm_task_session *session) {
	if (!session->active) {
		return;
	}
	struct wsm_node *node =
		wsm_task_model_get(&session->model, session->selected);
	if (node != NULL) {
		struct wsm_container *container = node->container;
		struct wsm_view *view = container->view;
		if (!view->enabled) {
			view_minimize(view, false);
		}
		if (container_is_scratchpad_hidden_or_child(container)) {
			root_scratchpad_show(container);
		}
		seat_set_focus(session->seat, node);
		container_raise(container);
		transaction_commit_dirty();
	}
	end_session(session);
}

void wsm_task_session_cancel(struct wsm_task_session *session) {
	end_session(session);
}
