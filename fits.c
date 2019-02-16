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
#include <string.h>
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
 * @brief keylist_add_record - add record to keylist
 * @param list (io) - pointer to root of list or NULL
 *                      if *root == NULL, created node will be placed there
 * @param rec (i)   - data inserted
 * @return pointer to created node
 */
KeyList *keylist_add_record(KeyList **list, char *rec){
    KeyList *node, *last;
    if((node = (KeyList*) MALLOC(KeyList, 1)) == 0)  return NULL; // allocation error
    node->record = strdup(rec); // insert data
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
    do{
        if(list->record){
            if(strncmp(list->record, key, L) == 0){ // key found
                return list;
            }
        }
        list = list->next;
    }while(list);
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
    char buf[FLEN_CARD];
    KeyList *rec = keylist_find_key(list, key);
    if(!rec) return NULL;
    char *comm = strchr(rec->record, '/');
    if(!comm) comm = "";
    // TODO: use fits_parse_template
    snprintf(buf, FLEN_CARD, "%-8s=%21s %s", key, newval, comm);
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
        keylist_add_record(&newlist, list->record);
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
    if(!fits || !fits->fp) return NULL;
    int fst, nkeys = -1, keypos = -1;
    KeyList *list = fits->keylist;
    fits_get_hdrpos(fits->fp, &nkeys, &keypos, &fst);
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
            KeyList *kl = keylist_add_record(&list, card);
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
    int N = fits->Ntables + 1;
    if(!(fits->tables = realloc(fits->tables, sizeof(FITStable**)*N)))
        ERR("realloc()");
    fits->tables[fits->Ntables++] = tbl;
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
    size_t i, N = fits->Ntables;
    if(N == 0) return;
    for(i = 0; i < N; ++i)
        table_print(fits->tables[i]);
}

/**
 * @brief table_write - write tables to FITS file
 * @param fits (i)    - pointer to FITS file structure
 */
