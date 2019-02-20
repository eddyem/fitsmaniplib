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

#include <fitsio.h>
#include <stdbool.h>
#include <stdint.h>

/**************************************************************************************
 *                                      fits.c                                        *
 **************************************************************************************/

//typedef double Item;

#define FLEN_FORMAT	(12)

/*
cfitsio.h BITPIX code values for FITS image types:
#define BYTE_IMG      8
#define SHORT_IMG    16
#define LONG_IMG     32
#define LONGLONG_IMG 64
#define FLOAT_IMG   -32
#define DOUBLE_IMG  -64
*/
/*
// FilterType (not only convolution!)
typedef enum{
    FILTER_NONE = 0     // simple start
    ,MEDIAN             // median filter
    ,ADPT_MEDIAN        // simple adaptive median
    ,LAPGAUSS           // laplasian of gaussian
    ,GAUSS              // gaussian
    ,SOBELH             // Sobel horizontal
    ,SOBELV             // -//- vertical
    ,SIMPLEGRAD         // simple gradient (by Sobel)
    ,PREWITTH           // Prewitt (horizontal) - simple derivative
    ,PREWITTV           // -//- (vertical)
    ,SCHARRH            // Scharr (modified Sobel)
    ,SCHARRV
    ,STEP               // "posterisation"
} FType;

typedef struct{
    double *data;
    size_t size;
}Itmarray;
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
	long repeat;				// amount of rows -> 'contents' size = width*repeat
	char colname[FLEN_KEYWORD];	// column name (arg ttype of fits_create_tbl)
	char format[FLEN_FORMAT];	// format codes (tform)
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
/*
typedef struct{
	size_t amount;		// amount of tables in file
	FITStable **tables;	// array of pointers to tables
} FITStables;
*/
/**
  FITS image
  */
typedef struct{
	int width;			// width
	int height;			// height
	int bitpix;			// original bitpix
    int dtype;          // type of stored data
    int pxsz;           // number of bytes for one pixel data
	void *data; 	    // picture data
} FITSimage;

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

/*
typedef struct _Filter{
    char *name;         // filter name
    FType FilterType;   // filter type
    int w;              // filter width
    int h;              // height
    double sx;          // x half-width
    double sy;          // y half-width (sx, sy - for Gaussian-type filters)
    FITS* (*imfunc)(FITS *in, struct _Filter *f, Itmarray *i);    // image function for given conversion type
} Filter;

// mathematical operations when there's no '-i' parameter (for >1 FITS-files)
typedef enum{
    MATH_NONE = 0
    ,MATH_SUM           // make sum of all files
    ,MATH_MEDIAN        // calculate median by all files
    ,MATH_MEAN          // calculate mean for all files
    ,MATH_DIFF          // difference of first and rest files
} MathOper;
*/

void keylist_free(KeyList **list);
KeyList *keylist_add_record(KeyList **list, char *rec, int check);
KeyList *keylist_find_key(KeyList *list, char *key);
void keylist_remove_key(KeyList **list, char *key);
KeyList *keylist_modify_key(KeyList *list, char *key, char *newval);
void keylist_remove_records(KeyList **list, char *sample);
KeyList *keylist_copy(KeyList *list);
KeyList *keylist_get_end(KeyList *list);
void keylist_print(KeyList *list);
KeyList *keylist_read(FITS *fits);

void table_free(FITStable **tbl);
FITStable *table_new(char *tabname);
FITStable *table_read(FITS *img);
FITStable *table_addcolumn(FITStable *tbl, table_column *column);
bool table_write(FITS *fits);
void table_print(FITStable *tbl);
void table_print_all(FITS *fits);

void image_free(FITSimage **ima);
FITSimage *image_read(FITS *fits);
int image_datatype_size(int bitpix, int *dtype);
void *image_data_malloc(size_t w, size_t h, int pxbytes);
FITSimage *image_new(size_t w, size_t h, int bitpix);
FITSimage *image_mksimilar(FITSimage *in);
FITSimage *image_copy(FITSimage *in);
//FITSimage *image_build(size_t h, size_t w, int dtype, uint8_t *indata);

void FITS_free(FITS **fits);
FITS *FITS_read(char *filename);
FITS *FITS_open(char *filename);
bool FITS_write(char *filename, FITS *fits);
bool FITS_rewrite(FITS *fits);

/**************************************************************************************
 *                                    fileops.c                                       *
 **************************************************************************************/

char* make_filename(char *buff, size_t buflen, char *prefix, char *suffix);
bool file_is_absent(char *name);



/*
// pointer to image conversion function
typedef FITS* (*imfuncptr)(FITS *in, Filter *f, Itmarray *i);
*/
