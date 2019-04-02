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
 *                                  FITS images                                       *
 **************************************************************************************/

/*
 * Functions for working with images: read to memory/copy/modify
 */
void image_free(FITSimage **img){
    FREE((*img)->data);
    FREE((*img)->naxes);
    FREE(*img);
}

/**
 * @brief image_datatype_size - calculate size of one data element for given bitpix
 * @param bitpix    - value of BITPIX
 * @param dtype (o) - nearest type of data to fit input type
 * @return amount of space need to store one pixel data
 */
int image_datatype_size(int bitpix, int *dtype){
    int s = bitpix/8;
    if(dtype){
        switch(s){
            case 1: // BYTE_IMG
                *dtype = TBYTE; // uchar
            break;
            case 2: // SHORT_IMG
                *dtype = TUSHORT;
            break;
            case 4: // LONG_IMG
                *dtype = TUINT;
            break;
            case 8: // LONGLONG_IMG
                *dtype = TULONG;
            break;
            case -4: // FLOAT_IMG
                *dtype = TFLOAT;
            break;
            case -8: // DOUBLE_IMG
                *dtype = TDOUBLE;
            break;
            default:
                return 0; // wrong bitpix
        }
    }
    DBG("bitpix: %d, dtype=%d, imgs=%d", bitpix, *dtype, s);
    return abs(s);
}

/**
 * @brief image_data_malloc - allocate memory for given bitpix
 * @param totpix  - total pixels amount
 * @param pxbytes - number of bytes for each pixel
 * @return allocated memory
 */
void *image_data_malloc(long totpix, int pxbytes){
    if(pxbytes <= 0 || totpix <= 0) return NULL;
    void *data = calloc(totpix, pxbytes);
    DBG("Allocate %zd members of size %d", totpix, pxbytes);
    if(!data) ERR("calloc()");
    return data;
}

/**
 * @brief image_new - create an empty image without headers, assign BITPIX to "bitpix"
 * @param naxis     - number of dimensions
 * @param naxes (i) - sizes by each dimension
 * @param bitpix    - BITPIX for given image
 * @return allocated structure or NULL
 */
FITSimage *image_new(int naxis, long *naxes, int bitpix){
    FITSimage *out = MALLOC(FITSimage, 1);
    int dtype, pxsz = image_datatype_size(bitpix, &dtype);
    long totpix = 0;
    if(naxis > 0){ // not empty image
        totpix = 1;
        for(int i = 0; i < naxis; ++i) if(naxes[i]) totpix *= naxes[i];
        out->data = image_data_malloc(totpix, pxsz);
        if(!out->data){
            FREE(out);
            return NULL;
        }
        out->naxes = MALLOC(long, naxis);
        memcpy(out->naxes, naxes, sizeof(long)*naxis);
    }
    out->totpix = totpix;
    out->naxis = naxis;
    out->pxsz = pxsz;
    out->bitpix = bitpix;
    out->dtype = dtype;
    return out;
}

// function for qsort
static int cmpdbl(const void *d1, const void *d2){
    register double D1 = *(const double*)d1, D2 = *(const double*)d2;
    if(fabs(D1 - D2) < DBL_EPSILON) return 0;
    if(D1 > D2) return 1;
    else return -1;
}

// functions to convert double to different datatypes
static void convu8(FITSimage *img, const double *dimg){
    uint8_t *dptr = (uint8_t*) img->data;
    OMP_FOR()
    for(long i = 0; i < img->totpix; ++i){
        dptr[i] = (uint8_t) dimg[i];
    }
}
static void convu16(FITSimage *img, const double *dimg){
    uint16_t *dptr = (uint16_t*) img->data;
    OMP_FOR()
    for(long i = 0; i < img->totpix; ++i){
        dptr[i] = (uint16_t) dimg[i];
    }
}
static void convu32(FITSimage *img, const double *dimg){
    uint32_t *dptr = (uint32_t*) img->data;
    OMP_FOR()
    for(long i = 0; i < img->totpix; ++i){
        dptr[i] = (uint32_t) dimg[i];
    }
}
static void convu64(FITSimage *img, const double *dimg){
    uint64_t *dptr = (uint64_t*) img->data;
    OMP_FOR()
    for(long i = 0; i < img->totpix; ++i){
        dptr[i] = (uint64_t) dimg[i];
    }
}
static void convf(FITSimage *img, const double *dimg){
    float *dptr = (float*) img->data;
    OMP_FOR()
    for(long i = 0; i < img->totpix; ++i){
        dptr[i] = (float) dimg[i];
    }
}

/**
 * @brief image_rebuild substitute content of image with array dimg, change its output type
 * @param img  - input image
 * @param dimg - data to change
 * @return rebuilt image; array dimg no longer used an can be FREEd
 */
