//
//  math_ia.h
//  CFD
//
//  Created by Antonella Calvia on 12/09/2021.
//

#pragma once

struct UI;
struct MathIA;

MathIA* make_math_ia(UI* ui, CFDDebugRenderer* debug);
void render_math_ia(MathIA& math_ia);
void destroy_math_ia(MathIA* ia);
