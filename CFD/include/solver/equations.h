#pragma once

#include "core/container/vector.h"
#include "core/math/vec3.h"
#include "core/container/hash_map.h"

#include <type_traits>

//Results
using Triple = Eigen::Triplet<real>;

template<typename Value> struct VecResult {
    using Tmp = Value;
    using T = Value;
    
    vector<Value> vector;
    
    void add_row(uint cell, Tmp tmp) {
        
    }
};

constexpr uint CoeffRowN = 4;
struct MatrixRow : hash_map<uint, real, CoeffRowN> {
    real source;
    
    MatrixRow(real source) : source(source) {}
};

template<typename Value>
struct MatrixResult {
    using Tmp = MatrixRow;
    using T = Value;
    
    vector<Triple> coeffs;
    vector<real> source;
    
    void add_row(uint cell, const MatrixRow& row) {
        for (uint i = 0; i < CoeffRowN; i++) {
            if (row.is_full(i)) {
                coeffs.append(Triple(cell, row.keys[i], row.values[i]));
            }
        }
        
        source[cell] += row.source;
    }
};

template<typename _>
struct ResultToVector {
    using T = void; //todo static assert
    
    static_assert(std::is_same<_, int>::value, "Expecting scalar");
};

template<>
struct ResultToVector<VecResult<real>> {
    using T = VecResult<vec3>;
};

template<>
struct ResultToVector<MatrixResult<real>> {
    using T = MatrixResult<vec3>;
};

template<typename LHS, typename RHS>
struct ResultUnion {
    static_assert(std::is_same<LHS, RHS>::value);
    
    using T = LHS;
};

template<typename LHS, typename RHS>
struct ResultUnion<MatrixResult<LHS>, VecResult<RHS>> {
    using T = MatrixResult<LHS>;
};

template<typename LHS, typename RHS>
struct ResultUnion<VecResult<LHS>, MatrixResult<RHS>> {
    using T = MatrixResult<RHS>;
};

template<>
struct ResultUnion<VecResult<real>, VecResult<vec3>> {
    using T = VecResult<vec3>;
};

template<>
struct ResultUnion<MatrixResult<vec3>, MatrixResult<real>> {
    using T = MatrixResult<vec3>;
};

//Basic Types
class VectorQuantity {};
class ScalarQuantity {};

template<typename Tag>
class VecUnknown : VectorQuantity {
public:
    using T = MatrixResult<vec3>;
    constexpr static bool has_unknowns() { return true; }
    constexpr static bool is_expr() { return true; }
};

template<typename Tag>
class ScalarUnknown : ScalarQuantity {
public:
    using T = MatrixResult<real>;
    constexpr static bool has_unknowns() { return true; }
    constexpr static bool is_expr() { return true; }
};

struct Constant {
    real value;
    
    using T = VecResult<real>;
    
    Constant(real value) : value(value) {};
    constexpr static bool has_unknowns() { return false; }
    constexpr static bool is_expr() { return true; }
};

template<typename Value>
struct VectorExpr {
    slice<Value> values;
    
    using T = VecResult<Value>;
    
    VectorExpr(const vector<real>& values) : values(values) {};
    constexpr static bool has_unknowns() { return false; }
    constexpr static bool is_expr() { return true; }
};

//Operators
template<typename LHS, typename RHS>
struct BinaryOp {
    LHS lhs;
    RHS rhs;
    
    BinaryOp(const LHS& lhs, const RHS& rhs) : lhs(lhs), rhs(rhs) {}
    
    using T = typename ResultUnion<typename LHS::T, typename RHS::T>::T;
    
    constexpr static bool has_unknowns() { return LHS::has_unknowns() && RHS::has_unknowns(); }
    constexpr static bool is_expr() { return true; }
};

struct ExprBase {};

template<typename Expr>
struct UnaryOp {
    Expr expr;
    
    using T = typename Expr::T;
    
    UnaryOp(const Expr& expr) : expr(expr) {}
    constexpr static bool has_unknowns() { return Expr::has_unknowns(); }
    constexpr static bool is_expr() { return true; }
};

template<typename LHS, typename RHS>
struct AddOp : public BinaryOp<LHS, RHS> {
    using BinaryOp<LHS, RHS>::BinaryOp;
};

template<typename LHS, typename RHS>
struct SubOp : public BinaryOp<LHS, RHS> {
    using BinaryOp<LHS, RHS>::BinaryOp;
};

template<typename LHS, typename RHS>
struct MulOp : public BinaryOp<LHS, RHS> {
    using BinaryOp<LHS, RHS>::BinaryOp;
    
    constexpr static bool has_unknowns() { return LHS::has_unknowns() || RHS::has_unknowns(); }
};

template<typename LHS, typename RHS>
struct DotOp : BinaryOp<LHS, RHS> {
    using BinaryOp<LHS, RHS>::BinaryOp;
};

template<typename Expr>
struct NegateOp : public UnaryOp<Expr> {
    using UnaryOp<Expr>::BinaryOp;
};

// Spatial Operators
template<typename Expr>
struct DivergenceOp : UnaryOp<Expr> {
    using UnaryOp<Expr>::UnaryOp;
};

template<typename Expr>
struct GradientOp : UnaryOp<Expr> {
    using T = typename ResultToVector<typename Expr::T>::T;
    
    using UnaryOp<Expr>::UnaryOp;
};

template<typename Expr>
struct LaplaceOp : UnaryOp<Expr> {
    using T = typename Expr::T;
    using UnaryOp<Expr>::UnaryOp;
};

template<typename Expr>
DivergenceOp<Expr> diver(const Expr& expr) {
    return DivergenceOp{expr};
}

template<typename E>
struct Expr {
    static_assert(E::is_expr(), "Expecting expression");
    
    using T = E;
};

template<>
struct Expr<real> {
    using T = Constant;
};

template<typename Value>
struct Expr<vector<Value>> {
    using T = VectorExpr<Value>;
};

template<typename LHS>
GradientOp<typename Expr<LHS>::T> grad(const LHS& expr) {
    return GradientOp<typename Expr<LHS>::T>{expr};
}

template<typename LHS>
LaplaceOp<typename Expr<LHS>::T> laplacian(const LHS& expr) {
    return LaplaceOp<typename Expr<LHS>::T>(expr);
}

namespace equation {
    template<typename LHS, typename RHS>
    AddOp<typename Expr<LHS>::T, typename Expr<RHS>::T> operator+(const LHS& lhs, const RHS& rhs) {
        static_assert(std::is_base_of<LHS, ExprBase>::value, "Expecting expression");
        return {lhs, rhs};
    }

    template<typename LHS, typename RHS>
    SubOp<typename Expr<LHS>::T, typename Expr<RHS>::T> operator-(const LHS& lhs, const LHS& rhs) {
        return {lhs, rhs};
    }

    template<typename LHS>
    NegateOp<typename Expr<LHS>::T> operator-(const LHS& lhs) {
        return {lhs};
    }

    template<typename LHS, typename RHS>
    MulOp<typename Expr<LHS>::T, typename Expr<RHS>::T> operator*(const LHS& lhs, const RHS& rhs) {
        return {lhs, rhs};
    }
}

