/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __BL_TIME_H__
#define __BL_TIME_H__

#include <time.h>
#include "bl_def.h" /* REMOVE_FUNCS_MLTERM_UNUSE */

#ifndef REMOVE_FUNCS_MLTERM_UNUSE

time_t bl_time_string_date_to_time_t(const char *format, const char *date);

struct tm *bl_time_string_date_to_tm(struct tm *tm_info, const char *format, const char *date);

int bl_time_string_wday_to_int(const char *wday);

char *bl_time_int_wday_to_string(int wday);

char *bl_time_int_wday_to_abbrev_string(int wday);

int bl_time_string_month_to_int(const char *month);

char *bl_time_int_month_to_string(int month);

char *bl_time_int_month_to_abbrev_string(int month);

#endif /* REMOVE_FUNCS_MLTERM_UNUSE */

#endif /* __BL_TIME_H__ */
