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

#ifndef INCLUDE_EXADG_POISSON_OVERSET_GRIDS_USER_INTERFACE_APPLICATION_BASE_H_
#define INCLUDE_EXADG_POISSON_OVERSET_GRIDS_USER_INTERFACE_APPLICATION_BASE_H_

// deal.II
#include <deal.II/grid/grid_tools_cache.h>

// ExaDG
#include <exadg/poisson/user_interface/application_base.h>

namespace ExaDG
{
namespace Poisson
{
/**
 * This function determines which faces of the dst triangulation are inside the src-triangulation.
 * A face is considered inside, if all vertices of the face are inside. Then, the boundary ID is
 * set to bid for all the faces of the dst-triangulation in the overlap region.
 */
template<int dim>
void
set_boundary_ids_overlap_region(dealii::Triangulation<dim> const & tria_dst,
                                dealii::types::boundary_id const & bid,
                                dealii::Mapping<dim> const &       mapping_src,
                                dealii::Triangulation<dim> const & tria_src)
{
  std::vector<dealii::Point<dim>> points;
  using CellIteratorType = typename dealii::Triangulation<dim>::cell_iterator;
  using Id               = std::tuple<CellIteratorType /* cell */, unsigned int /*face*/>;
  std::vector<std::pair<Id, unsigned int /* first_point_in_vector */>> id_to_vector_index;

  // fill vector of points for all boundary faces
  for(auto cell : tria_dst.cell_iterators())
  {
    for(auto const & f : cell->face_indices())
    {
      if(cell->face(f)->at_boundary())
      {
        for(auto const & v : cell->face(f)->vertex_indices())
        {
          if(v == 0)
          {
            Id const id = std::make_tuple(cell, f);
            id_to_vector_index.push_back({id, points.size()});
          }

          points.push_back(cell->face(f)->vertex(v));
        }
      }
    }
  }

  // create and reinit RemotePointEvaluation: find points on src-side
  std::vector<bool> marked_vertices = {};
  double const      tolerance       = 1.e-10;

  dealii::Utilities::MPI::RemotePointEvaluation<dim> rpe =
    dealii::Utilities::MPI::RemotePointEvaluation<dim>(tolerance,
                                                       false
#if DEAL_II_VERSION_GTE(9, 4, 0)
                                                       ,
                                                       0,
                                                       [marked_vertices]() {
                                                         return marked_vertices;
                                                       }
#endif
    );

  rpe.reinit(points, tria_src, mapping_src);

  // check which points have been found and whether a face on dst-side is located inside the src
  // triangulation
  for(auto iter = id_to_vector_index.begin(); iter != id_to_vector_index.end(); ++iter)
  {
    unsigned int const begin = iter->second;
    unsigned int const end =
      (iter + 1 != id_to_vector_index.end()) ? (iter + 1)->second : points.size();

    bool inside = true;
    for(unsigned int i = begin; i < end; ++i)
    {
      inside = (inside and rpe.point_found(i));
    }

    if(inside)
    {
      auto const & [cell, f] = iter->first;
      cell->face(f)->set_boundary_id(bid);
    }
  }
}

template<int dim, int n_components, typename Number>
class ApplicationOversetGridsBase
{
public:
  ApplicationOversetGridsBase(std::string parameter_file, MPI_Comm const & comm)
    : mpi_comm(comm), parameter_file(parameter_file)
  {
  }

  void
  add_parameters(dealii::ParameterHandler & prm)
  {
    resolution1.add_parameters(prm, "ResolutionDomain1");
    resolution2.add_parameters(prm, "ResolutionDomain2");

    domain1->add_parameters(prm);
    domain2->add_parameters(prm);
  }

  virtual ~ApplicationOversetGridsBase()
  {
  }

  void
  setup()
  {
    // parse and set resolution parameters for both domains
    parse_resolution_parameters();
    domain1->set_parameters_refinement_study(resolution1.degree,
                                             resolution1.refine_space,
                                             0 /* not used */);
    domain2->set_parameters_refinement_study(resolution2.degree,
                                             resolution2.refine_space,
                                             0 /* not used */);

    domain1->setup_pre();
    domain2->setup_pre();

    set_boundary_ids();

    domain1->setup_post();
    domain2->setup_post();
  }

  std::shared_ptr<ApplicationBase<dim, n_components, Number>> domain1, domain2;

protected:
  MPI_Comm const & mpi_comm;

  // use "-1" since max() is defined invalid by deal.II
  dealii::types::boundary_id boundary_id_overlap =
    std::numeric_limits<dealii::types::boundary_id>::max() - 1;

private:
  /**
   * Here, parse only those parameters not covered by ApplicationBase implementations
   * (domain1 and domain2).
   */
  void
  parse_resolution_parameters()
  {
    dealii::ParameterHandler prm;

    resolution1.add_parameters(prm, "ResolutionDomain1");
    resolution2.add_parameters(prm, "ResolutionDomain2");

    prm.parse_input(parameter_file, "", true, true);
  }

  void
  set_boundary_ids()
  {
    // set boundary IDs for domain 1
    set_boundary_ids_overlap_region(*domain1->get_grid()->triangulation,
                                    boundary_id_overlap,
                                    *domain2->get_grid()->mapping,
                                    *domain2->get_grid()->triangulation);

    // set boundary IDs for domain 2
    set_boundary_ids_overlap_region(*domain2->get_grid()->triangulation,
                                    boundary_id_overlap,
                                    *domain1->get_grid()->mapping,
                                    *domain1->get_grid()->triangulation);
  }

  std::string parameter_file;

  ResolutionParameters resolution1, resolution2;
};

} // namespace Poisson

} // namespace ExaDG

#endif /* INCLUDE_EXADG_POISSON_OVERSET_GRIDS_USER_INTERFACE_APPLICATION_BASE_H_ */
