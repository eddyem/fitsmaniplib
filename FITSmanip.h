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

typedef struct klist_{
	char *record;
	struct klist_ *next;
	struct klist_ *last;
} KeyList;

typedef struct{
	void *contents;				// contents of table
	int coltype;				// type of columns
	long width;					// data width
	long repeat;				// amount of rows -> 'contents' size = width*repeat
	char colname[FLEN_KEYWORD];	// column name (arg ttype of fits_create_tbl)
	char format[FLEN_FORMAT];	// format codes (tform)
	char unit[FLEN_CARD];		// units (tunit)
}table_column;

typedef struct{
	int ncols;                  // amount of columns
	long nrows;                 // max amount of rows
	char tabname[FLEN_CARD];	// table name
	table_column *columns;      // array of structures 'table_column'
}FITStable;
/*
typedef struct{
	size_t amount;		// amount of tables in file
	FITStable **tables;	// array of pointers to tables
} FITStables;
*/
typedef struct{
	int width;			// width
	int height;			// height
	int dtype;			// picture data type
	void *data; 	    // picture data
} FITSimage;

typedef struct{
    fitsfile *fp;       // cfitsio file structure
    int Nimages;        // amount of images in file
    FITSimage **images; // image array
    int Ntables;        // amount of tables in file
    FITStable **tables; // table array
    KeyList *keylist;	// list of options for each key
} FITS;

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

void keylist_free(KeyList **list);
KeyList *keylist_add_record(KeyList **list, char *rec);
KeyList *keylist_find_key(KeyList *list, char *key);
void keylist_remove_key(KeyList **list, char *key);
KeyList *keylist_modify_key(KeyList *list, char *key, char *newval);
void keylist_remove_records(KeyList **list, char *sample);
KeyList *keylist_copy(KeyList *list);
KeyList *keylist_get_end(KeyList *list);
void keylist_print(KeyList *list);

void table_free(FITStable **tbl);
FITStable *table_new(char *tabname);
FITStable *table_read(FITS *img);
FITStable *table_addcolumn(FITStable *tbl, table_column *column);
bool table_write(FITS *fits);
void table_print(FITStable *tbl);
void table_print_all(FITS *fits);

void image_free(FITSimage **ima);
FITSimage *image_new(size_t h, size_t w, int dtype);
FITSimage *image_mksimilar(FITSimage *in, int dtype);
FITSimage *image_copy(FITSimage *in);
FITSimage *image_build(size_t h, size_t w, int dtype, uint8_t *indata);

void fits_free(FITS **fits);
FITS *fits_read(char *filename);
bool fits_write(char *filename, FITS *fits);

/**************************************************************************************
 *                                    fileops.c                                       *
 **************************************************************************************/

char* make_filename(char *buff, size_t buflen, char *prefix, char *suffix);
bool file_is_absent(char *name);




// pointer to image conversion function
typedef FITS* (*imfuncptr)(FITS *in, Filter *f, Itmarray *i);