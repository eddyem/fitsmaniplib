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
 *   with given pallette. Also make simplest intensity (including histogram)
 *   transformations.
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
    int   nlvl;         // amount of histogram levels
    int   histeq;       // histogram equalisation
    double histcutlow;  // low limit of histogram cut-off
    double histcuthigh; // top limit -//-
} glob_pars;

/*
 * here are global parameters initialisation
 */
static int help;
static glob_pars G = {
    .nhdu = 1,
    .nlvl = 100
};

/*
 * Define command line options by filling structure:
 *  name        has_arg     flag    val     type        argptr              help
*/
static myoption cmdlnopts[] = {
// common options
    {"help",    NO_ARGS,    NULL,   'h',    arg_none,   APTR(&help),        _("show this help")},
    {"inname",  NEED_ARG,   NULL,   'i',    arg_string, APTR(&G.fitsname),  _("name of input file")},
    {"outpname",NEED_ARG,   NULL,   'o',    arg_string, APTR(&G.outfile),   _("output file name (jpeg)")},
    {"textline",NEED_ARG,   NULL,   't',    arg_string, APTR(&G.text),      _("add text line to output image (at bottom)")},
    {"palette", NEED_ARG,   NULL,   'p',    arg_string, APTR(&G.palette),   _("convert as given palette (br, cold, gray, hot, jet)")},
    {"hdunumber",NEED_ARG,  NULL,   'n',    arg_int,    APTR(&G.nhdu),      _("open image from given HDU number")},
    {"transform",NEED_ARG,  NULL,   'T',    arg_string, APTR(&G.transform), _("type of intensity transformation (exp, lin, log, pow, sqrt)")},
    {"rewrite", NO_ARGS,    NULL,   'r',    arg_none,   APTR(&G.rewrite),   _("rewrite output file")},
    {"histlvl", NEED_ARG,   NULL,   'l',    arg_int,    APTR(&G.nlvl),      _("amount of levels for histogram calculation")},
    {"hcutlow", NEED_ARG,   NULL,   'L',    arg_double, APTR(&G.histcutlow),_("histogram cut-off low limit")},
    {"hcuthigh",NEED_ARG,   NULL,   'H',    arg_double, APTR(&G.histcuthigh),_("histogram cut-off high limit")},
    {"histeq",  NO_ARGS,    NULL,   'E',    arg_none,   APTR(&G.histeq),    _("histogram equalisation")},
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
    }
    fprintf(stderr, "Possible arguments of " COLOR_RED "\"Transformation\"" COLOR_OLD ":\n");
    fprintf(stderr, "exp - exponential transform\n");
    fprintf(stderr, "linear (default) - linear transform (do nothing)\n");
    fprintf(stderr, "log - logariphmic transform\n");
    fprintf(stderr, "pow - x^2\n");
    fprintf(stderr, "sqrt - sqrt(x)\n");
    return TRANSF_WRONG;
}

/**
 * @brief palette_transform - transform string with colormap name into its number
 * @param p - colormap name
 * @return PALETTE_WRONG if wrong or appropriate palette
 */
static image_palette palette_transform(char *p){
    if(!p) return PALETTE_WRONG;
    switch(p[0]){
        case 'B':
        case 'b':
            return PALETTE_BR;
        break;
        case 'c':
        case 'C':
            return PALETTE_COLD;
        break;
        case 'g':
        case 'G':
            return PALETTE_GRAY;
        break;
        case 'h': // hot, help
        case 'H':
            switch(p[1]){
                case 'o':
                case 'O':
                    return PALETTE_HOT;
                break;
            }
        break;
        case 'j':
        case 'J':
            return PALETTE_JET;
        break;
    }
    fprintf(stderr, "Possible arguments of " COLOR_RED "\"palette\"" COLOR_OLD ":\n");
    fprintf(stderr, "br - blue->red->yellow->white\n");
    fprintf(stderr, "cold - black->blue->cyan->white\n");
    fprintf(stderr, "gray (default) - simple gray\n");
    fprintf(stderr, "hot -  black->red->yellow->white\n");
    fprintf(stderr, "jet - black->white->blue\n");
    return PALETTE_WRONG;
}

void print_histo(histogram *H){
    if(!H) return;
    size_t *histo = H->data;
    double *lvls = H->levels;
    green("Histogram:\n");
    for(size_t i = 0; i < H->size; ++i){
        if(histo[i] == 0) continue;
        printf("%5zd [%3zd%%]: %zd (%g..%g)\n", i, (100*histo[i])/H->totpix, histo[i], lvls[i], lvls[i+1]);
    }
    printf("\n");
}

int main(int argc, char *argv[]){
    initial_setup();
    parse_args(argc, argv);
    image_palette colormap = PALETTE_GRAY;
    if(G.palette){ // convert normalized image due to choosen palette
        colormap = palette_transform(G.palette);
        if(colormap == PALETTE_WRONG) ERRX(_("Wrong colormap: %s"), G.palette);
    }
    intens_transform tr = TRANSF_LINEAR;
    if(G.transform){
        tr = gettransf(G.transform);
        if(tr == TRANSF_WRONG) ERRX(_("Wrong transform: %s"), G.transform);
    }
    if(!G.fitsname) ERRX(_("No input filename given!"));
    if(!G.outfile)  ERRX(_("Point the name of output file!"));
    if(!file_absent(G.outfile) && !G.rewrite) ERRX(_("File %s exists"), G.outfile);
    DBG("Open file %s", G.fitsname);
    FITS *f = FITS_read(G.fitsname);
    if(!f) ERRX(_("Failed to open"));
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
    green("Histogram before transformations:\n");
    histogram *h = dbl2histogram(dblimg, G.nlvl);
    print_histo(h);
    histogram_free(&h);
    if(G.histeq){ // equalize histogram
        if(!dbl_histeq(dblimg, G.nlvl))
            ERRX(_("Can't do histogram equalization"));
    }
    if(G.histcutlow > DBL_EPSILON || G.histcuthigh > DBL_EPSILON){
        if(!dbl_histcutoff(dblimg, G.nlvl, G.histcutlow, G.histcuthigh))
            ERRX(_("Can't make histogram cut-off"));
    }
    if(!mktransform(dblimg, st, tr)) ERRX(_("Can't do given transform"));
#ifdef EBUG
    st = get_imgstat(dblimg, NULL);
#endif
    DBG("After transformation: MIN=%g, MAX=%g, AVR=%g, STD=%g", st->min, st->max, st->mean, st->std);
    green("Histogram after transformations:\n");
    h = dbl2histogram(dblimg, G.nlvl);
    print_histo(h);
    histogram_free(&h);
    uint8_t *colored = convert2palette(dblimg, colormap);
    DBG("Save jpeg to %s", G.outfile);
    if(!write_jpeg(G.outfile, colored, G.text, img)) ERRX(_("Can't save modified file %s"), G.outfile);
    green("File %s saved\n", G.outfile);
    return 0;
}
