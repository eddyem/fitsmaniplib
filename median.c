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

// FOR MEDIATOR:
// Copyright (c) 2011 ashelly.myopenid.com under <http://www.opensource.org/licenses/mit-license>
// FOR opt_medXX:
// Copyright (c) 1998 Nicolas Devillard. Public domain.
// FOR qickselect:
// "Numerical recipes in C", Second Edition,
//  Cambridge University Press, 1992, Section 8.5, ISBN 0-521-43108-5
//  Code by Nicolas Devillard - 1998. Public domain.

// TODO: resolve problem with borders

#include "FITSmanip.h"
#include "local.h"

// largest radius for adaptive median filter
#define LARGEST_ADPMED_RADIUS  (3)

#define ELEM_SWAP(a, b) {register double t = a; a = b; b = t;}
#define PIX_SORT(a, b)  {if (p[a] > p[b]) ELEM_SWAP(p[a], p[b]);}

/*
 * simplest short functions for median calculation
 */
// even values are from "FAST, EFFICIENT MEDIAN FILTERS WITH EVEN LENGTH WINDOWS",
// J.P. HAVLICEK, K.A. SAKADY, G.R.KATZ
static double opt_med2(double *p){
	return (p[0] + p[1]) * 0.5;
}
static double opt_med3(double *p){
	PIX_SORT(0, 1); PIX_SORT(1, 2); PIX_SORT(0, 1);
	return (p[1]);
}
static double opt_med4(double *p){
	PIX_SORT(0, 2); PIX_SORT(1, 3);
	PIX_SORT(0, 1); PIX_SORT(2, 3);
	return (p[1] + p[2]) / 2.;
}
static double opt_med5(double *p){
	PIX_SORT(0, 1); PIX_SORT(3, 4); PIX_SORT(0, 3);
	PIX_SORT(1, 4); PIX_SORT(1, 2); PIX_SORT(2, 3) ;
	PIX_SORT(1, 2);
	return (p[2]);
}
static double opt_med6(double *p){
	PIX_SORT(1, 2); PIX_SORT(3, 4);
	PIX_SORT(0, 1); PIX_SORT(2, 3); PIX_SORT(4, 5);
	PIX_SORT(1, 2); PIX_SORT(3, 4);
	PIX_SORT(0, 1); PIX_SORT(2, 3); PIX_SORT(4, 5);
	PIX_SORT(1, 2); PIX_SORT(3, 4);
	return (p[2] + p[3]) / 2.;
}
static double opt_med7(double *p){
	PIX_SORT(0, 5); PIX_SORT(0, 3); PIX_SORT(1, 6);
	PIX_SORT(2, 4); PIX_SORT(0, 1); PIX_SORT(3, 5);
	PIX_SORT(2, 6); PIX_SORT(2, 3); PIX_SORT(3, 6);
	PIX_SORT(4, 5); PIX_SORT(1, 4); PIX_SORT(1, 3);
	PIX_SORT(3, 4);
    return (p[3]);
}
// optimal Batcher's sort for 8 elements (http://myopen.googlecode.com/svn/trunk/gtkclient_tdt/include/fast_median.h)
static double opt_med8(double *p){
	PIX_SORT(0, 4); PIX_SORT(1, 5); PIX_SORT(2, 6);
	PIX_SORT(3, 7); PIX_SORT(0, 2); PIX_SORT(1, 3);
	PIX_SORT(4, 6); PIX_SORT(5, 7); PIX_SORT(2, 4);
	PIX_SORT(3, 5); PIX_SORT(0, 1); PIX_SORT(2, 3);
	PIX_SORT(4, 5); PIX_SORT(6, 7); PIX_SORT(1, 4);
	PIX_SORT(3, 6);
	return (p[3] + p[4]) / 2.;
}
static double opt_med9(double *p){
	PIX_SORT(1, 2); PIX_SORT(4, 5); PIX_SORT(7, 8);
	PIX_SORT(0, 1); PIX_SORT(3, 4); PIX_SORT(6, 7);
	PIX_SORT(1, 2); PIX_SORT(4, 5); PIX_SORT(7, 8);
	PIX_SORT(0, 3); PIX_SORT(5, 8); PIX_SORT(4, 7);
	PIX_SORT(3, 6); PIX_SORT(1, 4); PIX_SORT(2, 5);
	PIX_SORT(4, 7); PIX_SORT(4, 2); PIX_SORT(6, 4);
	PIX_SORT(4, 2);
    return (p[4]);
}
static double opt_med16(double *p){
	PIX_SORT(0, 8); PIX_SORT(1, 9); PIX_SORT(2, 10); PIX_SORT(3, 11);
	PIX_SORT(4, 12); PIX_SORT(5, 13); PIX_SORT(6, 14); PIX_SORT(7, 15);
	PIX_SORT(0, 4); PIX_SORT(1, 5); PIX_SORT(2, 6); PIX_SORT(3, 7);
	PIX_SORT(8, 12); PIX_SORT(9, 13); PIX_SORT(10, 14); PIX_SORT(11, 15);
	PIX_SORT(4, 8); PIX_SORT(5, 9); PIX_SORT(6, 10); PIX_SORT(7, 11);
	PIX_SORT(0, 2); PIX_SORT(1, 3); PIX_SORT(4, 6); PIX_SORT(5, 7);
	PIX_SORT(8, 10); PIX_SORT(9, 11); PIX_SORT(12, 14); PIX_SORT(13, 15);
	PIX_SORT(2, 8); PIX_SORT(3, 9); PIX_SORT(6, 12); PIX_SORT(7, 13);
	PIX_SORT(2, 4); PIX_SORT(3, 5); PIX_SORT(6, 8); PIX_SORT(7, 9);
	PIX_SORT(10, 12); PIX_SORT(11, 13); PIX_SORT(0, 1); PIX_SORT(2, 3);
	PIX_SORT(4, 5); PIX_SORT(6, 7); PIX_SORT(8, 9); PIX_SORT(10, 11);
	PIX_SORT(12, 13); PIX_SORT(14, 15); PIX_SORT(1, 8); PIX_SORT(3, 10);
	PIX_SORT(5, 12); PIX_SORT(7, 14); PIX_SORT(5, 8); PIX_SORT(7, 10);
	return (p[7] + p[8]) / 2.;
}
static double opt_med25(double *p){
	PIX_SORT(0, 1)  ; PIX_SORT(3, 4)  ; PIX_SORT(2, 4) ;
	PIX_SORT(2, 3)  ; PIX_SORT(6, 7)  ; PIX_SORT(5, 7) ;
	PIX_SORT(5, 6)  ; PIX_SORT(9, 10) ; PIX_SORT(8, 10) ;
	PIX_SORT(8, 9)  ; PIX_SORT(12, 13); PIX_SORT(11, 13) ;
	PIX_SORT(11, 12); PIX_SORT(15, 16); PIX_SORT(14, 16) ;
	PIX_SORT(14, 15); PIX_SORT(18, 19); PIX_SORT(17, 19) ;
	PIX_SORT(17, 18); PIX_SORT(21, 22); PIX_SORT(20, 22) ;
	PIX_SORT(20, 21); PIX_SORT(23, 24); PIX_SORT(2, 5) ;
	PIX_SORT(3, 6)  ; PIX_SORT(0, 6)  ; PIX_SORT(0, 3) ;
	PIX_SORT(4, 7)  ; PIX_SORT(1, 7)  ; PIX_SORT(1, 4) ;
	PIX_SORT(11, 14); PIX_SORT(8, 14) ; PIX_SORT(8, 11) ;
	PIX_SORT(12, 15); PIX_SORT(9, 15) ; PIX_SORT(9, 12) ;
	PIX_SORT(13, 16); PIX_SORT(10, 16); PIX_SORT(10, 13) ;
	PIX_SORT(20, 23); PIX_SORT(17, 23); PIX_SORT(17, 20) ;
	PIX_SORT(21, 24); PIX_SORT(18, 24); PIX_SORT(18, 21) ;
	PIX_SORT(19, 22); PIX_SORT(8, 17) ; PIX_SORT(9, 18) ;
	PIX_SORT(0, 18) ; PIX_SORT(0, 9)  ; PIX_SORT(10, 19) ;
	PIX_SORT(1, 19) ; PIX_SORT(1, 10) ; PIX_SORT(11, 20) ;
	PIX_SORT(2, 20) ; PIX_SORT(2, 11) ; PIX_SORT(12, 21) ;
	PIX_SORT(3, 21) ; PIX_SORT(3, 12) ; PIX_SORT(13, 22) ;
	PIX_SORT(4, 22) ; PIX_SORT(4, 13) ; PIX_SORT(14, 23) ;
	PIX_SORT(5, 23) ; PIX_SORT(5, 14) ; PIX_SORT(15, 24) ;
	PIX_SORT(6, 24) ; PIX_SORT(6, 15) ; PIX_SORT(7, 16) ;
	PIX_SORT(7, 19) ; PIX_SORT(13, 21); PIX_SORT(15, 23) ;
	PIX_SORT(7, 13) ; PIX_SORT(7, 15) ; PIX_SORT(1, 9) ;
	PIX_SORT(3, 11) ; PIX_SORT(5, 17) ; PIX_SORT(11, 17) ;
	PIX_SORT(9, 17) ; PIX_SORT(4, 10) ; PIX_SORT(6, 12) ;
	PIX_SORT(7, 14) ; PIX_SORT(4, 6)  ; PIX_SORT(4, 7) ;
	PIX_SORT(12, 14); PIX_SORT(10, 14); PIX_SORT(6, 7) ;
	PIX_SORT(10, 12); PIX_SORT(6, 10) ; PIX_SORT(6, 17) ;
	PIX_SORT(12, 17); PIX_SORT(7, 17) ; PIX_SORT(7, 10) ;
	PIX_SORT(12, 18); PIX_SORT(7, 12) ; PIX_SORT(10, 18) ;
	PIX_SORT(12, 20); PIX_SORT(10, 20); PIX_SORT(10, 12) ;
	return (p[12]);
}
#undef PIX_SORT

