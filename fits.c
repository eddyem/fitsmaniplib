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
#include "local.h"
#include "FITSmanip.h"

#include <errno.h>
#include <libgen.h> // dirname, basename
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <usefull_macros.h>

/**************************************************************************************
 *                                  FITS keywords                                     *
 **************************************************************************************/

/*
 * TODO:
 *  -  int fits_parse_value(char *card, char *value, char *comment, int *status)
 *      will return value and comment of given record
 *  -  int fits_get_keytype(char *value, char *dtype, int *status)
 *      dtype returns with a value of 'C', 'L', 'I', 'F' or 'X', for character string,
 *      logical, integer, floating point, or complex, respectively
 *  -  int fits_get_keyclass(char *card) returns a classification code of keyword record
 *  -  int fits_parse_template(char *template, char *card, int *keytype, int *status)
 *      makes record from template
 *  - add HYSTORY?
 */

/**
 * @brief keylist_get_end - find last element in list
 * @param list (i) - pointer to first element of list
 * @return pointer to last element or NULL
 */
KeyList *keylist_get_end(KeyList *list){
    if(!list) return NULL;
    if(list->last) return list->last;
    KeyList *first = list;
    while(list->next) list = list->next;
    first->last = list;
    return list;
}

/**
 * @brief keylist_add_record - add record to keylist with optional check
 * @param list (io) - pointer to root of list or NULL
 *                      if *root == NULL, created node will be placed there
 * @param rec (i)   - data inserted
 * @param check     - !=0 to check `rec` with fits_parse_template
 * @return pointer to created node (if list == NULL - don't add created record to any list)
 */
KeyList *keylist_add_record(KeyList **list, char *rec, int check){
    if(!rec) return NULL;
    KeyList *node, *last;
    if((node = (KeyList*) MALLOC(KeyList, 1)) == 0)  return NULL; // allocation error
    int tp = 0, st = 0;
    if(check){
        char card[FLEN_CARD];
        fits_parse_template(rec, card, &tp, &st);
        if(st){
            fits_report_error(stderr, st);
            return NULL;
        }
        DBG("\n   WAS: %s\nBECOME: %s\ntp=%d", rec, card, tp);
        rec = card;
    }
    node->record = strdup(rec);
    if(!node->record){
        /// "Не могу скопировать данные"
        WARNX(_("Can't copy data"));
        return NULL;
    }
    node->keyclass = fits_get_keyclass(rec);
    if(list){
        if(*list){ // there was root node - search last
            last = keylist_get_end(*list);
            last->next = node; // insert pointer to new node into last element in list
            (*list)->last = node;
        //  DBG("last node %s", (*list)->last->record);
        }else *list = node;
    }
    return node;
}

/**
 * @brief keylist_find_key - find record with given key
 * @param list (i) - pointer to first list element
 * @param key (i)  - key to find
 * @return record with given key or NULL
 */
KeyList *keylist_find_key(KeyList *list, char *key){
    if(!list || !key) return NULL;
    size_t L = strlen(key);
    DBG("try to find %s", key);
    do{
        if(list->record){
            if(strncasecmp(list->record, key, L) == 0){ // key found
                DBG("found:\n%s", list->record);
                return list;
            }
        }
        list = list->next;
    }while(list);
    DBG("not found");
    return NULL;
}

/**
 * @brief keylist_modify_key -  modify key value
 * @param list (i)   - pointer to first list element
 * @param key (i)    - key name
 * @param newval (i) - new value of given key
 * @return modified record or NULL if given key is absent
 */
KeyList *keylist_modify_key(KeyList *list, char *key, char *newval){
    // TODO: look modhead.c in cexamples for protected keys
    char buf[FLEN_CARD], test[2*FLEN_CARD];
    KeyList *rec = keylist_find_key(list, key);
    if(!rec) return NULL;
    snprintf(test, 2*FLEN_CARD, "%s = %s", key, newval);
    int tp, st = 0;
    fits_parse_template(test, buf, &tp, &st);
    if(st){
        fits_report_error(stderr, st);
        return NULL;
    }
    DBG("new record:\n%s", buf);
    FREE(rec->record);
    rec->record = strdup(buf);
    return rec;
}

