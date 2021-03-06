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

/*
  This example allows to list all keywords in given FITS-file, list amount and types of HDUs in file,
    add new keywords, modify old keywords.
  New data could be saved to new file or in place into opened file.
 */

#include "common.h"
#include <signal.h>
#include <string.h>

typedef struct{
    char *fitsname;     // input file name
    int list;           // print keyword list
    int contents;       // print short description of file contents
    char **addrec;      // add some records to keywords in first HDU
    char *outfile;      // output file name
    char **modify;      // keys which values should be modified
} glob_pars;

/*
 * here are global parameters initialisation
 */
static int help;
static glob_pars G; /* = {
    ;
};*/

/*
 * Define command line options by filling structure:
 *  name        has_arg     flag    val     type        argptr              help
*/
static myoption cmdlnopts[] = {
// common options
    {"help",    NO_ARGS,    NULL,   'h',    arg_int,    APTR(&help),        _("show this help")},
    {"contents",NO_ARGS,    NULL,   'c',    arg_none,   APTR(&G.contents), _("show short file contents")},
    {"list",    NO_ARGS,    NULL,   'l',    arg_none,   APTR(&G.list),      _("list all keywords")},
    {"addrec",  MULT_PAR,   NULL,   'a',    arg_string, APTR(&G.addrec),    _("add record to first HDU (you can add more than one record in once, point more -a)")},
    {"output",  NEED_ARG,   NULL,   'o',    arg_string, APTR(&G.outfile),   _("save result to file (else save to same file)")},
    {"modify",  MULT_PAR,   NULL,   'm',    arg_string, APTR(&G.modify),    _("modify values of given keys (each param should be \"key = new_value\")")},
    {"infile",  NEED_ARG,   NULL,   'i',    arg_string, APTR(&G.fitsname),  _("input file name (you can also point it without any keys)")},
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
    char *helpstring = "Usage: %s [args] infile.fits\n\n\tWhere args are:\n";
    change_helpstring(helpstring);
    // parse arguments
    parseargs(&argc, &argv, cmdlnopts);
    if(help) showhelp(-1, cmdlnopts);
    if(argc && !G.fitsname)(G.fitsname = strdup(argv[0]));
    if(argc > 1){
        for (i = 1; i < argc; i++)
            printf("Ignore extra argument: %s\n", argv[i]);
    }
    return &G;
}

static void ch(int s){
    signal(s, SIG_IGN);
    printf("signal: %d\n", s);
    signal(s, ch);
}

static void print_imgHDU(FITSimage *image){
    printf("Image: naxis=%d, totpix=%ld, ", image->naxis, image->totpix);
    printf("naxes=(");
    for(int i = 0; i < image->naxis; ++i)
        printf("%s%ld", i?", ":"", image->naxes[i]);
    if(image->naxis) printf("), ");
    else printf("none), ");
    printf("bitpix=%d, dtype=%d", image->bitpix, image->dtype);
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
        for(int i = 1; i <= N; ++i){
            printf("\tHDU #%d - ", i);
            switch(f->HDUs[i].hdutype){
                case IMAGE_HDU:
                    print_imgHDU(f->HDUs[i].contents.image);
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
    int differs = 0;
    if(G.addrec){
        char **ptr = G.addrec;
        while(*ptr){
            printf("record: %s\n", *ptr);
            if(keylist_add_record(&(f->HDUs[1].keylist), *ptr++, 1)) differs = 1;
        }
    }
    if(G.modify){
        char **ptr = G.modify;
        while(*ptr){
            printf("modify: %s\n", *ptr);
            char *val = strchr(*ptr, '=');
            if(!val){
                WARNX("should be: 'parameter = value / comment'");
                continue;
            }
            *val++ = 0; // now `val` is value + comment; ptr is key
            if(!keylist_modify_key(f->HDUs[1].keylist, *ptr, val)){
                WARNX("key %s not found", *ptr);
            }else differs = 1;
            ++ptr;
        }
    }
    if(differs){ // file differs, need to save new file
        // test for signals handler: ctrl+c, ctrl+z
        signal(SIGINT, ch);
        signal(SIGTSTP, ch);
        // protect critical zone blocking all possible signals:
        sigset_t mask, oldmask;
        sigfillset(&mask);
        sigprocmask(SIG_SETMASK, &mask, &oldmask);
        if(G.outfile){ // save result to new file
            FITS_write(G.outfile, f);
        }else{
            FITS_rewrite(f);
        }
        DBG("Written! Sleep for 2 seconds in ctitical section");
        sleep(2);
        // return ignoring
        DBG("Unblock signals, sleep for 2 seconds");
        sigprocmask(SIG_SETMASK, &oldmask, NULL);
        sleep(2);
    }
    /*
     * Do something here
     */
    return 0;
}