#define PIX_SORT(a, b)  {if (a > b) ELEM_SWAP(a, b);}
/**
 * @brief quick_select - algorithm for approximate median calculation for array idata of size n
 * @param idata (i) - input data array
 * @param n - size of `idata`
 * @return median value
 */
double quick_select(const double *idata, int n){
	int low, high;
	int median;
	int middle, ll, hh;
	double *arr = MALLOC(double, n);
	memcpy(arr, idata, n*sizeof(double));
	low = 0 ; high = n-1 ; median = (low + high) / 2;
	for(;;){
		if(high <= low) // One element only
			break;
		if(high == low + 1){ // Two elements only
			PIX_SORT(arr[low], arr[high]) ;
			break;
		}
		// Find median of low, middle and high doubles; swap into position low
		middle = (low + high) / 2;
		PIX_SORT(arr[middle], arr[high]) ;
		PIX_SORT(arr[low], arr[high]) ;
		PIX_SORT(arr[middle], arr[low]) ;
		// Swap low double (now in position middle) into position (low+1)
		ELEM_SWAP(arr[middle], arr[low+1]) ;
		// Nibble from each end towards middle, swapping doubles when stuck
		ll = low + 1;
		hh = high;
		for(;;){
			do ll++; while (arr[low] > arr[ll]);
			do hh--; while (arr[hh] > arr[low]);
			if(hh < ll) break;
			ELEM_SWAP(arr[ll], arr[hh]) ;
		}
		// Swap middle double (in position low) back into correct position
		ELEM_SWAP(arr[low], arr[hh]) ;
		// Re-set active partition
		if (hh <= median) low = ll;
		if (hh >= median) high = hh - 1;
	}
	double ret = arr[median];
	FREE(arr);
	return ret;
}
#undef PIX_SORT
#undef ELEM_SWAP

