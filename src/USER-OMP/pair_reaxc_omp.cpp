/* ----------------------------------------------------------------------
   LAMMPS - Large-scale Atomic/Molecular Massively Parallel Simulator
   https://lammps.sandia.gov/, Sandia National Laboratories
   Steve Plimpton, sjplimp@sandia.gov

   Copyright (2003) Sandia Corporation.  Under the terms of Contract
   DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government retains
   certain rights in this software.  This software is distributed under
   the GNU General Public License.

   See the README file in the top-level LAMMPS directory.
------------------------------------------------------------------------- */

/* ----------------------------------------------------------------------
   Contributing author:
   Hasan Metin Aktulga, Michigan State University, hma@cse.msu.edu

   Per-atom energy/virial added by Ray Shan (Materials Design, Inc.)
   Fix reax/c/bonds and fix reax/c/species for pair_style reax/c added
   by Ray Shan (Materials Design)

   OpenMP based threading support for pair_style reax/c/omp added
   by Hasan Metin Aktulga (MSU), Chris Knight (ALCF), Paul Coffman (ALCF),
   Kurt O'Hearn (MSU), Ray Shan (Materials Design), Wei Jiang (ALCF)

   Integration of the pair_style reax/c/omp into the User-OMP package
   by Axel Kohlmeyer (Temple U.)

   Please cite the related publication:
   H. M. Aktulga, C. Knight, P. Coffman, K. A. O'Hearn, T. R. Shan,
   W. Jiang, "Optimizing the performance of reactive molecular dynamics
   simulations for multi-core architectures", International Journal of
   High Performance Computing Applications, to appear.
 ------------------------------------------------------------------------- */

#include "pair_reaxc_omp.h"

#include <cmath>
#include "atom.h"
#include "update.h"
#include "force.h"
#include "comm.h"
#include "neighbor.h"
#include "neigh_list.h"
#include "neigh_request.h"
#include "modify.h"
#include "fix_reaxc.h"
#include "citeme.h"
#include "memory.h"
#include "error.h"


#include "reaxc_defs.h"
#include "reaxc_types.h"
#include "reaxc_allocate.h"
#include "reaxc_forces_omp.h"
#include "reaxc_init_md_omp.h"
#include "reaxc_io_tools.h"
#include "reaxc_list.h"
#include "reaxc_reset_tools.h"
#include "reaxc_tool_box.h"

#include "reaxff_api.h"

#if defined(_OPENMP)
#include <omp.h>
#endif

#include "suffix.h"
using namespace LAMMPS_NS;

#ifdef OMP_TIMING
double ompTimingData[LASTTIMINGINDEX];
int ompTimingCount[LASTTIMINGINDEX];
int ompTimingCGCount[LASTTIMINGINDEX];
#endif

static const char cite_pair_reax_c_omp[] =
  "pair reax/c/omp and fix qeq/reax/omp command:\n\n"
  "@Article{Aktulga17,\n"
  " author =  {H. M. Aktulga, C. Knight, P. Coffman, K. A. OHearn, T. R. Shan, W. Jiang},\n"
  " title =   {Optimizing the performance of reactive molecular dynamics simulations for multi-core architectures},\n"
  " journal = {International Journal of High Performance Computing Applications},\n"
  " year =    to appear\n"
  "}\n\n";

/* ---------------------------------------------------------------------- */

PairReaxCOMP::PairReaxCOMP(LAMMPS *lmp) : PairReaxC(lmp), ThrOMP(lmp, THR_PAIR)
{
  if (lmp->citeme) lmp->citeme->add(cite_pair_reax_c_omp);

  suffix_flag |= Suffix::OMP;
  api->system->pair_ptr = this;
  api->system->omp_active = 1;

  num_nbrs_offset = nullptr;

#ifdef OMP_TIMING
  for (int i=0;i<LASTTIMINGINDEX;i++) {
    ompTimingData[i] = 0;
    ompTimingCount[i] = 0;
    ompTimingCGCount[i] = 0;
  }
#endif
}

/* ---------------------------------------------------------------------- */

