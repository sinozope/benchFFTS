/*

 This file is part of FFTS -- The Fastest Fourier Transform in the South

 Copyright (c) 2012, Anthony M. Blake <amb@anthonix.com>
 Copyright (c) 2012, The University of Waikato

 All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:
 	* Redistributions of source code must retain the above copyright
 		notice, this list of conditions and the following disclaimer.
 	* Redistributions in binary form must reproduce the above copyright
 		notice, this list of conditions and the following disclaimer in the
 		documentation and/or other materials provided with the distribution.
 	* Neither the name of the organization nor the
	  names of its contributors may be used to endorse or promote products
 		derived from this software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL ANTHONY M. BLAKE BE LIABLE FOR ANY
 DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#include "ffts_real_nd.h"
#include "ffts_real.h"

#ifdef __ARM_NEON__
#include "neon.h"
#endif

#ifdef HAVE_NEON
#include <arm_neon.h>
#endif

#ifdef HAVE_SSE
#include <xmmintrin.h>
#endif

#include <stdio.h>

static void ffts_free_nd_real(ffts_plan_t *p)
{
    if (p->plans) {
        int i;

        for (i = 0; i < p->rank; i++) {
            ffts_plan_t *plan = p->plans[i];

            if (plan) {
                int j;

                for (j = i + 1; j < p->rank; j++) {
                    if (plan == p->plans[j]) {
                        p->plans[j] = NULL;
                    }
                }

                ffts_free(plan);
            }
        }

        free(p->plans);
    }

    if (p->transpose_buf) {
        ffts_aligned_free(p->transpose_buf);
    }

    if (p->buf) {
        ffts_aligned_free(p->buf);
    }

    if (p->Ns) {
        free(p->Ns);
    }

    if (p->Ms) {
        free(p->Ms);
    }

    free(p);
}

static void ffts_scalar_transpose(uint64_t *src, uint64_t *dst, int w, int h, uint64_t *buf)
{
    const int bw = 1;
    const int bh = 8;
    int i = 0, j = 0;

    for (; i <= h - bh; i += bh) {
        for (j = 0; j <= w - bw; j += bw) {
            uint64_t const *ib = &src[w*i + j];
            uint64_t *ob = &dst[h*j + i];

            uint64_t s_0_0 = ib[0*w + 0];
            uint64_t s_1_0 = ib[1*w + 0];
            uint64_t s_2_0 = ib[2*w + 0];
            uint64_t s_3_0 = ib[3*w + 0];
            uint64_t s_4_0 = ib[4*w + 0];
            uint64_t s_5_0 = ib[5*w + 0];
            uint64_t s_6_0 = ib[6*w + 0];
            uint64_t s_7_0 = ib[7*w + 0];

            ob[0*h + 0] = s_0_0;
            ob[0*h + 1] = s_1_0;
            ob[0*h + 2] = s_2_0;
            ob[0*h + 3] = s_3_0;
            ob[0*h + 4] = s_4_0;
            ob[0*h + 5] = s_5_0;
            ob[0*h + 6] = s_6_0;
            ob[0*h + 7] = s_7_0;
        }
    }

    if (i < h) {
        int i1;

        for (i1 = 0; i1 < w; i1++) {
            for (j = i; j < h; j++) {
                dst[i1*h + j] = src[j*w + i1];
            }
        }
    }

    if (j < w) {
        int j1;

        for (i = j; i < w; i++) {
            for (j1 = 0; j1 < h; j1++) {
                dst[i*h + j1] = src[j1*w + i];
            }
        }
    }
}

static void ffts_execute_nd_real(ffts_plan_t *p, const void *in, void *out)
{
    const size_t Ms0 = p->Ms[0];
    const size_t Ns0 = p->Ns[0];

    uint32_t *din = (uint32_t*) in;
    uint64_t *buf = p->buf;
    uint64_t *dout = (uint64_t*) out;
    uint64_t *transpose_buf = (uint64_t*) p->transpose_buf;

    ffts_plan_t *plan;
    size_t i, j;

    plan = p->plans[0];
    for (i = 0; i < Ns0; i++) {
        plan->transform(plan, din + (i * Ms0), buf + (i * (Ms0 / 2 + 1)));
    }

    ffts_scalar_transpose(buf, dout, Ms0 / 2 + 1, Ns0, transpose_buf);

    for (i = 1; i < p->rank; i++) {
        const size_t Ms = p->Ms[i];
        const size_t Ns = p->Ns[i];

        plan = p->plans[i];

        for (j = 0; j < Ns; j++) {
            plan->transform(plan, dout + (j * Ms), buf + (j * Ms));
        }

        ffts_scalar_transpose(buf, dout, Ms, Ns, transpose_buf);
    }
}

static void ffts_execute_nd_real_inv(ffts_plan_t *p, const void *in, void *out)
{
    const size_t Ms0 = p->Ms[0];
    const size_t Ms1 = p->Ms[1];
    const size_t Ns0 = p->Ns[0];
    const size_t Ns1 = p->Ns[1];

    uint64_t *din = (uint64_t*) in;
    uint64_t *buf = p->buf;
    uint64_t *buf2;
    uint64_t *transpose_buf = (uint64_t*) p->transpose_buf;
    float    *doutr = (float*) out;

    ffts_plan_t *plan;
    size_t vol;

    size_t i, j;

    vol = p->Ns[0];
    for (i = 1; i < p->rank; i++) {
        vol *= p->Ns[i];
    }

    buf2 = buf + vol;

    ffts_scalar_transpose(din, buf, Ms0, Ns0, transpose_buf);

    plan = p->plans[0];
    for (i = 0; i < Ms0; i++) {
        plan->transform(plan, buf + (i * Ns0), buf2 + (i * Ns0));
    }

    ffts_scalar_transpose(buf2, buf, Ns0, Ms0, transpose_buf);

    plan = p->plans[1];
    for (j = 0; j < Ms1; j++) {
        plan->transform(plan, buf + (j * Ms0), &doutr[j * Ns1]);
    }
}

ffts_plan_t *ffts_init_nd_real(int rank, size_t *Ns, int sign)
{
    int i;
    size_t vol = 1;
    size_t bufsize;
    ffts_plan_t *p;

    p = (ffts_plan_t*) calloc(1, sizeof(*p));
    if (!p) {
        return NULL;
    }

    if (sign < 0) {
        p->transform = &ffts_execute_nd_real;
    } else {
        p->transform = &ffts_execute_nd_real_inv;
    }

    p->destroy = &ffts_free_nd_real;
    p->rank    = rank;

    p->Ms = (size_t*) malloc(rank * sizeof(*p->Ms));
    if (!p->Ms) {
        goto cleanup;
    }

    p->Ns = (size_t*) malloc(rank * sizeof(*p->Ns));
    if (!p->Ns) {
        goto cleanup;
    }

    for (i = 0; i < rank; i++) {
        p->Ns[i] = Ns[i];
        vol *= Ns[i];
    }

    /* there is probably a prettier way of doing this, but it works.. */
    if (sign < 0) {
        bufsize = 2 * vol;
    } else {
        bufsize = 2 * (Ns[0] * ((vol / Ns[0]) / 2 + 1) + vol);
    }

    p->buf = ffts_aligned_malloc(bufsize * sizeof(float));
    if (!p->buf) {
        goto cleanup;
    }

    p->transpose_buf = ffts_aligned_malloc(2 * 8 * 8 * sizeof(float));
    if (!p->transpose_buf) {
        goto cleanup;
    }

    p->plans = (ffts_plan_t**) calloc(rank, sizeof(*p->plans));
    if (!p->plans) {
        goto cleanup;
    }

    for (i = 0; i < rank; i++) {
        int k;

        p->Ms[i] = vol / p->Ns[i];

        if (sign < 0) {
            if (!i) {
                p->plans[i] = ffts_init_1d_real(p->Ms[i], sign);
            } else {
                for (k = 1; k < i; k++) {
                    if (p->Ms[k] == p->Ms[i]) {
                        p->plans[i] = p->plans[k];
                        break;
                    }
                }

                if (!p->plans[i]) {
                    p->plans[i] = ffts_init_1d(p->Ms[i], sign);
                    p->Ns[i] = p->Ns[i] / 2 + 1;
                }
            }
        } else {
            if (i == rank - 1) {
                p->plans[i] = ffts_init_1d_real(p->Ns[i], sign);
            } else {
                for (k = 0; k < i; k++) {
                    if (p->Ns[k] == p->Ns[i]) {
                        p->plans[i] = p->plans[k];
                        break;
                    }
                }

                if (!p->plans[i]) {
                    p->plans[i] = ffts_init_1d(p->Ns[i], sign);
                    p->Ms[i] = p->Ms[i] / 2 + 1;
                }
            }
        }

        if (!p->plans[i]) {
            goto cleanup;
        }
    }

    return p;

cleanup:
    ffts_free_nd_real(p);
    return NULL;
}

ffts_plan_t *ffts_init_2d_real(size_t N1, size_t N2, int sign)
{
    size_t Ns[2];

    Ns[0] = N1;
    Ns[1] = N2;
    return ffts_init_nd_real(2, Ns, sign);
}