/**
 * @brief calc_median - calculate median of array idata with size n
 *      the specific type of algorythm is choosen according to `n`
 * @param idata (i) - input data array
 * @param n - size of array `idata`
 * @return median value
 */
double calc_median(const double *idata, int n){
    if(!idata || n < 1){
        WARNX(_("Wrong parameters"));
        return 0.;
    }
	typedef double (*medfunc)(double *p);
	medfunc fn = NULL;
	const medfunc fnarr[] = {opt_med2, opt_med3, opt_med4, opt_med5, opt_med6,
			opt_med7, opt_med8, opt_med9};
	if(n == 1) return *idata;
	if(n < 10) fn = fnarr[n - 2];
	else if(n == 16) fn = opt_med16;
	else if(n == 25) fn = opt_med25;
	if(fn){
        // copy data to new buffer - `idata` should leave unchanged
        double *dataarr = MALLOC(double, n);
        memcpy(dataarr, idata, sizeof(double)*n);
        double medval = fn(dataarr);
        FREE(dataarr);
		return medval;
	}else{
		return quick_select(idata, n);
	}
}

#define doubleLess(a,b) ((a)<(b))
#define doubleMean(a,b) (((a)+(b))/2)

typedef struct Mediator_t{
	double* data; // circular queue of values
	int* pos;   // index into `heap` for each value
	int* heap;  // max/median/min heap holding indexes into `data`.
	int N;      // allocated size.
	int idx;    // position in circular queue
	int ct;     // count of doubles in queue
} Mediator;

