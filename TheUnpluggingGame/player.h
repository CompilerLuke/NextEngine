#pragma once

#include <reflection/reflection.h>

#define COMPONENT()

COMPONENT()
struct Player {
	REFLECT()
	float health = 100;
};