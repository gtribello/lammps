/* ----------------------------------------------------------------------
   LAMMPS - Large-scale Atomic/Molecular Massively Parallel Simulator
   https://www.lammps.org/, Sandia National Laboratories
   Steve Plimpton, sjplimp@sandia.gov

   Copyright (2003) Sandia Corporation.  Under the terms of Contract
   DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government retains
   certain rights in this software.  This software is distributed under the GNU General Public License.

   See the README file in the top-level LAMMPS directory.
------------------------------------------------------------------------- */

/* ----------------------------------------------------------------------
   This is an optimized version of ilp/water/2dm based on the contribution of:
     author: Wengen Ouyang (Wuhan University)
     e-mail: w.g.ouyang at gmail dot com

   Optimizations are done by:
     author1: Xiaohui Duan (National Supercomputing Center in Wuxi, China)
     e-mail: sunrise_duan at 126 dot com

     author2: Ping Gao (National Supercomputing Center in Wuxi, China)
     e-mail: qdgaoping at gmail dot com

   Optimizations are described in:
     Gao, Ping and Duan, Xiaohui, et al:
       LMFF: Efficient and Scalable Layered Materials Force Field on Heterogeneous Many-Core Processors
     DOI: 10.1145/3458817.3476137

   Potential is described by:
     [Feng and Ouyang et al, J. Phys. Chem. C 127, 8704-8713 (2023).]
*/
#include "pair_ilp_water_2dm_opt.h"

#include "atom.h"
#include "citeme.h"
#include "comm.h"
#include "error.h"
#include "force.h"
#include "interlayer_taper.h"
#include "memory.h"
#include "neigh_list.h"
#include "neigh_request.h"
#include "neighbor.h"

#include <cmath>
#include <cstring>

using namespace LAMMPS_NS;
using namespace InterLayer;

PairILPWATER2DMOpt::PairILPWATER2DMOpt(LAMMPS *lmp) :
  PairILPGrapheneHBN(lmp), PairILPTMD(lmp), PairILPWATER2DM(lmp), PairILPGrapheneHBNOpt(lmp)
{
}

void PairILPWATER2DMOpt::coeff(int narg, char **args)
{
  PairILPTMD::coeff(narg, args);
  memory->create(special_type, atom->ntypes + 1, "PairILPWATER2DMOpt:check_sublayer");
  for (int i = 1; i <= atom->ntypes; i++) {
    int itype = map[i];
    if (strcmp(elements[itype], "Mo") == 0 || strcmp(elements[itype], "W") == 0 ||
        strcmp(elements[itype], "S") == 0 || strcmp(elements[itype], "Se") == 0 ||
        strcmp(elements[itype], "Te") == 0) {
      special_type[i] = TMD_METAL;
    } else if (strcmp(elements[itype], "Hw") == 0 || strcmp(elements[itype], "Ow") == 0) {
      special_type[i] = WATER;
    } else {
      special_type[i] = NOT_SPECIAL;
    }
  }
}
