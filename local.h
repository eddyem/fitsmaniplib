/*
 * This file is part of the FITSmaniplib project.
 * Copyright 2019  Edward V. Emelianov <edward.emelianoff@gmail.com>, <eddy@sao.ru>.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once

#include <float.h> // xx_EPSILON etc.
#include <errno.h>
#include <libgen.h> // dirname, basename
#include <limits.h>
#include <linux/limits.h> // PATH_MAX
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <usefull_macros.h>

#if defined GETTEXT
#include <libintl.h>
#define _(String)               gettext(String)
#define gettext_noop(String)    String
#define N_(String)              gettext_noop(String)
#else
#define _(String)               (String)
#define N_(String)              (String)
#endif

#ifndef DBL_EPSILON
#define DBL_EPSILON    (2.2204460492503131e-16)
#endif
#ifndef DBL_MAX
#define DBL_MAX        (1.7976931348623157e+308)
#endif


