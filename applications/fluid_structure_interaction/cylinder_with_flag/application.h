/*  ______________________________________________________________________
 *
 *  ExaDG - High-Order Discontinuous Galerkin for the Exa-Scale
 *
 *  Copyright (C) 2021 by the ExaDG authors
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *  ______________________________________________________________________
 */

#ifndef APPLICATIONS_FSI_CYLINDER_WITH_FLAG_H_
#define APPLICATIONS_FSI_CYLINDER_WITH_FLAG_H_

#include <exadg/grid/one_sided_cylindrical_manifold.h>

namespace ExaDG
{
namespace FSI
{
using namespace dealii;

// set problem specific parameters
#define TESTCASE 2

#if TESTCASE == 1
// FSI 1
double const U_MEAN          = 0.2;
double const FLUID_VISCOSITY = 1.0e-3;
double const FLUID_DENSITY   = 1.0e3;

double const DENSITY_STRUCTURE       = 1.0e3;
double const POISSON_RATIO_STRUCTURE = 0.4;
double const E_STRUCTURE             = 0.5e6 * 2.0 * (1.0 + POISSON_RATIO_STRUCTURE);
#elif TESTCASE == 2
// FSI 2
double const U_MEAN          = 1.0;
double const FLUID_VISCOSITY = 1.0e-3;
double const FLUID_DENSITY   = 1.0e3;

double const DENSITY_STRUCTURE       = 1.0e4;
double const POISSON_RATIO_STRUCTURE = 0.4;
double const E_STRUCTURE             = 0.5e6 * 2.0 * (1.0 + POISSON_RATIO_STRUCTURE);
#elif TESTCASE == 3
// FSI 3
double const U_MEAN          = 2.0;
double const FLUID_VISCOSITY = 1.0e-3;
double const FLUID_DENSITY   = 1.0e3;

double const DENSITY_STRUCTURE       = 1.0e3;
double const POISSON_RATIO_STRUCTURE = 0.4;
double const E_STRUCTURE             = 2.0e6 * 2.0 * (1.0 + POISSON_RATIO_STRUCTURE);
#endif

// physical dimensions (diameter D and center coordinate Y_C can be varied)
double const X_0    = 0.0;  // origin (x-coordinate)
double const Y_0    = 0.0;  // origin (y-coordinate)
double const L      = 2.5;  // x-coordinate of outflow boundary
double const H      = 0.41; // height of channel
double const X_C    = 0.2;  // center of cylinder (x-coordinate)
double const Y_C    = 0.2;  // center of cylinder (y-coordinate)
double const X_2    = 2.0 * X_C;
double const D      = 0.1;                    // cylinder diameter
double const R      = D / 2.0;                // cylinder radius
double const T      = 0.02;                   // thickness of flag
double const L_FLAG = 0.35;                   // length of flag
double const X_3    = X_C + R + L_FLAG * 1.6; // only relevant for mesh
double const Y_3    = H / 3.0;                // only relevant for mesh

// boundary conditions
types::boundary_id const BOUNDARY_ID_WALLS    = 0;
types::boundary_id const BOUNDARY_ID_INFLOW   = 1;
types::boundary_id const BOUNDARY_ID_OUTFLOW  = 2;
types::boundary_id const BOUNDARY_ID_CYLINDER = 3;
types::boundary_id const BOUNDARY_ID_FLAG     = 4;

bool STRUCTURE_COVERS_FLAG_ONLY = true;

unsigned int N_CELLS_FLAG_X = 16;

double const U_X_MAX  = 1.5 * U_MEAN;
double const END_TIME = 4.0 * L / U_MEAN;

double const       OUTPUT_INTERVAL_TIME                = END_TIME / 600;
unsigned int const OUTPUT_SOLVER_INFO_EVERY_TIME_STEPS = 1e2;

double const REL_TOL = 1.e-6;
double const ABS_TOL = 1.e-8;

double const REL_TOL_LINEARIZED = 1.e-6;
double const ABS_TOL_LINEARIZED = 1.e-12;

template<int dim>
class InflowBC : public Function<dim>
{
public:
  InflowBC() : Function<dim>(dim, 0.0)
  {
  }

  double
  value(Point<dim> const & p, unsigned int const component = 0) const
  {
    (void)p;
    double result = 0.0;

    double const t = this->get_time();
    double const T = 2.0;

    double const y = p[1];

    if(component == 0)
      result = U_X_MAX * (y * (H - y) / std::pow(H / 2.0, 2.0)) *
               ((t < T) ? 0.5 * (1.0 - std::cos(t * numbers::PI / T)) : 1.0);

    return result;
  }
};

template<int dim>
class SpatiallyVaryingE : public Function<dim>
{
public:
  SpatiallyVaryingE() : Function<dim>(1, 0.0)
  {
  }

  double
  value(Point<dim> const & p, unsigned int const component = 0) const
  {
    (void)component;

    double const value =
      (std::abs(p[1] - H / 2.) < H / 8.) ? std::cos(4.0 * p[1] / H * numbers::PI) : 0.0;
    double result = 1. + 100. * value * value;

    return result;
  }
};

template<int dim, typename Number>
class Application : public ApplicationBase<dim, Number>
{
public:
  typedef typename ApplicationBase<dim, Number>::PeriodicFaces PeriodicFaces;

  Application(std::string input_file) : ApplicationBase<dim, Number>(input_file)
  {
    // parse application-specific parameters
    ParameterHandler prm;
    this->add_parameters(prm);
    prm.parse_input(input_file, "", true, true);
  }

