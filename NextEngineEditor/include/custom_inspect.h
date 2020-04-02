#pragma once

struct AssetManager;

void register_on_inspect_callbacks(AssetManager& asset_manager);
bool accept_drop(const char* drop_type, void* ptr, unsigned int size);