PairReaxCOMP::~PairReaxCOMP()
{
  if (setup_flag) {
    reax_list * bonds = api->lists+BONDS;
    for (int i=0; i<bonds->num_intrs; ++i)
      sfree(error, bonds->select.bond_list[i].bo_data.CdboReduction, "CdboReduction");
  }
  memory->destroy(num_nbrs_offset);

#ifdef OMP_TIMING
  int myrank;

  MPI_Comm_rank(world,&myrank);

  // Write screen output
  if (timer->has_full() && myrank == 0 && screen) {
    fprintf(screen,"\n\nWrite_Lists    took %11.3lf seconds", ompTimingData[COMPUTEWLINDEX]);

    fprintf(screen,"\n\nCompute_Forces took %11.3lf seconds:", ompTimingData[COMPUTEINDEX]);
    fprintf(screen,"\n ->Initial Forces: %11.3lf seconds", ompTimingData[COMPUTEIFINDEX]);
    fprintf(screen,"\n ->Bond Order:     %11.3lf seconds", ompTimingData[COMPUTEBOINDEX]);
    fprintf(screen,"\n ->Atom Energy:    %11.3lf seconds", ompTimingData[COMPUTEATOMENERGYINDEX]);
    fprintf(screen,"\n ->Bond:           %11.3lf seconds", ompTimingData[COMPUTEBONDSINDEX]);
    fprintf(screen,"\n ->Hydrogen bonds: %11.3lf seconds", ompTimingData[COMPUTEHBONDSINDEX]);
    fprintf(screen,"\n ->Torsion Angles: %11.3lf seconds", ompTimingData[COMPUTETORSIONANGLESBOINDEX]);
    fprintf(screen,"\n ->Valence Angles: %11.3lf seconds", ompTimingData[COMPUTEVALENCEANGLESBOINDEX]);
    fprintf(screen,"\n ->Non-Bonded For: %11.3lf seconds", ompTimingData[COMPUTENBFINDEX]);
    fprintf(screen,"\n ->Total Forces:   %11.3lf seconds", ompTimingData[COMPUTETFINDEX]);

    fprintf(screen,"\n\nfixQEQ:          %11.3lf seconds", ompTimingData[COMPUTEQEQINDEX]);
    fprintf(screen,"\n ->QEQ init:       %11.3lf seconds", ompTimingData[COMPUTEINITMVINDEX]);

    double avg = double(ompTimingCGCount[COMPUTECG1INDEX]) / double(ompTimingCount[COMPUTECG1INDEX]);
    fprintf(screen,"\n ->QEQ CG1:        %11.3lf seconds with %4.1lf iterations on average.", ompTimingData[COMPUTECG1INDEX], avg);

    avg = double(ompTimingCGCount[COMPUTECG2INDEX]) / double(ompTimingCount[COMPUTECG2INDEX]);
    fprintf(screen,"\n ->QEQ CG2:        %11.3lf seconds with %4.1lf iterations on average.", ompTimingData[COMPUTECG2INDEX], avg);
    fprintf(screen,"\n ->QEQ CalcQ:      %11.3lf seconds\n", ompTimingData[COMPUTECALCQINDEX]);
  }

  // Write logfile output
  if (timer->has_full() && myrank == 0 && logfile) {
    fprintf(logfile,"\n\nWrite_Lists    took %11.3lf seconds", ompTimingData[COMPUTEWLINDEX]);

    fprintf(logfile,"\n\nCompute_Forces took %11.3lf seconds:", ompTimingData[COMPUTEINDEX]);
    fprintf(logfile,"\n ->Initial Forces: %11.3lf seconds", ompTimingData[COMPUTEIFINDEX]);
    fprintf(logfile,"\n ->Bond Order:     %11.3lf seconds", ompTimingData[COMPUTEBOINDEX]);
    fprintf(logfile,"\n ->Atom Energy:    %11.3lf seconds", ompTimingData[COMPUTEATOMENERGYINDEX]);
    fprintf(logfile,"\n ->Bond:           %11.3lf seconds", ompTimingData[COMPUTEBONDSINDEX]);
    fprintf(logfile,"\n ->Hydrogen bonds: %11.3lf seconds", ompTimingData[COMPUTEHBONDSINDEX]);
    fprintf(logfile,"\n ->Torsion Angles: %11.3lf seconds", ompTimingData[COMPUTETORSIONANGLESBOINDEX]);
    fprintf(logfile,"\n ->Valence Angles: %11.3lf seconds", ompTimingData[COMPUTEVALENCEANGLESBOINDEX]);
    fprintf(logfile,"\n ->Non-Bonded For: %11.3lf seconds", ompTimingData[COMPUTENBFINDEX]);
    fprintf(logfile,"\n ->Total Forces:   %11.3lf seconds", ompTimingData[COMPUTETFINDEX]);

    fprintf(logfile,"\n\nfixQEQ:          %11.3lf seconds", ompTimingData[COMPUTEQEQINDEX]);
    fprintf(logfile,"\n ->QEQ init:       %11.3lf seconds", ompTimingData[COMPUTEINITMVINDEX]);

    double avg = double(ompTimingCGCount[COMPUTECG1INDEX]) / double(ompTimingCount[COMPUTECG1INDEX]);
    fprintf(logfile,"\n ->QEQ CG1:        %11.3lf seconds with %4.1lf iterations on average.", ompTimingData[COMPUTECG1INDEX], avg);

    avg = double(ompTimingCGCount[COMPUTECG2INDEX]) / double(ompTimingCount[COMPUTECG2INDEX]);
    fprintf(logfile,"\n ->QEQ CG2:        %11.3lf seconds with %4.1lf iterations on average.", ompTimingData[COMPUTECG2INDEX], avg);
    fprintf(logfile,"\n ->QEQ CalcQ:      %11.3lf seconds\n", ompTimingData[COMPUTECALCQINDEX]);
  }
#endif
}

