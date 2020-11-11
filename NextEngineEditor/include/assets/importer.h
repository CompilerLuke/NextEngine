#pragma once

#include "core/container/string_view.h"
#include "core/container/sstring.h"

struct Editor;
struct World;
struct AssetTab;

sstring name_from_filename(string_view filename);
void import_filename(Editor& editor, World& world, AssetTab& self, string_view filename);
