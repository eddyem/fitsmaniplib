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

/**************************************************************************************
 *                              Histogram routines                                    *
 **************************************************************************************/

void histogram_free(histogram **H){
    if(!H || !*H) return;
    FREE((*H)->levels);
    FREE((*H)->data);
    FREE(*H);
}

/**
 * @brief dbl2histogram - calculate histogram of normalized image `im`
 * @param im (i)  - input image
 * @param nvalues - amount of levels (more than 2, less than 65536)
 * @return array with image histogram (allocated here)
 */
histogram *dbl2histogram(doubleimage *im, size_t nvalues){
    if(!im || !im->data || nvalues < 2 || im->totpix < 1) return NULL;
    if(nvalues > 65535){
        WARNX(_("Amount of histogram levels should be less than 65536!"));
        return NULL;
    }
    histogram *H = MALLOC(histogram, 1);
    size_t *histo = MALLOC(size_t, nvalues);
    double *lvls = MALLOC(double, nvalues+1); // have greatest value -> its size larger
    H->data = histo;
    H->levels = lvls;
    H->size = nvalues;
    H->totpix = im->totpix;
    for(size_t i = 0; i < im->totpix; ++i){
        size_t v = im->data[i] * nvalues;
        if(v >= nvalues) v = nvalues-1;
        ++histo[v];
    }
    for(size_t i = 0; i <= nvalues; ++i)
        lvls[i] = ((double)i) / ((double)nvalues);
    return H;
}

/**
 * @brief lininterp - linear interpolation
 * @param bordx - borders by X-axis (xlow, xhigh)
 * @param bordy - borders by Y-axis (ylow, yhigh)
 * @param x     - coordinate of x value
 * @return
 *
static double lininterp(const double bordx[2], const double bordy[2], double x){
    return (bordx[0] + (x-bordx[0])*(bordy[1]-bordy[1])/(bordx[1]-bordx[0]));
}*/

/**
 * @brief dbl_histcutoff - cutoff histogram of double image with uniform intensity recalculation
 * @param im (io) - image
 * @param nlevls  - amount of levels (2..65535) of histogram
 * @param fracbtm - fraction of deleted pixels from zero
 * @param fractop - fraction of deleted pixels from top
 * @return pointer to im (equalized) or NULL
 *      WARNING! Works only for normalized image!
 */
doubleimage *dbl_histcutoff(doubleimage *im, size_t nlevls, double fracbtm, double fractop){
    if(!im || !im->data) return NULL;
    double success = TRUE; // return OK, if true; NULL if false
    if(fracbtm > 1. || fracbtm < 0.){
        WARNX(_("Bottom fraction should be in [0, 1)"));
        return NULL;
    }
    if(fractop > 1. || fractop < 0.){
        WARNX(_("Top fraction should be in (0, 1]"));
        return NULL;
    }
    histogram *hist = dbl2histogram(im, nlevls);
    if(!hist || !hist->data || !hist->levels){
        success = FALSE;
        goto theret;
    }
    // prepare data for top & bottom throwing out
    size_t Nbot = fracbtm * hist->totpix, Ntop = fractop * hist->totpix;
    size_t Ncur = 0; // pixel counter
    ssize_t botidx = -1, topidx = (ssize_t)nlevls; // botidx->0, topidx -> 1.
    DBG("Nbot: %zd, Ntop: %zd, total: %zd", Nbot, Ntop, hist->totpix);
    if(Nbot + Ntop >= hist->totpix){
        WARNX(_("No pixels leave to process, have: %zd, need: %zd"), hist->totpix, Nbot + Ntop);
        success = FALSE;
        goto theret;
    }
    Ntop = hist->totpix - Ntop;
    // search lower and upper limits
    for(size_t i = 0; i < nlevls; ++i){
        Ncur += hist->data[i];
        DBG("i=%zd, Ncur=%zd", i, Ncur);
        if(Ncur > Nbot &&  botidx == -1){
            botidx = i; // found bottom index
            if(Ntop == hist->totpix) break;
        }else if(Ncur > Ntop){
            topidx = i;
            break;
        }
    }
    if(botidx < 0){
        WARNX(_("Can't find bottom index"));
        success = FALSE;
        goto theret;
    }
    // top and bottom values which will be new 0 & 1
    double botval = hist->levels[botidx]; // lowest value -> 0.
    double topval = hist->levels[topidx]; // highest value -> 1.
    DBG("Bot: %zd, Top: %zd, botval: %g, topval: %g", botidx, topidx, botval, topval);
    double range = topval - botval; // range -> 1.
    DBG("botval=%g, topval=%g, range=%g", botval, topval, range);
    // Now we should convert intensities according to new limits
    // botidx -> 0., topidx -> 1.
    OMP_FOR()
    for(size_t i = 0; i < im->totpix; ++i){// index in old histogram
        double xx = im->data[i];
        if(xx < botval) xx = 0.;
        else xx = (xx - botval) / range;
        if(xx > 1.) xx = 1.;
        im->data[i] = xx;
    }
    theret:
    histogram_free(&hist);
    if(success) return im;
    else return NULL;
}

/**
 * @brief dbl_histcutoff - modify image by histogram equalisation
 * @param im     - image to transform
 * @param nlevls - levels amount (2..65535)
 * @return
 */
doubleimage *dbl_histeq(doubleimage *im, size_t nlevls){
    if(!im || !im->data) return NULL;
    double success = TRUE;
    histogram *hist = dbl2histogram(im, nlevls);
    if(!hist || !hist->data || !hist->levels){
        success = FALSE;
        goto theret;
    }
    double *newlevels = MALLOC(double, nlevls+1);
    size_t cumul = 0;
    for(size_t i = 0; i < nlevls; ++i){
        cumul += hist->data[i];
        // calculate new gray level
        newlevels[i+1] = ((double)cumul) / hist->totpix;
        DBG("newlevels[%zd]=%g", i+1, newlevels[i+1]);
    }
    // now we can change image values due to new level
    OMP_FOR()
    for(size_t i = 0; i < im->totpix; ++i){
        double d = im->data[i];
        double dnl = d * nlevls;
        size_t v = (size_t)dnl;
        if(v >= nlevls){
            v = nlevls-1;
        }
        double frac = dnl - v;
        if(frac < 0.){
            DBG("frac=%g<0", frac);
            frac = 0.;
        }else if(frac > 1.){
            DBG("frac=%g>1", frac);
            frac = 1;
        }
        im->data[i] = (newlevels[v+1] - newlevels[v]) * frac + newlevels[v];
    }
    FREE(newlevels);
    theret:
    histogram_free(&hist);
    if(success) return im;
    else return NULL;
}
