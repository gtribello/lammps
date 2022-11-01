// clang-format off
/* -*- c++ -*- ----------------------------------------------------------
   LAMMPS - Large-scale Atomic/Molecular Massively Parallel Simulator
   https://www.lammps.org/, Sandia National Laboratories
   LAMMPS development team: developers@lammps.org

   Copyright (2003) Sandia Corporation.  Under the terms of Contract
   DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government retains
   certain rights in this software.  This software is distributed under
   the GNU General Public License.

   See the README file in the top-level LAMMPS directory.
------------------------------------------------------------------------- */

#include "gsm_heat.h"
#include "granular_model.h"
#include "error.h"

using namespace LAMMPS_NS;
using namespace Granular_NS;

/* ----------------------------------------------------------------------
   Default heat conduction
------------------------------------------------------------------------- */

GSMHeat::GSMHeat(GranularModel *gm, LAMMPS *lmp) : GSM(gm, lmp) {}

/* ----------------------------------------------------------------------
   Area-based heat conduction
------------------------------------------------------------------------- */

GSMHeatNone::GSMHeatNone(GranularModel *gm, LAMMPS *lmp) : GSMHeat(gm, lmp) {}

/* ---------------------------------------------------------------------- */

double GSMHeatNone::calculate_heat()
{
  return 0.0;
}

/* ----------------------------------------------------------------------
   Area-based heat conduction
------------------------------------------------------------------------- */

GSMHeatArea::GSMHeatArea(GranularModel *gm, LAMMPS *lmp) : GSMHeat(gm, lmp)
{
  num_coeffs = 1;
}

/* ---------------------------------------------------------------------- */

void GSMHeatArea::coeffs_to_local()
{
  conductivity = coeffs[0];

  if (conductivity < 0.0) error->all(FLERR, "Illegal area heat model");
}

/* ---------------------------------------------------------------------- */

double GSMHeatArea::calculate_heat()
{
  return conductivity * gm->area * (gm->Tj - gm->Ti);
}
