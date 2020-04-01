#include "stdafx.h"
#include "ecs/system.h"
#include "engine/engine.h"
#include "core/time.h"
#include "core/io/input.h"

UpdateCtx::UpdateCtx(Time* time, Input* input) : 
	input(*input), 
	delta_time(gb::time.delta_time) {};