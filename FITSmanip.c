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

/**
 * Functions to make intensity transforms of different kinds
 * Carefully! All they valid only @[0,1]!!!
 */
static double lintrans(double in){
    return in;
}
static double logtrans(double in){ // logaryphmic
    return log(1. + in);
}
double exptrans(double in){ // exponential
    return exp(in - 1.);
}
double powtrans(double in){ // x^2
    return in*in;
}
double sqrtrans(double in){ // square root
    return sqrt(in);
}

typedef double (*transfunct)(double in);
static transfunct tfunctions[TRANSF_COUNT] = {
    [TRANSF_EXP] = exptrans,
    [TRANSF_LOG] = logtrans,
    [TRANSF_LINEAR] = lintrans,
    [TRANSF_POW] = powtrans,
    [TRANSF_SQR] = sqrtrans
};

/**
 * @brief mktransform - make image intensity transformation
 * @param dimg (io) - double image
 * @param st   (i)  - image statistics
 * @param transf    - type of transformation
 * @return NULL if failed
 *      Be carefull: image should be equalized before some types of transform
 */
doubleimage *mktransform(doubleimage *im, imgstat *st, intens_transform transf){
    if(!im || !im->data || !st || transf <= TRANSF_WRONG || transf >= TRANSF_COUNT) return NULL;
    double max = st->max, min = st->min;
    if((max-min) < 2.*DBL_EPSILON){
        WARNX(_("Data range is too small"));
        return NULL;
    }
    double *dimg = im->data;
    if(transf == TRANSF_LINEAR) return im; // identity
    double (*transfn)(double in) = tfunctions[transf];
    if(!transfn) ERRX(_("Given transform type not supported yet"));
    size_t totpix = im->totpix;
    OMP_FOR()
    for(size_t i = 0; i < totpix; ++i){
        double d = dimg[i] - min;
        dimg[i] = transfn(d);
    }
    return im;
}

/**
 * @brief palette_gray - simplest gray conversion
 * @param gray  - nornmalized double value
 * @param rgb - red, green and blue components
 */
static void palette_gray(double gray, uint8_t *rgb){
    rgb[0] = rgb[1] = rgb[2] = (uint8_t)(255.*gray);
}

//  palette from black through red&yellow to white
static void palette_hot(double gray, uint8_t *rgb){
    int i = (int)(gray * 3.);
	double x = 3.*gray - (double)i;
	uint8_t r = 255, g = 255, b = 255;
	switch(i){
		case 0:
            r = (uint8_t)(255. * x);
			g = 0;
			b = 0;
		break;
		case 1:
			g = (uint8_t)(255. * x);
            b = 0;
		break;
		case 2:
			b = (uint8_t)(255. * x);
		break;
	}
	rgb[0] = r;
	rgb[1] = g;
	rgb[2] = b;
}
//  another palette from black through blue&cyan to white
static void palette_cold(double gray, uint8_t *rgb){
    int i = (int)(gray * 3.);
	double x = 3.*gray - (double)i;
	uint8_t r = 255, g = 255, b = 255;
	switch(i){
		case 0:
            r = 0;
			g = 0;
			b = (uint8_t)(255. * x);
		break;
		case 1:
			g = (uint8_t)(255. * x);
            r = 0;
		break;
		case 2:
			r = (uint8_t)(255. * x);
		break;
	}
	rgb[0] = r;
	rgb[1] = g;
	rgb[2] = b;
}
//  black->white->blue
static void palette_jet(double gray, uint8_t *rgb){
    int i = (int)(gray * 8.);
	double x = 8.*gray - (double)i;
	uint8_t r = 0, g = 0, b = 127;
	switch(i){
		case 0:
            r = 128 + (uint8_t)(127. * x);
			g = 0;
			b = 0;
		break;
		case 1:
            r = 255;
            g = (uint8_t)(128. * x);
            b = 0;
		break;
        case 2:
            r = 255;
            g = 128 + (uint8_t)(127. * x);
            b = 0;
		break;
        case 3:
            r = 255 - (uint8_t)(128. * x);
            g = 255;
            b = (uint8_t)(128. * x);
		break;
        case 4:
            r = 127 - (uint8_t)(127. * x);
            g = 255;
            b = 128 + (uint8_t)(127. * x);
		break;
        case 5:
            r = 0;
            g = 255 - (uint8_t)(128. * x);
            b = 255;
		break;
        case 6:
            r = 0;
            g = 127 - (uint8_t)(127. * x);
            b = 255;
		break;
        case 7:
            r = 0;
            g = 0;
            b = 255 - (uint8_t)(128. * x);
		break;
	}
	rgb[0] = r;
	rgb[1] = g;
	rgb[2] = b;
}
//  blue->red->yellow->white
static void palette_BR(double gray, uint8_t *rgb){
	int i = (int)(gray * 4.);
	double x = 4.*gray - (double)i;
	uint8_t r = 0, g = 0, b = 0;
	switch(i){
		case 0:
			g = (uint8_t)(255. * x);
			b = 255;
		break;
		case 1:
			g = 255;
			b = (uint8_t)(255. * (1. - x));
		break;
		case 2:
			r = (uint8_t)(255. * x);
			g = 255;
		break;
		case 3:
			r = 255;
			g = (uint8_t)(255. * (1. - x));
		break;
		default:
			r = 255;
	}
	rgb[0] = r;
	rgb[1] = g;
	rgb[2] = b;
}

typedef void (*palette)(double, uint8_t[3]); // pointer to palette function
static palette palette_F[PALETTE_COUNT] = {
    [PALETTE_GRAY] = palette_gray,
    [PALETTE_BR] = palette_BR,
    [PALETTE_HOT] = palette_hot,
    [PALETTE_COLD] = palette_cold,
    [PALETTE_JET] = palette_jet
};

/**
 * @brief convert2palette - convert normalized double image into colour using some palette
 * @param im (i) - image to convert
 * @param cmap   - palette (colormap) used
 * @return allocated here array with color image
 */
uint8_t *convert2palette(doubleimage *im, image_palette cmap){
    if(!im || !im->data || cmap <= PALETTE_WRONG || cmap >= PALETTE_COUNT) return NULL;
    palette impalette = palette_F[cmap];
    if(impalette == NULL) ERRX(_("Given colormap doesn't support yet"));
    size_t totpix = im->totpix;
    if(totpix == 0) return NULL;
    double *inarr = im->data;
    uint8_t *colored = MALLOC(uint8_t, totpix * 3);
    initomp();
    OMP_FOR()
    for(size_t i = 0; i < totpix; ++i){
        impalette(inarr[i], &colored[i*3]);
    }
    return colored;
}
