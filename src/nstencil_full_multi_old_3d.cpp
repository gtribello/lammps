/* ----------------------------------------------------------------------
   LAMMPS - Large-scale Atomic/Molecular Massively Parallel Simulator
   https://www.lammps.org/, Sandia National Laboratories
   LAMMPS development team: developers@lammps.org

   Copyright (2003) Sandia Corporation.  Under the terms of Contract
   DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government retains
   certain rights in this software.  This software is distributed under
   the GNU General Public License.

   See the README file in the top-level LAMMPS directory.
------------------------------------------------------------------------- */

#include "nstencil_full_multi_old_3d.h"
#include "atom.h"

using namespace LAMMPS_NS;

/* ---------------------------------------------------------------------- */

NStencilFullMultiOld3d::NStencilFullMultiOld3d(LAMMPS *lmp) : NStencil(lmp) {}

/* ----------------------------------------------------------------------
   create stencil based on bin geometry and cutoff
------------------------------------------------------------------------- */

void NStencilFullMultiOld3d::create()
{
  int i, j, k, n;
  double rsq, typesq;
  int *s;
  double *distsq;

  int ntypes = atom->ntypes;
  for (int itype = 1; itype <= ntypes; itype++) {
    typesq = cuttypesq[itype];
    s = stencil_multi_old[itype];
    distsq = distsq_multi_old[itype];
    n = 0;
    for (k = -sz; k <= sz; k++)
      for (j = -sy; j <= sy; j++)
        for (i = -sx; i <= sx; i++) {
          rsq = bin_distance(i, j, k);
          if (rsq < typesq) {
            distsq[n] = rsq;
            s[n++] = k * mbiny * mbinx + j * mbinx + i;
          }
        }
    nstencil_multi_old[itype] = n;
  }
}