/* ---------------------------------------------------------------------- */

void PairReaxCOMP::compute(int eflag, int vflag)
{
  double evdwl,ecoul;

  // communicate num_bonds once every reneighboring
  // 2 num arrays stored by fix, grab ptr to them

  if (neighbor->ago == 0) comm->forward_comm_fix(fix_reax);
  int *num_bonds = fix_reax->num_bonds;
  int *num_hbonds = fix_reax->num_hbonds;

  evdwl = ecoul = 0.0;
  ev_init(eflag,vflag);

  if (vflag_global) api->control->virial = 1;
  else api->control->virial = 0;

  if (vflag_atom)
     error->all(FLERR,"Pair style reax/c/omp does not support "
                "computing per-atom stress");

  api->system->n = atom->nlocal; // my atoms
  api->system->N = atom->nlocal + atom->nghost; // mine + ghosts
  api->system->bigN = static_cast<int> (atom->natoms);  // all atoms in the system

  // setup data structures

  setup();

  Reset( api->system, api->control, api->data, api->workspace, &api->lists );

  // Why not update workspace like in MPI-only code?
  // Using the MPI-only way messes up the hb energy
  //workspace->realloc.num_far = write_reax_lists();
  write_reax_lists();

  // forces

#ifdef OMP_TIMING
  double startTimeBase,endTimeBase;
  startTimeBase = MPI_Wtime();
#endif

  Compute_ForcesOMP(api->system,api->control,api->data,api->workspace,&api->lists,api->out_control);
  read_reax_forces(vflag);

#ifdef OMP_TIMING
  endTimeBase = MPI_Wtime();
  ompTimingData[COMPUTEINDEX] += (endTimeBase-startTimeBase);
#endif

#if defined(_OPENMP)
#pragma omp parallel for schedule(static)
#endif
  for (int k = 0; k < api->system->N; ++k) {
    num_bonds[k] = api->system->my_atoms[k].num_bonds;
    num_hbonds[k] = api->system->my_atoms[k].num_hbonds;
  }

  // energies and pressure

  if (eflag_global) {
    evdwl += api->data->my_en.e_bond;
    evdwl += api->data->my_en.e_ov;
    evdwl += api->data->my_en.e_un;
    evdwl += api->data->my_en.e_lp;
    evdwl += api->data->my_en.e_ang;
    evdwl += api->data->my_en.e_pen;
    evdwl += api->data->my_en.e_coa;
    evdwl += api->data->my_en.e_hb;
    evdwl += api->data->my_en.e_tor;
    evdwl += api->data->my_en.e_con;
    evdwl += api->data->my_en.e_vdW;

    ecoul += api->data->my_en.e_ele;
    ecoul += api->data->my_en.e_pol;

    // Store the different parts of the energy
    // in a list for output by compute pair command

    pvector[0] = api->data->my_en.e_bond;
    pvector[1] = api->data->my_en.e_ov + api->data->my_en.e_un;
    pvector[2] = api->data->my_en.e_lp;
    pvector[3] = 0.0;
    pvector[4] = api->data->my_en.e_ang;
    pvector[5] = api->data->my_en.e_pen;
    pvector[6] = api->data->my_en.e_coa;
    pvector[7] = api->data->my_en.e_hb;
    pvector[8] = api->data->my_en.e_tor;
    pvector[9] = api->data->my_en.e_con;
    pvector[10] = api->data->my_en.e_vdW;
    pvector[11] = api->data->my_en.e_ele;
    pvector[12] = 0.0;
    pvector[13] = api->data->my_en.e_pol;
  }

  if (vflag_fdotr) virial_fdotr_compute();

  // Set internal timestep counter to that of LAMMPS

  api->data->step = update->ntimestep;

  Output_Results( api->system, api->control, api->data, &api->lists, api->out_control, world );

  // populate tmpid and tmpbo arrays for fix reax/c/species

  if (fixspecies_flag) {
    if (api->system->N > nmax) {
      memory->destroy(tmpid);
      memory->destroy(tmpbo);
      nmax = api->system->N;
      memory->create(tmpid,nmax,MAXSPECBOND,"pair:tmpid");
      memory->create(tmpbo,nmax,MAXSPECBOND,"pair:tmpbo");
    }

#if defined(_OPENMP)
#pragma omp parallel for collapse(2) schedule(static) default(shared)
#endif
    for (int i = 0; i < api->system->N; i++)
      for (int j = 0; j < MAXSPECBOND; j++) {
        tmpbo[i][j] = 0.0;
        tmpid[i][j] = 0;
      }

    FindBond();
  }
}

