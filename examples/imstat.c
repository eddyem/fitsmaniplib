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
 * This example allows to calculate simple statistics over fits file,
 * add constant number or multiply by constant all images in file
 */
#include "common.h"
#include <float.h>
#include <math.h>
#include <string.h>

typedef struct{
    char *outfile;      // output file name (for single operation)
    char **infiles;     // input files list
    int Ninfiles;       // input files number
} glob_pars;

/*
 * here are global parameters initialisation
 */
int help;
glob_pars G = {
};

/*
 * Define command line options by filling structure:
 *  name        has_arg     flag    val     type        argptr              help
*/
myoption cmdlnopts[] = {
// common options
    {"help",    NO_ARGS,    NULL,   'h',    arg_int,    APTR(&help),        _("show this help")},
    {"outfile", NEED_ARG,   NULL,   'o',    arg_string, APTR(&G.outfile),   _("output file name (collect all input files)")},
    end_option
};

typedef struct{
    double mean;
    double std;
    double min;
    double max;
} imgstat;

/**
 * Parse command line options and return dynamically allocated structure
 *      to global parameters
 * @param argc - copy of argc from main
 * @param argv - copy of argv from main
 * @return allocated structure with global parameters
 */
glob_pars *parse_args(int argc, char **argv){
    int i;
    char *helpstring = "Usage: %%s [args] input files\n"
            "Get statistics and modify images from first image HDU of each input file\n"
            "\tWhere args are:\n";
    change_helpstring(helpstring);
    // parse arguments
    parseargs(&argc, &argv, cmdlnopts);
    if(help) showhelp(-1, cmdlnopts);
    if(argc > 0){
        G.Ninfiles = argc;
        G.infiles = MALLOC(char*, argc);
        for (i = 0; i < argc; i++)
            G.infiles[i] = strdup(argv[i]);
    }
    return &G;
}

imgstat *get_imgstat(double *dimg, long totpix){
    static imgstat st;
    if(!dimg || !totpix) return &st; // return some trash if wrong data
    st.min = dimg[0];
    st.max = dimg[0];
    double sum = dimg[0], sum2 = dimg[0];
    for(long i = 1; i < totpix; ++i){
        double val = dimg[i];
        if(st.min > val) st.min = val;
        if(st.max < val) st.max = val;
        sum += val;
        sum2 += val*val;
    }
    DBG("tot:%ld, sum=%g, sum2=%g, min=%g, max=%g", totpix, sum, sum2, st.min, st.max);
    st.mean = sum / totpix;
    st.std = sqrt(sum2/totpix - st.mean*st.mean);
    return &st;
}

void printstat(imgstat *stat){
    green("Statistics:\n");
    printf("MEAN=%g\nSTD=%g\nMIN=%g\nMAX=%g\n", stat->mean, stat->std, stat->min, stat->max);
}

bool process_fitsfile(char *inname, FITS *output){
    DBG("File %s", inname);
    bool mod = FALSE;
    FITS *f = FITS_read(inname);
    if(!f){WARNX("Can't read %s", inname); return FALSE;}
    // search first image HDU
    f->curHDU = NULL;
    for(int i = 1; i <= f->NHDUs; ++i){
        green("File %s, %dth HDU, type: %d\n", inname, i, f->HDUs[i].hdutype);
        if(f->HDUs[i].hdutype != IMAGE_HDU) continue;
        FITSimage *img = f->HDUs[i].contents.image;
        if(!img){
            WARNX("empty image");
            continue;
        }
        if(img->totpix == 0){
            WARNX("totpix=0");
            continue; // no data - only header
        }
        f->curHDU = &f->HDUs[i];
        break;
    }
    if(!f->curHDU){
        WARNX("Didn't find image HDU in %s", inname);
    }else{ // OK, we have an image and can do something with it
        green("\tGet image from this HDU.\n");
        FITSimage *img = f->curHDU->contents.image;
        double *dImg = image2double(img);
        // calculate image statistics
        imgstat *stat = get_imgstat(dImg, img->totpix);
        printstat(stat);
        //mod = TRUE;
        FREE(dImg);
    }
    if(mod || output){ // file modified (or output file pointed), write differences
        if(!output){
            green("Rewrite file %s.\n", f->filename);
            if(!FITS_rewrite(f)) WARNX("Can't rewrite %s", inname);
        }else{ // copy first image HDU and its header to output file
            green("Add image to %s.\n", output->filename);
            FITSHDU *hdu = FITS_addHDU(output);
            if(!hdu) return NULL;
            hdu->hdutype = IMAGE_HDU;
            hdu->keylist = keylist_copy(f->curHDU->keylist);
            hdu->contents.image = image_copy(f->curHDU->contents.image);
        }
    }
    FITS_free(&f);
    return mod;
}

int main(int argc, char *argv[]){
    FITS *ofits = NULL;
    bool mod = FALSE;
    initial_setup();
    parse_args(argc, argv);
    if(!G.Ninfiles) ERRX(_("No input filename[s] given!"));
    if(G.outfile){
        ofits = MALLOC(FITS, 1);
        ofits->filename = G.outfile;
    }
    for(int i = 0; i < G.Ninfiles; ++i){
        if(process_fitsfile(G.infiles[i], ofits)) mod = 1;
    }
    if(ofits && mod){
        green("\nWrite all modified images to output file %s\n", ofits->filename);
        FITS_write(ofits->filename, ofits);
    }
    return 0;
}
