#pragma once

#include "core/container/string_view.h"
#include "assetTab.h"
#include "core/container/handle_manager.h"

enum ChannelType { Channel1, Channel2, Channel3, Channel4, ChannelNone };

struct shader_node_handle {
	uint id = INVALID_HANDLE;

	REFLECT()
};

struct ShaderNode {
	enum Type { PBR_NODE, TEXTURE_NODE, TEX_COORDS, MATH_NODE, BLEND_NODE, TIME_NODE, CLAMP_NODE, VEC_NODE, REMAP_NODE, STEP_NODE, NOISE_NODE, PARAM_NODE } type;

	struct Link {
		shader_node_handle to{ INVALID_HANDLE };
		shader_node_handle from{ INVALID_HANDLE };

		unsigned int index = 0;

		REFLECT(NO_ARG)
	};

	struct InputChannel {
		Link link;

		ChannelType type;

		glm::vec2 position;

		union {
			float value1;
			glm::vec2 value2;
			glm::vec3 value3;
			glm::vec4 value4;
		};

		InputChannel() {}
		InputChannel(string_view, ChannelType);
		InputChannel(string_view, glm::vec4);
		InputChannel(string_view, glm::vec3);
		InputChannel(string_view, glm::vec2);
		InputChannel(string_view, float);

		REFLECT_UNION(NO_ARG)
	};

	struct OutputChannel {
		vector<Link> links;

		glm::vec2 position;

		ChannelType type = ChannelNone;
		bool is_being_dragged = false;

		REFLECT(NO_ARG)
	};

	vector<InputChannel> inputs;
	OutputChannel output;

	glm::vec2 position;
	bool collapsed = false;

	ShaderNode(Type, ChannelType);
	ShaderNode();

	enum MathOp { Add = 0, Sub = 1, Mul = 2, Div = 3, Sin = 4, Cos = 5, SinOne = 6, CosOne = 7 };

	struct TextureNode {
		bool from_param;
		union {
			unsigned int param_id;
			texture_handle tex_handle;
		};
	};

	union {
		TextureNode tex_node;
		MathOp math_op;
		unsigned int param_id;
	};
};

struct ShaderGraph {
	vector<shader_node_handle> selected;

	HandleManager<ShaderNode, shader_node_handle> nodes_manager;

	shader_node_handle terminal_node = { INVALID_HANDLE };

	struct Action {
		enum { Dragging_Node, Moving_Node, Dragging_Connector, Releasing_Connector, None, ApplyMove, BoxSelect, BoxSelectApply } type = None;
		
		shader_node_handle drag_node;
		glm::vec2 mouse_pos;
		glm::vec2 mouse_delta;
		ShaderNode::Link* link = NULL;
	} action;

	glm::vec2 scrolling;

	RotatablePreview rot_preview;

	vector<ParamDesc> parameters;
	vector<ParamDesc> dependencies;
	bool requires_time = false;

	float scale = 1.0f;

	void render(struct World&, struct RenderPass&, struct Input&);
};

struct ShaderEditor {
	bool open = false;

	asset_handle current_shader{ INVALID_HANDLE };

	ShaderEditor();
	void render(struct World&, struct Editor&, struct RenderPass&, struct Input&);

	struct ImFont* font[10];
};

struct ShaderCompiler {
	string_buffer contents;
	ShaderGraph& graph;

	ShaderCompiler(ShaderGraph&);

	void begin();
	void end();

	void compile_node(shader_node_handle handle);
	void compile_input(ShaderNode::InputChannel& input);

	void find_dependencies();

	void compile();
};

void render_input(ShaderGraph& graph, shader_node_handle handle, unsigned int i, const char** names, bool render_default = true);
void render_output(ShaderGraph& graph, shader_node_handle handle);
glm::vec2 screenspace_to_position(ShaderGraph& graph, glm::vec2 position);
string_buffer get_dependency(shader_node_handle node);

ShaderAsset* create_new_shader(World& world, AssetTab& self, Editor& editor);
void asset_properties(struct ShaderAsset* tex, struct Editor& editor, struct AssetTab& self, struct RenderPass& params);

string_view get_param_name(ShaderGraph& graph, unsigned int i);

struct Assets;
struct SerializerBuffer;

void load_Shader_for_graph(ShaderAsset* asset);
void compile_shader_graph(AssetInfo&, ShaderAsset* asset);
void serialize_shader_asset(struct SerializerBuffer& buffer, ShaderAsset* asset);
void deserialize_shader_asset(struct DeserializerBuffer& buffer, ShaderAsset* asset);

extern string_buffer node_popup_filter;