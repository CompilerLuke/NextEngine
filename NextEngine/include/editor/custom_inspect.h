#pragma once

void register_on_inspect_callbacks();
void accept_drop(const char* drop_type, void* ptr, unsigned int size);
bool Material_inspect(void* data, reflect::TypeDescriptor* type, const std::string& prefix, World& world);