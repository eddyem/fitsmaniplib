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

typedef struct{
    char *fitsname;     // input file name
    char *outfile;      // output file name
    int list;           // list tables
} glob_pars;

/*
 * here are global parameters initialisation
 */
static int help;
static glob_pars G;

/*
 * Define command line options by filling structure:
 *  name        has_arg     flag    val     type        argptr              help
*/
static myoption cmdlnopts[] = {
// common options
    {"help",    NO_ARGS,    NULL,   'h',    arg_int,    APTR(&help),        _("show this help")},
    {"fitsname",NEED_ARG,   NULL,   'i',    arg_string, APTR(&G.fitsname),  _("name of input file")},
    {"list",    NO_ARGS,    NULL,   'l',    arg_none,   APTR(&G.list),      _("list all tables in file")},
    {"outfile", NEED_ARG,   NULL,   'o',    arg_none,   APTR(&G.outfile),   _("output file name")},
    end_option
};

/**
 * Parse command line options and return dynamically allocated structure
 *      to global parameters
 * @param argc - copy of argc from main
 * @param argv - copy of argv from main
 * @return allocated structure with global parameters
 */
static glob_pars *parse_args(int argc, char **argv){
    int i;
    char *helpstring = "Usage: %s [args]\n\n\tWhere args are:\n";
    change_helpstring(helpstring);
    // parse arguments
    parseargs(&argc, &argv, cmdlnopts);
    if(help) showhelp(-1, cmdlnopts);
    if(argc > 0){
        for (i = 0; i < argc; i++)
            printf("Ignore extra argument: %s\n", argv[i]);
    }
    return &G;
}


int main(int argc, char *argv[]){
    FITS *ofits = NULL;
    initial_setup();
    parse_args(argc, argv);
    if(!G.fitsname) ERRX(_("No input filename given!"));
    DBG("Open file %s", G.fitsname);
    if(G.outfile){
        ofits = MALLOC(FITS, 1);
        ofits->filename = G.outfile;
    }
    FITS *f = FITS_read(G.fitsname);
    if(G.list) table_print_all(f);
    if(ofits){
        green("\nWrite to output file %s\n", ofits->filename);
        FITS_write(ofits->filename, ofits);
    }
    return 0;
}

