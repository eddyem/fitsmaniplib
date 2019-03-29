/*
 * fits.c - cfitsio routines
 *
 * Copyright 2015 Edward V. Emelianov <eddy@sao.ru, edward.emelianoff@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */
#include "FITSmanip.h"
#include "local.h"

/**************************************************************************************
 *                                  FITS tables                                       *
 **************************************************************************************/
/*
 * Here are functions to work with both types of tables in FITS files
 */
/*
 * TODO: make it work!
 * Just now it have owfull functionality
 */

/**
 * @brief datatype_size - calculate size of one data element for given datatype
 * @param datatype - type of data
 * @return amount of space need to store one data element
 */
int datatype_size(int datatype){
    int s = sizeof(void*); // default - pointer to something
    switch(datatype){
        case TBYTE:
        case TLOGICAL:
        case TBIT:
            s = sizeof(char);
        break;
        case TSHORT:
        case TUSHORT:
            s = sizeof(short);
        break;
        case TINT:
        case TUINT:
            s = sizeof(int);
        break;
        case TLONG:
        case TULONG:
            s = sizeof(long);
        break;
        case TLONGLONG:
            s = sizeof(long long);
        break;
        case TFLOAT:
            s = sizeof(float);
        break;
        case TDOUBLE:
            s = sizeof(double);
        break;
        case TCOMPLEX:
            s = 2*sizeof(float);
        break;
        case TDBLCOMPLEX:
            s = 2*sizeof(double);
        break;
        default: // void*
        break;
    }
    return s;
}

/**
 * @brief table_free - free memory of table
 * @param tbl (io) - address of pointer to table
 */
void table_free(FITStable **tbl){
    if(!tbl || !*tbl) return;
    FITStable *intab = *tbl;
    int i, N = intab->ncols;
    for(i = 0; i < N; ++i){
        table_column *col = &(intab->columns[i]);
        if(col->coltype == TSTRING && col->width){
            long r, R = col->repeat;
            void **cont = (void**) col->contents;
            for(r = 0; r < R; ++r) free(*(cont++));
        }
        FREE(col->contents);
        FREE(col);
    }
    FREE(*tbl);
}

/**
 * @brief table_copy - make full copy of FITStable structure
 * @param intab (i) - pointer to table
 * @return pointer to copy of table
 */
FITStable *table_copy(FITStable *intab){
    if(!intab || intab->ncols <= 0 || intab->nrows <= 0) return NULL;
    FITStable *tbl = MALLOC(FITStable, 1);
    memcpy(tbl, intab, sizeof(FITStable));
    int ncols = intab->ncols, col;
    tbl->columns = MALLOC(table_column, ncols);
    memcpy(tbl->columns, intab->columns, sizeof(table_column)*ncols);
    table_column *ocurcol = tbl->columns, *icurcol = intab->columns;
    for(col = 0; col < ncols; ++col, ++ocurcol, ++icurcol){
        if(ocurcol->coltype == TSTRING && ocurcol->width){ // string array - copy all
            long r, R = ocurcol->repeat;
            char **oarr = (char**)ocurcol->contents, **iarr = (char**)icurcol->contents;
            for(r = 0; r < R; ++r, ++oarr, ++iarr){
                *oarr = strdup(*iarr);
                if(!oarr) ERR(_("strdup() failed!"));
            }
        }else memcpy(ocurcol->contents, icurcol->contents, icurcol->repeat * icurcol->width);
    }
    return tbl;
}

/**
 * @brief table_read - add FITS table to image structure
 * @param fits (i)   - pointer to FITS file structure
 * @return
 */
