/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __UI_EVENT_SOURCE_H__
#define __UI_EVENT_SOURCE_H__

int ui_event_source_init(void);

int ui_event_source_final(void);

int ui_event_source_process(void);

int ui_event_source_add_fd(int fd, void (*handler)(void));

int ui_event_source_remove_fd(int fd);

#endif
