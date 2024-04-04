#pragma once

#include "core/core.h"
#include <math.h>

real lerp(real a, real b, real f) {
    f = f > 1 ? 1 : (f < 0 ? 0 : f);
    return a*(1.0-f) + b*f;
}
