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

/**
 * Return TRUE if file _name_ not exists
 */
bool file_is_absent(char *name){
    struct stat filestat;
    if(!stat(name, &filestat)) return FALSE;
    if(errno == ENOENT) return TRUE;
    return FALSE;
}
/**
 * find the first non-existing filename like prefixXXXX.suffix & put it into buff
 */
char* make_filename(char *buff, size_t buflen, char *prefix, char *suffix){
    int num;
    for(num = 1; num < 10000; ++num){
        if(snprintf(buff, buflen, "%s_%04d.%s", prefix, num, suffix) < 1)
            return NULL;
        if(file_is_absent(buff)) // OK, file not exists
            return buff;
    }
    return NULL;
}