FITSimage *image_rebuild(FITSimage *img, double *dimg){
    if(!img || !dimg) return NULL;
    // first we should calculate statistics of new image
    double *sr = MALLOC(double, img->totpix);
    memcpy(sr, dimg, sizeof(double)*img->totpix);
    initomp();
    qsort(sr, img->totpix, sizeof(double), cmpdbl);
    double mindiff = DBL_MAX;
    bool isint = TRUE;
    for(long i = 1; i < img->totpix; ++i){
        double d = fabs(sr[i] - sr[i-1]);
        if(d > DBL_EPSILON && d < mindiff) mindiff = d;
        if(fabs(d - floor(d)) > DBL_EPSILON) isint = FALSE;
    }
    // now we know min, max and minimal difference between elements
    double min = sr[0], max = sr[img->totpix-1];
    FREE(sr);
    DBG("min: %g, max: %g, mindiff: %g", min, max, mindiff);
    int bitpix = -64; // double by default
    void (*convdata)(FITSimage*, const double*) = NULL;
    if(isint)do{ // check which integer type will suits better
        DBG("INTEGER?");
        if(min < 0){ isint = FALSE; break;} // TODO: correct with BZERO
        if(max < UINT8_MAX){bitpix = 8; convdata = convu8; break; }
        if(max < UINT16_MAX){bitpix = 16; convdata = convu16; break; }
        if(max < UINT32_MAX){bitpix = 32; convdata = convu32; break; }
        if(max < UINT64_MAX){bitpix = 64; convdata = convu64; break; }
    }while(0);
    if(!isint){ // float or double
        DBG("mindiff: %g(%g), min: %g(%g), max: %g(%g)", mindiff,FLT_EPSILON,
            min, FLT_MIN, max, FLT_MAX);
        if(mindiff > FLT_EPSILON && (min > -FLT_MAX) && max < FLT_MAX){
            bitpix = -32;
            convdata = convf;
        }
    }
    DBG("NOW: bitpix = %d", bitpix);
    img->bitpix = bitpix;
    void *data = calloc(img->totpix, abs(bitpix/8));
    if(!data){ WARN("calloc()"); return NULL; }
    FREE(img->data);
    img->data = data;
    if(convdata) convdata(img, dimg);
    else memcpy(img->data, dimg, sizeof(double)*img->totpix);
    img->pxsz = image_datatype_size(bitpix, &img->dtype);
    return img;
}

/**
 * build IMAGE image from data array indata
 *
FITSimage *image_build(size_t h, size_t w, int dtype, uint8_t *indata){
    size_t stride = 0;
    double (*fconv)(uint8_t *data) = NULL;
    double ubyteconv(uint8_t *data){return (double)*data;}
    double ushortconv(uint8_t *data){return (double)*(short*)data;}
    double ulongconv(uint8_t *data){return (double)*(unsigned long*)data;}
    double ulonglongconv(uint8_t *data){return (double)*(uint64_t*)data;}
    double floatconv(uint8_t *data){return (double)*(float*)data;}
    FITSimage *out = image_new(h, w, dtype);
    switch (dtype){
        case BYTE_IMG:
            stride = 1;
            fconv = ubyteconv;
        break;
        //case USHORT_IMG?
        case SHORT_IMG:
            stride = 2;
            fconv = ushortconv;
        break;
        case LONG_IMG:
            stride = 4;
            fconv = ulongconv;
        break;
        case FLOAT_IMG:
            stride = 4;
            fconv = floatconv;
        break;
        case LONGLONG_IMG:
            fconv = ulonglongconv;
            stride = 8;
        break;
        case DOUBLE_IMG:
            memcpy(out->data, indata, sizeof(double)*w*h);
            return out;
        break;
        default:
        /// Неправильный тип данных
            ERRX(_("Wrong data type"));
    }
    size_t y, W = w*stride;
    double *data = out->data;
    OMP_FOR(shared(data))
    for(y = 0; y < h; ++y){
        double *dout = &data[y*w];
        uint8_t *din = &indata[y*W];
        size_t x;
        for(x = 0; x < w; ++x, din += stride)
            *dout++ = fconv(din);
    }
    return out;
}*/

/**
 * create an empty copy of image "in" without headers, assign data type to "dtype"
 */
FITSimage *image_mksimilar(FITSimage *img){
    return image_new(img->naxis, img->naxes, img->bitpix);
}

/**
 * make full copy of image 'in'
 */
FITSimage *image_copy(FITSimage *in){
    FITSimage *out = image_mksimilar(in);
    if(!out) return NULL;
    memcpy(out->data, in->data, (in->pxsz)*(in->totpix));
    return out;
}

/**
 * @brief image_read - read image from current HDU
 * @param fits - fits structure pointer
 * @return - pointer to allocated image structure or NULL if failed
 */
