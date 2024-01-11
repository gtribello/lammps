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

#ifdef COMMAND_CLASS
// clang-format off
CommandStyle(replicate,Replicate);
// clang-format on
#else

#ifndef LMP_REPLICATE_H
#define LMP_REPLICATE_H

#include "command.h"

#include <unordered_map>

namespace LAMMPS_NS {

class Replicate : public Command {
 public:
  Replicate(class LAMMPS *);
  void command(int, char **) override;

 private:
  std::unordered_map<tagint, int> old_map;
  double **old_x;
  double old_prd_half[3], old_center[3];
  tagint *old_tag;
  tagint maxtag;
  int thisrep[3], allnrep[3];
  void newtag(tagint, tagint &);
};

}    // namespace LAMMPS_NS

#endif
#endif
