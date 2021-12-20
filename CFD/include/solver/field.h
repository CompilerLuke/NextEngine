#pragma once

#include "core/math/vec3.h"
#include "vendor/eigen/Eigen/Eigen"

using MaskField = Eigen::Array<bool, 1, Eigen::Dynamic>;
using IndexField = Eigen::Array<uint, 1, Eigen::Dynamic>;
using ScalarField = Eigen::Array<real, 1, Eigen::Dynamic>;
using VectorField = Eigen::Array<real, 3, Eigen::Dynamic>;
//Eigen::Array<Eigen::Vector3d, 1, Eigen::Dynamic>;
using TensorField = Eigen::Array<Eigen::Matrix3d, 1, Eigen::Dynamic>;

using Index = Eigen::Index;
using Sequence = Eigen::ArithmeticSequence<Index, Index>;

inline VectorField tensor_dot(const TensorField& tensor, const VectorField& vec) {
	uint n = vec.cols();
	VectorField result(3, n);

	for (uint i = 0; i < n; i++) {
		result.col(i) = tensor(i) * vec.col(i).matrix();
	}
	return result;
}

template<typename A, typename B>
ScalarField vec_dot(const A& a, const B& b) {
	assert(a.rows() == b.rows());
	assert(a.cols() == b.cols());

	VectorField prod = a * b;
	return prod.colwise().sum().transpose();
}