/**
 * @brief keylist_remove_key - remove record by key
 * @param keylist (io) - address of pointer to first list element
 * @param key (i)      - key value
 */
void keylist_remove_key(KeyList **keylist, char *key){
    if(!keylist || !*keylist || !key) return;
    size_t L = strlen(key);
    KeyList *prev = NULL, *list = *keylist, *last = keylist_get_end(list);
    do{
        if(list->record){
            if(strncmp(list->record, key, L) == 0){ // key found
                if(prev){ // not first record
                    prev->next = list->next;
                }else{ // first or only record
                    if(*keylist == last){
                        *keylist = NULL; // the only record - erase it
                    }else{ // first record - modyfy heading record
                        *keylist = list->next;
                        (*keylist)->last = last;
                    }
                }
                DBG("remove record by key \"%s\":\n%s",key, list->record);
                FREE(list->record);
                FREE(list);
                return;
            }
        }
        prev = list;
        list = list->next;
    }while(list);
}

/**
 * @brief keylist_remove_records - remove records by any sample
 * @param keylist (io) - address of pointer to first list element
 * @param sample (i)   - any (case sensitive) substring of record to be delete
 */
void keylist_remove_records(KeyList **keylist, char *sample){
    if(!keylist || !sample) return;
    KeyList *prev = NULL, *list = *keylist, *last = keylist_get_end(list);
    DBG("remove %s", sample);
    do{
        if(list->record){
            if(strstr(list->record, sample)){ // key found
                if(prev){
                    prev->next = list->next;
                }else{
                    if(*keylist == last){
                        *keylist = NULL; // the only record - erase it
                    }else{ // first record - modyfy heading record
                        *keylist = list->next;
                        (*keylist)->last = last;
                    }
                }
                KeyList *tmp = list->next;
                FREE(list->record);
                FREE(list);
                list = tmp;
                continue;
            }
        }
        prev = list;
        list = list->next;
    }while(list);
}

/**
 * @brief keylist_free - free list memory & set it to NULL
 * @param list (io) - address of pointer to first list element
 */
void keylist_free(KeyList **list){
    if(!list || !*list) return;
    KeyList *node = *list, *next;
    do{
        next = node->next;
        FREE(node->record);
        free(node);
        node = next;
    }while(node);
    *list = NULL;
}

/**
 * @brief keylist_copy - make a full copy of given list
 * @param list (i) - pointer to first list element
 * @return copy of list
 */
KeyList *keylist_copy(KeyList *list){
    if(!list) return NULL;
    KeyList *newlist = NULL;
    #ifdef EBUG
    int n = 0;
    #endif
    do{
        keylist_add_record(&newlist, list->record, 0);
        list = list->next;
        #ifdef EBUG
        ++n;
        #endif
    }while(list);
    DBG("copy list of %d entries", n);
    return newlist;
}

/**
 * @brief keylist_print - print out given list
 * @param list (i) - pointer to first list element
 */
void keylist_print(KeyList *list){
    while(list){
        printf("%s\n", list->record);
        list = list->next;
    }
}

/**
 * @brief keylist_read read all keys from current FITS file
 * This function read keys from current HDU, starting from current position
 * @param fits - opened structure
 * @return keylist read
 */
