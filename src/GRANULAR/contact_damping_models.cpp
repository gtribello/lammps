/* -*- c++ -*- ----------------------------------------------------------
   LAMMPS - Large-scale Atomic/Molecular Massively Parallel Simulator
   https://www.lammps.org/, Sandia National Laboratories
   Steve Plimpton, sjplimp@sandia.gov

   Copyright (2003) Sandia Corporation.  Under the terms of Contract
   DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government retains
   certain rights in this software.  This software is distributed under
   the GNU General Public License.

   See the README file in the top-level LAMMPS directory.
------------------------------------------------------------------------- */

#include "contact_damping_models.h"
#include "contact_normal_models.h"
#include "contact.h"
#include "math_special.h"

using namespace LAMMPS_NS;
using namespace Contact;
using namespace MathSpecial;

/* ----------------------------------------------------------------------
   Default damping model
------------------------------------------------------------------------- */

void DampingModel::coeffs_to_local()
{
  damp = contact->normal_model->damp;
}

/* ----------------------------------------------------------------------
   Velocity damping
------------------------------------------------------------------------- */

double DampingVelocity::calculate_forces()
{
  return -damp * contact->vnnr;
}

/* ----------------------------------------------------------------------
   Mass velocity damping
------------------------------------------------------------------------- */

double DampingMassVelocity::calculate_forces()
{
  return -damp * contact->meff * contact->vnnr;
}

/* ----------------------------------------------------------------------
   Default, viscoelastic damping
------------------------------------------------------------------------- */

double DampingViscoelastic::calculate_forces()
{
  return -damp * contact->meff * contact->area * contact->vnnr;
}

/* ----------------------------------------------------------------------
   Tsuji damping
------------------------------------------------------------------------- */

void DampingTsuji::coeffs_to_local()
{
  double tmp = contact->normal_model->damp;
  damp = 1.2728 - 4.2783 * tmp + 11.087 * square(tmp);
  damp += -22.348 * cube(tmp)+ 27.467 * powint(tmp, 4);
  damp += -18.022 * powint(tmp, 5) + 4.8218 * powint(tmp,6);
}

/* ---------------------------------------------------------------------- */

double DampingTsuji::calculate_forces()
{
  return -damp * sqrt(contact->meff * contact->normal_model->knfac) * contact->vnnr;
}
