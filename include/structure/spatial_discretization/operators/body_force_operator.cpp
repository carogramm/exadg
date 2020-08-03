/*
 * body_force_operator.cpp
 *
 *  Created on: 21.03.2020
 *      Author: fehn
 */

#include "body_force_operator.h"

#include "continuum_mechanics.h"

#include "../../../functions_and_boundary_conditions/evaluate_functions.h"

namespace Structure
{
template<int dim, typename Number>
BodyForceOperator<dim, Number>::BodyForceOperator() : matrix_free(nullptr), time(0.0)
{
}

template<int dim, typename Number>
void
BodyForceOperator<dim, Number>::initialize(MatrixFree<dim, Number> const & matrix_free,
                                           BodyForceData<dim> const &      data)
{
  this->matrix_free = &matrix_free;
  this->data        = data;
}

template<int dim, typename Number>
MappingFlags
BodyForceOperator<dim, Number>::get_mapping_flags()
{
  MappingFlags flags;

  // gradients because of potential pull-back of body forces
  flags.cells = update_gradients | update_JxW_values | update_quadrature_points;

  return flags;
}

template<int dim, typename Number>
void
BodyForceOperator<dim, Number>::evaluate_add(VectorType &       dst,
                                             VectorType const & src,
                                             double const       time) const
{
  this->time = time;

  matrix_free->cell_loop(&This::cell_loop, this, dst, src, false /*zero_dst_vector*/);
}

template<int dim, typename Number>
void
BodyForceOperator<dim, Number>::cell_loop(MatrixFree<dim, Number> const & matrix_free,
                                          VectorType &                    dst,
                                          VectorType const &              src,
                                          Range const &                   cell_range) const
{
  IntegratorCell integrator(matrix_free, data.dof_index, data.quad_index);

  for(unsigned int cell = cell_range.first; cell < cell_range.second; ++cell)
  {
    integrator.reinit(cell);

    if(data.pull_back_body_force)
    {
      integrator.read_dof_values_plain(src);
      integrator.evaluate(false, true);
    }

    for(unsigned int q = 0; q < integrator.n_q_points; ++q)
    {
      auto q_points = integrator.quadrature_point(q);
      auto b        = FunctionEvaluator<1, dim, Number>::value(data.function, q_points, time);

      if(data.pull_back_body_force)
      {
        auto F = get_F<dim, Number>(integrator.get_gradient(q));
        // b_0 = dv/dV * b = det(F) * b
        b *= determinant(F);
      }

      integrator.submit_value(b, q);
    }

    integrator.integrate(true, false);
    integrator.distribute_local_to_global(dst);
  }
}


template class BodyForceOperator<2, float>;
template class BodyForceOperator<2, double>;

template class BodyForceOperator<3, float>;
template class BodyForceOperator<3, double>;

} // namespace Structure
