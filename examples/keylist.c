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

#include "common.h"
#include <string.h>

typedef struct{
    char *fitsname;     // input file name
    int list;           // print keyword list
    int contents;       // print short description of file contents
    char **addrec;      // add some records to keywords in first HDU
} glob_pars;

/*
 * here are global parameters initialisation
 */
int help;
glob_pars G; /* = {
    ;
};*/

/*
 * Define command line options by filling structure:
 *  name        has_arg     flag    val     type        argptr              help
*/
myoption cmdlnopts[] = {
// common options
    {"help",    NO_ARGS,    NULL,   'h',    arg_int,    APTR(&help),        _("show this help")},
    {"contents",NO_ARGS,    NULL,   'c',    arg_none,   APTR(&G.contents), _("show short file contents")},
    {"list",    NO_ARGS,    NULL,   'l',    arg_none,   APTR(&G.list),      _("list all keywords")},
    {"addrec",  MULT_PAR,   NULL,   'a',    arg_string, APTR(&G.addrec),    _("add record to file (you can add more than one record in once, point more -a)")},
    end_option
};

/**
 * Parse command line options and return dynamically allocated structure
 *      to global parameters
 * @param argc - copy of argc from main
 * @param argv - copy of argv from main
 * @return allocated structure with global parameters
 */
glob_pars *parse_args(int argc, char **argv){
    int i;
    char *helpstring = "Usage: %%s [args] infile.fits\n\n\tWhere args are:\n";
    change_helpstring(helpstring);
    // parse arguments
    parseargs(&argc, &argv, cmdlnopts);
    if(help) showhelp(-1, cmdlnopts);
    if(argc)(G.fitsname = strdup(argv[0]));
    if(argc > 1){
        for (i = 1; i < argc; i++)
            printf("Ignore extra argument: %s\n", argv[i]);
    }
    return &G;
}

int main(int argc, char *argv[]){
    initial_setup();
    parse_args(argc, argv);
    if(!G.fitsname) ERRX(_("No input filename given!"));
    green("Open file %s\n", G.fitsname);
    FITS *f = FITS_read(G.fitsname);
    if(!f) ERRX(_("Can't open file %s"), G.fitsname);
    int N = f->NHDUs;
    if(G.list){
        green("\n\nList of keywords:\n");
        for(int i = 1; i <= N; ++i){
            green("\nHDU #%d\n", i);
            keylist_print(f->HDUs[i].keylist);
        }
    }
    if(G.contents){
        green("\n\nFile consists of %d HDUs:\n", N);
        for(int i = 0; i <= N; ++i){
            printf("\tHDU #%d - ", i);
            switch(f->HDUs[i].hdutype){
                case IMAGE_HDU:
                    printf("Image");
                break;
                case ASCII_TBL:
                    printf("ASCII table");
                break;
                case BINARY_TBL:
                    printf("Binary table");
                break;
                default:
                    printf("Some shit");
            }
            printf("\n");
        }
    }
    if(G.addrec){
        char **ptr = G.addrec;
        while(*ptr){
            printf("record: %s\n", *ptr);
            ++ptr;
        }
    }
    return 0;
}

