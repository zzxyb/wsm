#ifndef WSM_TASK_SESSION_H
#define WSM_TASK_SESSION_H

#include "wsm_task_model.h"
#include "wsm_task_view.h"

struct wsm_seat;

struct wsm_task_session {
	struct wsm_seat *seat;
	struct wsm_task_model model;
	struct wsm_task_view view;
	size_t selected;
	bool active;
};

void wsm_task_session_init(struct wsm_task_session *session,
	struct wsm_seat *seat, const struct wsm_task_view *view);
void wsm_task_session_finish(struct wsm_task_session *session);
bool wsm_task_session_begin(struct wsm_task_session *session, bool reverse);
void wsm_task_session_step(struct wsm_task_session *session, bool reverse);
void wsm_task_session_commit(struct wsm_task_session *session);
void wsm_task_session_cancel(struct wsm_task_session *session);

#endif