/*--- Helper Functions ---*/

#define minCt(m) (((m)->ct-1)/2) //count of doubles in minheap
#define maxCt(m) (((m)->ct)/2) //count of doubles in maxheap

//returns 1 if heap[i] < heap[j]
static inline int mmless(Mediator* m, int i, int j){
	return doubleLess(m->data[m->heap[i]],m->data[m->heap[j]]);
}

//swaps doubles i&j in heap, maintains indexes
static inline int mmexchange(Mediator* m, int i, int j){
	int t = m->heap[i];
	m->heap[i] = m->heap[j];
	m->heap[j] = t;
	m->pos[m->heap[i]] = i;
	m->pos[m->heap[j]] = j;
	return 1;
}

//swaps doubles i&j if i<j; returns true if swapped
static inline int mmCmpExch(Mediator* m, int i, int j){
	return (mmless(m,i,j) && mmexchange(m,i,j));
}

//maintains minheap property for all doubles below i/2.
static void minSortDown(Mediator* m, int i){
	for(; i <= minCt(m); i*=2){
		if(i>1 && i < minCt(m) && mmless(m, i+1, i)) ++i;
		if(!mmCmpExch(m,i,i/2)) break;
	}
}

//maintains maxheap property for all doubles below i/2. (negative indexes)
static void maxSortDown(Mediator* m, int i){
	for(; i >= -maxCt(m); i*=2){
		if(i<-1 && i > -maxCt(m) && mmless(m, i, i-1)) --i;
	if(!mmCmpExch(m,i/2,i)) break;
	}
}

//maintains minheap property for all doubles above i, including median
//returns true if median changed
static int minSortUp(Mediator* m, int i){
	while (i > 0 && mmCmpExch(m, i, i/2)) i /= 2;
	return (i == 0);
}

//maintains maxheap property for all doubles above i, including median
//returns true if median changed
static int maxSortUp(Mediator* m, int i){
	while (i < 0 && mmCmpExch(m, i/2, i)) i /= 2;
	return (i == 0);
}

