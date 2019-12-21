#pragma once

void register_on_inspect_callbacks();
bool accept_drop(const char* drop_type, void* ptr, unsigned int size);