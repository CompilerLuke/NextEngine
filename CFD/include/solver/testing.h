#pragma once

#include "field.h"

#define UNIT_TEST extern "C"

struct CFDDebugRenderer;

struct Testing {
    CFDDebugRenderer& debug;
    
    using ID = uint;
    
    struct Result {
        ID parent;
        string_view name;
        bool failed;
        real duration;
        vector<ID> children;
    };
    
    vector<Result> tests;
    ID current_id;
    uint depth = 0;
    
    Testing(CFDDebugRenderer& debug);
    Result& get_current();
    ID begin(string_view name);
    void fail();
    void end(real duration);
};


struct UnitTest {
    Testing& testing;
    bool ended = false;
    real start;
    
    UnitTest(Testing& testing, string_view name);
    ~UnitTest();
    void end();
};

struct Testing;
struct CFDDebugRenderer;
struct FV_Mesh;

struct SolverTesting {
    Testing& testing;
    CFDDebugRenderer& debug;
    FV_Mesh& mesh;
    
    SolverTesting(Testing& testing, FV_Mesh& mesh);
    void assert_all_cells(const MaskField& field);
    void assert_almost_eq(const ScalarField& field, const ScalarField& ref, string_view mesg = "");
    void assert_almost_eq(const VectorField& field, const VectorField& ref, string_view mesg = "") ;
    
    inline operator Testing&() { return testing; }
};

UNIT_TEST void test_solver(Testing&);