/*--- Public Interface ---*/

//creates new Mediator: to calculate `ndoubles` running median.
//mallocs single block of memory, caller must free.
static Mediator* MediatorNew(int ndoubles){
	int size = sizeof(Mediator) + ndoubles*(sizeof(double)+sizeof(int)*2);
	Mediator* m = malloc(size);
	m->data = (double*)(m + 1);
	m->pos = (int*) (m->data + ndoubles);
	m->heap = m->pos + ndoubles + (ndoubles / 2); //points to middle of storage.
	m->N = ndoubles;
	m->ct = m->idx = 0;
	while (ndoubles--){ //set up initial heap fill pattern: median,max,min,max,...
		m->pos[ndoubles] = ((ndoubles+1)/2) * ((ndoubles&1)? -1 : 1);
		m->heap[m->pos[ndoubles]] = ndoubles;
	}
	return m;
}

//Inserts double, maintains median in O(lg ndoubles)
static void MediatorInsert(Mediator* m, double v){
	int isNew=(m->ct<m->N);
	int p = m->pos[m->idx];
	double old = m->data[m->idx];
	m->data[m->idx]=v;
	m->idx = (m->idx+1) % m->N;
	m->ct+=isNew;
	if(p>0){ //new double is in minHeap
		if (!isNew && doubleLess(old,v)) minSortDown(m,p*2);
		else if (minSortUp(m,p)) maxSortDown(m,-1);
	}else if (p<0){ //new double is in maxheap
		if (!isNew && doubleLess(v,old)) maxSortDown(m,p*2);
		else if (maxSortUp(m,p)) minSortDown(m, 1);
	}else{ //new double is at median
		if (maxCt(m)) maxSortDown(m,-1);
		if (minCt(m)) minSortDown(m, 1);
	}
}

//returns median double (or average of 2 when double count is even)
static double MediatorMedian(Mediator* m){
	double v = m->data[m->heap[0]];
	if ((m->ct&1) == 0) v = doubleMean(v, m->data[m->heap[-1]]);
	return v;
}
/*
// median + min/max
static double MediatorStat(Mediator* m, double *minval, double *maxval){
	double v= m->data[m->heap[0]];
	if ((m->ct&1) == 0) v = doubleMean(v,m->data[m->heap[-1]]);
	double min = v, max = v;
	int i;
	for(i = -maxCt(m); i < 0; ++i){
		int v = m->data[m->heap[i]];
		if(v < min) min = v;
	}
	*minval = min;
	for(i = 1; i <= minCt(m); ++i){
		int v = m->data[m->heap[i]];
		if(v > max) max = v;
	}
	*maxval = max;
	return v;
}*/

// TODO: add adaptive filtering
/**
 * @brief get_adp_median_cross - adaptive median filter by cross 3x3
 * We have 5 datapoints and 4 inserts @ each step, so it's better to use opt_med5 instead of Mediator
 * @param img (i) - input image
 * @param out (o) - output image (allocated outside)
 * @param adp - TRUE for adaptive filtering and FALSE for regular
 */
