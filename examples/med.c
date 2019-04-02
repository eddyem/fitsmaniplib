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

/*
 * Median filtering of image
 */

typedef struct{
    char *fitsname;         // input file name
    char *outfile;          // output file name
    int   rewrite;          // rewrite existing file
    int   medr;             // radius of filter
} glob_pars;

/*
 * here are global parameters initialisation
 */
static int help;
static glob_pars G = {
    .medr = 1,
};

/*
 * Define command line options by filling structure:
 *  name        has_arg     flag    val     type        argptr              help
*/
static myoption cmdlnopts[] = {
// common options
    {"help",    NO_ARGS,    NULL,   'h',    arg_int,    APTR(&help),        _("show this help")},
    {"fitsname",NEED_ARG,   NULL,   'i',    arg_string, APTR(&G.fitsname),  _("name of input file")},
    {"outpname",NEED_ARG,   NULL,   'o',    arg_string, APTR(&G.outfile),   _("output file name (jpeg)")},
    {"rewrite", NO_ARGS,    NULL,   'r',    arg_none,   APTR(&G.rewrite),   _("rewrite output file")},
    {"radius",  NEED_ARG,   NULL,   'R',    arg_int,    APTR(&G.medr),      _("radius of median (0 for cross 3x3)")},
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
    char *helpstring = "Usage: %%s [args]\n\n\tWhere args are:\n";
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
    initial_setup();
    parse_args(argc, argv);
    if(!G.fitsname) ERRX(_("No input filename given!"));
    if(!G.outfile) ERRX(_("No output filename given!"));
    if(G.medr < 0) ERRX(_("Median radius should be >= 0"));
    if(!file_absent(G.outfile) && !G.rewrite) ERRX(_("File %s exists"), G.outfile);
    FITS *f = FITS_read(G.fitsname);
    if(!f) ERRX(_("Failed to open %s"), G.fitsname);
    f->curHDU = NULL;
    int i;
    for(i = 1; i <= f->NHDUs; ++i){
        if(f->HDUs[i].hdutype == IMAGE_HDU){f->curHDU = &f->HDUs[i]; break;}
    }
    if(!f->curHDU) ERRX(_("No image HDUs in %s"), G.fitsname);
    green("First HDU with image: #%d\n", i);
    FITSimage *img = f->curHDU->contents.image;
    if(img->naxis != 2) ERRX(_("Support only 2-dimensional images"));
    doubleimage *dblimg = image2double(img);
    if(!dblimg) ERRX(_("Can't convert image to double"));
    doubleimage *filtered = get_median(dblimg, G.medr);
    if(!filtered) ERRX(_("WTF?"));
    if(!image_rebuild(img, filtered->data)) ERRX(_("Can't rebuild image"));
    doubleimage_free(&dblimg);
    f->filename = G.outfile;
    bool w = FALSE;
    if(file_absent(G.outfile)) w = FITS_write(G.outfile, f);
    else w = FITS_rewrite(f);
    if(!w) ERRX(_("Can't write %s"), f->filename);
    FITS_free(&f);
    return 0;
}