  void
  set_input_parameters_fluid(IncNS::InputParameters & param)
  {
    using namespace IncNS;

    // MATHEMATICAL MODEL
    param.problem_type                   = ProblemType::Unsteady;
    param.equation_type                  = EquationType::NavierStokes;
    param.formulation_viscous_term       = FormulationViscousTerm::LaplaceFormulation;
    param.formulation_convective_term    = FormulationConvectiveTerm::ConvectiveFormulation;
    param.use_outflow_bc_convective_term = true;
    param.right_hand_side                = false;

    // ALE
    param.ale_formulation                     = true;
    param.mesh_movement_type                  = MeshMovementType::Elasticity;
    param.neumann_with_variable_normal_vector = false;

    // PHYSICAL QUANTITIES
    param.start_time = 0.0;
    param.end_time   = END_TIME;
    param.viscosity  = FLUID_VISCOSITY;
    param.density    = FLUID_DENSITY;

    // TEMPORAL DISCRETIZATION
    param.solver_type                     = SolverType::Unsteady;
    param.temporal_discretization         = TemporalDiscretization::BDFDualSplittingScheme;
    param.treatment_of_convective_term    = TreatmentOfConvectiveTerm::Explicit;
    param.order_time_integrator           = 2;
    param.start_with_low_order            = true;
    param.adaptive_time_stepping          = true;
    param.calculation_of_time_step_size   = TimeStepCalculation::CFL;
    param.time_step_size                  = END_TIME;
    param.max_velocity                    = U_X_MAX;
    param.cfl                             = 0.5;
    param.cfl_exponent_fe_degree_velocity = 1.5;

    // output of solver information
    param.solver_info_data.interval_time_steps = OUTPUT_SOLVER_INFO_EVERY_TIME_STEPS;

    // restart
    param.restarted_simulation             = false;
    param.restart_data.write_restart       = false;
    param.restart_data.interval_time       = 0.25;
    param.restart_data.interval_wall_time  = 1.e6;
    param.restart_data.interval_time_steps = 1e8;
    param.restart_data.filename            = "output/vortex/vortex";


    // SPATIAL DISCRETIZATION
    param.triangulation_type = TriangulationType::Distributed;
    param.degree_p           = DegreePressure::MixedOrder;
    param.mapping            = MappingType::Isoparametric;

    // convective term
    param.upwind_factor = 1.0;

    // viscous term
    param.IP_formulation_viscous = InteriorPenaltyFormulation::SIPG;

    // velocity pressure coupling terms
    param.gradp_formulation = FormulationPressureGradientTerm::Weak;
    param.divu_formulation  = FormulationVelocityDivergenceTerm::Weak;

    // div-div and continuity penalty
    param.use_divergence_penalty                     = true;
    param.divergence_penalty_factor                  = 1.0e0;
    param.use_continuity_penalty                     = true;
    param.continuity_penalty_factor                  = param.divergence_penalty_factor;
    param.continuity_penalty_components              = ContinuityPenaltyComponents::Normal;
    param.continuity_penalty_use_boundary_data       = true;
    param.apply_penalty_terms_in_postprocessing_step = true;

    // NUMERICAL PARAMETERS
    param.implement_block_diagonal_preconditioner_matrix_free = false;
    param.use_cell_based_face_loops                           = false;
    param.quad_rule_linearization = QuadratureRuleLinearization::Overintegration32k;

    // PROJECTION METHODS

    // formulation
    param.store_previous_boundary_values = true;

    // pressure Poisson equation
    param.solver_pressure_poisson              = SolverPressurePoisson::CG;
    param.solver_data_pressure_poisson         = SolverData(1000, ABS_TOL, REL_TOL, 100);
    param.preconditioner_pressure_poisson      = PreconditionerPressurePoisson::Multigrid;
    param.multigrid_data_pressure_poisson.type = MultigridType::cphMG;
    param.multigrid_data_pressure_poisson.smoother_data.smoother = MultigridSmoother::Chebyshev;
    param.multigrid_data_pressure_poisson.smoother_data.preconditioner =
      PreconditionerSmoother::PointJacobi;
    param.multigrid_data_pressure_poisson.coarse_problem.solver = MultigridCoarseGridSolver::CG;
    param.multigrid_data_pressure_poisson.coarse_problem.preconditioner =
      MultigridCoarseGridPreconditioner::AMG;

    // projection step
    param.solver_projection         = SolverProjection::CG;
    param.solver_data_projection    = SolverData(1000, ABS_TOL, REL_TOL);
    param.preconditioner_projection = PreconditionerProjection::InverseMassMatrix;

    // HIGH-ORDER DUAL SPLITTING SCHEME

    // formulations
    param.order_extrapolation_pressure_nbc =
      param.order_time_integrator <= 2 ? param.order_time_integrator : 2;
    param.formulation_convective_term_bc = FormulationConvectiveTerm::ConvectiveFormulation;

    // viscous step
    param.solver_viscous         = SolverViscous::CG;
    param.solver_data_viscous    = SolverData(1000, ABS_TOL, REL_TOL);
    param.preconditioner_viscous = PreconditionerViscous::InverseMassMatrix;


    // PRESSURE-CORRECTION SCHEME

    // formulation
    param.order_pressure_extrapolation =
      std::min(2, (int)param.order_time_integrator) - 1; // J_p = J-1, but not larger than 1
    param.rotational_formulation = true;

    // momentum step

    // Newton solver
    param.newton_solver_data_momentum = Newton::SolverData(100, ABS_TOL, REL_TOL);

    // linear solver
    param.solver_momentum = SolverMomentum::FGMRES;
    if(param.treatment_of_convective_term == TreatmentOfConvectiveTerm::Implicit)
      param.solver_data_momentum = SolverData(1e4, ABS_TOL_LINEARIZED, REL_TOL_LINEARIZED, 100);
    else
      param.solver_data_momentum = SolverData(1e4, ABS_TOL, REL_TOL, 100);
    param.update_preconditioner_momentum = false;
    param.preconditioner_momentum        = MomentumPreconditioner::InverseMassMatrix;


    // COUPLED NAVIER-STOKES SOLVER
    param.use_scaling_continuity = false;

    // nonlinear solver (Newton solver)
    param.newton_solver_data_coupled = Newton::SolverData(100, ABS_TOL, REL_TOL);

    // linear solver
    param.solver_coupled = SolverCoupled::FGMRES;
    if(param.treatment_of_convective_term == TreatmentOfConvectiveTerm::Implicit)
      param.solver_data_coupled = SolverData(1e4, ABS_TOL_LINEARIZED, REL_TOL_LINEARIZED, 100);
    else
      param.solver_data_coupled = SolverData(1e4, ABS_TOL, REL_TOL, 100);

    // preconditioner linear solver
    param.preconditioner_coupled        = PreconditionerCoupled::BlockTriangular;
    param.update_preconditioner_coupled = false;

    // preconditioner momentum block
    param.preconditioner_velocity_block = MomentumPreconditioner::InverseMassMatrix;

    // preconditioner Schur-complement block
    param.preconditioner_pressure_block =
      SchurComplementPreconditioner::PressureConvectionDiffusion;
  }

