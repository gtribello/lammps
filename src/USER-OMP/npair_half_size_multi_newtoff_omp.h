/* -*- c++ -*- ----------------------------------------------------------
   LAMMPS - Large-scale Atomic/Molecular Massively Parallel Simulator
   https://lammps.sandia.gov/, Sandia National Laboratories
   Steve Plimpton, sjplimp@sandia.gov

   Copyright (2003) Sandia Corporation.  Under the terms of Contract
   DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government retains
   certain rights in this software.  This software is distributed under
   the GNU General Public License.

   See the README file in the top-level LAMMPS directory.
------------------------------------------------------------------------- */

#ifdef NPAIR_CLASS
// clang-format off
NPairStyle(half/size/multi/newtoff/omp,
           NPairHalfSizeMultiNewtoffOmp,
           NP_HALF | NP_SIZE | NP_MULTI | NP_NEWTOFF | NP_OMP |
           NP_ORTHO | NP_TRI);
// clang-format on
#else

#ifndef LMP_NPAIR_HALF_SIZE_MULTI_NEWTOFF_OMP_H
#define LMP_NPAIR_HALF_SIZE_MULTI_NEWTOFF_OMP_H

#include "npair.h"

namespace LAMMPS_NS {

class NPairHalfSizeMultiNewtoffOmp : public NPair {
 public:
  NPairHalfSizeMultiNewtoffOmp(class LAMMPS *);
  ~NPairHalfSizeMultiNewtoffOmp() {}
  void build(class NeighList *);
};

}

#endif
#endif

/* ERROR/WARNING messages:

*/