/* ---------------------------------------------------------------------- */

void PairReaxCOMP::init_style( )
{
  if (!atom->q_flag)
    error->all(FLERR,"Pair reax/c/omp requires atom attribute q");

  // firstwarn = 1;

  bool have_qeq = ((modify->find_fix_by_style("^qeq/reax") != -1)
                   || (modify->find_fix_by_style("^qeq/shielded") != -1));
  if (!have_qeq && qeqflag == 1)
    error->all(FLERR,"Pair reax/c requires use of fix qeq/reax or qeq/shielded");

  api->system->n = atom->nlocal; // my atoms
  api->system->N = atom->nlocal + atom->nghost; // mine + ghosts
  api->system->bigN = static_cast<int> (atom->natoms);  // all atoms in the system
  api->system->wsize = comm->nprocs;

  if (atom->tag_enable == 0)
    error->all(FLERR,"Pair style reax/c/omp requires atom IDs");
  if (force->newton_pair == 0)
    error->all(FLERR,"Pair style reax/c/omp requires newton pair on");

 if ((atom->map_tag_max > 99999999) && (comm->me == 0))
    error->warning(FLERR,"Some Atom-IDs are too large. Pair style reax/c/omp "
                   "native output files may get misformatted or corrupted");

  // because system->bigN is an int, we cannot have more atoms than MAXSMALLINT

  if (atom->natoms > MAXSMALLINT)
    error->all(FLERR,"Too many atoms for pair style reax/c/omp");

  // need a half neighbor list w/ Newton off and ghost neighbors
  // built whenever re-neighboring occurs

  int irequest = neighbor->request(this,instance_me);
  neighbor->requests[irequest]->newton = 2;
  neighbor->requests[irequest]->ghost = 1;

  cutmax = MAX3(api->control->nonb_cut, api->control->hbond_cut, api->control->bond_cut);
  if ((cutmax < 2.0*api->control->bond_cut) && (comm->me == 0))
    error->warning(FLERR,"Total cutoff < 2*bond cutoff. May need to use an "
                   "increased neighbor list skin.");

  for (int i = 0; i < LIST_N; ++i)
    api->lists[i].allocated = 0;

  if (fix_reax == nullptr) {
    modify->add_fix(fmt::format("{} all REAXC",fix_id));
    fix_reax = (FixReaxC *) modify->fix[modify->nfix-1];
  }

  api->control->nthreads = comm->nthreads;
}

/* ---------------------------------------------------------------------- */