FITStable *table_read(FITS *fits){
    ERRX("table_read: don't use this function.");
    int ncols, i, fst = 0;
    long nrows;
    char extname[FLEN_VALUE];
    fitsfile *fp = fits->fp;
    fits_get_num_rows(fp, &nrows, &fst);
    if(fst || nrows < 1){
        FITS_reporterr(&fst);
        WARNX(_("Can't read row number!"));
        return NULL;
    }
    fits_get_num_cols(fp, &ncols, &fst);
    if(fst){FITS_reporterr(&fst); return NULL;}
    fits_read_key(fp, TSTRING, "EXTNAME", extname, NULL, &fst);
    if(fst){FITS_reporterr(&fst); return NULL;}
    DBG("Table named %s with %ld rows and %d columns", extname, nrows, ncols);
    FITStable *tbl = table_new(extname);
    if(!tbl) return NULL;
    tbl->nrows = nrows;
    for(i = 1; i <= ncols; ++i){
        int typecode;
        long repeat, width;
        fits_get_coltype(fp, i, &typecode, &repeat, &width, &fst);
        if(fst){
            FITS_reporterr(&fst);
            WARNX(_("Can't read column %d!"), i);
            continue;
        }
        DBG("typecode=%d, repeat=%ld, width=%ld, nrows=%ld", typecode, repeat, width, nrows);
        table_column col = {.repeat = repeat, .width = width, .coltype = typecode, .nrows = nrows};
        char key[32];
        snprintf(key, 32, "TFORM%d", i);
        char *kv = keylist_find_keyval(fits->curHDU->keylist, key, NULL);
        if(kv){
            DBG("Found format of %d column: %s", i, kv);
            strncpy(col.format, kv, FLEN_FORMAT);
            FREE(kv);
        }
        snprintf(key, 32, "TTYPE%d", i);
        kv = keylist_find_keyval(fits->curHDU->keylist, key, NULL);
        if(kv){
            DBG("Found name of %d column: %s", i, kv);
            strncpy(col.colname, kv, FLEN_KEYWORD);
            FREE(kv);
        }
        snprintf(key, 32, "TUNIT%d", i);
        kv = keylist_find_keyval(fits->curHDU->keylist, key, NULL);
        if(kv){
            DBG("Found data unit of %d column: %s", i, kv);
            strncpy(col.unit, kv, FLEN_CARD);
            FREE(kv);
        }
        FREE(col.contents);
        col.contents = malloc(width * repeat * nrows);
        if(!col.contents) ERRX("malloc");
        int anynul;
        int64_t nullval = 0;
        // TODO: WTF? fits_read_col don't work!
        if(typecode != TSTRING){
            fits_read_col(fp, typecode, i, 1, 1, nrows, (void*)nullval, col.contents,
                          &anynul, &fst);
        }else{
            for(int j = 0; j < nrows; ++j){
                char *carr = (char*) col.contents;
                char strnull[2] = "";
                /*fits_read_col(fp, typecode, i, j+1, 1, 1, (void*)strnull,
                            &carr[j*width*repeat], &anynul, &fst);
                */
                fits_read_col(fp, TSTRING, i, j+1, 1, 1, (void*)strnull,
                                            &carr[j], &anynul, &fst);
            DBG("RDDDDD %c", carr[j]);
                /*if(fst){
                    FITS_reporterr(&fst);
                    WARNX(_("Can't read column %d!"), i);
                    continue;
                }*/
            }
        }
        DBG("Column, cont[2]=%d, type=%d, w=%ld, r=%ld, nm=%s, u=%s", ((int*)col.contents)[2], col.coltype,
            col.width, col.repeat, col.colname, col.unit);
    for(int i = 0; i <nrows; ++i ){
        printf("data[%d]=%g\n", i, ((double*)col.contents)[i]);
    }
        table_addcolumn(tbl, &col);
        FREE(col.contents);
    }
    return tbl;
}

/**
 * @brief table_new   - create empty FITS table
 * @param tabname (i) - table name
 * @return
 */
FITStable *table_new(char *tabname){
    FITStable *tab = MALLOC(FITStable, 1);
    snprintf(tab->tabname, FLEN_CARD, "%s", tabname);
    return tab;
}

/**
 * @brief table_addcolumn - add to table 'tbl' column 'column'
 *      Be carefull! All fields of 'column' exept of 'format' should be filled
 *      - if data is character array, 'width' should be equal 0
 *      - all input data will be copied, so caller should run 'free' after this function!
 * @param tbl (i)    - pointer to table
 * @param column (i) - column to add
 * @return
 */
