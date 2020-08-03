/*
 * 3D_taylor_green_vortex.h
 *
 *  Created on: Mar 7, 2018
 *      Author: fehn
 */

#ifndef APPLICATIONS_COMPRESSIBLE_NAVIER_STOKES_TEST_CASES_3D_TAYLOR_GREEN_VORTEX_H_
#define APPLICATIONS_COMPRESSIBLE_NAVIER_STOKES_TEST_CASES_3D_TAYLOR_GREEN_VORTEX_H_

#include "grid/periodic_box.h"

namespace CompNS
{
namespace TaylorGreen
{
// set parameters according to Wiart et al. ("Assessment of discontinuous Galerkin method
// for the simulation of vortical flows at high Reynolds number"):

// set Re, rho_0, V_0, L -> calculate viscosity mu
const double Re            = 1600.0;
const double RHO_0         = 1.0;
const double V_0           = 1.0;
const double L             = 1.0;
const double DYN_VISCOSITY = RHO_0 * V_0 * L / Re;

// set R, gamma, Pr -> calculate c_p, lambda
const double R       = 287.0;
const double GAMMA   = 1.4;
const double C_P     = GAMMA / (GAMMA - 1.0) * R;
const double PRANDTL = 0.71; // Pr = mu * c_p / lambda
const double LAMBDA  = DYN_VISCOSITY * C_P / PRANDTL;

// set Ma number -> calculate speed of sound c_0, temperature T_0, pressure p_0
const double MACH           = 0.1;
const double SPEED_OF_SOUND = V_0 / MACH;
const double T_0            = SPEED_OF_SOUND * SPEED_OF_SOUND / GAMMA / R;
const double p_0            = RHO_0 * R * T_0;

const double MAX_VELOCITY        = V_0;
const double CHARACTERISTIC_TIME = L / V_0;

template<int dim>
class InitialSolution : public Function<dim>
{
public:
  InitialSolution(const unsigned int n_components = dim + 2, const double time = 0.)
    : Function<dim>(n_components, time)
  {
  }

  double
  value(const Point<dim> & x, const unsigned int component = 0) const
  {
    double const u1 = V_0 * std::sin(x[0] / L) * std::cos(x[1] / L) * std::cos(x[2] / L);
    double const u2 = -V_0 * std::cos(x[0] / L) * std::sin(x[1] / L) * std::cos(x[2] / L);
    double const p  = p_0 + RHO_0 * V_0 * V_0 / 16.0 *
                             (std::cos(2.0 * x[0] / L) + std::cos(2.0 * x[1] / L)) *
                             (std::cos(2.0 * x[2] / L) + 2.0);
    double const T   = T_0;
    double const rho = p / (R * T);
    double const E   = R / (GAMMA - 1.0) * T /* e = c_v * T */
                     + 0.5 * (u1 * u1 + u2 * u2 /* + u3*u3 with u3=0*/);

    double result = 0.0;

    if(component == 0)
      result = rho;
    else if(component == 1)
      result = rho * u1;
    else if(component == 2)
      result = rho * u2;
    else if(component == 3)
      result = 0.0;
    else if(component == 1 + dim)
      result = rho * E;

    return result;
  }
};

enum class MeshType
{
  Cartesian,
  Curvilinear
};

void
string_to_enum(MeshType & enum_type, std::string const & string_type)
{
  // clang-format off
  if     (string_type == "Cartesian")   enum_type = MeshType::Cartesian;
  else if(string_type == "Curvilinear") enum_type = MeshType::Curvilinear;
  else AssertThrow(false, ExcMessage("Not implemented."));
  // clang-format on
}

template<int dim, typename Number>
class Application : public ApplicationBase<dim, Number>
{
public:
  Application(std::string input_file) : ApplicationBase<dim, Number>(input_file)
  {
    // parse application-specific parameters
    ParameterHandler prm;
    add_parameters(prm);
    prm.parse_input(input_file, "", true, true);

    string_to_enum(mesh_type, mesh_type_string);
  }

  void
  add_parameters(ParameterHandler & prm)
  {
    // clang-format off
     prm.enter_subsection("Application");
       prm.add_parameter("MeshType",         mesh_type_string, "Type of mesh (Cartesian versus curvilinear).", Patterns::Selection("Cartesian|Curvilinear"));
       prm.add_parameter("OutputDirectory",  output_directory, "Directory where output is written.");
       prm.add_parameter("OutputName",       output_name,      "Name of output files.");
     prm.leave_subsection();
    // clang-format on
  }

