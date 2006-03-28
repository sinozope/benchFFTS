/*
 * Copyright (c) 2001 Matteo Frigo
 * Copyright (c) 2001 Massachusetts Institute of Technology
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

/* $Id: report.c,v 1.11 2006-03-28 04:41:05 stevenj Exp $ */

#include "bench.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

void (*report)(const bench_problem *p, double *t, int st);

#undef min
#undef max /* you never know */

struct stats {
     double min;
     double max;
     double avg;
     double median;
};

static void mkstat(double *t, int st, struct stats *a)
{
     int i, j;
     
     a->min = t[0];
     a->max = t[0];
     a->avg = 0.0;

     for (i = 0; i < st; ++i) {
	  if (t[i] < a->min)
	       a->min = t[i];
	  if (t[i] > a->max)
	       a->max = t[i];
	  a->avg += t[i];
     }
     a->avg /= (double)st;

     /* compute median --- silly bubblesort algorithm */
     for (i = st - 1; i > 1; --i) {
	  for (j = 0; j < i - 1; ++j) {
	       double t0, t1;
	       if ((t0 = t[j]) > (t1 = t[j + 1])) {
		    t[j] = t0;
		    t[j + 1] = t1;
	       }
	  } 
     }
     a->median = t[st / 2];
}

void report_mflops(const bench_problem *p, double *t, int st)
{
     struct stats s;
     mkstat(t, st, &s);
     ovtpvt("(%g %g %g %g)\n", 
	    mflops(p, s.max), mflops(p, s.avg), 
	    mflops(p, s.min), mflops(p, s.median));
}

void report_time(const bench_problem *p, double *t, int st)
{
     struct stats s;
     UNUSED(p);
     mkstat(t, st, &s);
     ovtpvt("(%g %g %g %g)\n", s.min, s.avg, s.max, s.median);
}

void report_benchmark(const bench_problem *p, double *t, int st)
{
     struct stats s;
     mkstat(t, st, &s);
     ovtpvt("%.5g %.8g %g\n", mflops(p, s.min), s.min, p->setup_time);
}

static void sprintf_time(double x, char *buf, int buflen)
{
#ifdef HAVE_SNPRINTF
#  define BENCH_BUFARG buf, buflen
#else
#  define snprintf sprintf
#  define BENCH_BUFARG buf
#endif
     if (x < 1.0E-6)
	  snprintf(BENCH_BUFARG, "%.2f ns", x * 1.0E9);
     else if (x < 1.0E-3)
	  snprintf(BENCH_BUFARG, "%.2f us", x * 1.0E6);
     else if (x < 1.0)
	  snprintf(BENCH_BUFARG, "%.2f ms", x * 1.0E3);
     else
	  snprintf(BENCH_BUFARG, "%.2f s", x);
#undef BENCH_BUFARG
}

void report_verbose(const bench_problem *p, double *t, int st)
{
     struct stats s;
     char bmin[64], bmax[64], bavg[64], bmedian[64], btmin[64];
     char bsetup[64];
     int copyp = tensor_sz(p->sz) == 1;

     mkstat(t, st, &s);

     sprintf_time(s.min, bmin, 64);
     sprintf_time(s.max, bmax, 64);
     sprintf_time(s.avg, bavg, 64);
     sprintf_time(s.median, bmedian, 64);
     sprintf_time(time_min, btmin, 64);
     sprintf_time(p->setup_time, bsetup, 64);

     ovtpvt("Problem: %s, setup: %s, time: %s, %s: %.5g\n",
	    p->pstring, bsetup, bmin, 
	    copyp ? "fp-move/us" : "``mflops''",
	    mflops(p, s.min));

     if (verbose) {
	  ovtpvt("Took %d measurements for at least %s each.\n", st, btmin);
	  ovtpvt("Time: min %s, max %s, avg %s, median %s\n",
		 bmin, bmax, bavg, bmedian);
     }
}
