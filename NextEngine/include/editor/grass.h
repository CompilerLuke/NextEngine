#include "core/handle.h"
#include "core/vector.h"
#include "components/transform.h"
#include "reflection/reflection.h"
#include "ecs/system.h"

struct Grass {
	Handle<struct Model> placement_model = { INVALID_HANDLE };
	bool cast_shadows = false;
	
	float width = 5.0f;
	float height = 5.0f;
	float max_height = 5.0f;
	float density = 10.0f;

	float random_rotation = 1.0f;
	float random_scale = 1.0f;
	bool align_to_terrain_normal = false;

	vector<Transform> transforms;

	REFLECT()

	void place(struct World&);
};

struct GrassSystem : System {
	void render(struct World&, struct RenderParams&) override;
};