  std::string mesh_type_string = "Cartesian";
  MeshType    mesh_type        = MeshType::Cartesian;

  double const start_time = 0.0;
  double const end_time   = 20.0 * CHARACTERISTIC_TIME;

  std::string output_directory = "output/compressible_flow/taylor_green/", output_name = "test";

  void
  set_input_parameters(InputParameters & param)
  {
    // MATHEMATICAL MODEL
    param.equation_type   = EquationType::NavierStokes;
    param.right_hand_side = false;

    // PHYSICAL QUANTITIES
    param.start_time            = start_time;
    param.end_time              = end_time;
    param.dynamic_viscosity     = DYN_VISCOSITY;
    param.reference_density     = RHO_0;
    param.heat_capacity_ratio   = GAMMA;
    param.thermal_conductivity  = LAMBDA;
    param.specific_gas_constant = R;
    param.max_temperature       = T_0;

    // TEMPORAL DISCRETIZATION

    // l=4, k=3 (64³), exponent_fe_degree_cfl = 2:
    // Classical RK:
    // classical RK1 (1 stage): CFL_crit = 0.04 - 0.05, costs = stages / CFL = 20 - 25
    // classical RK2 (2 stages):CFL_crit = 0.31  - 0.32, costs = stages / CFL =  6.3 - 6.5
    // classical RK3 (3 stages):CFL_crit = 0.35 - 0.4, costs = stages / CFL =  7.5 - 8.5
    // classical RK4 (4 stages):CFL_crit = 0.4  - 0.45, costs = stages / CFL =  8.9 - 10

    // Kennedy et al.:
    // LowStorageRK3Stage3Reg2: CFL_crit = 0.3 - 0.4, costs = stages / CFL =  7.5 - 10
    // LowStorageRK3Stage4Reg2: CFL_crit = 0.4 - 0.5, costs = stages / CFL =  8 - 10
    // LowStorageRK4Stage5Reg2: CFL_crit = 0.725 - 0.75, costs = stages / CFL =  6.7 - 6.9
    // LowStorageRK4Stage5Reg3: CFL_crit = 0.7 - 0.725, costs = stages / CFL =  6.9 - 7.1
    // LowStorageRK5Stage9Reg2: CFL_crit = 0.8 - 0.9, costs = stages / CFL =  10  - 11.3

    // Toulorge & Desmet: 3rd order scheme with 7 stages currently the most efficient scheme
    // LowStorageRK3Stage7Reg2: CFL_crit = 1.3 - 1.35, costs = stages / CFL =  5.2 - 5.4,
    // computational costs = 0.69 CPUh LowStorageRK4Stage8Reg2: CFL_crit = 1.25 - 1.3, costs =
    // stages / CFL =  6.2 - 6.4

    // Kubatko et al.:
    // SSPRK(7,3): CFL_crit = 1.2 - 1.25, costs = stages / CFL = 5.6 - 5.8
    // SSPRK(8,3): CFL_crit = 1.5 - 1.6, costs = stages / CFL = 5.0 - 5.3, computational costs =
    // 0.77 CPUh SSPRK(8,4): CFL_crit = 1.25 - 1.3, costs = stages / CFL = 6.2 - 6.4

    param.temporal_discretization       = TemporalDiscretization::ExplRK3Stage7Reg2;
    param.order_time_integrator         = 3;
    param.stages                        = 7;
    param.calculation_of_time_step_size = TimeStepCalculation::CFLAndDiffusion;
    param.time_step_size                = 1.0e-3;
    param.max_velocity                  = MAX_VELOCITY;
    param.cfl_number                    = 0.6;
    param.diffusion_number              = 0.02;
    param.exponent_fe_degree_cfl        = 1.5;
    param.exponent_fe_degree_viscous    = 3.0;

    // output of solver information
    param.solver_info_data.interval_time = (param.end_time - param.start_time) / 20;

    // restart
    param.restart_data.write_restart = false;
    param.restart_data.interval_time = 1.0;
    param.restart_data.filename      = output_directory + output_name + "_restart";

    // SPATIAL DISCRETIZATION
    param.triangulation_type    = TriangulationType::Distributed;
    param.mapping               = MappingType::Affine;
    param.n_q_points_convective = QuadratureRule::Overintegration32k;
    param.n_q_points_viscous    = QuadratureRule::Overintegration32k;

    // viscous term
    param.IP_factor = 1.0;

    // NUMERICAL PARAMETERS
    param.use_combined_operator = true;
  }

