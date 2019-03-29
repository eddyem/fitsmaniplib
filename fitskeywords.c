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
 *                                  FITS keywords                                     *
 **************************************************************************************/
/*
 * The functionality for working with keywords: reading/writing/parsing
 */
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
            FITS_reporterr(&st);
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
    //DBG("try to find %s", key);
    do{
        if(list->record){
            if(strncasecmp(list->record, key, L) == 0){ // key found
                //DBG("found:\n%s", list->record);
                return list;
            }
        }
        list = list->next;
    }while(list);
    DBG("key %s not found", key);
    return NULL;
}

/**
 * @brief record_get_keyval find given key name in record & return its value
 * @param r    (i) - record
 * @param comm (o) - comment (maybe NULL)
 * @return value of key found or NULL; this value allocated, so you should use `free`
 */
char *record_get_keyval(char *r, char **comm){
    int st = 0;
    char val[FLEN_VALUE], cmnt[FLEN_COMMENT];
    fits_parse_value(r, val, cmnt, &st);
    if(st){
        WARNX(_("Can't get value & comment"));
        FITS_reporterr(&st);
        return NULL;
    }
    if(comm && *comm) *comm = strdup(cmnt);
    char *rtn = val;
    if(*val == '\''){
        ++rtn;
        char *e = rtn;
        while(*e) ++e;
        while(e > rtn){
            if(*e == '\''){
                *e = 0;
                break;
            }
            --e;
        }
    }
    //DBG("val: %s, comment: %s", rtn, cmnt);
    return strdup(rtn);
    /*
    char *omitwsp(char *s){while(*s && (*s == ' ' || *s == '\t')) ++s; return s;}
    char *xrec = strdup(r), *rec = xrec;
    if(!rec) return NULL;
    char *eq = strchr(rec, '=');
    if(!eq || !*(++eq)){FREE(rec); return NULL;}
    char *bgn = omitwsp(eq);
    char *e = strchr(bgn, '/');
    if(*bgn == '\''){ // string
        ++bgn;
        bgn = omitwsp(bgn);
        e = bgn+1;
        int meet = 0;
        while(*e){
            if(*e == '\''){
                if(meet){ meet = 0; continue; }
                else meet = 1;
            }else{
                if(meet){
                    --e;
                    break;
                }
            }
            ++e;
        }
    }else if(e){
        if(*e == '/') --e;
        while(e > bgn){
            if(*e == ' ' || *e == '\t') --e;
            else break;
        }
        ++e;
    }
    if(e) *e = 0;
    char *result = strdup(bgn);
    FREE(xrec);
    return result;
    */
}

/**
 * @brief keylist_find_keyval find given key name & return its value
 * @param l    - pointer to keylist
 * @param key  - key to find
 * @param comment - comment (maybe NULL if don't need)
 * @return value of key found or NULL; this value should be free'd
 */
char *keylist_find_keyval(KeyList *l, char *key, char **comment){
    KeyList *kl = keylist_find_key(l, key);
    if(!kl) return NULL;
    char *rec = kl->record;
    return record_get_keyval(rec, comment);
}

/**
 * @brief keylist_modify_key -  modify key value
 * @param list (i)   - pointer to first list element
 * @param key (i)    - key name
 * @param newval (i) - new value of given key
 * @return modified record or NULL if given key is absent
 */
KeyList *keylist_modify_key(KeyList *list, char *key, char *newval){
    // TODO: add comments!
    // TODO: look modhead.c in cexamples for protected keys
    char buf[FLEN_CARD], test[2*FLEN_CARD];
    KeyList *rec = keylist_find_key(list, key);
    if(!rec) return NULL;
    snprintf(test, 2*FLEN_CARD, "%s = %s", key, newval);
    int tp, st = 0;
    fits_parse_template(test, buf, &tp, &st);
    if(st){
        FITS_reporterr(&st);
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
        FITS_reporterr(&fst);
        return NULL;
    }
    DBG("Find %d keys, keypos=%d", nkeys, keypos);
    for(int j = 1; j <= nkeys; ++j){
        char card[FLEN_CARD];
        fits_read_record(fits->fp, j, card, &fst);
        if(fst) FITS_reporterr(&fst);
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