FITSimage *image_read(FITS *fits){
    // TODO: open not only 2-dimensional files!
    // get image dimensions
    int naxis, fst = 0, bitpix;
    fits_get_img_dim(fits->fp, &naxis, &fst);
    if(fst){FITS_reporterr(&fst); return NULL;}
    long *naxes = MALLOC(long, naxis);
    fits_get_img_param(fits->fp, naxis, &bitpix, &naxis, naxes, &fst);
    if(fst){FITS_reporterr(&fst); return NULL;}
    FITSimage *img = image_new(naxis, naxes, bitpix);
    FREE(naxes);
    int stat = 0;
    if(!img) return NULL;
    if(!img->data) return img; // empty "image" - no data inside
    DBG("try to read, dt=%d, sz=%ld", img->dtype, img->totpix);
    //int bscale = 1, bzero = 32768, status = 0;
    //fits_set_bscale(fits->fp, bscale, bzero, &status);
    fits_read_img(fits->fp, img->dtype, 1, img->totpix, NULL, img->data, &stat, &fst);
    //fits_read_img(fits->fp, TUSHORT, 1, img->totpix, NULL, img->data, &stat, &fst);
    if(fst){
        FITS_reporterr(&fst);
        image_free(&img);
        return NULL;
    }
    if(stat) WARNX(_("Found %d pixels with undefined value"), stat);
    DBG("ready");
    return img;
}

void doubleimage_free(doubleimage **im){
    FREE((*im)->data);
    FREE(*im);
}

static double ubyteconv(const void *data){return (double)*((const uint8_t*)data);}
static double ushortconv(const void *data){return (double)*((const uint16_t*)data);}
static double ulongconv(const void *data){return (double)*((const uint32_t*)data);}
static double ulonglongconv(const void *data){return (double)*((const uint64_t*)data);}
static double floatconv(const void *data){return (double)*((const float*)data);}
/**
 * @brief image2double convert image values to double
 * @param img - input image
 * @return array of double with size img->totpix
 */
doubleimage *image2double(FITSimage *img){
    size_t tot = img->totpix;
    double *ret = MALLOC(double, tot);
    doubleimage *dblim = MALLOC(doubleimage, 1);
    dblim->data = ret;
    dblim->width = img->naxes[0];
    dblim->height = img->naxes[1];
    dblim->totpix = tot;
    DBG("image: %ldx%ld=%ld", dblim->width, dblim->height, tot);
    double (*fconv)(const void *x);
    switch(img->dtype){
        case TBYTE:
            fconv = ubyteconv;
        break;
        case TUSHORT:
            fconv = ushortconv;
        break;
        case TUINT:
            fconv = ulongconv;
        break;
        case TULONG:
            fconv = ulonglongconv;
        break;
        case TFLOAT:
            fconv = floatconv;
        break;
        case TDOUBLE:
            memcpy(ret, img->data, sizeof(double)*img->totpix);
            return dblim;
        break;
        default:
            WARNX(_("Undefined image type, cant convert to double"));
            FREE(ret);
            FREE(dblim);
            return NULL;
    }
    uint8_t *din = img->data;
    initomp();
    OMP_FOR()
    for(size_t i = 0; i < tot; ++i){
        ret[i] = fconv(&din[i*img->pxsz]);
    }
    return dblim;
}

/**
 * @brief get_imgstat - calculate simplest statistics: mean/std/min/max
 * @param dimg   - double array
 * @param totpix - total amount of pixels
 * @param est    - structure for output data (for thread-safe operations)
 * @return structure with statistics data
 */
imgstat *get_imgstat(const doubleimage *im, imgstat *est){
    static imgstat st;
    if(!im || !im->totpix) return &st; // return some trash if wrong data
    double *dimg = im->data;
    size_t totpix = im->totpix;
    st.min = dimg[0];
    st.max = dimg[0];
    double sum = dimg[0], sum2 = dimg[0];
    for(size_t i = 1; i < totpix; ++i){
        double val = dimg[i];
        if(st.min > val) st.min = val;
        if(st.max < val) st.max = val;
        sum += val;
        sum2 += val*val;
    }
    DBG("tot:%ld, sum=%g, sum2=%g, min=%g, max=%g", totpix, sum, sum2, st.min, st.max);
    st.mean = sum / totpix;
    st.std = sqrt(sum2/totpix - st.mean*st.mean);
    if(est){
        memcpy(est, &st, sizeof(imgstat));
        return est;
    }
    return &st;
}

/**
 * @brief normalize_dbl - convert double image array to normalized (0..1)
 * @param dimg (io) - array with image pixels
 * @param st   (i)  - image statistics (maybe NULL, then calculates here)
 * @return pointer to dimg
 */
doubleimage *normalize_dbl(doubleimage *im, imgstat *st){
    if(!im || !im->data || !st) return NULL;
    double *dimg = im->data;
    size_t totpix = im->totpix;
    if(totpix < 1) return NULL;
    imgstat imst;
    if(!st) st = get_imgstat(im, &imst);
    double rng = st->max - st->min;
    if(rng < 2*DBL_EPSILON){
        WARNX(_("Data range is too small"));
        return NULL;
    }
    initomp();
    OMP_FOR()
    for(size_t i = 0; i < totpix; ++i){
        dimg[i] = (dimg[i] - st->min) / rng;
    }
    return im;
}

/**
 * @brief new_doubleimage - create image of double numbers
 * @param w - width
 * @param h - height
 * @return empty image
 */
doubleimage *doubleimage_new(size_t w, size_t h){
    doubleimage *out = MALLOC(doubleimage, 1);
    out->height = h;
    out->width = w;
    out->totpix = w*h;
    out->data = MALLOC(double, out->totpix);
    return out;
}
