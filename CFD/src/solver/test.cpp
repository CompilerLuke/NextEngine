//
//  test.cpp
//  CFD
//
//  Created by Antonella Calvia on 12/12/2021.
//

#include <stdio.h>
#include "solver/fmesh.h"
#include "solver/fvm.h"
#include "solver/patches.h"
#include "core/container/string_view.h"
#include "core/time.h"
#include "parametric_shapes.h"
#include "mesh.h"
#include "solver/fmesh.h"
#include "solver/testing.h"

using Eigen::all;

Testing::Testing(CFDDebugRenderer& debug) : debug(debug) {
    current_id = 0;
    depth = 0;
    tests.append(Result{UINT_MAX, "Root"});
}
    
Testing::Result& Testing::get_current() {
    return tests[current_id];
}

const char* whitespace = "\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t";

Testing::ID Testing::begin(string_view name) {
    Result result = {};
    result.name = name;
    result.parent = current_id;
    
    ID id = tests.length;
    tests.append(result);
    
    auto& current = get_current();
    if (current.children.length == 0 && current_id != 0) {
        printf("%.*s%s {\n", depth-1, whitespace, current.name.c_str());
    }
    
    current.children.append(id);
    current_id = id;
    depth++;
    
    return id;
}
    
void Testing::fail() {
    ID id = current_id;
    while (id < UINT_MAX) {
        tests[id].failed = true;
        id = tests[id].parent;
    }
    get_current().failed = true;
}

void Testing::end(real duration) {
    depth--;
    
    Result& current = get_current();
    current.duration = duration;
    current_id = current.parent;
    
    uint count = 0;
    for (ID child : current.children) {
        count += !tests[child].failed;
    }
    
    real ms = duration*1000;
    
    if (current.children.length == 0) {
        if (current.failed) {
            printf("%.*s%s - FAIL, %.1fms\n", depth, whitespace, current.name.c_str(), ms);
        }
        else {
            printf("%.*s%s - OK, %.1fms\n", depth, whitespace, current.name.c_str(), ms);
        }
    } else {
        if (current.failed) {
            printf("%.*s} - FAIL (%i/%i), %.1fms\n", depth, whitespace, count, current.children.length, ms);
        }
        else {
            printf("%.*s} - OK (%i/%i) %.1fms\n", depth, whitespace, count, current.children.length, ms);
        }
    }
}

SolverTesting::SolverTesting(Testing& testing, FV_Mesh& mesh) : testing(testing), mesh(mesh), debug(testing.debug) {}

void SolverTesting::assert_all_cells(const MaskField& field) {
        bool passed = field.all();
        
        if (!passed) testing.fail();
    }
    
void SolverTesting::assert_almost_eq(const ScalarField& field, const ScalarField& ref, string_view mesg) {
    real threshold = fmaxf(ref.cwiseAbs().maxCoeff() * 1e-3, FLT_EPSILON);
    MaskField incorrect = (field-ref).cwiseAbs() > threshold;
    if (incorrect.any()) {
        std::cout << (field-ref).cwiseAbs() << std::endl;
        std::cout << "Expected: " << ref << std::endl;
        std::cout << "Got     : " << field << std::endl;
        testing.fail();
    }
}

void SolverTesting::assert_almost_eq(const VectorField& field, const VectorField& ref, string_view mesg) {
    real threshold = fmaxf(ref.cwiseAbs().maxCoeff() * 1e-3, FLT_EPSILON);
    
    MaskField incorrect = ((field-ref).cwiseAbs() > threshold).rowwise().any();
    if (incorrect.any()) {
        std::cout << "Expected:\n" << ref << std::endl;
        std::cout << "Got     :\n" << field << std::endl;
        testing.fail();
    }
}
    
UnitTest::UnitTest(Testing& testing, string_view name) : testing(testing) {
    testing.begin(name);
    start = Time::now();
}
    
void UnitTest::end() {
    if (!ended) {
        ended = true;
        testing.end(Time::now() - start);
    }
}

UnitTest::~UnitTest() {
    end();
}

void test_scalar_fvc(SolverTesting& test) {
    UnitTest unit(test, "Scalar finite volume computation");
    
    FV_Mesh_Data& mesh = *test.mesh.data;
    FV_Scalar U(mesh);
    
    auto* fixed = new FixedGradient(test.mesh.inlet);
    fixed->grads = mesh.normal(2,test.mesh.inlet.faces());
    
    U.add(new FaceAverage(test.mesh.interior));
    U.add(new FixedGradient(test.mesh.wall));
    U.add(fixed);
    
    auto& pos = mesh.centers();
    U.values() = pos(2,all);// + Eigen::sin(pos(2,all));
    
    {
        UnitTest unit(test, "Gradient");
        
        VectorField grad_ref = VectorField::Zero(3, mesh.cell_count);
        grad_ref(2,all) = 1;
        //grad_ref(2,all) = Eigen::cos(pos(2,all));
        
        test.assert_almost_eq(fvc::grad(U), grad_ref);
    }
    
    {
        UnitTest unit(test, "Laplace");
        
        FV_ScalarMatrix mat = fvm::laplace(U);
        mat.set_ref(0, pos(2,0));
        mat.solve();
        
        test.assert_almost_eq(U.values(), pos(2,all));
    }
}

void test_vector_fvc(SolverTesting& test) {
    UnitTest unit(test, "Vector finite volume computation");
    
    FV_Mesh_Data& mesh = *test.mesh.data;
    uint cell_count = mesh.cell_count;
    
    FV_Vector U(mesh);
    
    auto& pos = mesh.centers();
    U.values() = 0;
    U.values()(2,all) = pos(2,all);
    
    auto* fixed = new ZeroVecGradient(test.mesh.inlet);
    fixed->grads(2,all) = mesh.normal(2,test.mesh.inlet.faces());
    
    U.add(new Upwind(test.mesh.interior));
    U.add(new ZeroVecGradient(test.mesh.wall));
    U.add(fixed);
    
    {
        UnitTest unit(test, "Divergence");
        
        ScalarField div_ref = ScalarField::Constant(1,cell_count,1);
        test.assert_almost_eq(fvc::div(U), div_ref);
    }
    
    {
        UnitTest unit(test, "Convective");
        
        VectorField conv_ref = VectorField::Zero(3, cell_count);
        conv_ref(2,all) = U.values()(2,all);
        test.assert_almost_eq(fvc::conv(U.face_values(), U), conv_ref);
    }
    
    {
        UnitTest unit(test, "Laplace");
        
        FV_VectorMatrix eq = fvm::laplace(U);
        //eq.set_ref(0, vec3(0,0,pos(0,2)));
        eq.solve();
        
        U.values()(2,all) += pos(2,0)-U.values()(2,all).minCoeff();
        
        VectorField laplace_ref = VectorField::Zero(3,cell_count);
        laplace_ref(2,all) = pos(2,all);
        
        test.assert_almost_eq(U.values(), laplace_ref);
    }
}

void test_scalar_fvm(SolverTesting& test) {
    
}

void test_vector_fvm(SolverTesting& test) {
    
}

UNIT_TEST
void test_solver(Testing& test) {
    CFDVolume volume = generate_parametric_mesh();
    FV_Mesh mesh = build_mesh(volume, test.debug);
    
    SolverTesting solver(test, mesh);
    
    {
        UnitTest unit(test, "Finite Volume Computation");
        test_scalar_fvc(solver);
        test_vector_fvc(solver);
    }
    
    {
        UnitTest unit(test, "Finite Volume Method");
        test_scalar_fvm(solver);
        test_vector_fvm(solver);
    }
}