void PairReaxCOMP::setup( )
{
  int oldN;
  int mincap = api->system->mincap;
  double safezone = api->system->safezone;

  api->system->n = atom->nlocal; // my atoms
  api->system->N = atom->nlocal + atom->nghost; // mine + ghosts
  oldN = api->system->N;
  api->system->bigN = static_cast<int> (atom->natoms);  // all atoms in the system

  if (api->system->N > nmax) {
    memory->destroy(num_nbrs_offset);
    // Don't update nmax here. It is updated at end of compute().
    memory->create(num_nbrs_offset, api->system->N, "pair:num_nbrs_offset");
  }

  if (setup_flag == 0) {

    setup_flag = 1;

    int *num_bonds = fix_reax->num_bonds;
    int *num_hbonds = fix_reax->num_hbonds;

    // determine the local and total capacity

    api->system->local_cap = MAX( (int)(api->system->n * safezone), mincap );
    api->system->total_cap = MAX( (int)(api->system->N * safezone), mincap );

    // initialize my data structures

    PreAllocate_Space( api->system, api->control, api->workspace );
    write_reax_atoms();

    int num_nbrs = estimate_reax_lists();
    if (!Make_List(api->system->total_cap, num_nbrs, TYP_FAR_NEIGHBOR,
                  api->lists+FAR_NBRS))
      error->all(FLERR,"Pair reax/c problem in far neighbor list");

    write_reax_lists();

    InitializeOMP(api->system, api->control, api->data, api->workspace, &api->lists, api->out_control, world);

    for (int k = 0; k < api->system->N; ++k) {
      num_bonds[k] = api->system->my_atoms[k].num_bonds;
      num_hbonds[k] = api->system->my_atoms[k].num_hbonds;
    }

  } else {

    // fill in reax datastructures

    write_reax_atoms();

    // reset the bond list info for new atoms

    for (int k = oldN; k < api->system->N; ++k)
      Set_End_Index( k, Start_Index( k, api->lists+BONDS ), api->lists+BONDS );

    // estimate far neighbor list size
    // Not present in MPI-only version
    api->workspace->realloc.num_far = estimate_reax_lists();

    // check if I need to shrink/extend my data-structs

    ReAllocate( api->system, api->control, api->data, api->workspace, &api->lists );
  }
}

/* ---------------------------------------------------------------------- */

void PairReaxCOMP::write_reax_atoms()
{
  int *num_bonds = fix_reax->num_bonds;
  int *num_hbonds = fix_reax->num_hbonds;

  if (api->system->N > api->system->total_cap)
    error->all(FLERR,"Too many ghost atoms");

#if defined(_OPENMP)
#pragma omp parallel for schedule(static) default(shared)
#endif
  for (int i = 0; i < api->system->N; ++i) {
    api->system->my_atoms[i].orig_id = atom->tag[i];
    api->system->my_atoms[i].type = map[atom->type[i]];
    api->system->my_atoms[i].x[0] = atom->x[i][0];
    api->system->my_atoms[i].x[1] = atom->x[i][1];
    api->system->my_atoms[i].x[2] = atom->x[i][2];
    api->system->my_atoms[i].q = atom->q[i];
    api->system->my_atoms[i].num_bonds = num_bonds[i];
    api->system->my_atoms[i].num_hbonds = num_hbonds[i];
  }
}

/* ---------------------------------------------------------------------- */

int PairReaxCOMP::estimate_reax_lists()
{
  int i;
  int *ilist = list->ilist;
  int *numneigh = list->numneigh;
  int numall = list->inum + list->gnum;
  int mincap = api->system->mincap;

  // for good performance in the OpenMP implementation, each thread needs
  // to know where to place the neighbors of the atoms it is responsible for.
  // The sumscan values for the list->numneigh will be used to determine the
  // neighbor offset of each atom. Note that this may cause some significant
  // memory overhead if delayed neighboring is used - so it may be desirable
  // to work on this part to reduce the memory footprint of the far_nbrs list.

  int num_nbrs = 0;

  for (int itr_i = 0; itr_i < numall; ++itr_i) {
    i = ilist[itr_i];
    num_nbrs += numneigh[i];
  }

  int new_estimate = MAX(num_nbrs, mincap*REAX_MIN_NBRS);

  return new_estimate;
}

/* ---------------------------------------------------------------------- */

