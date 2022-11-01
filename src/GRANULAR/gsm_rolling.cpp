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

#include "gsm_normal.h"
#include "gsm_rolling.h"
#include "granular_model.h"
#include "error.h"
#include "math_const.h"
#include "math_extra.h"

using namespace LAMMPS_NS;
using namespace Granular_NS;
using namespace MathConst;
using namespace MathExtra;

/* ----------------------------------------------------------------------
   Default rolling friction model
------------------------------------------------------------------------- */

GSMRolling::GSMRolling(GranularModel *gm, LAMMPS *lmp) : GSM(gm, lmp) {}

/* ----------------------------------------------------------------------
   No model
------------------------------------------------------------------------- */

GSMRollingNone::GSMRollingNone(GranularModel *gm, LAMMPS *lmp) : GSMRolling(gm, lmp) {}

/* ----------------------------------------------------------------------
   SDS rolling friction model
------------------------------------------------------------------------- */

GSMRollingSDS::GSMRollingSDS(GranularModel *gm, LAMMPS *lmp) : GSMRolling(gm, lmp)
{
  num_coeffs = 3;
  size_history = 3;
}

/* ---------------------------------------------------------------------- */

void GSMRollingSDS::coeffs_to_local()
{
  k = coeffs[0];
  gamma = coeffs[1];
  mu = coeffs[2];

  if (k < 0.0 || mu < 0.0 || gamma < 0.0)
    error->all(FLERR, "Illegal SDS rolling model");
}

/* ---------------------------------------------------------------------- */

void GSMRollingSDS::calculate_forces()
{
  int rhist0, rhist1, rhist2, frameupdate;
  double Frcrit, rolldotn, rollmag, prjmag, magfr, hist_temp[3], scalefac, temp_array[3];
  double k_inv, magfr_inv;

  rhist0 = history_index;
  rhist1 = rhist0 + 1;
  rhist2 = rhist1 + 1;

  Frcrit = mu * gm->normal_model->Fncrit;

  if (gm->history_update) {
    hist_temp[0] = gm->history[rhist0];
    hist_temp[1] = gm->history[rhist1];
    hist_temp[2] = gm->history[rhist2];
    rolldotn = dot3(hist_temp, gm->nx);

    frameupdate = (fabs(rolldotn) * k)  > (EPSILON * Frcrit);
    if (frameupdate) { // rotate into tangential plane
      rollmag = len3(hist_temp);
      // projection
      scale3(rolldotn, gm->nx, temp_array);
      sub3(hist_temp, temp_array, hist_temp);

      // also rescale to preserve magnitude
      prjmag = len3(hist_temp);
      if (prjmag > 0) scalefac = rollmag / prjmag;
      else scalefac = 0;
      scale3(scalefac, hist_temp);
    }
    scale3(gm->dt, gm->vrl, temp_array);
    add3(hist_temp, temp_array, hist_temp);
  }

  scaleadd3(-k, hist_temp, -gamma, gm->vrl, gm->fr);

  // rescale frictional displacements and forces if needed
  magfr = len3(gm->fr);
  if (magfr > Frcrit) {
    rollmag = len3(hist_temp);
    if (rollmag != 0.0) {
      k_inv = 1.0 / k;
      magfr_inv = 1.0 / magfr;
      scale3(-Frcrit * k_inv * magfr_inv, gm->fr, hist_temp);
      scale3(-gamma * k_inv, gm->vrl, temp_array);
      add3(hist_temp, temp_array, hist_temp);

      scale3(Frcrit * magfr_inv, gm->fr);
    } else {
      zero3(gm->fr);
    }
  }

  if (gm->history_update) {
    gm->history[rhist0] = hist_temp[0];
    gm->history[rhist1] = hist_temp[1];
    gm->history[rhist2] = hist_temp[2];
  }
}