static void get_adp_median_cross(const doubleimage *img, doubleimage *out, _U_ bool adp){
	size_t w = img->width, h = img->height;
	double *med = out->data, *inputima = img->data, *iptr;
#ifdef EBUG
	double t0 = dtime();
#endif
	OMP_FOR()
	for(size_t x = 1; x < w - 1; ++x){
		double buffer[5];
		size_t curpix = x + w, // index of current pixel image arrays
			y, ymax = h - 1;
		for(y = 1; y < ymax; ++y, curpix += w){
			double md, *I = &inputima[curpix]; //, Ival = *I;
			memcpy(buffer, I - 1, 3*sizeof(double));
			buffer[3] = I[-w]; buffer[4] = I[w];
			md = opt_med5(buffer);
            /*
			if(adp){
				double s, l;
				s = DBL_EPSILON + MIN(buffer[0], buffer[1]);
				l = MAX(buffer[3], buffer[4]) - DBL_EPSILON;
				if(s < md && md < l){
					if(s < Ival && Ival < l) med[curpix] = Ival;
					else med[curpix] = md;
				}else{
					med[curpix] = adp_med_5by5(img, x, y);
				}
			}else */
				med[curpix] = md;
		}
	}
	// process borders & corners (without adaptive)
	double buf[5];
	// left top
	buf[0] = inputima[0]; buf[1] = inputima[0];
	buf[2] = inputima[1]; buf[3] = inputima[w];
	buf[4] = inputima[w + 1];
	med[0] = opt_med5(buf);
	// right top
	iptr = &inputima[w - 1];
	buf[0] = iptr[0]; buf[1] = iptr[0];
	buf[2] = iptr[-1]; buf[3] = iptr[w - 1];
	buf[4] = iptr[w];
	med[w - 1] = opt_med5(buf);
	// left bottom
	iptr = &inputima[(h - 1) * w];
	buf[0] = iptr[0]; buf[1] = iptr[0];
	buf[2] = iptr[-w]; buf[3] = iptr[1 - w];
	buf[4] = iptr[1];
	med[(h - 1) * w] = opt_med5(buf);
	// right bottom
	iptr = &inputima[h * w - 1];
	buf[0] = iptr[0]; buf[1] = iptr[0];
	buf[2] = iptr[-w-1]; buf[3] = iptr[-w];
	buf[4] = iptr[-1];
	med[h * w - 1] = opt_med5(buf);
	// process borders without corners
	// top
	OMP_FOR(shared(med))
	for(size_t x = 1; x < w - 1; ++x){
		double *iptr = &inputima[x];
		buf[0] = buf[1] = *iptr;
		buf[2] = iptr[-1]; buf[3] = iptr[2];
		buf[4] = iptr[w];
		med[x] = opt_med5(buf);
	}
	// bottom
	size_t curidx = (h-2)*w;
	OMP_FOR(shared(curidx, med))
	for(size_t x = 1; x < w - 1; --x){
		double *iptr = &inputima[curidx + x];
		buf[0] = buf[1] = *iptr;
		buf[2] = iptr[-w]; buf[3] = iptr[-1];
		buf[4] = iptr[1];
		med[curidx + x] = opt_med5(buf);
	}
	// left
	OMP_FOR(shared(med))
	for(size_t y = 1; y < h - 1; ++y){
		size_t cur = y * w;
		double *iptr = &inputima[cur];
		buf[0] = buf[1] = *iptr;
		buf[2] = iptr[-w]; buf[3] = iptr[1];
		buf[4] = iptr[w];
		med[cur] = opt_med5(buf);
	}
	// right
	curidx = w - 1;
	OMP_FOR(shared(curidx, med))
	for(size_t y = 1; y < h - 1; ++y){
		size_t cur = curidx + y * w;
		double *iptr = &inputima[cur];
		buf[0] = buf[1] = *iptr;
		buf[2] = iptr[-w]; buf[3] = iptr[-1];
		buf[4] = iptr[w];
		med[cur] = opt_med5(buf);
	}
	DBG("time for median filtering by cross 3x3 of image %zdx%zd: %gs", w, h,
		dtime() - t0);
}

// TODO: add borders and corners
/**
 * @brief get_median - filter image by median (radius*2 + 1) x (radius*2 + 1)
 * @param img (i) - input image
 * @param radius  - zone radius (0 for cross 3x3)
 * @return image filtered by median (allocated here)
 */
