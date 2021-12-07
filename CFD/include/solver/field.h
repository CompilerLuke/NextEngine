#pragma once

#include "core/math/vec3.h"
#include "vendor/eigen/Eigen/Eigen"

using IndexField = Eigen::Matrix<uint, 1, Eigen::Dynamic>;
using ScalarField = Eigen::Matrix<real, 1, Eigen::Dynamic>;
using VectorField = Eigen::Matrix<real, 3, Eigen::Dynamic>;
//Eigen::Array<Eigen::Vector3d, 1, Eigen::Dynamic>;
using TensorField = Eigen::Array<Eigen::Matrix3d, 1, Eigen::Dynamic>;

using Index = Eigen::Index;
using Sequence = Eigen::ArithmeticSequence<Index, Index>;

VectorField tensor_dot(const TensorField& tensor, const VectorField& vec);
