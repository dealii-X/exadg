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

#ifndef INCLUDE_STRUCTURE_SPATIAL_DISCRETIZATION_NONLINEAR_OPERATOR_H_
#define INCLUDE_STRUCTURE_SPATIAL_DISCRETIZATION_NONLINEAR_OPERATOR_H_

#include <exadg/grid/mapping_dof_vector.h>
#include <exadg/structure/spatial_discretization/operators/elasticity_operator_base.h>
#ifdef DEAL_II_WITH_TRILINOS
#  include <deal.II/lac/trilinos_sparse_matrix.h>
#endif
#ifdef DEAL_II_WITH_PETSC
#  include <deal.II/lac/petsc_sparse_matrix.h>
#endif

namespace ExaDG
{
namespace Structure
{
template<int dim, typename Number>
class NonLinearOperator : public ElasticityOperatorBase<dim, Number>
{
private:
  typedef ElasticityOperatorBase<dim, Number> Base;

  typedef typename Base::VectorType     VectorType;
  typedef typename Base::IntegratorCell IntegratorCell;
  typedef typename Base::IntegratorFace IntegratorFace;

  typedef NonLinearOperator<dim, Number> This;

  typedef std::pair<unsigned int, unsigned int> Range;

  typedef dealii::VectorizedArray<Number>                                  scalar;
  typedef dealii::Tensor<1, dim, dealii::VectorizedArray<Number>>          vector;
  typedef dealii::Tensor<2, dim, dealii::VectorizedArray<Number>>          tensor;
  typedef dealii::SymmetricTensor<2, dim, dealii::VectorizedArray<Number>> symmetric_tensor;

public:
  /**
   * Initialize function.
   */
  void
  initialize(dealii::MatrixFree<dim, Number> const &   matrix_free,
             dealii::AffineConstraints<Number> const & affine_constraints,
             OperatorData<dim> const &                 data) override;

  /**
   * Evaluates the non-linear operator.
   *
   * This function ensures that constrained degrees of freedom are set to zero
   * in the dst vector. Note, however, that this function does not ensure
   * that Dirichlet degrees of freedom are set to the prescribed inhomogeneous
   * boundary data in the src vector. This needs to be done separately prior to
   * calling this function.
   */
  void
  evaluate_nonlinear(VectorType & dst, VectorType const & src) const;

  /**
   * Returns true if deformation state is valid.
   */
  bool
  valid_deformation(VectorType const & displacement) const;

  /**
   * Linearized operator: Sets the linearization vector.
   */
  void
  set_solution_linearization(VectorType const & vector,
                             bool const         update_cell_data,
                             bool const         update_mapping,
                             bool const         update_matrix_if_necessary) const;

  /**
   * Set the mapping pointer for the undeformed mapping.
   */
  void
  set_mapping_undeformed(std::shared_ptr<dealii::Mapping<dim> const> mapping) const;

  /**
   * Linearized operator: Returns the linearization vector.
   */
  VectorType const &
  get_solution_linearization() const;

  /**
   * Set cell data given a linearization vector.
   */
  void
  set_cell_linearization_data(VectorType const & linearization_vector) const;

  void
  cell_loop_set_linearization_data(dealii::MatrixFree<dim, Number> const & matrix_free,
                                   VectorType &                            dst,
                                   VectorType const &                      src,
                                   Range const &                           range) const;

  /**
   * Overwrite members in OperatorBase to optionally use spatial integration
   * via matrix_free_spatial.
   */
  void
  apply(VectorType & dst, VectorType const & src) const override;

  void
  apply_add(VectorType & dst, VectorType const & src) const override;

  void
  rhs(VectorType & dst) const override;

  void
  rhs_add(VectorType & dst) const override;

  void
  evaluate(VectorType & dst, VectorType const & src) const override;

  void
  evaluate_add(VectorType & dst, VectorType const & src) const override;

  void
  add_diagonal(VectorType & diagonal) const override;

  /*
   * Initialize sparse matrix.
   */
#ifdef DEAL_II_WITH_TRILINOS
  void
  calculate_system_matrix(dealii::TrilinosWrappers::SparseMatrix & system_matrix) const override;
#endif

#ifdef DEAL_II_WITH_PETSC
  void
  calculate_system_matrix(dealii::PETScWrappers::MPI::SparseMatrix & system_matrix) const override;
#endif

private:
  /*
   * Non-linear operator.
   */
  void
  reinit_cell_nonlinear(IntegratorCell & integrator, unsigned int const cell) const;

