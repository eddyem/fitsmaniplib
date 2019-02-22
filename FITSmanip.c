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
#include "FITSmanip.h"
#include "local.h"
#include <omp.h>
#include <stdio.h>
#include <unistd.h>
#include <usefull_macros.h>

/*
 * Here are some common functions used in library
 */

/**
 * @brief initomp init optimal CPU number for OPEN MP
 */
void initomp(){
    static bool got = FALSE;
    if(got) return;
    int cpunumber = sysconf(_SC_NPROCESSORS_ONLN);
    if(omp_get_max_threads() != cpunumber)
        omp_set_num_threads(cpunumber);
    got = TRUE;
}

/**
 * @brief FITS_reporterr - show error in red
 * @param errcode
 */
void FITS_reporterr(int *errcode){
    if(isatty(STDERR_FILENO))
        fprintf(stderr, COLOR_RED);
    fits_report_error(stderr, *errcode);
    if(isatty(STDERR_FILENO))
        fprintf(stderr, COLOR_OLD);
    *errcode = 0;
}
