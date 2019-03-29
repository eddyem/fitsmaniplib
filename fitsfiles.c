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
 *                                  FITS files                                        *
 **************************************************************************************/
/*
 * Functions for working with files I/O:
 * read/write/rewrite/modify
 */
/**
 * @brief FITS_addHDU - add new HDU to FITS file structure
 * @param fits (io) - fits file to use
 * @return pointer to new HDU or NULL in case of error
 */
FITSHDU *FITS_addHDU(FITS *fits){
    if(fits->NHDUs < 0) fits->NHDUs = 0;
    int hdunum = fits->NHDUs + 1;
    // add 1 to `hdunum` because HDU numbering starts @1
    FITSHDU *newhdu = realloc(fits->HDUs, sizeof(FITSHDU)*(1+hdunum));
    if(!newhdu){
        WARN("FITS_addHDU, realloc() failed");
        return NULL;
    }
    fits->HDUs = newhdu;
    fits->curHDU = &fits->HDUs[hdunum];
    fits->NHDUs = hdunum;
    return fits->curHDU;
}

/**
 * @brief FITS_free - delete FITS structure from memory
 * @param fits - address of FITS pointer
 */
void FITS_free(FITS **fits){
    if(!fits || !*fits) return;
    FITS *f = *fits;
    FREE(f->filename);
    int fst = 0;
    fits_close_file(f->fp, &fst);
    int n, N = f->NHDUs;
    for(n = 1; n < N; ++n){
        FITSHDU *hdu = &f->HDUs[n];
        if(!hdu) continue;
        keylist_free(&hdu->keylist);
        if(!hdu->contents.image) continue;
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
  TODO: what's about HCOMPRESS?
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
        FITS_reporterr(&fst);
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
                //curHDU->contents.table = table_read(fits);
            break;
            case ASCII_TBL:
                DBG("ASCII table");
                //curHDU->contents.table = table_read(fits);
            break;
            default:
                WARNX(_("Unknown HDU type"));
        }
    }
    if(fst == END_OF_FILE){
        fst = 0;
    }
returning:
    if(fst){
        FITS_reporterr(&fst);
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
            if(st){FITS_reporterr(&st); ret = FALSE;}
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
    if(fst){FITS_reporterr(&fst); return FALSE;}
    int N = fits->NHDUs;
    for(int i = 1; i <= N; ++i){
        FITSHDU *hdu = &fits->HDUs[i];
        if(!hdu) continue;
        FITSimage *img;
        KeyList *records = hdu->keylist;
        DBG("HDU #%d (type %d)", i, hdu->hdutype);
        switch(hdu->hdutype){
            case IMAGE_HDU:
                img = hdu->contents.image;
                if(!img && records){ // something wrong - just write keylist
                    DBG("create empty image with records");
                    fits_create_img(fp, SHORT_IMG, 0, NULL, &fst);
                    if(fst){FITS_reporterr(&fst); continue;}
                    keylist_write(records, fp);
                    DBG("OK");
                    continue;
                }
                DBG("create, bitpix: %d, naxis = %d, totpix = %ld", img->bitpix, img->naxis, img->totpix);
                fits_create_img(fp, img->bitpix, img->naxis, img->naxes, &fst);
                if(fst){FITS_reporterr(&fst); continue;}
                keylist_write(records, fp);
                DBG("OK, now write image");
                //int bscale = 1, bzero = 32768, status = 0;
                //fits_set_bscale(fp, bscale, bzero, &status);
                if(img && img->data){
                    DBG("bitpix: %d, dtype: %d", img->bitpix, img->dtype);
                    fits_write_img(fp, img->dtype, 1, img->totpix, img->data, &fst);
                    DBG("status: %d", fst);
                    if(fst){FITS_reporterr(&fst); continue;}
                }
            break;
            case BINARY_TBL:
            case ASCII_TBL:
                // TODO: save table
            break;
        }
    }

    fits_close_file(fp, &fst);
    if(fst){FITS_reporterr(&fst);}
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

/*
 * Different file functions
 */
/**
 * @brief file_absent - check whether the file exists
 * @param name - filename to check
 * @return TRUE if file _name_ not exists
 */
bool file_absent(char *name){
    struct stat filestat;
    if(!stat(name, &filestat)) return FALSE;
    if(errno == ENOENT) return TRUE;
    return FALSE;
}

/**
 * @brief make_filename - find the first non-existing filename
 * @param buff   - buffer for filename
 * @param buflen - its length (including trailing zero)
 * @param prefix - filename prefix
 * @param suffix - filename suffix (e.g. "fits", "fit", "fit.gz" etc)
 * @return
 */
char* make_filename(char *buff, size_t buflen, char *prefix, char *suffix){
    int num;
    for(num = 1; num < 10000; ++num){
        if(snprintf(buff, buflen, "%s_%04d.%s", prefix, num, suffix) < 1)
            return NULL;
        if(file_absent(buff)) // OK, file not exists
            return buff;
    }
    return NULL;
}
