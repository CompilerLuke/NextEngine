#pragma once

using real = float;

extern real o3dstaticfilter;
extern real o3derrboundA;
extern real ispstaticfilter;
extern real isperrboundA;

void exactinit(real, real, real);
real insphere(const real *pa, const real *pb, const real *pc, const real *pd, const real *pe);
real orient3d(const real *pa, const real *pb, const real *pc, const real *pd);
