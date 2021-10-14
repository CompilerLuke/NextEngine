#pragma once

#include "core/math/vec3.h"
#include "cfd_core.h"
#include "vendor/eigen/Eigen/Eigen"
#include "equations.h"
#include "core/container/vector.h"

// Mesh
using vec_ix = Eigen::Array<uint,1,Eigen::Dynamic>;
using vec_3fx = Eigen::Array<real,3,Eigen::Dynamic>;
using vec_fx = Eigen::Array<real,1,Eigen::Dynamic>;

struct FV_Face {
    real dx;
    vec3 normal;
    vec3 ortho;
    vec3 cell_to_face;
    uint cell;
};

template<typename Face, typename Base>
struct FV_Faces {
    vector<Face> faces;
};

struct FV_IFace : FV_Face {
    uint neigh;
};

struct FV_BFace : FV_Face {
    
};

template<typename Q>
struct FV_Neuman_B {
    Q dir_deriv;
};

template<typename Q>
struct FV_Diri_B {
    Q value;
};

struct FV_Cells {
    uint length;
    vector<real> inv_volumes;
};

#include <functional>
template<typename... Face>
struct FV_Mesh {
    FV_Cells cells;
    std::tuple<vector<Face>...> faces;
};