  void create_triangulation_fluid(Triangulation<2> & tria)
  {
    std::vector<Triangulation<2>> tria_vec;
    tria_vec.resize(11);

    GridGenerator::general_cell(tria_vec[0],
                                {Point<2>(X_0, 0.0),
                                 Point<2>(X_C - R / std::sqrt(2.0), Y_C - R / std::sqrt(2.0)),
                                 Point<2>(X_0, H),
                                 Point<2>(X_C - R / std::sqrt(2.0), Y_C + R / std::sqrt(2.0))});

    GridGenerator::general_cell(tria_vec[1],
                                {Point<2>(X_0, 0.0),
                                 Point<2>(X_2, 0.0),
                                 Point<2>(X_C - R / std::sqrt(2.0), Y_C - R / std::sqrt(2.0)),
                                 Point<2>(X_C + R / std::sqrt(2.0), Y_C - R / std::sqrt(2.0))});

    GridGenerator::general_cell(tria_vec[2],
                                {Point<2>(X_C - R / std::sqrt(2.0), Y_C + R / std::sqrt(2.0)),
                                 Point<2>(X_C + R / std::sqrt(2.0), Y_C + R / std::sqrt(2.0)),
                                 Point<2>(X_0, H),
                                 Point<2>(X_2, H)});

    GridGenerator::general_cell(tria_vec[3],
                                {Point<2>(X_C + R / std::sqrt(2.0), Y_C - R / std::sqrt(2.0)),
                                 Point<2>(X_2, 0.0),
                                 Point<2>(X_C + R * std::cos(std::asin(T / (2.0 * R))),
                                          Y_C - T / 2.0),
                                 Point<2>(X_2, Y_C - T / 2.0)});

    GridGenerator::general_cell(tria_vec[4],
                                {Point<2>(X_C + R * std::cos(std::asin(T / (2.0 * R))),
                                          Y_C + T / 2.0),
                                 Point<2>(X_2, Y_C + T / 2.0),
                                 Point<2>(X_C + R / std::sqrt(2.0), Y_C + R / std::sqrt(2.0)),
                                 Point<2>(X_2, H)});

    GridGenerator::subdivided_hyper_rectangle(tria_vec[5],
                                              {1, 1} /* subdivisions x,y */,
                                              Point<2>(X_2, 0.0),
                                              Point<2>(X_C + R + L_FLAG, Y_C - T / 2.0));

    GridGenerator::subdivided_hyper_rectangle(tria_vec[6],
                                              {1, 1} /* subdivisions x,y */,
                                              Point<2>(X_2, Y_C + T / 2.0),
                                              Point<2>(X_C + R + L_FLAG, H));

    GridGenerator::general_cell(tria_vec[7],
                                {Point<2>(X_C + R + L_FLAG, 0.0),
                                 Point<2>(X_3, 0.0),
                                 Point<2>(X_C + R + L_FLAG, Y_C - T / 2.0),
                                 Point<2>(X_3, Y_3)});

    GridGenerator::general_cell(tria_vec[8],
                                {Point<2>(X_C + R + L_FLAG, Y_C + T / 2.0),
                                 Point<2>(X_3, 2.0 * Y_3),
                                 Point<2>(X_C + R + L_FLAG, H),
                                 Point<2>(X_3, H)});

    GridGenerator::general_cell(tria_vec[9],
                                {Point<2>(X_C + R + L_FLAG, Y_C - T / 2.0),
                                 Point<2>(X_3, Y_3),
                                 Point<2>(X_C + R + L_FLAG, Y_C + T / 2.0),
                                 Point<2>(X_3, 2.0 * Y_3)});

    GridGenerator::subdivided_hyper_rectangle(tria_vec[10],
                                              {8, 3} /* subdivisions x,y */,
                                              Point<2>(X_3, 0.0),
                                              Point<2>(L, H));

    std::vector<Triangulation<2> const *> tria_vec_ptr(tria_vec.size());
    for(unsigned int i = 0; i < tria_vec.size(); ++i)
      tria_vec_ptr[i] = &tria_vec[i];

    GridGenerator::merge_triangulations(tria_vec_ptr, tria);
  }

  void create_triangulation_fluid(Triangulation<3> & tria)
  {
    (void)tria;

    AssertThrow(false, ExcMessage("not implemented."));
  }

