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

// C++
#include <stdlib.h>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>

// deal.II
#include <deal.II/base/conditional_ostream.h>
#include <deal.II/base/mpi.h>
#include <deal.II/base/quadrature_lib.h>
#include <deal.II/base/utilities.h>
#include <deal.II/dofs/dof_handler.h>
#include <deal.II/fe/fe_q.h>
#include <deal.II/fe/fe_system.h>
#include <deal.II/grid/grid_generator.h>
#include <deal.II/grid/tria.h>
#include <deal.II/lac/affine_constraints.h>
#include <deal.II/matrix_free/matrix_free.h>

// ExaDG
#include <exadg/structure/material/library/compressible_neo_hookean.h>
#include <exadg/structure/material/library/incompressible_fibrous_tissue.h>
#include <exadg/structure/material/library/incompressible_neo_hookean.h>
#include <exadg/structure/material/library/st_venant_kirchhoff.h>
#include <exadg/structure/material/material.h>
#include <exadg/utilities/enum_utilities.h>

namespace ExaDG
{
namespace StabilityTest
{
using namespace ExaDG::Structure;

template<int dim, typename Number>
std::shared_ptr<Material<dim, Number>>
setup_material(MaterialType                            material_type,
               bool const                              spatial_integration,
               bool const                              stable_formulation,
               dealii::MatrixFree<dim, Number> const & matrix_free,
               unsigned int const                      cache_level)
{
  // Construct Material objects.
  std::shared_ptr<Material<dim, Number>> material;

  // Dummy objects for initialization.
  unsigned int constexpr dof_index  = 0;
  unsigned int constexpr quad_index = 0;
  bool constexpr large_deformation  = true;
  unsigned int constexpr check_type = 0;

  if(material_type == MaterialType::StVenantKirchhoff)
  {
    // St.-Venant-Kirchhoff.
    Type2D const two_dim_type = Type2D::PlaneStress;
    double const nu           = 0.3;
    double const E_modul      = 200.0;

    StVenantKirchhoffData<dim> const data(MaterialType::StVenantKirchhoff,
                                          E_modul,
                                          nu,
                                          two_dim_type);

    material = std::make_shared<StVenantKirchhoff<dim, Number>>(
      matrix_free, dof_index, quad_index, data, large_deformation);
  }
  else
  {
    AssertThrow(dim == 3,
                dealii::ExcMessage("These material models are implemented for dim == 3 only."));

    Type2D constexpr two_dim_type          = Type2D::Undefined;
    bool constexpr force_material_residual = false;

    if(material_type == MaterialType::CompressibleNeoHookean)
    {
      // Compressible neo-Hookean.
      double const shear_modulus = 1.0e2;
      double const nu            = 0.3;
      double const lambda        = shear_modulus * 2.0 * nu / (1.0 - 2.0 * nu);

      CompressibleNeoHookeanData<dim> const data(MaterialType::CompressibleNeoHookean,
                                                 shear_modulus,
                                                 lambda,
                                                 two_dim_type);

      if(check_type == 0)
      {
        if(stable_formulation)
        {
          if(cache_level == 0)
          {
            material = std::make_shared<CompressibleNeoHookean<dim, Number, 0, true, 0>>(
              matrix_free,
              dof_index,
              quad_index,
              data,
              spatial_integration,
              force_material_residual);
          }
          else if(cache_level == 1)
          {
            material = std::make_shared<CompressibleNeoHookean<dim, Number, 0, true, 1>>(
              matrix_free,
              dof_index,
              quad_index,
              data,
              spatial_integration,
              force_material_residual);
          }
          else if(cache_level == 2)
          {
            material = std::make_shared<CompressibleNeoHookean<dim, Number, 0, true, 2>>(
              matrix_free,
              dof_index,
              quad_index,
              data,
              spatial_integration,
              force_material_residual);
          }
          else
          {
            AssertThrow(cache_level < 3, dealii::ExcMessage("Cache levels 0 1 and 2 implemented."));
          }
        }
        else
        {
          if(cache_level == 0)
          {
            material = std::make_shared<CompressibleNeoHookean<dim, Number, 0, false, 0>>(
              matrix_free,
              dof_index,
              quad_index,
              data,
              spatial_integration,
              force_material_residual);
          }
          else if(cache_level == 1)
          {
            material = std::make_shared<CompressibleNeoHookean<dim, Number, 0, false, 1>>(
              matrix_free,
              dof_index,
              quad_index,
              data,
              spatial_integration,
              force_material_residual);
          }
          else if(cache_level == 2)
          {
            material = std::make_shared<CompressibleNeoHookean<dim, Number, 0, false, 2>>(
              matrix_free,
              dof_index,
              quad_index,
              data,
              spatial_integration,
              force_material_residual);
          }
          else
          {
            AssertThrow(cache_level < 3, dealii::ExcMessage("Cache levels 0 1 and 2 implemented."));
          }
        }
      }
      else
      {
        AssertThrow(check_type == 0,
                    dealii::ExcMessage("Templates only for check_type == 0 implemented."));
      }
    }
    else if(material_type == MaterialType::IncompressibleNeoHookean)
    {
      // Incompressible neo-Hookean.
      double constexpr shear_modulus = 62.1e3;
      double constexpr nu            = 0.49;
      double constexpr bulk_modulus  = shear_modulus * 2.0 * (1.0 + nu) / (3.0 * (1.0 - 2.0 * nu));

      IncompressibleNeoHookeanData<dim> const data(MaterialType::IncompressibleNeoHookean,
                                                   shear_modulus,
                                                   bulk_modulus,
                                                   two_dim_type);

      if(check_type == 0)
      {
        if(stable_formulation)
        {
          if(cache_level == 0)
          {
            material = std::make_shared<IncompressibleNeoHookean<dim, Number, 0, true, 0>>(
              matrix_free,
              dof_index,
              quad_index,
              data,
              spatial_integration,
              force_material_residual);
          }
          else if(cache_level == 1)
          {
            material = std::make_shared<IncompressibleNeoHookean<dim, Number, 0, true, 1>>(
              matrix_free,
              dof_index,
              quad_index,
              data,
              spatial_integration,
              force_material_residual);
          }
          else if(cache_level == 2)
          {
            material = std::make_shared<IncompressibleNeoHookean<dim, Number, 0, true, 2>>(
              matrix_free,
              dof_index,
              quad_index,
              data,
              spatial_integration,
              force_material_residual);
          }
          else
          {
            AssertThrow(cache_level < 3, dealii::ExcMessage("Cache levels 0 1 and 2 implemented."));
          }
        }
        else
        {
          if(cache_level == 0)
          {
            material = std::make_shared<IncompressibleNeoHookean<dim, Number, 0, false, 0>>(
              matrix_free,
              dof_index,
              quad_index,
              data,
              spatial_integration,
              force_material_residual);
          }
          else if(cache_level == 1)
          {
            material = std::make_shared<IncompressibleNeoHookean<dim, Number, 0, false, 1>>(
              matrix_free,
              dof_index,
              quad_index,
              data,
              spatial_integration,
              force_material_residual);
          }
          else if(cache_level == 2)
          {
            material = std::make_shared<IncompressibleNeoHookean<dim, Number, 0, false, 2>>(
              matrix_free,
              dof_index,
              quad_index,
              data,
              spatial_integration,
              force_material_residual);
          }
          else
          {
            AssertThrow(cache_level < 3, dealii::ExcMessage("Cache levels 0 1 and 2 implemented."));
          }
        }
      }
      else
      {
        AssertThrow(check_type == 0,
                    dealii::ExcMessage("Templates only for check_type == 0 implemented."));
      }
    }
    else if(material_type == MaterialType::IncompressibleFibrousTissue)
    {
      // Incompressible fibrous tissue.
      double constexpr shear_modulus = 62.1e3;
      double constexpr nu            = 0.49;
      double constexpr bulk_modulus  = shear_modulus * 2.0 * (1.0 + nu) / (3.0 * (1.0 - 2.0 * nu));

      // Parameters corresponding to aortic tissue might be found in
      // [Weisbecker et al., J Mech Behav Biomed Mater 12, 2012] or
      // [Rolf-Pissarczyk et al., Comput Methods Appl Mech Eng 373, 2021].
      // a = 3.62, b = 34.3 for medial tissue lead to the H_ii below,
      // while the k_1 coefficient is scaled relative to the shear modulus
      // (for medial tissue, e.g., 62.1 kPa).
      double const fiber_angle_phi_in_degree = 27.47;                            // [deg]
      double const fiber_H_11                = 0.9168;                           // [-]
      double const fiber_H_22                = 0.0759;                           // [-]
      double const fiber_H_33                = 0.0073;                           // [-]
      double const fiber_k_1                 = 1.4e3 * (shear_modulus / 62.1e3); // [Pa]
      double const fiber_k_2                 = 22.1;                             // [-]
      double const fiber_switch_limit        = 0.0;                              // [-]

      // Read the orientation files from binary format.
      typedef typename IncompressibleFibrousTissueData<dim>::VectorType VectorType;

      std::vector<VectorType>   e1_orientation;
      std::vector<VectorType>   e2_orientation;
      std::vector<unsigned int> degree_per_level;

      std::shared_ptr<std::vector<VectorType> const> e1_ori, e2_ori;
      e1_ori = std::make_shared<std::vector<VectorType> const>(e1_orientation);
      e2_ori = std::make_shared<std::vector<VectorType> const>(e2_orientation);

      IncompressibleFibrousTissueData<dim> const data(MaterialType::IncompressibleFibrousTissue,
                                                      shear_modulus,
                                                      bulk_modulus,
                                                      fiber_angle_phi_in_degree,
                                                      fiber_H_11,
                                                      fiber_H_22,
                                                      fiber_H_33,
                                                      fiber_k_1,
                                                      fiber_k_2,
                                                      fiber_switch_limit,
                                                      nullptr /* e1_ori */,
                                                      nullptr /* e2_ori */,
                                                      {} /* degree_per_level */,
                                                      0.0 /* point_match_tol */,
                                                      two_dim_type);

      if(check_type == 0)
      {
        if(stable_formulation)
        {
          if(cache_level == 0)
          {
            material = std::make_shared<IncompressibleFibrousTissue<dim, Number, 0, true, 0>>(
              matrix_free,
              dof_index,
              quad_index,
              data,
              spatial_integration,
              force_material_residual);
          }
          else if(cache_level == 1)
          {
            material = std::make_shared<IncompressibleFibrousTissue<dim, Number, 0, true, 1>>(
              matrix_free,
              dof_index,
              quad_index,
              data,
              spatial_integration,
              force_material_residual);
          }
          else if(cache_level == 2)
          {
            material = std::make_shared<IncompressibleFibrousTissue<dim, Number, 0, true, 2>>(
              matrix_free,
              dof_index,
              quad_index,
              data,
              spatial_integration,
              force_material_residual);
          }
          else
          {
            AssertThrow(cache_level < 3, dealii::ExcMessage("Cache levels 0 1 and 2 implemented."));
          }
        }
        else
        {
          if(cache_level == 0)
          {
            material = std::make_shared<IncompressibleFibrousTissue<dim, Number, 0, false, 0>>(
              matrix_free,
              dof_index,
              quad_index,
              data,
              spatial_integration,
              force_material_residual);
          }
          else if(cache_level == 1)
          {
            material = std::make_shared<IncompressibleFibrousTissue<dim, Number, 0, false, 1>>(
              matrix_free,
              dof_index,
              quad_index,
              data,
              spatial_integration,
              force_material_residual);
          }
          else if(cache_level == 2)
          {
            material = std::make_shared<IncompressibleFibrousTissue<dim, Number, 0, false, 2>>(
              matrix_free,
              dof_index,
              quad_index,
              data,
              spatial_integration,
              force_material_residual);
          }
          else
          {
            AssertThrow(cache_level < 3, dealii::ExcMessage("Cache levels 0 1 and 2 implemented."));
          }
        }
      }
      else
      {
        AssertThrow(check_type == 0,
                    dealii::ExcMessage("Templates only for check_type == 0 implemented."));
      }
    }
    else
    {
      AssertThrow(false, dealii::ExcMessage("Material model not implemented."));
    }
  }

  return material;
}

template<typename Number>
std::vector<Number>
logspace(Number const & min_scale, Number const & max_scale, unsigned int const n_entries)
{
  Number const exponent_start = std::log10(min_scale);
  Number const exponent_end   = std::log10(max_scale);

  AssertThrow(n_entries > 2,
              dealii::ExcMessage("Constructing less than 3 data points does not make sense."));
  Number const exponent_delta =
    (exponent_end - exponent_start) / (static_cast<Number>(n_entries) - 1.0);

  std::vector<Number> vector(n_entries);
  Number              exponent = exponent_start;
  for(unsigned int i = 0; i < n_entries; ++i)
  {
    vector[i] = std::pow(10.0, exponent);
    exponent += exponent_delta;
  }
  return vector;
}

template<int dim, typename Number, bool use_random_sign>
dealii::Tensor<2, dim, dealii::VectorizedArray<Number>>
get_random_tensor(Number scale)
{
  dealii::Tensor<2, dim, dealii::VectorizedArray<Number>> random_tensor;

  for(unsigned int i = 0; i < dim; ++i)
  {
    for(unsigned int j = 0; j < dim; ++j)
    {
      // Pseudo-random number, but in the interval [0.1, 0.9] to
      // keep the order of magnitude unchanged.
      Number const random_number_0c0_1c0 =
        static_cast<Number>(std::rand()) / static_cast<Number>(RAND_MAX);
      Number const random_number_0c1_1c0 = 0.1 + (random_number_0c0_1c0 / 0.9);

      if constexpr(use_random_sign)
      {
        // Generate second random number for random sign.
        Number random_sign  = static_cast<Number>(std::rand()) / static_cast<Number>(RAND_MAX);
        random_sign         = random_sign > 0.5 ? 1.0 : -1.0;
        random_tensor[i][j] = scale * random_number_0c1_1c0 * random_sign;
      }
      else
      {
        random_tensor[i][j] = scale * random_number_0c1_1c0;
      }
    }
  }

  return random_tensor;
}

template<int dim, typename NumberIn, typename NumberOut>
dealii::Tensor<2, dim, dealii::VectorizedArray<NumberOut>>
tensor_cast_shallow(dealii::Tensor<2, dim, dealii::VectorizedArray<NumberIn>> const & tensor_in)
{
  dealii::Tensor<2, dim, dealii::VectorizedArray<NumberOut>> tensor_out;

  for(unsigned int i = 0; i < dim; ++i)
  {
    for(unsigned int j = 0; j < dim; ++j)
    {
      // Copy and cast only the very first entry from `tensor_in` to `tensor_out`.
      tensor_out[i][j][0] = static_cast<NumberOut>(tensor_in[i][j][0]);
    }
  }

  return tensor_out;
}

template<int dim, typename NumberIn, typename NumberOut>
dealii::SymmetricTensor<2, dim, dealii::VectorizedArray<NumberOut>>
symmetric_tensor_cast_shallow(
  dealii::SymmetricTensor<2, dim, dealii::VectorizedArray<NumberIn>> const & tensor_in)
{
  dealii::SymmetricTensor<2, dim, dealii::VectorizedArray<NumberOut>> tensor_out;

  for(unsigned int i = 0; i < dim; ++i)
  {
    for(unsigned int j = i; j < dim; ++j)
    {
      // Copy and cast only the very first entry from `tensor_in` to `tensor_out`.
      tensor_out[i][j][0] = static_cast<NumberOut>(tensor_in[i][j][0]);
    }
  }

  return tensor_out;
}

template<int dim, typename Number>
std::vector<dealii::SymmetricTensor<2, dim, dealii::VectorizedArray<Number>>>
evaluate_material(
  Material<dim, Number> const &                                   material,
  unsigned int const                                              cache_level,
  dealii::Tensor<2, dim, dealii::VectorizedArray<Number>> const & gradient_displacement,
  dealii::Tensor<2, dim, dealii::VectorizedArray<Number>> const & gradient_increment,
  unsigned int const                                              n_cell_batches,
  unsigned int const                                              n_q_points,
  bool const                                                      spatial_integration,
  std::vector<double> &                                           execution_time_stress_jacobian,
  unsigned int const                                              n_executions_timer)
{
  typedef dealii::SymmetricTensor<2, dim, dealii::VectorizedArray<Number>> symmetric_tensor;

  std::vector<symmetric_tensor> evaluation(2);
  execution_time_stress_jacobian.resize(2);

  // Stress computation and timing.
  {
    auto timer_start = std::chrono::steady_clock::now();
    for(unsigned int i = 0; i < n_executions_timer; ++i)
    {
      for(unsigned int cell = 0; cell < n_cell_batches; ++cell)
      {
        for(unsigned int q = 0; q < n_q_points; ++q)
        {
          if(spatial_integration)
          {
            if(cache_level < 2)
            {
              evaluation[0] = material.kirchhoff_stress(gradient_displacement, cell, q);
            }
            else
            {
              evaluation[0] = material.kirchhoff_stress(cell, q);
            }
          }
          else
          {
            if(cache_level < 2)
            {
              evaluation[0] =
                material.second_piola_kirchhoff_stress(gradient_displacement, cell, q);
            }
            else
            {
              evaluation[0] = material.second_piola_kirchhoff_stress(cell, q);
            }
          }
        }
      }
    }
    auto timer_end = std::chrono::steady_clock::now();
    execution_time_stress_jacobian[0] =
      std::chrono::duration<double>(timer_end - timer_start).count();
  }

  // Jacobian computation and timing.
  symmetric_tensor const symmetric_gradient_increment = symmetrize(gradient_increment);

  {
    auto timer_start = std::chrono::steady_clock::now();
    for(unsigned int i = 0; i < n_executions_timer; ++i)
    {
      for(unsigned int cell = 0; cell < n_cell_batches; ++cell)
      {
        for(unsigned int q = 0; q < n_q_points; ++q)
        {
          if(spatial_integration)
          {
            if(cache_level < 2)
            {
              evaluation[1] = material.contract_with_J_times_C(symmetric_gradient_increment,
                                                               gradient_displacement,
                                                               cell,
                                                               q);
            }
            else
            {
              evaluation[1] =
                material.contract_with_J_times_C(symmetric_gradient_increment, cell, q);
            }
          }
          else
          {
            evaluation[1] = material.second_piola_kirchhoff_stress_displacement_derivative(
              gradient_increment, gradient_displacement, cell, q);
          }
        }
      }
    }
    auto timer_end = std::chrono::steady_clock::now();
    execution_time_stress_jacobian[1] =
      std::chrono::duration<double>(timer_end - timer_start).count();
  }

  return evaluation;
}

} // namespace StabilityTest
} // namespace ExaDG

