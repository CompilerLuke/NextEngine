#pragma once

#include <core/reflection.h>

#define COMPONENT()

COMPONENT()
struct Player {
	REFLECT()
	float health = 100;
};