  void
  create_grid_fluid(std::shared_ptr<Triangulation<dim>> triangulation,
                    PeriodicFaces &                     periodic_faces,
                    unsigned int const                  n_refine_space,
                    std::shared_ptr<Mapping<dim>> &     mapping,
                    unsigned int const                  mapping_degree) final
  {
    (void)periodic_faces;

    create_triangulation_fluid(*triangulation);

    triangulation->set_all_manifold_ids(0);

    // vectors of manifold_ids and face_ids
    unsigned int const        manifold_id_start = 10;
    std::vector<unsigned int> manifold_ids;
    std::vector<unsigned int> face_ids;

    Point<dim> center;
    center[0] = X_C;
    center[1] = Y_C;

    for(auto cell : triangulation->active_cell_iterators())
    {
      for(unsigned int f = 0; f < GeometryInfo<dim>::faces_per_cell; ++f)
      {
        double const x   = cell->face(f)->center()(0);
        double const y   = cell->face(f)->center()(1);
        double const TOL = 1.e-12;

        if(std::fabs(y - Y_0) < TOL || std::fabs(y - H) < TOL)
        {
          cell->face(f)->set_boundary_id(BOUNDARY_ID_WALLS);
        }

        if(std::fabs(x - X_0) < TOL)
        {
          cell->face(f)->set_boundary_id(BOUNDARY_ID_INFLOW);
        }

        if(std::fabs(x - L) < TOL)
        {
          cell->face(f)->set_boundary_id(BOUNDARY_ID_OUTFLOW);
        }

        if(std::fabs(cell->face(f)->center().distance(center)) < R + TOL)
        {
          cell->face(f)->set_boundary_id(BOUNDARY_ID_CYLINDER);
        }

        if(std::fabs(y - (Y_C - T / 2.0)) < TOL || std::fabs(y - (Y_C + T / 2.0)) < TOL ||
           (std::fabs(y - Y_C) < T / 2.0 + TOL && std::fabs(x - (X_C + R + L_FLAG)) < TOL))
        {
          cell->face(f)->set_boundary_id(BOUNDARY_ID_FLAG);
        }

        // manifold IDs
        for(unsigned int f = 0; f < GeometryInfo<2>::faces_per_cell; ++f)
        {
          bool face_at_sphere_boundary = cell->face(f)->at_boundary();
          for(unsigned int v = 0; v < GeometryInfo<2 - 1>::vertices_per_cell; ++v)
          {
            if(std::abs(center.distance(cell->face(f)->vertex(v)) - R) > TOL)
              face_at_sphere_boundary = false;
          }
          if(face_at_sphere_boundary)
          {
            face_ids.push_back(f);
            unsigned int manifold_id = manifold_id_start + manifold_ids.size() + 1;
            cell->set_all_manifold_ids(manifold_id);
            manifold_ids.push_back(manifold_id);
          }
        }
      }
    }

    // generate vector of manifolds and apply manifold to all cells that have been marked
    static std::vector<std::shared_ptr<Manifold<dim>>> manifold_vec;
    manifold_vec.resize(manifold_ids.size());

    for(unsigned int i = 0; i < manifold_ids.size(); ++i)
    {
      for(auto cell : triangulation->active_cell_iterators())
      {
        if(cell->manifold_id() == manifold_ids[i])
        {
          manifold_vec[i] = std::shared_ptr<Manifold<dim>>(static_cast<Manifold<dim> *>(
            new OneSidedCylindricalManifold<dim>(cell, face_ids[i], center)));
          triangulation->set_manifold(manifold_ids[i], *(manifold_vec[i]));
        }
      }
    }

    triangulation->refine_global(n_refine_space);

    mapping.reset(new MappingQGeneric<dim>(mapping_degree));
  }

  void
  set_boundary_conditions_fluid(
    std::shared_ptr<IncNS::BoundaryDescriptorU<dim>> boundary_descriptor_velocity,
    std::shared_ptr<IncNS::BoundaryDescriptorP<dim>> boundary_descriptor_pressure) final
  {
    typedef typename std::pair<types::boundary_id, std::shared_ptr<Function<dim>>> pair;
    typedef typename std::pair<types::boundary_id, std::shared_ptr<FunctionCached<1, dim>>>
      pair_fsi;

    // fill boundary descriptor velocity
    boundary_descriptor_velocity->dirichlet_bc.insert(
      pair(BOUNDARY_ID_WALLS, new Functions::ZeroFunction<dim>(dim)));
    boundary_descriptor_velocity->dirichlet_bc.insert(
      pair(BOUNDARY_ID_INFLOW, new InflowBC<dim>()));
    boundary_descriptor_velocity->neumann_bc.insert(
      pair(BOUNDARY_ID_OUTFLOW, new Functions::ZeroFunction<dim>(dim)));
    boundary_descriptor_velocity->dirichlet_bc.insert(
      pair(BOUNDARY_ID_CYLINDER, new Functions::ZeroFunction<dim>(dim)));
    // fluid-structure interface
    boundary_descriptor_velocity->dirichlet_mortar_bc.insert(
      pair_fsi(BOUNDARY_ID_FLAG, new FunctionCached<1, dim>()));

    // fill boundary descriptor pressure
    boundary_descriptor_pressure->neumann_bc.insert(
      pair(BOUNDARY_ID_WALLS, new Functions::ZeroFunction<dim>(dim)));
    boundary_descriptor_pressure->neumann_bc.insert(
      pair(BOUNDARY_ID_INFLOW, new Functions::ZeroFunction<dim>(dim)));
    boundary_descriptor_pressure->dirichlet_bc.insert(
      pair(BOUNDARY_ID_OUTFLOW, new Functions::ZeroFunction<dim>(1)));
    boundary_descriptor_pressure->neumann_bc.insert(
      pair(BOUNDARY_ID_CYLINDER, new Functions::ZeroFunction<dim>(dim)));
    boundary_descriptor_pressure->neumann_bc.insert(
      pair(BOUNDARY_ID_FLAG, new Functions::ZeroFunction<dim>(dim)));
  }

  void
  set_field_functions_fluid(std::shared_ptr<IncNS::FieldFunctions<dim>> field_functions) final
  {
    field_functions->initial_solution_velocity.reset(new Functions::ZeroFunction<dim>(dim));
    field_functions->initial_solution_pressure.reset(new Functions::ZeroFunction<dim>(1));
    field_functions->analytical_solution_pressure.reset(new Functions::ZeroFunction<dim>(1));
    field_functions->right_hand_side.reset(new Functions::ZeroFunction<dim>(dim));
  }