int
main(int argc, char ** argv)
{
  using namespace ExaDG::StabilityTest;

  unsigned int constexpr dim = 3;

  typedef dealii::Tensor<2, dim, dealii::VectorizedArray<double>>          tensor_double;
  typedef dealii::Tensor<2, dim, dealii::VectorizedArray<float>>           tensor_float;
  typedef dealii::SymmetricTensor<2, dim, dealii::VectorizedArray<double>> symmetric_tensor_double;
  typedef dealii::SymmetricTensor<2, dim, dealii::VectorizedArray<float>>  symmetric_tensor_float;

  try
  {
    dealii::Utilities::MPI::MPI_InitFinalize mpi(argc, argv, 1);

    dealii::ConditionalOStream pcout(std::cout,
                                     dealii::Utilities::MPI::this_mpi_process(MPI_COMM_WORLD) == 0);

    // Perform the stability test or measure the evaluation time.
    bool constexpr stab_test       = true;
    bool constexpr use_max_err     = true;
    bool constexpr use_random_sign = true;

    unsigned int const cache_level = stab_test ? 0 : 0 /* 0, 1, 2 */;

    // Gradient and increment scale.
    unsigned int const n_points_over_log_scale = stab_test ? 1e3 : 1;

    std::vector<double> grad_u_scale{{1e-2}};
    if(stab_test)
    {
      grad_u_scale = logspace(1e-8, 1e+2, n_points_over_log_scale);
    }

    double constexpr h_e                  = 1e-3;
    double constexpr grad_increment_scale = 1.0 / h_e;

    // Setup dummy MatrixFree object in case the material
    // model stores some data.
    dealii::MatrixFree<dim, float>  matrix_free_float;
    dealii::MatrixFree<dim, double> matrix_free_double;

    dealii::Triangulation<dim> triangulation;
    dealii::GridGenerator::hyper_cube(triangulation, 0., 1.);
    if(not stab_test)
    {
      triangulation.refine_global(4);
    }
    unsigned int const      degree     = stab_test ? 1 : 3;
    unsigned int const      n_q_points = stab_test ? 1 : degree + 1;
    dealii::FE_Q<dim> const fe_q(degree);
    dealii::FESystem<dim>   fe_system(fe_q, dim);
    dealii::MappingQ1<dim>  mapping;
    dealii::DoFHandler<dim> dof_handler(triangulation);
    dof_handler.distribute_dofs(fe_system);
    {
      // For MatrixFree<dim, double> object.
      dealii::AffineConstraints<double> empty_constraints_double;
      empty_constraints_double.close();

      typename dealii::MatrixFree<dim, double>::AdditionalData additional_data_double;
      // additional_data_double.mapping_update_flags =
      //   (update_gradients | update_JxW_values | update_quadrature_points);
      matrix_free_double.reinit(mapping,
                                dof_handler,
                                empty_constraints_double,
                                dealii::QGauss<dim>(n_q_points),
                                additional_data_double);

      // For MatrixFree<dim, float> object.
      dealii::AffineConstraints<float> empty_constraints_float;
      empty_constraints_float.close();

      typename dealii::MatrixFree<dim, float>::AdditionalData additional_data_float;
      // additional_data_float.mapping_update_flags =
      //   (update_gradients | update_JxW_values | update_quadrature_points);
      matrix_free_float.reinit(mapping,
                               dof_handler,
                               empty_constraints_float,
                               dealii::QGauss<dim>(n_q_points),
                               additional_data_float);
    }

    // Material type choices.
    std::vector<MaterialType> material_type_vec{MaterialType::StVenantKirchhoff,
                                                MaterialType::CompressibleNeoHookean,
                                                MaterialType::IncompressibleNeoHookean,
                                                MaterialType::IncompressibleFibrousTissue};

    std::vector<bool> spatial_integration_vec{false, true};
    std::vector<bool> stable_formulation_vec{false, true};

    // Repeat the experiment with `n_samples` of random tensors.
#ifdef DEBUG
    unsigned int const n_samples = stab_test ? 2e2 : 10;
#else
    unsigned int const n_samples          = stab_test ? 2e2 : 2e1;
#endif

    // For execution time measurements, execute the evaluation `n_executions_timer`
    // times and compute average/min/max. Above sampling is not narrow enough.
#ifdef DEBUG
    unsigned int const n_executions_timer = stab_test ? 1 : 10;
#else
    unsigned int const n_executions_timer = stab_test ? 1 : 1e2;
#endif

    for(MaterialType const material_type : material_type_vec)
    {
      for(bool const spatial_integration : spatial_integration_vec)
      {
        for(bool const stable_formulation : stable_formulation_vec)
        {
          // No spatial alternative for STVK model.
          if(material_type == MaterialType::StVenantKirchhoff && spatial_integration)
          {
            continue;
          }

          pcout << "\n\n"
                << "  Material model : " << ExaDG::Utilities::enum_to_string(material_type) << "\n"
                << "  cache_level         = " << cache_level << "\n"
                << "  spatial_integration = " << spatial_integration << "\n"
                << "  stable_formulation  = " << stable_formulation << "\n";

          // Setup material objects in float and double precision for comparison.
          // Note that the integration point storage for cache_level > 0 is filled
          // at construction with data corresponding to zero displacement.
          std::shared_ptr<Material<dim, double>> material_double =
            setup_material<dim, double>(material_type,
                                        spatial_integration,
                                        stable_formulation,
                                        matrix_free_double,
                                        cache_level);
          std::shared_ptr<Material<dim, float>> material_float = setup_material<dim, float>(
            material_type, spatial_integration, stable_formulation, matrix_free_float, cache_level);

          // Evaluate the residual and derivative for all given scalings.
          std::vector<std::vector<double>> relative_error_samples(2);
          relative_error_samples[0].resize(n_points_over_log_scale);
          relative_error_samples[1].resize(n_points_over_log_scale);

          // variables for measuring execution time
          std::vector<double>              sum_min_max_init = {0.0, 1e20, -1e20};
          std::vector<std::vector<double>> time_stress_double_float_sum_min_max(2,
                                                                                sum_min_max_init);
          std::vector<std::vector<double>> time_jacobian_double_float_sum_min_max(2,
                                                                                  sum_min_max_init);

          for(unsigned int i = 0; i < n_points_over_log_scale; ++i)
          {
            for(unsigned int j = 0; j < n_samples; ++j)
            {
              // Generate pseudo-random gradient of the displacement field.
              tensor_float const gradient_displacement_float =
                get_random_tensor<dim, float, use_random_sign>(grad_u_scale[i]);
              tensor_float const gradient_increment_float =
                get_random_tensor<dim, float, use_random_sign>(grad_increment_scale);

              // Deep copy the tensor entries casting to *representable*
              // double type to have the *same* random number.
              tensor_double const gradient_displacement_double =
                tensor_cast_shallow<dim, float /*NumberIn*/, double /*NumberOut*/>(
                  gradient_displacement_float);
              tensor_double const gradient_increment_double =
                tensor_cast_shallow<dim, float /*NumberIn*/, double /*NumberOut*/>(
                  gradient_increment_float);

              // Compute stresses and derivatives.
              std::vector<double>                        time_double(2), time_float(2);
              std::vector<symmetric_tensor_double> const evaluation_double =
                evaluate_material<dim, double>(*material_double,
                                               cache_level,
                                               gradient_displacement_double,
                                               gradient_increment_double,
                                               matrix_free_double.n_cell_batches(),
                                               n_q_points,
                                               spatial_integration,
                                               time_double,
                                               n_executions_timer);
              std::vector<symmetric_tensor_float> const evaluation_float =
                evaluate_material<dim, float>(*material_float,
                                              cache_level,
                                              gradient_displacement_float,
                                              gradient_increment_float,
                                              matrix_free_float.n_cell_batches(),
                                              n_q_points,
                                              spatial_integration,
                                              time_float,
                                              n_executions_timer);

              // Store execution time average/min/max.
              // clang-format off
              time_stress_double_float_sum_min_max[0][0] += time_double[0];
              time_stress_double_float_sum_min_max[0][1] = std::min(time_double[0], time_stress_double_float_sum_min_max[0][1]);
              time_stress_double_float_sum_min_max[0][2] = std::max(time_double[0], time_stress_double_float_sum_min_max[0][2]);
              time_stress_double_float_sum_min_max[1][0] += time_float[0];
              time_stress_double_float_sum_min_max[1][1] = std::min(time_float[0], time_stress_double_float_sum_min_max[1][1]);
              time_stress_double_float_sum_min_max[1][2] = std::max(time_float[0], time_stress_double_float_sum_min_max[1][2]);

              time_jacobian_double_float_sum_min_max[0][0] += time_double[1];
              time_jacobian_double_float_sum_min_max[0][1] = std::min(time_double[1], time_jacobian_double_float_sum_min_max[0][1]);
              time_jacobian_double_float_sum_min_max[0][2] = std::max(time_double[1], time_jacobian_double_float_sum_min_max[0][2]);
              time_jacobian_double_float_sum_min_max[1][0] += time_float[1];
              time_jacobian_double_float_sum_min_max[1][1] = std::min(time_float[1], time_jacobian_double_float_sum_min_max[1][1]);
              time_jacobian_double_float_sum_min_max[1][2] = std::max(time_float[1], time_jacobian_double_float_sum_min_max[1][2]);
              // clang-format on

              // Store the current sample, if it gives the largest relative error.
              AssertThrow(evaluation_double.size() == 2,
                          dealii::ExcMessage("Expecting stress and derivative."));
              for(unsigned int k = 0; k < evaluation_double.size(); ++k)
              {
                // Cast first entry of computed tensor to double from *representable* float.
                symmetric_tensor_double diff_evaluation =
                  symmetric_tensor_cast_shallow<dim, float /*NumberIn*/, double /*NumberOut*/>(
                    evaluation_float[k]);

                diff_evaluation -= evaluation_double[k];

                // Maximum/minimum relative error in the |tensor|_inf norm.
                // Note that we might be interested in the minimum, since sampled
                // Grad(u) might not yield reasonable J close to 1. Alternatively,
                // disable the fast Newton solvers and run tests.
                double constexpr use_abs_error = 0.0;
                double rel_norm_evaluation     = use_max_err ? 1e-20 : 1e+20;
                for(unsigned int l = 0; l < dim; ++l)
                {
                  for(unsigned int m = l; m < dim; ++m)
                  {
                    double const rel_norm =
                      std::abs((diff_evaluation[l][m][0] + 1e-40) /
                               (use_abs_error +
                                (1.0 - use_abs_error) * (evaluation_double[k][l][m][0] + 1e-20)));
                    if(use_max_err)
                    {
                      rel_norm_evaluation = std::max(rel_norm_evaluation, rel_norm);
                    }
                    else
                    {
                      rel_norm_evaluation = std::min(rel_norm_evaluation, rel_norm);
                    }
                  }
                }

                // Extract only the first entry, since all entries in the
                // dealii::VectorizedArray<Number> have the same value.
                relative_error_samples[k][i] =
                  std::max(relative_error_samples[k][i], rel_norm_evaluation);
              }
            }
          }

          if(stab_test)
          {
            // Output the results to file.
            std::string const file_name = "./stability_forward_test_spatial_integration_" +
                                          std::to_string(spatial_integration) +
                                          "_stable_formulation_" +
                                          std::to_string(stable_formulation) + +"_" +
                                          ExaDG::Utilities::enum_to_string(material_type) + ".txt";

            std::ofstream fstream;
            size_t const  fstream_buffer_size = 256 * 1024;
            char          fstream_buffer[fstream_buffer_size];
            fstream.rdbuf()->pubsetbuf(fstream_buffer, fstream_buffer_size);
            fstream.open(file_name.c_str(), std::ios::trunc);

            std::string const min_max_str = use_max_err ? "MAXIMAL" : "MINIMAL";
            fstream << "  " << min_max_str << " relative errors in stress and stress derivative,\n"
                    << "  grad_increment_scale = 1 / h_e^2 , h_e = " << h_e << "\n"
                    << "  in |.| = max_(samples){ | T_f64 - T_f32 |inf / |T_f64|inf }\n"
                    << "  grad_u_scl    |stress|      |Jacobian|\n";

            for(unsigned int i = 0; i < n_points_over_log_scale; ++i)
            {
              fstream << "  " << std::scientific << std::setw(10) << grad_u_scale[i];
              for(unsigned int k = 0; k < relative_error_samples.size(); ++k)
              {
                fstream << "  " << std::scientific << std::setw(10) << relative_error_samples[k][i];
              }
              fstream << "\n";
            }
            fstream.close();
          }
          else
          {
            // Compute average time and output the timings to the console.
            pcout << "\n"
                  << "  Time in s/" << n_executions_timer << " executions, "
                  << "statistics over " << n_samples * n_points_over_log_scale << " x "
                  << n_executions_timer << " executions.\n";
            for(unsigned int i = 0; i < 2; ++i)
            {
              std::string const str_precision = i == 0 ? "DOUBLE" : "SINGLE";
              // clang-format off
				double const n_total_executions = static_cast<double>(n_points_over_log_scale * n_samples);
				double const t_avg_stress       = time_stress_double_float_sum_min_max[i][0]   / n_total_executions;
				double const t_avg_jacobian     = time_jacobian_double_float_sum_min_max[i][0] / n_total_executions;
				pcout << "  " << str_precision << " PRECISION:\n"
					  << "               avg           / -% to min       / +% to max\n"
					  << "    stress   : "
					  << std::scientific << std::showpos
					  << std::setw(10) << t_avg_stress << " / "
					  << std::setw(10) << 100.0 * (time_stress_double_float_sum_min_max[i][1] - t_avg_stress) / t_avg_stress << " % / "
					  << std::setw(10) << 100.0 * (time_stress_double_float_sum_min_max[i][2] - t_avg_stress) / t_avg_stress << " % \n"
					  << "    Jacobian : "
					  << std::setw(10) << t_avg_jacobian << " / "
					  << std::setw(10) << 100.0 * (time_jacobian_double_float_sum_min_max[i][1] - t_avg_jacobian) / t_avg_jacobian << " % / "
					  << std::setw(10) << 100.0 * (time_jacobian_double_float_sum_min_max[i][2] - t_avg_jacobian) / t_avg_jacobian << " % \n\n";
              // clang-format on
            }
          }
        }
      }
    }
  }
  catch(std::exception & exc)
  {
    std::cerr << std::endl
              << std::endl
              << "----------------------------------------------------" << std::endl;
    std::cerr << "Exception on processing: " << std::endl
              << exc.what() << std::endl
              << "Aborting!" << std::endl
              << "----------------------------------------------------" << std::endl;
    return 1;
  }
  catch(...)
  {
    std::cerr << std::endl
              << std::endl
              << "----------------------------------------------------" << std::endl;
    std::cerr << "Unknown exception!" << std::endl
              << "Aborting!" << std::endl
              << "----------------------------------------------------" << std::endl;
    return 1;
  }

  return 0;
}