doubleimage *get_median(const doubleimage *img, size_t radius){
	size_t w = img->width, h = img->height;
    doubleimage *out = doubleimage_new(img->width, img->height);
    if(!out){
        WARNX(_("Can't create output image"));
        return NULL;
    }
    memcpy(out->data, img->data, sizeof(double)*img->totpix);
	double *med = out->data, *inputima = img->data;
	if(radius == 0){
		get_adp_median_cross(img, out, 0);
		return out;
	}

	size_t blksz = radius * 2 + 1, fullsz = blksz * blksz;
#ifdef EBUG
	double t0 = dtime();
#endif
	OMP_FOR(shared(inputima, med))
	for(size_t x = radius; x < w - radius; ++x){
		size_t xx, yy, xm = x + radius + 1, y, ymax = blksz - 1, xmin = x - radius;
		Mediator* m = MediatorNew(fullsz);
		// initial fill
		for(yy = 0; yy < ymax; ++yy)
			for(xx = xmin; xx < xm; ++xx)
				MediatorInsert(m, inputima[xx + yy*w]);
		ymax = 2*radius*w;
		xmin += ymax;
		xm += ymax;
		ymax = h - radius;
		size_t medidx = x + radius * w;
		for(y = radius; y < ymax; ++y, xmin += w, xm += w, medidx += w){
			for(xx = xmin; xx < xm; ++xx)
				MediatorInsert(m, inputima[xx]);
			med[medidx] = MediatorMedian(m);
		}
		FREE(m);
	}
	DBG("time for median filtering %zdx%zd of image %zdx%zd: %gs", blksz, blksz, w, h,
		dtime() - t0);
	return out;
}

#if 0

/**
 * procedure for finding median value in window 5x5
 * PROBLEM: bounds
 */
static double adp_med_5by5(const IMAGE *img, size_t x, size_t y){
	size_t blocklen, w = img->width, h = img->height, yy, _2w = 2 * w;
	double arr[25], *arrptr = arr, *dataptr, *currpix;
	int position = ((x < 1) ? 1 : 0)        // left columns
				 + ((x > w - 2) ? 2 : 0)    // right columns
				 + ((y < 1) ? 4 : 0)        // top rows
				 + ((y > w - 2) ? 8 : 0);   // bottom rows
	/* Now by value of "position" we know where is the point:
	 ***************************
	 * 5 *        4        * 6 *
	 ***************************
	 *   *                 *   *
	 *   *                 *   *
	 * 1 *        0        * 2 *
	 *   *                 *   *
	 *   *                 *   *
	 ***************************
	 * 9 *        8        *10 *
	 ***************************/
	currpix = &img->data[x + y * w]; // pointer to current pixel
	dataptr = currpix - _2w - 2;     // pointer to left upper corner of 5x5 square
	inline void copy5times(double val){
		for(int i = 0; i < 5; ++i) *arrptr++ = val;
	}
	inline void copy9times(double val){
		for(int i = 0; i < 9; ++i) *arrptr++ = val;
	}
	void copycolumn(double *startpix){
		for(int i = 0; i < 5; ++i, startpix += w) *arrptr++ = *startpix;
	}
	inline void copyvertblock(size_t len){
		for(int i = 0; i < 5; ++i, dataptr += w, arrptr += len)
			memcpy(arrptr, dataptr, len * sizeof(double));
	}
	inline void copyhorblock(size_t len){
		for(size_t i = 0; i < len; ++i, dataptr += w, arrptr += 5)
			memcpy(arrptr, dataptr, 5 * sizeof(double));
	}
	inline void copyblock(){
		for(size_t i = 0; i < 4; ++i, dataptr += w, arrptr += 4)
			memcpy(arrptr, dataptr, 4 * sizeof(double));
	}
	switch(position){
		case 1: // left
			copy5times(*currpix); // make 5 copies of current pixel
			if(x == 0){ // copy 1st column too
				dataptr += 2;
				copycolumn(dataptr);
				blocklen = 3;
			}else{ // 2nd column - no copy need
				++dataptr;
				blocklen = 4;
			}
			copyvertblock(blocklen);
		break;
		case 2: // right
			copy5times(*currpix);
			if(x == w - 1){ // copy last column too
				copycolumn(dataptr + 2);
				blocklen = 3;
			}else{ // 2nd column - no copy need
				blocklen = 4;
			}
			copyvertblock(blocklen);
		break;
		case 4: // top
			copy5times(*currpix);
			if(y == 0){
				dataptr += _2w;
				memcpy(arrptr, dataptr, 5 * sizeof(double));
				blocklen = 3;
			}else{
				dataptr += w;
				blocklen = 4;
			}
			copyhorblock(blocklen);
		break;
		case 8: // bottom
			copy5times(*currpix);
			if(y == h - 1){
				memcpy(arrptr, dataptr + _2w, 5 * sizeof(double));
				blocklen = 3;
			}else{
				blocklen = 4;
			}
			copyhorblock(blocklen);
		break;
		case 5: // top left corner: in all corners we just copy 4x4 square & 9 times this pixel
			copy9times(*currpix);
			dataptr = img->data;
			copyblock();
		break;
		case 6: // top right corner
			copy9times(*currpix);
			dataptr = &img->data[w - 4];
			copyblock();
		break;
		case 9: // bottom left cornet
			copy9times(*currpix);
			dataptr = &img->data[(y - 4) * w];
			copyblock();
		break;
		case 10: // bottom right cornet
			copy9times(*currpix);
			dataptr = &img->data[(y - 3) * w - 4];
			copyblock();
		break;
		default:  // 0
			for(yy = 0; yy < 5; ++yy, dataptr += w, arrptr += 5)
				memcpy(arrptr, dataptr, 5*sizeof(double));
	}
	return opt_med25(arr);
}