  std::shared_ptr<IncNS::PostProcessorBase<dim, Number>>
  construct_postprocessor_fluid(unsigned int const degree, MPI_Comm const & mpi_comm) final
  {
    IncNS::PostProcessorData<dim> pp_data;

    // write output for visualization of results
    pp_data.output_data.write_output       = this->write_output;
    pp_data.output_data.directory          = this->output_directory + "vtu/";
    pp_data.output_data.filename           = this->output_name + "_fluid";
    pp_data.output_data.write_boundary_IDs = true;
    pp_data.output_data.write_processor_id = true;
    pp_data.output_data.start_time         = 0.0;
    pp_data.output_data.interval_time      = OUTPUT_INTERVAL_TIME;
    pp_data.output_data.write_higher_order = true;
    pp_data.output_data.degree             = degree;

    std::shared_ptr<IncNS::PostProcessorBase<dim, Number>> pp;
    pp.reset(new IncNS::PostProcessor<dim, Number>(pp_data, mpi_comm));

    return pp;
  }

  void
  set_input_parameters_ale(Poisson::InputParameters & param)
  {
    using namespace Poisson;

    // MATHEMATICAL MODEL
    param.right_hand_side = false;

    // SPATIAL DISCRETIZATION
    param.triangulation_type     = TriangulationType::Distributed;
    param.mapping                = MappingType::Isoparametric;
    param.spatial_discretization = SpatialDiscretization::CG;

    // SOLVER
    param.solver         = Poisson::Solver::FGMRES;
    param.solver_data    = SolverData(1e4, ABS_TOL, REL_TOL, 100);
    param.preconditioner = Preconditioner::Multigrid;

    param.multigrid_data.type                          = MultigridType::phMG;
    param.multigrid_data.p_sequence                    = PSequenceType::Bisect;
    param.multigrid_data.smoother_data.smoother        = MultigridSmoother::Chebyshev;
    param.multigrid_data.coarse_problem.solver         = MultigridCoarseGridSolver::CG;
    param.multigrid_data.coarse_problem.preconditioner = MultigridCoarseGridPreconditioner::AMG;
  }

  void set_boundary_conditions_ale(
    std::shared_ptr<Poisson::BoundaryDescriptor<1, dim>> boundary_descriptor) final
  {
    typedef typename std::pair<types::boundary_id, std::shared_ptr<Function<dim>>> pair;
    typedef typename std::pair<types::boundary_id, std::shared_ptr<FunctionCached<1, dim>>>
      pair_fsi;

    boundary_descriptor->dirichlet_bc.insert(
      pair(BOUNDARY_ID_WALLS, new Functions::ZeroFunction<dim>(dim)));
    boundary_descriptor->dirichlet_bc.insert(
      pair(BOUNDARY_ID_INFLOW, new Functions::ZeroFunction<dim>(dim)));
    boundary_descriptor->dirichlet_bc.insert(
      pair(BOUNDARY_ID_OUTFLOW, new Functions::ZeroFunction<dim>(dim)));
    boundary_descriptor->dirichlet_bc.insert(
      pair(BOUNDARY_ID_CYLINDER, new Functions::ZeroFunction<dim>(dim)));

    // fluid-structure interface
    boundary_descriptor->dirichlet_mortar_bc.insert(
      pair_fsi(BOUNDARY_ID_FLAG, new FunctionCached<1, dim>()));
  }


  void
  set_field_functions_ale(std::shared_ptr<Poisson::FieldFunctions<dim>> field_functions) final
  {
    field_functions->initial_solution.reset(new Functions::ZeroFunction<dim>(dim));
    field_functions->right_hand_side.reset(new Functions::ZeroFunction<dim>(dim));
  }

  void
  set_input_parameters_ale(Structure::InputParameters & parameters)
  {
    using namespace Structure;

    parameters.problem_type         = ProblemType::Steady;
    parameters.body_force           = false;
    parameters.pull_back_body_force = false;
    parameters.large_deformation    = false;
    parameters.pull_back_traction   = false;

    parameters.triangulation_type = TriangulationType::Distributed;
    parameters.mapping            = MappingType::Isoparametric;

    parameters.newton_solver_data = Newton::SolverData(1e4, ABS_TOL, REL_TOL);
    parameters.solver             = Structure::Solver::FGMRES;
    if(parameters.large_deformation)
      parameters.solver_data = SolverData(1e4, ABS_TOL_LINEARIZED, REL_TOL_LINEARIZED, 100);
    else
      parameters.solver_data = SolverData(1e4, ABS_TOL, REL_TOL, 100);
    parameters.preconditioner                       = Preconditioner::Multigrid;
    parameters.multigrid_data.type                  = MultigridType::phMG;
    parameters.multigrid_data.coarse_problem.solver = MultigridCoarseGridSolver::CG;
    parameters.multigrid_data.coarse_problem.preconditioner =
      MultigridCoarseGridPreconditioner::AMG;

    parameters.update_preconditioner                         = parameters.large_deformation;
    parameters.update_preconditioner_every_newton_iterations = 10;
  }

  void
  set_boundary_conditions_ale(
    std::shared_ptr<Structure::BoundaryDescriptor<dim>> boundary_descriptor) final
  {
    typedef typename std::pair<types::boundary_id, std::shared_ptr<Function<dim>>> pair;
    typedef typename std::pair<types::boundary_id, ComponentMask>                  pair_mask;

    typedef typename std::pair<types::boundary_id, std::shared_ptr<FunctionCached<1, dim>>>
      pair_fsi;

    boundary_descriptor->dirichlet_bc.insert(
      pair(BOUNDARY_ID_WALLS, new Functions::ZeroFunction<dim>(dim)));
    boundary_descriptor->dirichlet_bc_component_mask.insert(
      pair_mask(BOUNDARY_ID_WALLS, ComponentMask()));
    boundary_descriptor->dirichlet_bc.insert(
      pair(BOUNDARY_ID_INFLOW, new Functions::ZeroFunction<dim>(dim)));
    boundary_descriptor->dirichlet_bc_component_mask.insert(
      pair_mask(BOUNDARY_ID_INFLOW, ComponentMask()));
    boundary_descriptor->dirichlet_bc.insert(
      pair(BOUNDARY_ID_OUTFLOW, new Functions::ZeroFunction<dim>(dim)));
    boundary_descriptor->dirichlet_bc_component_mask.insert(
      pair_mask(BOUNDARY_ID_OUTFLOW, ComponentMask()));
    boundary_descriptor->dirichlet_bc.insert(
      pair(BOUNDARY_ID_CYLINDER, new Functions::ZeroFunction<dim>(dim)));
    boundary_descriptor->dirichlet_bc_component_mask.insert(
      pair_mask(BOUNDARY_ID_CYLINDER, ComponentMask()));

    // fluid-structure interface
    boundary_descriptor->dirichlet_mortar_bc.insert(
      pair_fsi(BOUNDARY_ID_FLAG, new FunctionCached<1, dim>()));
  }

