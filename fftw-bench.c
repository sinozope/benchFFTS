/* See bench.c.  We keep a few common subroutines in this file so
   that they can be re-used in the MPI test program. */

#include <math.h>
#include <stdio.h>
#include <string.h>
#include "fftw-bench.h"

#if defined(HAVE_THREADS) || defined(HAVE_OPENMP)
#define HAVE_SMP
#endif

FFTW(plan) the_plan = 0;

static const char *wisdat = "wis.dat";
unsigned the_flags = 0;
int paranoid = 0;
int usewisdom = 0;
int havewisdom = 0;
int nthreads = 1;
int amnesia = 0;

extern void install_hook(void);  /* in hook.c */
extern void uninstall_hook(void);  /* in hook.c */

void useropt(const char *arg)
{
     int x;
     double y;

     if (!strcmp(arg, "patient")) the_flags |= FFTW_PATIENT;
     else if (!strcmp(arg, "estimate")) the_flags |= FFTW_ESTIMATE;
     else if (!strcmp(arg, "estimatepat")) the_flags |= FFTW_ESTIMATE_PATIENT;
     else if (!strcmp(arg, "exhaustive")) the_flags |= FFTW_EXHAUSTIVE;
     else if (!strcmp(arg, "unaligned")) the_flags |= FFTW_UNALIGNED;
     else if (!strcmp(arg, "nosimd")) the_flags |= FFTW_NO_SIMD;
     else if (!strcmp(arg, "noindirectop")) the_flags |= FFTW_NO_INDIRECT_OP;
     else if (sscanf(arg, "flag=%d", &x) == 1) the_flags |= x;
     else if (sscanf(arg, "bflag=%d", &x) == 1) the_flags |= 1U << x;
     else if (!strcmp(arg, "paranoid")) paranoid = 1;
     else if (!strcmp(arg, "wisdom")) usewisdom = 1;
     else if (!strcmp(arg, "amnesia")) amnesia = 1;
     else if (sscanf(arg, "nthreads=%d", &x) == 1) nthreads = x;
     else if (sscanf(arg, "timelimit=%lg", &y) == 1) {
	  FFTW(set_timelimit)(y);
     }

     else fprintf(stderr, "unknown user option: %s.  Ignoring.\n", arg);
}

void rdwisdom(void)
{
     FILE *f;
     double tim;
     int success = 0;

     if (havewisdom) return;

#ifdef HAVE_SMP
     BENCH_ASSERT(FFTW(init_threads)());
     FFTW(plan_with_nthreads)(nthreads);
#endif

     if (!usewisdom) return;

     timer_start(USER_TIMER);
     if ((f = fopen(wisdat, "r"))) {
	  if (!import_wisdom(f))
	       fprintf(stderr, "bench: ERROR reading wisdom\n");
	  else
	       success = 1;
	  fclose(f);
     }
     tim = timer_stop(USER_TIMER);

     if (success) {
	  if (verbose > 1) printf("READ WISDOM (%g seconds): ", tim);
	  
	  if (verbose > 3)
	       export_wisdom(stdout);
	  if (verbose > 1)
	       printf("\n");
     }
     havewisdom = 1;
}

void wrwisdom(void)
{
     FILE *f;
     double tim;
     if (!havewisdom) return;

     timer_start(USER_TIMER);
     if ((f = fopen(wisdat, "w"))) {
	  export_wisdom(f);
	  fclose(f);
     }
     tim = timer_stop(USER_TIMER);
     if (verbose > 1) printf("write wisdom took %g seconds\n", tim);
}

static unsigned preserve_input_flags(bench_problem *p)
{
     /*
      * fftw3 cannot preserve input for multidimensional c2r transforms.
      * Enforce FFTW_DESTROY_INPUT
      */
     if (p->kind == PROBLEM_REAL && 
	 p->sign > 0 && 
	 !p->in_place && 
	 p->sz->rnk > 1)
	  p->destroy_input = 1;

     if (p->destroy_input)
	  return FFTW_DESTROY_INPUT;
     else
	  return FFTW_PRESERVE_INPUT;
}

int can_do(bench_problem *p)
{
     double tim;

     if (verbose > 2 && p->pstring)
	  printf("Planning %s...\n", p->pstring);
     rdwisdom();

     timer_start(USER_TIMER);
     the_plan = mkplan(p, preserve_input_flags(p) | the_flags | FFTW_ESTIMATE);
     tim = timer_stop(USER_TIMER);
     if (verbose > 2) printf("estimate-planner time: %g s\n", tim);

     if (the_plan) {
	  FFTW(destroy_plan)(the_plan);
	  return 1;
     }
     return 0;
}

void setup(bench_problem *p)
{
     double tim;

     if (amnesia)
	  FFTW(forget_wisdom)();

     /* Regression test: check that fftw_malloc exists and links
      * properly */
     FFTW(free(FFTW(malloc(42))));

     rdwisdom();
     install_hook();

#ifdef HAVE_SMP
     if (verbose > 1 && nthreads > 1) printf("NTHREADS = %d\n", nthreads);
#endif

     timer_start(USER_TIMER);
     the_plan = mkplan(p, preserve_input_flags(p) | the_flags);
     tim = timer_stop(USER_TIMER);
     if (verbose > 1) printf("planner time: %g s\n", tim);

     BENCH_ASSERT(the_plan);
     
     {
	  double add, mul, nfma, cost;
	  FFTW(flops)(the_plan, &add, &mul, &nfma);
	  cost = FFTW(estimate_cost)(the_plan);
	  if (verbose > 1) {
	       FFTW(print_plan)(the_plan);
	       printf("\n");
	       printf("flops: %0.0f add, %0.0f mul, %0.0f fma\n",
		      add, mul, nfma);
	       printf("estimated cost: %f\n", cost);
	  }
     }
}


void doit(int iter, bench_problem *p)
{
     int i;
     FFTW(plan) q = the_plan;

     UNUSED(p);
     for (i = 0; i < iter; ++i) 
	  FFTW(execute)(q);
}

void done(bench_problem *p)
{
     UNUSED(p);

     FFTW(destroy_plan)(the_plan);
     uninstall_hook();
}

void cleanup(void)
{
     initial_cleanup();

     wrwisdom();
#ifdef HAVE_SMP
     FFTW(cleanup_threads)();
#else
     FFTW(cleanup)();
#endif

#    ifdef FFTW_DEBUG_MALLOC
     {
	  /* undocumented memory checker */
	  FFTW_EXTERN void FFTW(malloc_print_minfo)(int v);
	  FFTW(malloc_print_minfo)(verbose);
     }
#    endif

     final_cleanup();
}