/**
 * filter image by median (radius*2 + 1) x (radius*2 + 1)
 */
doubleimage *get_adaptive_median(const doubleimage *img, size_t radius);{
	int radius = f->w;
	size_t w = img->width, h = img->height, siz = w*h, bufsiz = siz*sizeof(double);
	IMAGE *out = similarFITS(img, img->dtype);
	double *med = out->data, *inputima = img->data;
	memcpy(med, inputima, bufsiz);
	if(radius == 0){
		get_adp_median_cross(img, out, 1);
		return out;
	}
	size_t blksz = radius * 2 + 1, fullsz = blksz * blksz;
#ifdef EBUG
	double t0 = dtime();
#endif
	OMP_FOR(shared(inputima, med))
	for(size_t x = radius; x < w - radius; ++x){
		size_t xx, yy, xm = x + radius + 1, y, ymax = blksz - 1, xmin = x - radius;
		Mediator* m = MediatorNew(fullsz);
		// initial fill
		for(yy = 0; yy < ymax; ++yy)
			for(xx = xmin; xx < xm; ++xx)
				MediatorInsert(m, inputima[xx + yy*w]);
		ymax = 2*radius*w;
		xmin += ymax;
		xm += ymax;
		ymax = h - radius;
		size_t curpos = x + radius * w;
		for(y = radius; y < ymax; ++y, xmin += w, xm += w, curpos += w){
			for(xx = xmin; xx < xm; ++xx)
				MediatorInsert(m, inputima[xx]);
			double s, l, md, I = inputima[curpos];
			md = MediatorStat(m, &s, &l);
			s += ITM_EPSILON, l -= ITM_EPSILON;
			if(s < md && md < l){
				if(s < I && I < l) med[curpos] = I;
				else med[curpos] = md;
			}else{
				if(radius > LARGEST_ADPMED_RADIUS)
					med[curpos] = I;
				else
					med[curpos] = adp_med_5by5(img, x, y);
			}
		}
		FREE(m);
	}
	DBG("time for adadptive median filtering %zdx%zd of image %zdx%zd: %gs", blksz, blksz, w, h,
		dtime() - t0);
	return out;
}
#endif