FITStable *table_addcolumn(FITStable *tbl, table_column *column){
    if(!tbl || !column || !column->contents) return NULL;
    long nrows = column->nrows;
    long width = column->width;
    if(tbl->nrows < nrows) tbl->nrows = nrows;
    size_t datalen = nrows * width;
    int cols = ++tbl->ncols;
    //char *curformat = column->bformat;
    //char *aformat = column->aformat;
    DBG("add column; width: %ld, nrows: %ld, name: %s", width, nrows, column->colname);
    /*
    switch(column->coltype){
        case TBIT:
            snprintf(curformat, FLEN_FORMAT, "%ldX", nrows);
            sprintf(aformat, FLEN_FORMAT, "I%ld", width);
        break;
        case TBYTE:
            snprintf(curformat, FLEN_FORMAT, "%ldB", nrows);
            sprintf(aformat, FLEN_FORMAT, "I%ld", width);
        break;
        case TLOGICAL:
            snprintf(curformat, FLEN_FORMAT, "%ldL", nrows);
            sprintf(aformat, FLEN_FORMAT, "I%ld", width);
        break;
        case TSTRING:
            if(width == 0){
                snprintf(curformat, FLEN_FORMAT, "%ldA", nrows);
            }else
                snprintf(curformat, FLEN_FORMAT, "%ldA%ld", nrows, width);
            sprintf(aformat, FLEN_FORMAT, "A%ld", nrows);
        break;
        case TSHORT:
            snprintf(curformat, FLEN_FORMAT, "%ldI", nrows);
            sprintf(aformat, FLEN_FORMAT, "I%ld", width);
        break;
        case TLONG:
            snprintf(curformat, FLEN_FORMAT, "%ldJ", nrows);
            sprintf(aformat, FLEN_FORMAT, "I%ld", width);
        break;
        case TLONGLONG:
            snprintf(curformat, FLEN_FORMAT, "%ldK", nrows);
            sprintf(aformat, FLEN_FORMAT, "I%ld", width);
        break;
        case TFLOAT:
            snprintf(curformat, FLEN_FORMAT, "%ldE", nrows);
        break;
        case TDOUBLE:
            snprintf(curformat, FLEN_FORMAT, "%ldD", nrows);
        break;
        case TCOMPLEX:
            snprintf(curformat, FLEN_FORMAT, "%ldM", nrows);
        break;
        case TDBLCOMPLEX:
            snprintf(curformat, FLEN_FORMAT, "%ldM", nrows);
        break;
        case TINT:
            snprintf(curformat, FLEN_FORMAT, "%ldJ", nrows);
            sprintf(aformat, FLEN_FORMAT, "I%ld", width);
        break;
        case TSBYTE:
            snprintf(curformat, FLEN_FORMAT, "%ldS", nrows);
            sprintf(aformat, FLEN_FORMAT, "I%ld", width);
        break;
        case TUINT:
            snprintf(curformat, FLEN_FORMAT, "%ldV", nrows);
            sprintf(aformat, FLEN_FORMAT, "I%ld", width);
        break;
        case TUSHORT:
            snprintf(curformat, FLEN_FORMAT, "%ldU", nrows);
            sprintf(aformat, FLEN_FORMAT, "I%ld", width);
        break;
        default:
            WARNX(_("Unsupported column data type!"));
            return NULL;
    }
    */
    //DBG("format: %s", curformat);
    if(!(tbl->columns = realloc(tbl->columns, sizeof(table_column)*(size_t)cols))) ERRX("malloc");
    table_column *newcol = &(tbl->columns[cols-1]);
    memcpy(newcol, column, sizeof(table_column));
    //strncpy(newcol->format, curformat, FLEN_FORMAT);
    newcol->contents = calloc(datalen, 1);
    if(!newcol->contents) ERRX("malloc");
    DBG("copy %zd bytes", datalen);
    memcpy(newcol->contents, column->contents, datalen);
    return tbl;
}

/**
 * @brief table_print - print out contents of table
 * @param tbl (i) - pointer to table to print
 */
