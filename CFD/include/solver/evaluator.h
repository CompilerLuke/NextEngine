#pragma once

#include "equations.h"
#include "finite_volume.h"

struct IntegralTag { uint id; };

struct VolumeIntegral : IntegralTag {};
struct SurfaceIntegral : IntegralTag {}; //Compiler bug

template<typename Face>
struct EvaluatorBase {
    const FV_Cells& cells;
    const Face& face;
    
    template<typename Tag>
    int eval(const VecUnknown<Tag>& unknown, VolumeIntegral tag) {
        return tag.id;
    }
    
    template<typename Tag>
    int eval(const ScalarUnknown<Tag>& unknown, VolumeIntegral tag) {
        return tag.id;
    }
    
    real eval(const Constant& constant, IntegralTag tag) const {
        return constant.value;
    }
    
    template<typename T>
    T eval(const VectorExpr<T>& expr, VolumeIntegral tag) const {
        return expr.values[tag.id];
    }
    
    template<typename Expr>
    typename DivergenceOp<Expr>::T::Tmp
    eval(const DivergenceOp<Expr>& expr, VolumeIntegral tag) {
        return dot(eval(expr.expr, SurfaceIntegral{0}), face.normal);
    }
};

template<typename Face, bool = std::is_base_of<FV_BFace, Face>::value>
struct DefaultEvaluator : EvaluatorBase<Face> {
    using EvaluatorBase<Face>::eval;
};

//Interior cells
template<>
struct DefaultEvaluator<FV_IFace, false> : EvaluatorBase<FV_IFace> {
    using EvaluatorBase<FV_IFace>::eval;
    
    template<typename Expr>
    auto eval(const LaplaceOp<Expr>& expr, VolumeIntegral tag) {
        auto lhs = eval(expr.expr, VolumeIntegral{face.cell});
        auto rhs = eval(expr.expr, VolumeIntegral{face.neigh});
        
        return lhs - rhs;
    }
    
    // Gradient Operator
    template<typename Expr>
    typename GradientOp<Expr>::T::Tmp
    eval(const GradientOp<Expr>& expr, VolumeIntegral tag) {
        auto lhs = eval(expr.expr, VolumeIntegral{face.cell});
        auto rhs = eval(expr.expr, VolumeIntegral{face.neigh});
        
        return (lhs - rhs) * face.normal;
    }
};

//Boundary cells
template<typename Face>
struct DefaultEvaluator<Face, true> : EvaluatorBase<Face> {
    using EvaluatorBase<Face>::face;
    using EvaluatorBase<Face>::eval;
    
    template<typename Q>
    real eval(const VecUnknown<Q>& unknown, SurfaceIntegral tag) {
        return face.value;
    }

    template<typename Q>
    real eval(const ScalarUnknown<Q>& unknown, SurfaceIntegral tag) {
        return face.value;
    }
    
    template<typename Expr>
    real eval(const GradientOp<Expr>& expr, VolumeIntegral tag) {
        auto lhs = eval(expr.expr, VolumeIntegral{face.cell});
        auto rhs = eval(expr.expr, SurfaceIntegral{0});
        
        return (lhs - rhs) * face.normal;
    }
    
    template<typename Expr>
    real eval(const LaplaceOp<Expr>& expr, VolumeIntegral tag) {
        auto lhs = eval(expr.expr, VolumeIntegral{this->face.cell});
        auto rhs = eval(expr.expr, SurfaceIntegral{0});
        
        return (lhs - rhs) / face.dx;
    }
};

template<typename Face, typename Expr>
void eval_on_faces(typename Expr::T& result, const FV_Cells& cells, const vector<Face>& faces, const Expr& expr) {
    
    for (uint i = 0; i < faces.length; i++) {
        DefaultEvaluator<Face> evaluator{cells, faces[i]};
        result.add_row(i, evaluator.eval(expr, VolumeIntegral()));
    }
}

template<typename Expr, typename... Components>
void eval_on_mesh(const FV_Mesh<Components...>& mesh, const Expr& expr) {
    typename Expr::T result;

    std::apply([&](auto&... faces){
        (eval_on_faces(result, mesh.cells, faces, expr), ...);
    }, mesh.faces);
}

template<typename LHS, typename _RHS, typename... Components>
void solve_on_mesh(const FV_Mesh<Components...>& mesh, const LHS& lhs, const _RHS& rhs) {
    //uint n = lhs.size();
    typedef typename Expr<_RHS>::T RHS;
    
    static_assert(LHS::has_unknowns(), "LHS of equation to be solved must contain unknown");
    static_assert(!RHS::has_unknowns(), "Source term of equation must be known");
    
    eval_on_mesh<LHS, Components...>(mesh, lhs);
    eval_on_mesh<RHS, Components...>(mesh, rhs);
    /*for (uint i = 0; i < n; i++) {
        
    }
    lhs.eval();
    rhs.eval();*/
}
