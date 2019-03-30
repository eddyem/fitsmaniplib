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
#include <gd.h>

/*
 * Read FITS image, convert it to double and save as JPEG
 * with given pallette.
 * WARNING! Supports only 2-dimensional images
 */

typedef struct{
    char *fitsname;     // input file name
    char *outfile;      // output file name
    char *text;         // add this text to image
    char *transform;    // type of intensity transform
    char *palette;      // palette to convert FITS image
    int   nhdu;         // HDU number to read image from
    int   rewrite;      // rewrite output file
} glob_pars;

/*
 * here are global parameters initialisation
 */
static int help;
static glob_pars G = {
    .nhdu = 1,
};

/*
 * Define command line options by filling structure:
 *  name        has_arg     flag    val     type        argptr              help
*/
static myoption cmdlnopts[] = {
// common options
    {"help",    NO_ARGS,    NULL,   'h',    arg_int,    APTR(&help),        _("show this help")},
    {"inname",  NEED_ARG,   NULL,   'i',    arg_string, APTR(&G.fitsname),  _("name of input file")},
    {"outpname",NEED_ARG,   NULL,   'o',    arg_string, APTR(&G.outfile),   _("output file name (jpeg)")},
    {"textline",NEED_ARG,   NULL,   't',    arg_string, APTR(&G.text),      _("add text line to output image (at bottom)")},
    {"palette", NEED_ARG,   NULL,   'p',    arg_string, APTR(&G.palette),   _("convert as given palette")},
    {"hdunumber",NEED_ARG,  NULL,   'n',    arg_int,    APTR(&G.nhdu),      _("open image from given HDU number")},
    {"transform",NEED_ARG,  NULL,   'T',    arg_string, APTR(&G.transform), _("type of intensity transformation (log, sqr, exp, pow)")},
    {"rewrite", NO_ARGS,    NULL,   'r',    arg_none,   APTR(&G.rewrite),   _("rewrite output file")},
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

/**
 * @brief write_jpeg - save JPEG file
 * @param fname - file name
 * @param data  - RGB array with data (order: R,G,B)
 * @param str   - string to add @ left top corner of image
 * @param img   - image structure (for size parameters)
 * @return true if all OK
 */
static bool write_jpeg(const char *fname, const uint8_t *data, const char *str, FITSimage *img){
    if(!img) return FALSE;
    int W = img->naxes[0], H = img->naxes[1];
    gdImagePtr im = gdImageCreateTrueColor(W, H);
    if(!im) return FALSE;
    DBG("Create image %dx%d", W,H);
    //OMP_FOR()
    for(int y = H - 1; y >= 0; --y){ // flip up-down
        for(int x = 0; x < (int)W; ++x){
            im->tpixels[y][x] = (data[0] << 16) | (data[1] << 8) | data[2];
            data += 3;
        }
    }
    DBG("Converted");
    FILE *fp = fopen(fname, "w");
    if(!fp){
        WARN(_("Can't save jpg image %s\n"), fname);
        gdImageDestroy(im);
        return FALSE;
    }
    if(str){
        gdFTUseFontConfig(1);
        char *font = (char*)"monotype";
        char *ret = gdImageStringFT(im, NULL, 0xffffff, font, 10, 0., 2, 12, (char*)str);
        if(ret) WARNX(_("Error: %s"), ret);
    }
    gdImageJpeg(im, fp, 90);
    fclose(fp);
    gdImageDestroy(im);
    return TRUE;
}

/**
 * @brief conv2rgb - convert grayscale normalized image to grayscale RGB
 * @param inarr (i) - input array
 * @param totpix    - its size
 * @return allocated here array with data
 *
static uint8_t *conv2rgb(doubleimage *in){
    if(!in) return NULL;
    size_t totpix = in->totpix;
    if(totpix == 0) return NULL;
    double *inarr = in->data;
    uint8_t *colored = MALLOC(uint8_t, totpix * 3);
    OMP_FOR()
    for(size_t i = 0; i < totpix; ++i){
        uint8_t *pcl = &colored[i*3];
        pcl[0] = pcl[1] = pcl[2] = (uint8_t)(inarr[i] * 255.);
    }
    return colored;
}*/

/**
 * @brief gettransf - convert string with transformation type into intens_transform
 * @param transf - type of transformation
 * @return TRANSF_WRONG if arg is wrong or appropriate transformation type
 * this function tests only first char[s] of type, so instead of, e.g. "log", you can
 * write "lo", "looo", "Logogo", etc.
 */
static intens_transform gettransf(const char *transf){
    if(!transf) return TRANSF_WRONG;
    switch(transf[0]){
        case 'e': // exp
            return TRANSF_EXP;
        break;
        case 'l': // linear, log
        case 'L':
            switch(transf[1]){
                case 'i':
                case 'I':
                    return TRANSF_LINEAR;
                break;
                case 'o':
                case 'O':
                    return TRANSF_LOG;
                break;
                default:
                    return TRANSF_WRONG;
            }
        break;
        case 'p':
            return TRANSF_POW;
        break;
        case 's':
            return TRANSF_SQR;
        break;
        default:
            return TRANSF_WRONG;
    }
}

/**
 * @brief palette_transform - transform string with colormap name into its number
 * @param p - colormap name
 * @return PALETTE_COUNT if wrong or appropriate palette
 */
static image_palette palette_transform(char *p){
    if(!p) return PALETTE_COUNT;
    switch(p[0]){
        case 'B':
        case 'b':
            return PALETTE_BR;
        break;
        default:
            return PALETTE_COUNT;
    }
}

int main(int argc, char *argv[]){
    intens_transform tr = TRANSF_LINEAR;
    initial_setup();
    parse_args(argc, argv);
    if(!G.fitsname) ERRX(_("No input filename given!"));
    if(!G.outfile)  ERRX(_("Point the name of output file!"));
    if(G.transform) tr = gettransf(G.transform);
    if(tr == TRANSF_WRONG) ERRX(_("Wrong transform: %s"), G.transform);
    if(!file_absent(G.outfile) && !G.rewrite) ERRX(_("File %s exists"), G.outfile);
    DBG("Open file %s", G.fitsname);
    FITS *f = FITS_read(G.fitsname);
    DBG("HERE");
    green("got file %s, HDUs: %d, working HDU #%d\n", G.fitsname, f->NHDUs, G.nhdu);
    if(f->NHDUs < G.nhdu) ERRX(_("File %s consists %d HDUs!"), G.fitsname, f->NHDUs);
    f->curHDU = &f->HDUs[G.nhdu];
    if(f->curHDU->hdutype != IMAGE_HDU) ERRX(_("HDU %d is not image!"), G.nhdu);
    FITSimage *img = f->curHDU->contents.image;
    if(img->naxis != 2) ERRX(_("Support only 2-dimensional images"));
    DBG("convert image from HDU #%d into double", G.nhdu);
    doubleimage *dblimg = image2double(img);
    if(!dblimg) ERRX(_("Can't convert image from HDU %s"), G.nhdu);
    DBG("Done");
    imgstat *st = get_imgstat(dblimg, NULL);
    DBG("Image statistics: MIN=%g, MAX=%g, AVR=%g, STD=%g", st->min, st->max, st->mean, st->std);
    if(!normalize_dbl(dblimg, st)) ERRX(_("Can't normalize image!"));
#ifdef EBUG
    st = get_imgstat(dblimg, NULL);
#endif
    DBG("NOW: MIN=%g, MAX=%g, AVR=%g, STD=%g", st->min, st->max, st->mean, st->std);
    if(!mktransform(dblimg, st, tr)) ERRX(_("Can't do given transform"));
#ifdef EBUG
    st = get_imgstat(dblimg, NULL);
#endif
    DBG("After transformation: MIN=%g, MAX=%g, AVR=%g, STD=%g", st->min, st->max, st->mean, st->std);
    image_palette colormap = PALETTE_GRAY;
    if(G.palette){ // convert normalized image due to choosen palette
        colormap = palette_transform(G.palette);
        if(colormap == PALETTE_COUNT) ERRX(_("Wrong colormap name"));
    }
    //uint8_t *colored = conv2rgb(dblimg);
    uint8_t *colored = convert2palette(dblimg, colormap);
    DBG("Save jpeg to %s", G.outfile);
    if(!write_jpeg(G.outfile, colored, G.text, img)) ERRX(_("Can't save modified file %s"), G.outfile);
    green("File %s saved\n", G.outfile);
    return 0;
}