int PairReaxCOMP::write_reax_lists()
{
#ifdef OMP_TIMING
  double startTimeBase, endTimeBase;
  startTimeBase = MPI_Wtime();
#endif

  int itr_i, itr_j, i, j, num_mynbrs;
  int *jlist;
  double d_sqr, dist, cutoff_sqr;
  rvec dvec;

  double **x = atom->x;
  int *ilist = list->ilist;
  int *numneigh = list->numneigh;
  int **firstneigh = list->firstneigh;
  reax_list *far_nbrs = api->lists + FAR_NBRS;
  far_neighbor_data *far_list = far_nbrs->select.far_nbr_list;

  int num_nbrs = 0;
  int inum = list->inum;
  int gnum = list->gnum;
  int numall = inum + gnum;

  // sumscan of the number of neighbors per atom to determine the offsets
  // most likely, we are overallocating. desirable to work on this part
  // to reduce the memory footprint of the far_nbrs list.

  num_nbrs = 0;

  for (itr_i = 0; itr_i < numall; ++itr_i) {
    i = ilist[itr_i];
    num_nbrs_offset[i] = num_nbrs;
    num_nbrs += numneigh[i];
  }

#if defined(_OPENMP)
#pragma omp parallel for schedule(dynamic,50) default(shared)           \
  private(itr_i, itr_j, i, j, jlist, cutoff_sqr, num_mynbrs, d_sqr, dvec, dist)
#endif
  for (itr_i = 0; itr_i < numall; ++itr_i) {
    i = ilist[itr_i];
    jlist = firstneigh[i];
    Set_Start_Index( i, num_nbrs_offset[i], far_nbrs );

    if (i < inum)
      cutoff_sqr = SQR(api->control->nonb_cut);
    else
      cutoff_sqr = SQR(api->control->bond_cut);

    num_mynbrs = 0;

    for (itr_j = 0; itr_j < numneigh[i]; ++itr_j) {
      j = jlist[itr_j];
      j &= NEIGHMASK;
      get_distance( x[j], x[i], &d_sqr, &dvec );

      if (d_sqr <= cutoff_sqr) {
        dist = sqrt( d_sqr );
        set_far_nbr((ReaxFF::far_neighbor_data *) &far_list[num_nbrs_offset[i] + num_mynbrs], j, dist, dvec );
        ++num_mynbrs;
      }
    }
    Set_End_Index( i, num_nbrs_offset[i] + num_mynbrs, far_nbrs );
  }

#ifdef OMP_TIMING
  endTimeBase = MPI_Wtime();
  ompTimingData[COMPUTEWLINDEX] += (endTimeBase-startTimeBase);
#endif

  return num_nbrs;
}

/* ---------------------------------------------------------------------- */

void PairReaxCOMP::read_reax_forces(int /* vflag */)
{
#if defined(_OPENMP)
#pragma omp parallel for schedule(static) default(shared)
#endif
  for (int i = 0; i < api->system->N; ++i) {
    api->system->my_atoms[i].f[0] = api->workspace->f[i][0];
    api->system->my_atoms[i].f[1] = api->workspace->f[i][1];
    api->system->my_atoms[i].f[2] = api->workspace->f[i][2];

    atom->f[i][0] = -api->workspace->f[i][0];
    atom->f[i][1] = -api->workspace->f[i][1];
    atom->f[i][2] = -api->workspace->f[i][2];
  }
}

/* ---------------------------------------------------------------------- */

void PairReaxCOMP::FindBond()
{
  const double bo_cut = 0.10;

#if defined(_OPENMP)
#pragma omp parallel for schedule(static) default(shared)
#endif
  for (int i = 0; i < api->system->n; i++) {
    int j, pj, nj;
    double bo_tmp;
    bond_data *bo_ij;

    nj = 0;
    for (pj = Start_Index(i, api->lists); pj < End_Index(i, api->lists); ++pj) {
      bo_ij = &( api->lists->select.bond_list[pj] );
      j = bo_ij->nbr;
      if (j < i) continue;

      bo_tmp = bo_ij->bo_data.BO;

      if (bo_tmp >= bo_cut) {
        tmpid[i][nj] = j;
        tmpbo[i][nj] = bo_tmp;
        nj ++;
        if (nj > MAXSPECBOND) error->all(FLERR,"Increase MAXSPECBOND in fix_reaxc_species.h");
      }
    }
  }
}