  void
  set_material_ale(Structure::MaterialDescriptor & material_descriptor)
  {
    using namespace Structure;

    typedef std::pair<types::material_id, std::shared_ptr<MaterialData>> Pair;

    MaterialType const type         = MaterialType::StVenantKirchhoff;
    Type2D const       two_dim_type = Type2D::PlaneStrain;

    double const                   E       = 1.0;
    double const                   poisson = 0.3;
    std::shared_ptr<Function<dim>> E_function;
    E_function.reset(new SpatiallyVaryingE<dim>());
    material_descriptor.insert(
      Pair(0, new StVenantKirchhoffData<dim>(type, E, poisson, two_dim_type, E_function)));
  }

  void
  set_field_functions_ale(std::shared_ptr<Structure::FieldFunctions<dim>> field_functions) final
  {
    field_functions->right_hand_side.reset(new Functions::ZeroFunction<dim>(dim));
    field_functions->initial_displacement.reset(new Functions::ZeroFunction<dim>(dim));
    field_functions->initial_velocity.reset(new Functions::ZeroFunction<dim>(dim));
  }

  // Structure
  void
  set_input_parameters_structure(Structure::InputParameters & parameters)
  {
    using namespace Structure;

    parameters.problem_type         = ProblemType::Unsteady;
    parameters.body_force           = false;
    parameters.pull_back_body_force = false;
    parameters.large_deformation    = true;
    parameters.pull_back_traction   = true;

    parameters.density = DENSITY_STRUCTURE;

    parameters.start_time                           = 0.0;
    parameters.end_time                             = END_TIME;
    parameters.time_step_size                       = END_TIME / 100.0;
    parameters.gen_alpha_type                       = GenAlphaType::BossakAlpha;
    parameters.spectral_radius                      = 0.8;
    parameters.solver_info_data.interval_time_steps = OUTPUT_SOLVER_INFO_EVERY_TIME_STEPS;

    parameters.triangulation_type = TriangulationType::Distributed;
    parameters.mapping            = MappingType::Isoparametric;

    parameters.newton_solver_data = Newton::SolverData(1e4, ABS_TOL, REL_TOL);
    parameters.solver             = Structure::Solver::FGMRES;
    if(parameters.large_deformation)
      parameters.solver_data = SolverData(1e4, ABS_TOL_LINEARIZED, REL_TOL_LINEARIZED, 100);
    else
      parameters.solver_data = SolverData(1e4, ABS_TOL, REL_TOL, 100);
    parameters.preconditioner                       = Preconditioner::Multigrid;
    parameters.multigrid_data.type                  = MultigridType::phMG;
    parameters.multigrid_data.coarse_problem.solver = MultigridCoarseGridSolver::CG;
    parameters.multigrid_data.coarse_problem.preconditioner =
      MultigridCoarseGridPreconditioner::AMG;

    parameters.update_preconditioner                         = true;
    parameters.update_preconditioner_every_time_steps        = 10;
    parameters.update_preconditioner_every_newton_iterations = 10;
  }

