#pragma once

struct UI;
struct World;
struct Selection;
struct Inspector;
struct Lister;

Inspector* make_inspector(World&,Selection&,UI&,Lister&);
void destroy_inspector(Inspector*);

void render_inspector(Inspector&);