  void
  cell_loop_nonlinear(dealii::MatrixFree<dim, Number> const & matrix_free,
                      VectorType &                            dst,
                      VectorType const &                      src,
                      Range const &                           range) const;

  void
  face_loop_nonlinear(dealii::MatrixFree<dim, Number> const & matrix_free,
                      VectorType &                            dst,
                      VectorType const &                      src,
                      Range const &                           range) const;

  void
  boundary_face_loop_nonlinear(dealii::MatrixFree<dim, Number> const & matrix_free,
                               VectorType &                            dst,
                               VectorType const &                      src,
                               Range const &                           range) const;

  /*
   * A cell loop that checks whether the Jacobian determinant is positive for all q-points.
   * Prior to calling this function, inhomogeneous Dirichlet degrees of freedom need to be
   * set correctly.
   */
  void
  cell_loop_valid_deformation(dealii::MatrixFree<dim, Number> const & matrix_free,
                              Number &                                dst,
                              VectorType const &                      src,
                              Range const &                           range) const;

  /*
   * Calculates the integral
   *
   *  (v_h, factor * d_h)_Omega + (Grad(v_h), P_h)_Omega
   *
   * with 1st Piola-Kirchhoff stress tensor P_h
   *
   *  P_h = F_h * S_h ,
   *
   * 2nd Piola-Kirchhoff stress tensor S_h
   *
   *  S_h = function(E_h) ,
   *
   * Green-Lagrange strain tensor E_h
   *
   *  E_h = 1/2 (F_h^T * F_h - 1) ,
   *
   * material deformation gradient F_h
   *
   *  F_h = 1 + Grad(d_h) ,
   *
   * where
   *
   *  d_h denotes the displacement vector.
   */
  void
  do_cell_integral_nonlinear(IntegratorCell & integrator) const;

  /*
   * Computes Neumann BC integral
   *
   *  - (v_h, t_0)_{Gamma_N}
   *
   * with traction
   *
   *  t_0 = da/dA t .
   *
   * If the traction is specified as force per surface area of the underformed
   * body, the specified traction t is interpreted as t_0 = t, and no pull-back
   * is necessary.
   */
  void
  do_boundary_integral_continuous(IntegratorFace &                   integrator_m,
                                  dealii::types::boundary_id const & boundary_id) const final;

  /*
   * Linearized operator.
   */
  void
  reinit_cell_derived(IntegratorCell & integrator, unsigned int const cell) const final;

  /*
   * Calculates the integral
   *
   *  (v_h, factor * delta d_h)_Omega + (Grad(v_h), delta P_h)_Omega
   *
   * with the directional derivative of the 1st Piola-Kirchhoff stress tensor P_h
   *
   *  delta P_h = d(P)/d(d)|_{d_lin} * delta d_h ,
   *
   * with the point of linearization
   *
   *  d_lin ,
   *
   * and displacement increment
   *
   *  delta d_h .
   *
   * Computing the linearization yields
   *
   *  delta P_h = + Grad(delta_d) * S(d_lin)
   *              + F(d_lin) * (C_lin : (F^T(d_lin) * Grad(delta d))) .
   *
   *  Note that a dependency of the Neumann BC on the displacements d through
   *  the area ratio da/dA = function(d) is neglected in the linearization.
   */
  void
  do_cell_integral(IntegratorCell & integrator) const override;

  /**
   * Overwrite members in OperatorBase to optionally use spatial integration
   * via matrix_free_spatial.
   */
  void
  cell_loop(dealii::MatrixFree<dim, Number> const & matrix_free,
            VectorType &                            dst,
            VectorType const &                      src,
            Range const &                           range) const override;

  mutable std::shared_ptr<IntegratorCell> integrator_lin;
  mutable VectorType                      displacement_lin;

  mutable dealii::MatrixFree<dim, Number>                matrix_free_spatial;
  mutable std::shared_ptr<MappingDoFVector<dim, Number>> mapping_spatial;
  mutable std::shared_ptr<dealii::Mapping<dim> const>    mapping_undeformed;
};

} // namespace Structure
} // namespace ExaDG

#endif