bool table_write(FITS *file){
    FNAME();
    size_t N = file->Ntables, i;
    if(N == 0) return FALSE;
    fitsfile *fp = file->fp;
    for(i = 0; i < N; ++i){
        int fst = 0;
        FITStable *tbl = file->tables[i];
        size_t c, cols = tbl->ncols;
        char **columns = MALLOC(char*, cols);
        char **formats = MALLOC(char*, cols);
        char **units = MALLOC(char*, cols);
        table_column *col = tbl->columns;
        for(c = 0; c < cols; ++c, ++col){
            columns[c] = col->colname;
            formats[c] = col->format;
            units[c] = col->unit;
            DBG("col: %s, form: %s, unit: %s", columns[c], formats[c], units[c]);
        }
        //fits_movabs_hdu(fptr, 2, &hdutype, &status)
        int ret = fits_create_tbl(fp, BINARY_TBL, tbl->nrows, cols,
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
    int fst;
    fits_close_file(f->fp, &fst);
    keylist_free(&f->keylist);
    int n;
    for(n = 0; n < f->Ntables; ++n)
        table_free(&f->tables[n]);
    for(n = 0; n < f->Nimages; ++n)
        image_free(&f->images[n]);
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
    int fst;
    // use fits_open_diskfile instead of fits_open_file to prevent using of extended name syntax
    fits_open_diskfile(&fits->fp, filename, READWRITE, &fst); // READWRITE allows to modify files on-the-fly
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
    int hdutype;
    for(int i = 1; !(fits_movabs_hdu(fits->fp, i, &hdutype, &fst)); ++i){
        DBG("try to read keys");
        keylist_read(fits);
        // types: IMAGE_HDU , ASCII_TBL, BINARY_TBL
        DBG("HDU[%d] type %d", i, hdutype);
        switch(hdutype){
            case IMAGE_HDU:
                DBG("Image");
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
#if 0
    // TODO: open not only images (liststruc.c from cexamples)!
    // TODO: open not only 2-dimensional files!
    // get image dimensions
    int i, j, hdunum = 0, hdutype, nkeys, keypos, naxis;
    long naxes[2];
    char card[FLEN_CARD];
    fits_get_img_param(fp, 2, &img->dtype, &naxis, naxes, &fst);
    if(fst){fits_report_error(stderr, fst); goto returning;}
    if(naxis > 2){
        WARNX(_("Images with > 2 dimensions are not supported"));
        fst = 1;
        goto returning;
    }
    img->width = naxes[0];
    img->height = naxes[1];
    DBG("got image %ldx%ld pix, bitpix=%d", naxes[0], naxes[1], img->dtype);
    // loop through all HDUs

    if(fits_movabs_hdu(fp, imghdu, &hdutype, &fst)){
        WARNX(_("Can't open image HDU #%d"), imghdu);
        fst = 1;
        goto returning;
    }
    size_t sz = naxes[0] * naxes[1];
    img->data = MALLOC(double, sz);
    int stat = 0;
    fits_read_img(fp, TDOUBLE, 1, sz, NULL, img->data, &stat, &fst);
    if(fst){fits_report_error(stderr, fst);}
    if(stat) WARNX(_("Found %d pixels with undefined value"), stat);
    DBG("ready");
#endif
returning:
    if(fst){
        fits_report_error(stderr, fst);
        fst = 0;
        FITS_free(&fits);
    }
    return fits;
}

bool FITS_write(char *filename, FITS *fits){
    if(!filename || !fits) return FALSE;
    /*int w = fits->width, h = fits->height, fst = 0;
    long naxes[2] = {w, h};
    size_t sz = w * h;
    fitsfile *fp;
    fits_create_diskfile(&fp, filename, &fst);
    if(fst){fits_report_error(stderr, fst); return FALSE;}
    // TODO: save FITS files in original (or given by user) data format!
    // check fits->dtype - does all data fits it
    fits_create_img(fp, fits->dtype, 2, naxes, &fst);
    if(fst){fits_report_error(stderr, fst); return FALSE;}
    if(fits->keylist){ // there's keys
        KeyList *records = fits->keylist;
        while(records){
            char *rec = records->record;
            records = records->next;
            // TODO: check types of headers from each record!
            if(strncmp(rec, "SIMPLE", 6) == 0 || strncmp(rec, "EXTEND", 6) == 0) // key "file does conform ..."
                continue;
                // comment of obligatory key in FITS head
            else if(strncmp(rec, "COMMENT   FITS", 14) == 0 || strncmp(rec, "COMMENT   and Astrophysics", 26) == 0)
                continue;
            else if(strncmp(rec, "NAXIS", 5) == 0 || strncmp(rec, "BITPIX", 6) == 0) // NAXIS, NAXISxxx, BITPIX
                continue;
            int ret = fits_write_record(fp, rec, &fst);
            if(fst){fits_report_error(stderr, fst);}
            else if(ret) WARNX(_("Can't write record %s"), rec);
        //  DBG("write key: %s", rec);
        }
    }
    //fits->lasthdu = 1;
    //FITSFUN(fits_write_record, fp, "COMMENT  modified by simple test routine");
    fits_write_img(fp, TDOUBLE, 1, sz, fits->data, &fst);
    if(fst){fits_report_error(stderr, fst); return FALSE;}
    if(fits->tables) table_write(fits, fp);
    fits_close_file(fp, &fst);
    if(fst){fits_report_error(stderr, fst);}*/
    return TRUE;
}


/**************************************************************************************
 *                                  FITS images                                       *
 **************************************************************************************/

void image_free(FITSimage **img){
    FREE((*img)->data);
    FREE(*img);
}

/**
 * create an empty image without headers, assign data type to "dtype"
 */
FITSimage *image_new(size_t h, size_t w, int dtype){
    size_t bufsiz = w*h;
    FITSimage *out = MALLOC(FITSimage, 1);
    // TODO: make allocation when reading!
    // TODO: allocate input data type ?
    out->data = MALLOC(double, bufsiz);
    out->width = w;
    out->height = h;
    out->dtype = dtype;
    return out;
}

/**
 * build IMAGE image from data array indata
 */
FITSimage *image_build(size_t h, size_t w, int dtype, uint8_t *indata){
    size_t stride = 0;
    double (*fconv)(uint8_t *data) = NULL;
    double ubyteconv(uint8_t *data){return (double)*data;}
    double ushortconv(uint8_t *data){return (double)*(int16_t*)data;}
    double ulongconv(uint8_t *data){return (double)*(uint32_t*)data;}
    double ulonglongconv(uint8_t *data){return (double)*(uint64_t*)data;}
    double floatconv(uint8_t *data){return (double)*(float*)data;}
    FITSimage *out = image_new(h, w, dtype);
    switch (dtype){
        case BYTE_IMG:
            stride = 1;
            fconv = ubyteconv;
        break;
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
}

/**
 * create an empty copy of image "in" without headers, assign data type to "dtype"
 */
FITSimage *image_mksimilar(FITSimage *img, int dtype){
    size_t w = img->width, h = img->height, bufsiz = w*h;
    FITSimage *out = MALLOC(FITSimage, 1);
    // TODO: allocate buffer as in original!
    out->data = MALLOC(double, bufsiz);
    out->width = w;
    out->height = h;
    out->dtype = dtype;
    return out;
}

/**
 * make full copy of image 'in'
 */
FITSimage *image_copy(FITSimage *in){
    FITSimage *out = image_mksimilar(in, in->dtype);
    // TODO: size of data as in original!
    memcpy(out->data, in->data, sizeof(double)*in->width*in->height);
    return out;
}

