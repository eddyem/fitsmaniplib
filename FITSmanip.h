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
#pragma once
#ifndef FITSMANIP_H__
#define FITSMANIP_H__
#include <fitsio.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>

/**************************************************************************************
 *                                common macros                                       *
 **************************************************************************************/


#define Stringify(x) #x
#define OMP_FOR(...) _Pragma(Stringify(omp parallel for __VA_ARGS__))
#ifndef MAX
#define MAX(x,y) ((x) > (y) ? (x) : (y))
#endif
#ifndef MIN
#define MIN(x,y) ((x) < (y) ? (x) : (y))
#endif

/**************************************************************************************
 *                                common typedef                                      *
 **************************************************************************************/

#ifndef FLEN_FORMAT
#define FLEN_FORMAT	(12)
#endif

/*
cfitsio.h BITPIX code values for FITS image types:
#define BYTE_IMG      8
#define SHORT_IMG    16
#define LONG_IMG     32
#define LONGLONG_IMG 64
#define FLOAT_IMG   -32
#define DOUBLE_IMG  -64
*/

/**
  Keylist: all keys from given HDU
 */
typedef struct klist_{
    int keyclass;               // key class [look int CFITS_API ffgkcl(char *tcard) ]
	char *record;               // record itself
	struct klist_ *next;        // next record
	struct klist_ *last;        // previous record
} KeyList;

/**
  Table column
  */
typedef struct{
	void *contents;				// contents of table
	int coltype;				// type of columns
	long width;					// data width
	long repeat;				// maybe != 1 for binary tables
    long nrows;                 // amount of rows -> 'contents' size = width*repeat
	char colname[FLEN_KEYWORD];	// column name (arg ttype of fits_create_tbl)
	char format[FLEN_FORMAT];	// format codes (tform) for tables
	char unit[FLEN_CARD];		// units (tunit)
} table_column;

/**
  FITS table
  */
typedef struct{
	int ncols;                  // amount of columns
	long nrows;                 // max amount of rows
	char tabname[FLEN_CARD];	// table name
	table_column *columns;      // array of structures 'table_column'
} FITStable;

/**
  FITS image
  */
typedef struct{
	int naxis;			// amount of image dimensions
	long *naxes;		// dimensions (x, y, z, etc)
    long totpix;        // total pixels amount
	int bitpix;			// original bitpix
    int dtype;          // type of stored data
    int pxsz;           // number of bytes for one pixel data
	void *data; 	    // picture data
} FITSimage;

// 2-dimensional image data as double value
typedef struct{
    size_t height;
    size_t width;
    size_t totpix;
    double *data;
} doubleimage;

// simplest statistics
typedef struct{
    double mean;
    double std;
    double min;
    double max;
} imgstat;

// type of intensity transformation
typedef enum{
    TRANSF_WRONG = 0,
    TRANSF_LINEAR,
    TRANSF_LOG,
    TRANSF_EXP,
    TRANSF_POW,
    TRANSF_SQR,
    TRANSF_COUNT // amount of transforms
} intens_transform;

// different color maps
typedef enum{
    PALETTE_WRONG = 0,
    PALETTE_GRAY,
    PALETTE_BR,
    PALETTE_HOT,
    PALETTE_COLD,
    PALETTE_JET,
    PALETTE_COUNT // amount of palettes
} image_palette;

typedef union{
    FITSimage *image;
    FITStable *table;
} FITSimtbl;

/**
  One of HDU
  */
typedef struct{
    int hdutype;        // type of current HDU: image/binary table/ACSII table/bad data
    FITSimtbl contents;// data contents of HDU
    KeyList *keylist;   // keylist of given HDU
} FITSHDU;

typedef struct{
    fitsfile *fp;       // cfitsio file structure
    char *filename;     // filename
    int NHDUs;          // HDU amount
    FITSHDU *HDUs;      // HDUs array itself
    FITSHDU *curHDU;    // pointer to current HDU
} FITS;

