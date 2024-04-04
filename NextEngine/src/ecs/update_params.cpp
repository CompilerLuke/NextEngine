#include "ecs/system.h"
#include "engine/engine.h"
#include "core/time.h"
#include "engine/input.h"

UpdateCtx::UpdateCtx(Time& time, Input& input) : 
	input(input), 
	delta_time(time.delta_time) {};