void table_print(FITStable *tbl){
    ERRX("table_print: don't use this function.");
    printf("\nTable name: %s\n", tbl->tabname);
    int c, cols = tbl->ncols;
    long r, rows = tbl->nrows;
    for(c = 0; c < cols; ++c){
        printf("%s", tbl->columns[c].colname);
        if(*tbl->columns[c].unit) printf(" (%s)", tbl->columns[c].unit);
        printf("\t");
    }
    printf("\n");
    for(r = 0; r < rows; ++r){
        for(c = 0; c < cols; ++c){
            double *dpair; float *fpair;
            table_column *col = &(tbl->columns[c]);
            if(col->repeat < r){ // table with columns of different length
                printf("(empty)\t");
                continue;
            }
            switch(col->coltype){
                case TBIT:
                case TBYTE:
                    printf("%u\t", ((uint8_t*)col->contents)[r]);
                break;
                case TLOGICAL:
                    printf("%s\t", ((int8_t*)col->contents)[r] == 0 ? "FALSE" : "TRUE");
                break;
                case TSTRING:
                    if(col->width == 0) printf("%c\t", ((char*)col->contents)[r]);
                    else printf("%s\t", ((char**)col->contents)[r]);
                break;
                case TSHORT:
                    printf("%d\t", ((int16_t*)col->contents)[r]);
                break;
                case TLONG:
                case TINT:
                    printf("%d\t", ((int32_t*)col->contents)[r]);
                break;
                case TLONGLONG:
                    printf("%zd\t", ((int64_t*)col->contents)[r]);
                break;
                case TFLOAT:
                    printf("%g\t", (double)((float*)col->contents)[r]);
                break;
                case TDOUBLE:
                    printf("%g\t", ((double*)col->contents)[r]);
                break;
                case TCOMPLEX:
                    fpair = (float*)col->contents + 2*r;
                    printf("%g %s %g*i\t", (double)fpair[0], fpair[1] > 0 ? "+" : "-", (double)fpair[1]);
                break;
                case TDBLCOMPLEX:
                    dpair = (double*)col->contents + 2*r;
                    printf("%g %s %g*i\t", dpair[0], dpair[1] > 0 ? "+" : "-", dpair[1]);
                break;
                case TSBYTE:
                    printf("%d\t", ((int8_t*)col->contents)[r]);
                break;
                case TUINT:
                    printf("%d\t", ((uint32_t*)col->contents)[r]);
                break;
                case TUSHORT:
                    printf("%d\t", ((uint16_t*)col->contents)[r]);
                break;
            }
        }
        printf("\n");
    }
}

/**
 * @brief table_print_all - print out all tables in given FITS file
 * @param fits - pointer to given file structure
 */
void table_print_all(FITS *fits){
    if(fits->NHDUs < 1) return;
    int N = fits->NHDUs+1;
    for(int i = 1; i < N; ++i){
        if(fits->HDUs[i].hdutype == BINARY_TBL || fits->HDUs[i].hdutype == ASCII_TBL)
            table_print(fits->HDUs[i].contents.table);
    }
}

/**
 * @brief table_write - write tables to FITS file from current HDU
 * @param fits (i)    - pointer to FITS file structure
 */
bool table_write(FITS *file){
    ERRX("table_write: don't use this function.");
    fitsfile *fp = file->fp;
    int fst = 0;
    int hdutype = file->curHDU->hdutype;
    if(hdutype != BINARY_TBL || hdutype != ASCII_TBL)
        return FALSE;
    FITStable *tbl = file->curHDU->contents.table;
    if(tbl->ncols < 1 || tbl->nrows < 1) return FALSE;
    size_t c, cols = tbl->ncols;
    char **columns = MALLOC(char*, cols);
   // char **formats = MALLOC(char*, cols);
    char **units   = MALLOC(char*, cols);
    table_column *col = tbl->columns;
    for(c = 0; c < cols; ++c, ++col){
        columns[c] = col->colname;
      //  formats[c] = col->format;
        units[c]   = col->unit;
        DBG("col: %s, unit: %s", columns[c], units[c]);
    }
    //fits_movabs_hdu(fptr, 2, &hdutype, &status)
    fits_create_tbl(fp, hdutype, tbl->nrows, cols,
                              columns, NULL, units, tbl->tabname, &fst);
    FREE(columns);
    //FREE(formats);
    FREE(units);
    if(fst){
        FITS_reporterr(&fst);
        WARNX(_("Can't write table %s!"), tbl->tabname);
        return FALSE;
    }
    //col = tbl->columns;
    for(c = 0; c < cols; ++c, ++col){
        DBG("write column %zd", c);
        int fst = 0;
        fits_write_col(fp, col->coltype, c+1, 1, 1, col->repeat, col->contents, &fst);
        if(fst){
            FITS_reporterr(&fst);
            WARNX(_("Can't write column %s!"), col->colname);
            return FALSE;
        }
    }
    return TRUE;
}
