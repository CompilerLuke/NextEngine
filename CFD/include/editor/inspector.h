#pragma once

struct UI;
struct World;
struct SceneSelection;
struct Inspector;
struct Lister;

Inspector* make_inspector(World&,SceneSelection&,UI&,Lister&);
void destroy_inspector(Inspector*);

void render_inspector(Inspector&);