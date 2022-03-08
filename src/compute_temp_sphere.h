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

#ifdef COMPUTE_CLASS
// clang-format off
ComputeStyle(temp/sphere,ComputeTempSphere);
// clang-format on
#else

#ifndef LMP_COMPUTE_TEMP_SPHERE_H
#define LMP_COMPUTE_TEMP_SPHERE_H

#include "compute.h"

namespace LAMMPS_NS {

class ComputeTempSphere : public Compute {
 public:
  ComputeTempSphere(class LAMMPS *, int, char **);
  ~ComputeTempSphere() override;
  void init() override;
  void setup() override;
  double compute_scalar() override;
  void compute_vector() override;

  void remove_bias(int, double *) override;
  void remove_bias_thr(int, double *, double *) override;
  void restore_bias(int, double *) override;
  void restore_bias_thr(int, double *, double *) override;

 private:
  int mode;
  double tfactor;
  char *id_bias;
  Compute *tbias;    // ptr to additional bias compute

  void dof_compute();
};

}    // namespace LAMMPS_NS

#endif
#endif

/* ERROR/WARNING messages:

E: Illegal ... command

Self-explanatory.  Check the input script syntax and compare to the
documentation for the command.  You can use -echo screen as a
command-line option when running LAMMPS to see the offending line.

E: Compute temp/sphere requires atom style sphere

Self-explanatory.

E: Could not find compute ID for temperature bias

Self-explanatory.

E: Bias compute does not calculate temperature

The specified compute must compute temperature.

E: Bias compute does not calculate a velocity bias

The specified compute must compute a bias for temperature.

E: Bias compute group does not match compute group

The specified compute must operate on the same group as the parent
compute.

E: Temperature compute degrees of freedom < 0

This should not happen if you are calculating the temperature
on a valid set of atoms.

*/