KeyList *keylist_read(FITS *fits){
    if(!fits || !fits->fp || !fits->curHDU) return NULL;
    int fst = 0, nkeys = -1, keypos = -1;
    KeyList *list = fits->curHDU->keylist;
    fits_get_hdrpos(fits->fp, &nkeys, &keypos, &fst);
    DBG("nkeys=%d, keypos=%d, status=%d", nkeys, keypos, fst);
    if(nkeys < 1){
        WARNX(_("No keywords in given HDU"));
        return NULL;
    }
    if(fst){
        fits_report_error(stderr, fst);
        return NULL;
    }
    DBG("Find %d keys, keypos=%d", nkeys, keypos);
    for(int j = 1; j <= nkeys; ++j){
        char card[FLEN_CARD];
        fits_read_record(fits->fp, j, card, &fst);
        if(fst) fits_report_error(stderr, fst);
        else{
            KeyList *kl = keylist_add_record(&list, card, 0);
            if(!kl){
                /// "Не могу добавить запись в список"
                WARNX(_("Can't add record to list"));
            }else{
                DBG("add key %d [class: %d]: \"%s\"", j, kl->keyclass, card);
            }
        }
    }
    return list;
}

/**************************************************************************************
 *                                  FITS tables                                       *
 **************************************************************************************/

/**
 * @brief table_free - free memory of table
 * @param tbl (io) - address of pointer to table
 */