  void
  create_grid(std::shared_ptr<parallel::TriangulationBase<dim>> triangulation,
              unsigned int const                                n_refine_space,
              std::vector<GridTools::PeriodicFacePair<typename Triangulation<dim>::cell_iterator>> &
                periodic_faces)
  {
    double const pi   = numbers::PI;
    double const left = -pi * L, right = pi * L;
    double const deformation = 0.1;

    bool curvilinear_mesh = false;
    if(mesh_type == MeshType::Cartesian)
    {
      // do nothing
    }
    else if(mesh_type == MeshType::Curvilinear)
    {
      curvilinear_mesh = true;
    }
    else
    {
      AssertThrow(false, ExcMessage("Not implemented."));
    }

    create_periodic_box(triangulation,
                        n_refine_space,
                        periodic_faces,
                        this->n_subdivisions_1d_hypercube,
                        left,
                        right,
                        curvilinear_mesh,
                        deformation);
  }

  void set_boundary_conditions(
    std::shared_ptr<CompNS::BoundaryDescriptor<dim>> /*boundary_descriptor_density*/,
    std::shared_ptr<CompNS::BoundaryDescriptor<dim>> /*boundary_descriptor_velocity*/,
    std::shared_ptr<CompNS::BoundaryDescriptor<dim>> /*boundary_descriptor_pressure*/,
    std::shared_ptr<CompNS::BoundaryDescriptorEnergy<dim>> /*boundary_descriptor_energy*/)
  {
    // test case with periodic BC -> boundary descriptors remain empty
  }

  void
  set_field_functions(std::shared_ptr<CompNS::FieldFunctions<dim>> field_functions)
  {
    field_functions->initial_solution.reset(new InitialSolution<dim>());
    field_functions->right_hand_side_density.reset(new Functions::ZeroFunction<dim>(1));
    field_functions->right_hand_side_velocity.reset(new Functions::ZeroFunction<dim>(dim));
    field_functions->right_hand_side_energy.reset(new Functions::ZeroFunction<dim>(1));
  }

  std::shared_ptr<CompNS::PostProcessorBase<dim, Number>>
  construct_postprocessor(unsigned int const degree, MPI_Comm const & mpi_comm)
  {
    CompNS::PostProcessorData<dim> pp_data;
    pp_data.output_data.output_folder = output_directory + "vtu/";
    pp_data.output_data.output_name   = output_name;
    pp_data.calculate_velocity = true; // activate this for kinetic energy calculations (see below)
    pp_data.output_data.write_output         = false;
    pp_data.output_data.write_pressure       = true;
    pp_data.output_data.write_velocity       = true;
    pp_data.output_data.write_temperature    = true;
    pp_data.output_data.write_vorticity      = true;
    pp_data.output_data.write_divergence     = true;
    pp_data.output_data.output_start_time    = start_time;
    pp_data.output_data.output_interval_time = (end_time - start_time) / 20;
    pp_data.output_data.degree               = degree;

    // kinetic energy
    pp_data.kinetic_energy_data.calculate                  = true;
    pp_data.kinetic_energy_data.calculate_every_time_steps = 1;
    pp_data.kinetic_energy_data.viscosity                  = DYN_VISCOSITY / RHO_0;
    pp_data.kinetic_energy_data.filename                   = output_directory + output_name;

    // kinetic energy spectrum
    pp_data.kinetic_energy_spectrum_data.calculate                     = true;
    pp_data.kinetic_energy_spectrum_data.calculate_every_time_steps    = -1;
    pp_data.kinetic_energy_spectrum_data.calculate_every_time_interval = 0.5;
    pp_data.kinetic_energy_spectrum_data.filename = output_directory + output_name + "_spectrum";
    pp_data.kinetic_energy_spectrum_data.degree   = degree;
    pp_data.kinetic_energy_spectrum_data.evaluation_points_per_cell = (degree + 1) * 1;
    pp_data.kinetic_energy_spectrum_data.exploit_symmetry           = false;

    std::shared_ptr<CompNS::PostProcessorBase<dim, Number>> pp;
    pp.reset(new CompNS::PostProcessor<dim, Number>(pp_data, mpi_comm));

    return pp;
  }
};

} // namespace TaylorGreen
} // namespace CompNS

#endif /* APPLICATIONS_COMPRESSIBLE_NAVIER_STOKES_TEST_CASES_3D_TAYLOR_GREEN_VORTEX_H_ */