typedef struct{
    size_t *data;       // histogram data
    size_t size;        // amount of levels
    size_t totpix;      // total amount of pixels
    double *levels;     // levels (for histograms of double images) for each H value
}histogram;

/**************************************************************************************
 *                                 fitskeywords.c                                     *
 **************************************************************************************/
void keylist_free(KeyList **list);
KeyList *keylist_add_record(KeyList **list, char *rec, int check);
KeyList *keylist_find_key(KeyList *list, char *key);
char *record_get_keyval(char *r, char **comm);
char *keylist_find_keyval(KeyList *l, char *key, char **comm);
void keylist_remove_key(KeyList **list, char *key);
KeyList *keylist_modify_key(KeyList *list, char *key, char *newval);
void keylist_remove_records(KeyList **list, char *sample);
KeyList *keylist_copy(KeyList *list);
KeyList *keylist_get_end(KeyList *list);
void keylist_print(KeyList *list);
KeyList *keylist_read(FITS *fits);

/**************************************************************************************
 *                                 fitstables.c                                       *
 **************************************************************************************/
int datatype_size(int datatype);
void table_free(FITStable **tbl);
FITStable *table_new(char *tabname);
FITStable *table_read(FITS *img);
FITStable *table_addcolumn(FITStable *tbl, table_column *column);
bool table_write(FITS *fits);
void table_print(FITStable *tbl);
void table_print_all(FITS *fits);

/**************************************************************************************
 *                                  fitsimages.c                                      *
 **************************************************************************************/
void image_free(FITSimage **ima);
FITSimage *image_read(FITS *fits);
FITSimage *image_rebuild(FITSimage *img, double *dimg);
int image_datatype_size(int bitpix, int *dtype);
void *image_data_malloc(long totpix, int pxbytes);
FITSimage *image_new(int naxis, long *naxes, int bitpix);
FITSimage *image_mksimilar(FITSimage *in);
FITSimage *image_copy(FITSimage *in);
doubleimage *doubleimage_new(size_t w, size_t h);
void doubleimage_free(doubleimage **im);
doubleimage *image2double(FITSimage *img);
imgstat *get_imgstat(const doubleimage *dimg, imgstat *est);
doubleimage *normalize_dbl(doubleimage *dimg, imgstat *st);
//FITSimage *image_build(size_t h, size_t w, int dtype, uint8_t *indata);

/**************************************************************************************
 *                                  fitsfiles.c                                       *
 **************************************************************************************/
FITSHDU *FITS_addHDU(FITS *fits);
void FITS_free(FITS **fits);
FITS *FITS_read(char *filename);
FITS *FITS_open(char *filename);
bool FITS_write(char *filename, FITS *fits);
bool FITS_rewrite(FITS *fits);
char* make_filename(char *buff, size_t buflen, char *prefix, char *suffix);
bool file_absent(char *name);

/**************************************************************************************
 *                                   FITSmanip.c                                      *
 **************************************************************************************/
void FITS_reporterr(int *errcode);
void initomp();
doubleimage *mktransform(doubleimage *im, imgstat *st, intens_transform transf);
uint8_t *convert2palette(doubleimage *im, image_palette cmap);

/**************************************************************************************
 *                                   histogram.c                                      *
 **************************************************************************************/
void histogram_free(histogram **H);
histogram *dbl2histogram(doubleimage *im, size_t nvalues);
doubleimage *dbl_histcutoff(doubleimage *im, size_t nlevls, double fracbtm, double fractop);
doubleimage *dbl_histeq(doubleimage *im, size_t nlevls);

/**************************************************************************************
 *                                     median.c                                       *
 **************************************************************************************/
doubleimage *get_median(const doubleimage *img, size_t radius);
//doubleimage *get_adaptive_median(const doubleimage *img, size_t radius);
double quick_select(const double *idata, int n);
double calc_median(const double *idata, int n);

#endif // FITSMANIP_H__