void table_free(FITStable **tbl){
    if(!tbl || !*tbl) return;
    FITStable *intab = *tbl;
    size_t i, N = intab->ncols;
    for(i = 0; i < N; ++i){
        table_column *col = &(intab->columns[i]);
        if(col->coltype == TSTRING && col->width){
            size_t r, R = col->repeat;
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
    if(!intab || intab->ncols == 0 || intab->nrows == 0) return NULL;
    FITStable *tbl = MALLOC(FITStable, 1);
    memcpy(tbl, intab, sizeof(FITStable));
    size_t ncols = intab->ncols, col;
    tbl->columns = MALLOC(table_column, ncols);
    memcpy(tbl->columns, intab->columns, sizeof(table_column)*ncols);
    table_column *ocurcol = tbl->columns, *icurcol = intab->columns;
    for(col = 0; col < ncols; ++col, ++ocurcol, ++icurcol){
        if(ocurcol->coltype == TSTRING && ocurcol->width){ // string array - copy all
            size_t r, R = ocurcol->repeat;
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
    int ncols, i, fst = 0, ret;
    long nrows;
    char extname[FLEN_VALUE];
    fitsfile *fp = fits->fp;
    fits_get_num_rows(fp, &nrows, &fst);
    if(fst){fits_report_error(stderr, fst); return NULL;}
    fits_get_num_cols(fp, &ncols, &fst);
    if(fst){fits_report_error(stderr, fst); return NULL;}
    fits_read_key(fp, TSTRING, "EXTNAME", extname, NULL, &fst);
    if(fst){fits_report_error(stderr, fst); return NULL;}
    DBG("Table named %s with %ld rows and %d columns", extname, nrows, ncols);
    FITStable *tbl = table_new(extname);
    if(!tbl) return NULL;
    fits->curHDU->contents.table = tbl;
    for(i = 1; i <= ncols; ++i){
        int typecode;
        long repeat, width;
        ret = fits_get_coltype(fp, i, &typecode, &repeat, &width, &fst);
        if(fst){fits_report_error(stderr, fst); ret = fst; fst = 0;}
        if(ret){
            WARNX(_("Can't read column %d!"), i);
            continue;
        }
        DBG("typecode=%d, repeat=%ld, width=%ld", typecode, repeat, width);
        table_column col = {.repeat = repeat, .width = width, .coltype = typecode};
        void *array = malloc(width*repeat);
        if(!array) ERRX("malloc");
        int anynul;
        int64_t nullval = 0;
        int j;
        for(j = 0; j < repeat; ++j){
            ret = fits_read_col(fp, typecode, i, j=1, 1, 1, (void*)nullval, array, &anynul, &fst);
            if(fst){fits_report_error(stderr, fst); ret = fst; fst = 0;}
            if(ret){
                WARNX(_("Can't read column %d row %d!"), i, j);
                continue;
            }
        }
        DBG("done");
        col.contents = array;
        char keyword[FLEN_KEYWORD];
        int stat = 0;
        fits_make_keyn("TTYPE", i, keyword, &stat);
        if(stat){WARNX(_("Can't read table data type")); stat = 0;}
        fits_read_key(fp, TSTRING, keyword, col.colname, NULL, &stat);
        if(stat){ sprintf(col.colname, "noname"); stat = 0;}
        fits_make_keyn("TUNIT", i, keyword, &stat);
        if(stat){WARNX(_("Can't read table data unit")); stat = 0;}
        fits_read_key(fp, TSTRING, keyword, col.unit, NULL, &stat);
        if(stat) *col.unit = 0;
        DBG("Column, cont[2]=%d, type=%d, w=%ld, r=%ld, nm=%s, u=%s", ((int*)col.contents)[2], col.coltype,
            col.width, col.repeat, col.colname, col.unit);
        //table_addcolumn(tbl, &col);
        FREE(array);
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
    DBG("add new table: %s", tabname);
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
    FNAME();
    if(!tbl || !column || !column->contents) return NULL;
    long nrows = column->repeat;
    int width = column->width;
    if(tbl->nrows < nrows) tbl->nrows = nrows;
    size_t datalen = nrows * width, cols = ++tbl->ncols;
    char *curformat = column->format;
    DBG("add column; width: %d, nrows: %ld, name: %s", width, nrows, column->colname);
    /*void convchar(){ // count maximum length of strings in array
        char **charr = (char**)column->contents, *dptr = charr;
        size_t n, N = column->repeat;
        for(n = 0; n < N; ++n){
            if(strlen(
            if(*dptr++ == 0){ --processed; if(len > maxlen) maxlen = len; len = 0; }
            else{ ++len; }
        }
    }*/
    #define CHKLEN(type) do{if(width != sizeof(type)) datalen = sizeof(type) * nrows;}while(0)
    switch(column->coltype){
        case TBIT:
            snprintf(curformat, FLEN_FORMAT, "%ldX", nrows);
            CHKLEN(int8_t);
        break;
        case TBYTE:
            snprintf(curformat, FLEN_FORMAT, "%ldB", nrows);
            CHKLEN(int8_t);
        break;
        case TLOGICAL:
            snprintf(curformat, FLEN_FORMAT, "%ldL", nrows);
            CHKLEN(int8_t);
        break;
        case TSTRING:
            if(width == 0){
                snprintf(curformat, FLEN_FORMAT, "%ldA", nrows);
                datalen = nrows;
            }else
                snprintf(curformat, FLEN_FORMAT, "%ldA%d", nrows, width);
        break;
        case TSHORT:
            snprintf(curformat, FLEN_FORMAT, "%ldI", nrows);
            CHKLEN(int16_t);
        break;
        case TLONG:
            snprintf(curformat, FLEN_FORMAT, "%ldJ", nrows);
            CHKLEN(int32_t);
        break;
        case TLONGLONG:
            snprintf(curformat, FLEN_FORMAT, "%ldK", nrows);
            CHKLEN(int64_t);
        break;
        case TFLOAT:
            snprintf(curformat, FLEN_FORMAT, "%ldE", nrows);
            CHKLEN(float);
        break;
        case TDOUBLE:
            snprintf(curformat, FLEN_FORMAT, "%ldD", nrows);
            CHKLEN(double);
        break;
        case TCOMPLEX:
            snprintf(curformat, FLEN_FORMAT, "%ldM", nrows);
            if(width != sizeof(float)*2) datalen = sizeof(float) * nrows * 2;
        break;
        case TDBLCOMPLEX:
            snprintf(curformat, FLEN_FORMAT, "%ldM", nrows);
            if(width != sizeof(double)*2) datalen = sizeof(double) * nrows * 2;
        break;
        case TINT:
            snprintf(curformat, FLEN_FORMAT, "%ldJ", nrows);
            CHKLEN(int32_t);
        break;
        case TSBYTE:
            snprintf(curformat, FLEN_FORMAT, "%ldS", nrows);
            CHKLEN(int8_t);
        break;
        case TUINT:
            snprintf(curformat, FLEN_FORMAT, "%ldV", nrows);
            CHKLEN(int32_t);
        break;
        case TUSHORT:
            snprintf(curformat, FLEN_FORMAT, "%ldU", nrows);
            CHKLEN(int16_t);
        break;
        default:
            WARNX(_("Unsupported column data type!"));
            return NULL;
    }
    #undef CHKLEN
    DBG("new size: %ld, old: %ld", sizeof(table_column)*cols, sizeof(table_column)*(cols-1));
    if(!(tbl->columns = realloc(tbl->columns, sizeof(table_column)*cols))) ERRX("malloc");
    table_column *newcol = &(tbl->columns[cols-1]);
    memcpy(newcol, column, sizeof(table_column));
    newcol->contents = calloc(datalen, 1);
    if(!newcol->contents) ERRX("malloc");
    DBG("copy %zd bytes", datalen);
    if(column->coltype == TSTRING && width){
        long n;
        char **optr = (char**)newcol->contents, **iptr = (char**)column->contents;
        for(n = 0; n < nrows; ++n, ++optr, ++iptr) *optr = strdup(*iptr);
    }else
        memcpy(newcol->contents, column->contents, datalen);
    return tbl;
}

/**
 * @brief table_print - print out contents of table
 * @param tbl (i) - pointer to table to print
 */
void table_print(FITStable *tbl){
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
                    printf("%g\t", ((float*)col->contents)[r]);
                break;
                case TDOUBLE:
                    printf("%g\t", ((double*)col->contents)[r]);
                break;
                case TCOMPLEX:
                    fpair = (float*)col->contents + 2*r;
                    printf("%g %s %g*i\t", fpair[0], fpair[1] > 0 ? "+" : "-", fpair[1]);
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
    size_t i, N = fits->NHDUs+1;
    if(N == 0) return;
    for(i = 1; i < N; ++i){
        if(fits->HDUs[i].hdutype == BINARY_TBL || fits->HDUs[i].hdutype == ASCII_TBL)
            table_print(fits->HDUs[i].contents.table);
    }
}

/**
 * @brief table_write - write tables to FITS file from current HDU
 * @param fits (i)    - pointer to FITS file structure
 */
bool table_write(FITS *file){
    FNAME();
    fitsfile *fp = file->fp;
    int fst = 0;
    int hdutype = file->curHDU->hdutype;
    if(hdutype != BINARY_TBL || hdutype != ASCII_TBL)
        return FALSE;
    FITStable *tbl = file->curHDU->contents.table;
    size_t c, cols = tbl->ncols;
    char **columns = MALLOC(char*, cols);
    char **formats = MALLOC(char*, cols);
    char **units   = MALLOC(char*, cols);
    table_column *col = tbl->columns;
    for(c = 0; c < cols; ++c, ++col){
        columns[c] = col->colname;
        formats[c] = col->format;
        units[c]   = col->unit;
        DBG("col: %s, form: %s, unit: %s", columns[c], formats[c], units[c]);
    }
    //fits_movabs_hdu(fptr, 2, &hdutype, &status)
    int ret = fits_create_tbl(fp, hdutype, tbl->nrows, cols,
                              columns, formats, units, tbl->tabname, &fst);
    if(fst){fits_report_error(stderr, fst); ret = fst; fst = 0;}
    FREE(columns); FREE(formats); FREE(units);
    if(ret){
        WARNX(_("Can't write table %s!"), tbl->tabname);
        return FALSE;
    }
    //col = tbl->columns;
    for(c = 0; c < cols; ++c, ++col){
        DBG("write column %zd", c);
        int fst = 0;
        fits_write_col(fp, col->coltype, c+1, 1, 1, col->repeat, col->contents, &fst);
        if(fst){
            fits_report_error(stderr, fst);
            WARNX(_("Can't write column %s!"), col->colname);
            return FALSE;
        }
    }
    return TRUE;
}

/**************************************************************************************
 *                                  FITS files                                        *
 **************************************************************************************/

void FITS_free(FITS **fits){
    if(!fits || !*fits) return;
    FITS *f = *fits;
    FREE(f->filename);
    int fst = 0;
    fits_close_file(f->fp, &fst);
    int n, N = f->NHDUs;
    for(n = 1; n < N; ++n){
        FITSHDU *hdu = &f->HDUs[n];
        keylist_free(&hdu->keylist);
        switch(hdu->hdutype){
            case IMAGE_HDU:
                image_free(&hdu->contents.image);
            break;
            case BINARY_TBL:
            case ASCII_TBL:
                table_free(&hdu->contents.table);
            break;
            default: // do nothing
            break;
        }
    }
    FREE(*fits);
}

/**

  TODO: READWRITE allows to modify files on-the-fly, need to use it!
  TODO: what's about HCOMPRESS?

 * read FITS file and fill 'IMAGE' structure (with headers and tables)
 * can't work with image stack - opens the first image met
 * works only with binary tables
 */

/**
 * @brief FITS_open - just open FITS file
 * @param filename - file to open
 * @return pointer to FITS structure or NULL
 */
FITS *FITS_open(char *filename){
    FITS *fits = MALLOC(FITS, 1);
    int fst = 0;
    // use fits_open_diskfile instead of fits_open_file to prevent using of extended name syntax
    fits_open_diskfile(&fits->fp, filename, READONLY, &fst);
    if(fst){
        fits_report_error(stderr, fst);
        FITS_free(&fits);
        return NULL;
    }
    fits->filename = strdup(filename);
    return fits;
}

/**
 * @brief FITS_read - try to open & read all contents of FITS file
 * This function won't work with unordinary files. Use it only with simple files with primitive structure.
 * @param filename - file to open
 * @return pointer to FITS structure or NULL
 */
FITS *FITS_read(char *filename){
    int hdunum = 0, fst = 0;
    FITS *fits = FITS_open(filename);
    if(!fits) return NULL;
    fits_get_num_hdus(fits->fp, &hdunum, &fst);
    DBG("Got %d HDUs", hdunum);
    if(fst || hdunum < 1){
        if(!fst) FITS_free(&fits);
        WARNX(_("Can't read HDU"));
        goto returning;
    }
    fits->NHDUs = hdunum;
    fits->HDUs = MALLOC(FITSHDU, hdunum+1);
    int hdutype;
    for(int i = 1; i <= hdunum && !(fits_movabs_hdu(fits->fp, i, &hdutype, &fst)); ++i){
        FITSHDU *curHDU = &fits->HDUs[i];
        fits->curHDU = curHDU;
        DBG("try to read keys from HDU #%d (type: %d)", i, hdutype);
        curHDU->hdutype = hdutype;
        curHDU->keylist = keylist_read(fits);
        // types: IMAGE_HDU , ASCII_TBL, BINARY_TBL
        DBG("HDU[%d] type %d", i, hdutype);
        switch(hdutype){
            case IMAGE_HDU:
                DBG("Image");
                curHDU->contents.image = image_read(fits);
            break;
            case BINARY_TBL:
                DBG("Binary table");
            break;
            case ASCII_TBL:
                DBG("ASCII table");
                //table_read(img, fp);
            break;
            default:
                WARNX(_("Unknown HDU type"));
        }
    }
    if(fst == END_OF_FILE){
        fst = 0;
    }else goto returning;
returning:
    if(fst){
        fits_report_error(stderr, fst);
        fst = 0;
        FITS_free(&fits);
    }
    return fits;
}

static bool keylist_write(KeyList *kl, fitsfile *fp){
    int st = 0;
    bool ret = TRUE;
    if(!fp || !kl) return FALSE;
    while(kl){
        if(kl->keyclass > TYP_CMPRS_KEY){ // this record should be written
            fits_write_record(fp, kl->record, &st);
            DBG("Write %s, st = %d", kl->record, st);
            if(st){fits_report_error(stderr, st); st = 0; ret = FALSE;}
        }
        kl = kl->next;
    }
    return ret;
}

/**
 * @brief FITS_write - write FITS file to disk
 * @param filename   - new filename (with possible cfitsio additions like ! and so on)
 * @param fits       - structure to write
 * @return TRUE if all OK
 */
bool FITS_write(char *filename, FITS *fits){
    if(!filename || !fits) return FALSE;
    fitsfile *fp;
    int fst = 0;
    fits_create_file(&fp, filename, &fst);
    DBG("create file %s", filename);
    if(fst){fits_report_error(stderr, fst); return FALSE;}
    int N = fits->NHDUs;
    for(int i = 1; i <= N; ++i){
        FITSHDU *hdu = &fits->HDUs[i];
        if(!hdu) continue;
        FITSimage *img;
        long naxes[2] = {0,0};
        KeyList *records = hdu->keylist;
        DBG("HDU #%d (type %d)", i, hdu->hdutype);
        switch(hdu->hdutype){
            case IMAGE_HDU:
                img = hdu->contents.image;
                if(!img && records){ // something wrong - just write keylist
                    DBG("create empty image with records");
                    fits_create_img(fp, SHORT_IMG, 0, naxes, &fst);
                    if(fst){fits_report_error(stderr, fst); fst = 0; continue;}
                    keylist_write(records, fp);
                    DBG("OK");
                    continue;
                }
                naxes[0] = img->width, naxes[1] = img->height;
                DBG("create, bitpix: %d, naxes = {%zd, %zd}", img->bitpix, naxes[0], naxes[1]);
                //fits_create_img(fp, img->bitpix, 2, naxes, &fst);
                fits_create_img(fp, SHORT_IMG, 2, naxes, &fst);
                if(fst){fits_report_error(stderr, fst); fst = 0; continue;}
                keylist_write(records, fp);
                DBG("OK, now write image");
                // TODO: change to original data type according to bitpix or more whide according to min/max
                DBG("bitpix: %d, dtype: %d", SHORT_IMG, img->dtype);
                //int bscale = 1, bzero = 32768, status = 0;
                //fits_set_bscale(fp, bscale, bzero, &status);
                if(img && img->data){
                    fits_write_img(fp, TUSHORT, 1, img->width * img->height, img->data, &fst);
                    //fits_write_img(fp, img->dtype, 1, img->width * img->height, img->data, &fst);
                    DBG("status: %d", fst);
                    if(fst){fits_report_error(stderr, fst); fst = 0; continue;}
                }
            break;
            case BINARY_TBL:
            case ASCII_TBL:
                // TODO: save table
            break;
        }
    }

    fits_close_file(fp, &fst);
    if(fst){fits_report_error(stderr, fst);}
    return TRUE;
}

/**
 * @brief FITS_rewrite - rewrite file in place
 * @param fits - pointer to FITS structure
 * @return TRUE if all OK
 */
bool FITS_rewrite(FITS *fits){
    FNAME();
    char rlpath[PATH_MAX];
    if(realpath(fits->filename, rlpath)){do{ // got real path - try to make link
        char *d = strdup(rlpath);
        if(!d){ WARN("strdup()"); FREE(d); break; }
        char *dir = dirname(d);
        if(!dir){ WARN("dirname()"); FREE(d); break; }
        char newpath[PATH_MAX];
        char *nm = tmpnam(NULL);
        if(!nm){ WARN("tmpnam()"); FREE(d); break; }
        char *fnm = basename(nm);
        if(!fnm){ WARN("basename()"); FREE(d); break; }
        snprintf(newpath, PATH_MAX, "%s/%s", dir, fnm);
        FREE(d);
        DBG("make link: %s -> %s", rlpath, newpath);
        if(link(rlpath, newpath)){ WARN("link()"); break; }
        if(unlink(rlpath)){ WARN("unlink()"); break; }
        if(FITS_write(rlpath, fits)){
            unlink(newpath);
            return TRUE;
        }
        // problems: restore old file
        if(link(newpath, rlpath)) WARN("link()");
        if(unlink(newpath)) WARN("unlink()");
    }while(0);}else WARN(_("Can't get real path for %s, use cfitsio to rewrite"), fits->filename);
    // Can't get realpath or some other error, try to use cfitsio
    snprintf(rlpath, PATH_MAX, "!%s", fits->filename);
    DBG("PATH: %s", rlpath);
    return FITS_write(rlpath, fits);
}

/**************************************************************************************
 *                                  FITS images                                       *
 **************************************************************************************/

void image_free(FITSimage **img){
    FREE((*img)->data);
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
            case 1:
            case 2:
            case 4:
                *dtype = TINT;
                s = 4;
            break;
            case 8:
                *dtype = TLONG;
            break;
            case -4:
            case -8:
                *dtype = TDOUBLE;
                s = 8;
            break;
            default:
                return 0; // wrong bitpix
        }
    }
    DBG("bitpix: %d, dtype=%d, imgs=%d", bitpix, *dtype, s);
    return s;
}

/**
 * @brief image_malloc - allocate memory for given bitpix
 * @param w      - image width
 * @param h      - image height
 * @param pxbytes- amount of bytes to store data from one pixel
 * @return allocated memory
 */
void *image_data_malloc(size_t w, size_t h, int pxbytes){
    if(!pxbytes || !w || !h) return NULL;
    void *data = calloc(w*h, pxbytes);
    DBG("Allocate %zd members of size %d", w*h, pxbytes);
    if(!data) ERR(_("calloc()"));
    return data;
}

/**
 * @brief image_new - create an empty image without headers, assign BITPIX to "bitpix"
 * @param w      - image width
 * @param h      - image height
 * @param dtype  - image data type
 * @return
 */
FITSimage *image_new(size_t  w, size_t h, int bitpix){
    FITSimage *out = MALLOC(FITSimage, 1);
    int dtype, pxsz = image_datatype_size(bitpix, &dtype);
    if(w && h){
        out->data = image_data_malloc(w, h, pxsz);
        if(!out->data){
            WARNX(_("Bad w, h or pxsz"));
            FREE(out);
            return NULL;
        }
    }
    out->width = w;
    out->height = h;
    out->pxsz = pxsz;
    out->bitpix = bitpix;
    out->dtype = dtype;
    return out;
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
    if(!img || img->height < 1 || img->width < 1) return NULL;
    return image_new(img->width, img->height, img->bitpix);
}

/**
 * make full copy of image 'in'
 */
FITSimage *image_copy(FITSimage *in){
    FITSimage *out = image_mksimilar(in);
    if(!out) return NULL;
    // TODO: size of data as in original!
    memcpy(out->data, in->data, (in->pxsz)*(in->width)*(in->height));
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
    long naxes[2] = {0,0};
    fits_get_img_param(fits->fp, 2, &bitpix, &naxis, naxes, &fst);
    if(fst){fits_report_error(stderr, fst); return NULL;}
    if(naxis > 2){
        WARNX(_("Images with > 2 dimensions are not supported"));
        return NULL;
    }/*
    if(naxis < 2){
        WARNX(_("Not an image: NAXIS = %d"), naxis);
        return NULL;
    }*/
    DBG("got image %ldx%ld pix, bitpix=%d", naxes[0], naxes[1], bitpix);

    FITSimage *img = image_new(naxes[0], naxes[1], bitpix);

    int stat = 0;
    if(!img) return NULL;
    if(!img->data) return img; // empty "image" - no data inside
    DBG("try to read, dt=%d, sz=%ld", img->dtype, naxes[0]*naxes[1]);
    //int bscale = 1, bzero = 32768, status = 0;
    //fits_set_bscale(fits->fp, bscale, bzero, &status);
    //fits_read_img(fits->fp, img->dtype, 1, naxes[0]*naxes[1], NULL, img->data, &stat, &fst);
    fits_read_img(fits->fp, TUSHORT, 1, naxes[0]*naxes[1], NULL, img->data, &stat, &fst);
    if(fst){
        fits_report_error(stderr, fst);
        image_free(&img);
        return NULL;
    }
    if(stat) WARNX(_("Found %d pixels with undefined value"), stat);
    DBG("ready");
    return img;
}
