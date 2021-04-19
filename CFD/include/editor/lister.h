#pragma once

#include "core/container/string_view.h"
#include "ecs/id.h"

struct Lister;
struct World;
struct SceneSelection;
struct sstring;

Lister* make_lister(World&, SceneSelection& selection, UI& ui);
void destroy_lister(Lister*);

sstring& name_of_entity(Lister& lister, ID id);
void register_entity(Lister&, string_view name, ID id);
void render_lister(Lister&);
