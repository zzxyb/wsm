# Task presentation framework

The switcher is split into reusable model, view, and session layers:

- `wsm_task_model` snapshots the seat's MRU task order and tracks task
  destruction while a presentation is open.
- `wsm_task_view` is the rendering interface. It deliberately has no switcher
  policy, so a grid-based multi-task view can provide another implementation.
- `wsm_task_session` owns selection, forward/reverse traversal, commit, and
  cancellation.
- `wsm_switcher` supplies the Alt-Tab keyboard policy and the compact
  `wlr_scene_rect` view.

A future multi-task view should reuse `wsm_task_model`, implement its own
`wsm_task_view_impl`, and either reuse `wsm_task_session` or place a pointer and
gesture controller in front of it. Window enumeration and focus commit should
remain outside the concrete views.
