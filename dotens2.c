/*
 * Copyright (c) 2002 Matteo Frigo
 * Copyright (c) 2002 Steven G. Johnson
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

/* $Id: dotens2.c,v 1.1 2002-09-02 15:46:57 athena Exp $ */

#include "ifftw.h"
#include "debug.h"

static void recur(uint rnk, const iodim *dims0, const iodim *dims1,
		  dotens2_closure *k, 
		  int indx0, int ondx0, int indx1, int ondx1)
{
     if (rnk == 0)
          k->apply(k, indx0, ondx0, indx1, ondx1);
     else {
          uint i, n = dims0[0].n;
          int is0 = dims0[0].is;
          int os0 = dims0[0].os;
          int is1 = dims1[0].is;
          int os1 = dims1[0].os;

	  A(n == dims1[0].n);

          for (i = 0; i < n; ++i) {
               recur(rnk - 1, dims0 + 1, dims1 + 1, k,
		     indx0, ondx0, indx1, ondx1);
	       indx0 += is0; ondx0 += os0;
	       indx1 += is1; ondx1 += os1;
	  }
     }
}

void X(dotens2)(tensor sz0, tensor sz1, dotens2_closure *k)
{
     A(sz0.rnk == sz1.rnk);
     if (sz0.rnk == RNK_MINFTY)
          return;
     recur(sz0.rnk, sz0.dims, sz1.dims, k, 0, 0, 0, 0);
}