  void create_triangulation_structure(Triangulation<2> & tria)
  {
    if(STRUCTURE_COVERS_FLAG_ONLY)
    {
      GridGenerator::subdivided_hyper_rectangle(
        tria,
        {N_CELLS_FLAG_X, 1} /* subdivisions x,y */,
        Point<2>(X_C + R * std::cos(std::asin(T / (2.0 * R))), Y_C - T / 2.0),
        Point<2>(X_C + R + L_FLAG, Y_C + T / 2.0));
    }
    else
    {
      std::vector<Triangulation<2>> tria_vec;

      tria_vec.resize(12);

      GridGenerator::general_cell(
        tria_vec[0],
        {Point<2>(X_C + R / std::sqrt(2.0), Y_C - R / std::sqrt(2.0)),
         Point<2>(X_C + R * std::cos(std::asin(T / (2.0 * R))), Y_C - T / 2.0),
         Point<2>(X_C + T / 2.0, Y_C - T),
         Point<2>(X_C + T / 2.0 +
                    (X_C + R * std::cos(std::asin(T / (2.0 * R))) - (X_C + T / 2.0)) / 2.0,
                  Y_C - T / 4.0)});

      GridGenerator::general_cell(
        tria_vec[1],
        {Point<2>(X_C + T / 2.0, Y_C - T),
         Point<2>(X_C + T / 2.0 +
                    (X_C + R * std::cos(std::asin(T / (2.0 * R))) - (X_C + T / 2.0)) / 2.0,
                  Y_C - T / 4.0),
         Point<2>(X_C + T / 2.0, Y_C + T),
         Point<2>(X_C + T / 2.0 +
                    (X_C + R * std::cos(std::asin(T / (2.0 * R))) - (X_C + T / 2.0)) / 2.0,
                  Y_C + T / 4.0)});

      GridGenerator::general_cell(
        tria_vec[2],
        {Point<2>(X_C + T / 2.0 +
                    (X_C + R * std::cos(std::asin(T / (2.0 * R))) - (X_C + T / 2.0)) / 2.0,
                  Y_C - T / 4.0),
         Point<2>(X_C + R * std::cos(std::asin(T / (2.0 * R))), Y_C - T / 2.0),
         Point<2>(X_C + T / 2.0 +
                    (X_C + R * std::cos(std::asin(T / (2.0 * R))) - (X_C + T / 2.0)) / 2.0,
                  Y_C + T / 4.0),
         Point<2>(X_C + R * std::cos(std::asin(T / (2.0 * R))), Y_C + T / 2.0)});

      GridGenerator::general_cell(
        tria_vec[3],
        {Point<2>(X_C + T / 2.0 +
                    (X_C + R * std::cos(std::asin(T / (2.0 * R))) - (X_C + T / 2.0)) / 2.0,
                  Y_C + T / 4.0),
         Point<2>(X_C + R * std::cos(std::asin(T / (2.0 * R))), Y_C + T / 2.0),
         Point<2>(X_C + T / 2.0, Y_C + T),
         Point<2>(X_C + R / std::sqrt(2.0), Y_C + R / std::sqrt(2.0))});

      GridGenerator::general_cell(tria_vec[4],
                                  {Point<2>(X_C - T / 2.0, Y_C - T),
                                   Point<2>(X_C + T / 2.0, Y_C - T),
                                   Point<2>(X_C - T / 2.0, Y_C + T),
                                   Point<2>(X_C + T / 2.0, Y_C + T)});

      GridGenerator::general_cell(tria_vec[5],
                                  {Point<2>(X_C - R / std::sqrt(2.0), Y_C - R / std::sqrt(2.0)),
                                   Point<2>(X_C + R / std::sqrt(2.0), Y_C - R / std::sqrt(2.0)),
                                   Point<2>(X_C - T / 2.0, Y_C - T),
                                   Point<2>(X_C + T / 2.0, Y_C - T)});

      GridGenerator::general_cell(tria_vec[6],
                                  {Point<2>(X_C - T / 2.0, Y_C + T),
                                   Point<2>(X_C + T / 2.0, Y_C + T),
                                   Point<2>(X_C - R / std::sqrt(2.0), Y_C + R / std::sqrt(2.0)),
                                   Point<2>(X_C + R / std::sqrt(2.0), Y_C + R / std::sqrt(2.0))});

      GridGenerator::general_cell(
        tria_vec[7],
        {Point<2>(X_C - R / std::sqrt(2.0), Y_C - R / std::sqrt(2.0)),
         Point<2>(X_C - T / 2.0, Y_C - T),
         Point<2>(X_C - R * std::cos(std::asin(T / (2.0 * R))), Y_C - T / 2.0),
         Point<2>(X_C - T / 2.0 -
                    (X_C + R * std::cos(std::asin(T / (2.0 * R))) - (X_C + T / 2.0)) / 2.0,
                  Y_C - T / 4.0)});

      GridGenerator::general_cell(
        tria_vec[8],
        {Point<2>(X_C - T / 2.0 -
                    (X_C + R * std::cos(std::asin(T / (2.0 * R))) - (X_C + T / 2.0)) / 2.0,
                  Y_C - T / 4.0),
         Point<2>(X_C - T / 2.0, Y_C - T),
         Point<2>(X_C - T / 2.0 -
                    (X_C + R * std::cos(std::asin(T / (2.0 * R))) - (X_C + T / 2.0)) / 2.0,
                  Y_C + T / 4.0),
         Point<2>(X_C - T / 2.0, Y_C + T)});

      GridGenerator::general_cell(
        tria_vec[9],
        {Point<2>(X_C - R * std::cos(std::asin(T / (2.0 * R))), Y_C - T / 2.0),
         Point<2>(X_C - T / 2.0 -
                    (X_C + R * std::cos(std::asin(T / (2.0 * R))) - (X_C + T / 2.0)) / 2.0,
                  Y_C - T / 4.0),
         Point<2>(X_C - R * std::cos(std::asin(T / (2.0 * R))), Y_C + T / 2.0),
         Point<2>(X_C - T / 2.0 -
                    (X_C + R * std::cos(std::asin(T / (2.0 * R))) - (X_C + T / 2.0)) / 2.0,
                  Y_C + T / 4.0)});

      GridGenerator::general_cell(
        tria_vec[10],
        {Point<2>(X_C - R * std::cos(std::asin(T / (2.0 * R))), Y_C + T / 2.0),
         Point<2>(X_C - T / 2.0 -
                    (X_C + R * std::cos(std::asin(T / (2.0 * R))) - (X_C + T / 2.0)) / 2.0,
                  Y_C + T / 4.0),
         Point<2>(X_C - R / std::sqrt(2.0), Y_C + R / std::sqrt(2.0)),
         Point<2>(X_C - T / 2.0, Y_C + T)});

      GridGenerator::subdivided_hyper_rectangle(
        tria_vec[11],
        {8, 1} /* subdivisions x,y */,
        Point<2>(X_C + R * std::cos(std::asin(T / (2.0 * R))), Y_C - T / 2.0),
        Point<2>(X_C + R + L_FLAG, Y_C + T / 2.0));

      std::vector<Triangulation<2> const *> tria_vec_ptr(tria_vec.size());
      for(unsigned int i = 0; i < tria_vec.size(); ++i)
        tria_vec_ptr[i] = &tria_vec[i];

      GridGenerator::merge_triangulations(tria_vec_ptr, tria);
    }
  }

  void create_triangulation_structure(Triangulation<3> & tria)
  {
    (void)tria;

    AssertThrow(false, ExcMessage("not implemented."));
  }

  void
  create_grid_structure(std::shared_ptr<Triangulation<dim>> triangulation,
                        PeriodicFaces &                     periodic_faces,
                        unsigned int const                  n_refine_space,
                        std::shared_ptr<Mapping<dim>> &     mapping,
                        unsigned int const                  mapping_degree) final
  {
    (void)periodic_faces;

    create_triangulation_structure(*triangulation);

    triangulation->set_all_manifold_ids(0);

    // vectors of manifold_ids and face_ids
    unsigned int const        manifold_id_start = 10;
    std::vector<unsigned int> manifold_ids;
    std::vector<unsigned int> face_ids;

    Point<dim> center;
    center[0] = X_C;
    center[1] = Y_C;

    for(auto cell : triangulation->active_cell_iterators())
    {
      for(unsigned int f = 0; f < GeometryInfo<dim>::faces_per_cell; ++f)
      {
        double const x   = cell->face(f)->center()(0);
        double const TOL = 1.e-12;

        if(STRUCTURE_COVERS_FLAG_ONLY)
        {
          if(cell->face(f)->at_boundary() &&
             (x < X_C + R * std::cos(std::asin(T / (2.0 * R))) + TOL))
          {
            cell->face(f)->set_boundary_id(BOUNDARY_ID_CYLINDER);
          }
        }
        else
        {
          if(cell->face(f)->at_boundary() && (x < X_C + R * std::cos(std::asin(T / (2.0 * R)))))
          {
            cell->face(f)->set_boundary_id(BOUNDARY_ID_CYLINDER);
          }
        }

        if(cell->face(f)->at_boundary() && (x > X_C + R * std::cos(std::asin(T / (2.0 * R)))))
        {
          cell->face(f)->set_boundary_id(BOUNDARY_ID_FLAG);
        }

        // manifold IDs
        for(unsigned int f = 0; f < GeometryInfo<2>::faces_per_cell; ++f)
        {
          bool face_at_sphere_boundary = cell->face(f)->at_boundary();
          for(unsigned int v = 0; v < GeometryInfo<2 - 1>::vertices_per_cell; ++v)
          {
            if(std::abs(center.distance(cell->face(f)->vertex(v)) - R) > TOL)
              face_at_sphere_boundary = false;
          }
          if(face_at_sphere_boundary)
          {
            face_ids.push_back(f);
            unsigned int manifold_id = manifold_id_start + manifold_ids.size() + 1;
            cell->set_all_manifold_ids(manifold_id);
            manifold_ids.push_back(manifold_id);
          }
        }
      }
    }

    // generate vector of manifolds and apply manifold to all cells that have been marked
    static std::vector<std::shared_ptr<Manifold<dim>>> manifold_vec;
    manifold_vec.resize(manifold_ids.size());

    for(unsigned int i = 0; i < manifold_ids.size(); ++i)
    {
      for(auto cell : triangulation->active_cell_iterators())
      {
        if(cell->manifold_id() == manifold_ids[i])
        {
          manifold_vec[i] = std::shared_ptr<Manifold<dim>>(static_cast<Manifold<dim> *>(
            new OneSidedCylindricalManifold<dim>(cell, face_ids[i], center)));
          triangulation->set_manifold(manifold_ids[i], *(manifold_vec[i]));
        }
      }
    }

    triangulation->refine_global(n_refine_space);

    mapping.reset(new MappingQGeneric<dim>(mapping_degree));
  }

  void
  set_boundary_conditions_structure(
    std::shared_ptr<Structure::BoundaryDescriptor<dim>> boundary_descriptor) final
  {
    typedef typename std::pair<types::boundary_id, std::shared_ptr<Function<dim>>> pair;
    typedef typename std::pair<types::boundary_id, ComponentMask>                  pair_mask;

    typedef typename std::pair<types::boundary_id, std::shared_ptr<FunctionCached<1, dim>>>
      pair_fsi;

    boundary_descriptor->dirichlet_bc.insert(
      pair(BOUNDARY_ID_CYLINDER, new Functions::ZeroFunction<dim>(dim)));
    boundary_descriptor->dirichlet_bc_component_mask.insert(
      pair_mask(BOUNDARY_ID_CYLINDER, ComponentMask()));

    // fluid-structure interface
    boundary_descriptor->neumann_mortar_bc.insert(
      pair_fsi(BOUNDARY_ID_FLAG, new FunctionCached<1, dim>()));
  }

  void
  set_field_functions_structure(
    std::shared_ptr<Structure::FieldFunctions<dim>> field_functions) final
  {
    field_functions->right_hand_side.reset(new Functions::ZeroFunction<dim>(dim));

    field_functions->initial_displacement.reset(new Functions::ZeroFunction<dim>(dim));
    field_functions->initial_velocity.reset(new Functions::ZeroFunction<dim>(dim));
  }

  void
  set_material_structure(Structure::MaterialDescriptor & material_descriptor)
  {
    using namespace Structure;

    typedef std::pair<types::material_id, std::shared_ptr<MaterialData>> Pair;

    MaterialType const type         = MaterialType::StVenantKirchhoff;
    Type2D const       two_dim_type = Type2D::PlaneStrain;

    material_descriptor.insert(Pair(
      0, new StVenantKirchhoffData<dim>(type, E_STRUCTURE, POISSON_RATIO_STRUCTURE, two_dim_type)));
  }

  std::shared_ptr<Structure::PostProcessor<dim, Number>>
  construct_postprocessor_structure(unsigned int const degree, MPI_Comm const & mpi_comm) final
  {
    using namespace Structure;

    PostProcessorData<dim> pp_data;
    pp_data.output_data.write_output       = this->write_output;
    pp_data.output_data.directory          = this->output_directory + "vtu/";
    pp_data.output_data.filename           = this->output_name + "_structure";
    pp_data.output_data.write_processor_id = true;
    pp_data.output_data.start_time         = 0.0;
    pp_data.output_data.interval_time      = OUTPUT_INTERVAL_TIME;
    pp_data.output_data.write_higher_order = true;
    pp_data.output_data.degree             = degree;

    std::shared_ptr<PostProcessor<dim, Number>> post(
      new PostProcessor<dim, Number>(pp_data, mpi_comm));

    return post;
  }
};

} // namespace FSI

template<int dim, typename Number>
std::shared_ptr<FSI::ApplicationBase<dim, Number>>
get_application(std::string input_file)
{
  return std::make_shared<FSI::Application<dim, Number>>(input_file);
}

} // namespace ExaDG

#endif /* APPLICATIONS_FSI_CYLINDER_WITH_FLAG_H_ */
