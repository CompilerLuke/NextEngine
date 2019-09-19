#pragma once

#include "core/string_view.h"
#include "editor/assetTab.h"
#include "core/HandleManager.h"

enum ChannelType { Channel1, Channel2, Channel3, Channel4, ChannelNone };

struct ShaderNode {
	enum Type { PBR_NODE, TEXTURE_NODE, TEX_COORDS, MATH_NODE, BLEND_NODE, TIME_NODE, CLAMP_NODE, VEC_NODE, REMAP_NODE, STEP_NODE, NOISE_NODE, PARAM_NODE } type;

	struct Link {
		Handle<ShaderNode> to{ INVALID_HANDLE };
		Handle<ShaderNode> from{ INVALID_HANDLE };

		unsigned int index = 0;

		REFLECT()
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

		InputChannel(StringView, ChannelType);
		InputChannel(StringView, glm::vec4);
		InputChannel(StringView, glm::vec3);
		InputChannel(StringView, glm::vec2);
		InputChannel(StringView, float);

		REFLECT_UNION()
	};

	struct OutputChannel {
		vector<Link> links;

		glm::vec2 position;

		ChannelType type = ChannelNone;
		bool is_being_dragged = false;

		REFLECT()
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
			Handle<struct Texture> tex_handle;
		};
	};

	union {
		TextureNode tex_node;
		MathOp math_op;
		unsigned int param_id;
	};
};

struct ShaderGraph {
	vector<Handle<ShaderNode>> selected;

	HandleManager<ShaderNode> nodes_manager;

	Handle<ShaderNode> terminal_node = { INVALID_HANDLE };

	struct Action {
		enum { Dragging_Node, Moving_Node, Dragging_Connector, Releasing_Connector, None, ApplyMove, BoxSelect, BoxSelectApply } type = None;
		
		Handle<ShaderNode> drag_node;
		glm::vec2 mouse_pos;
		glm::vec2 mouse_delta;
		ShaderNode::Link* link = NULL;
	} action;

	glm::vec2 scrolling;

	RotatablePreview rot_preview;

	vector<StringView> param_names;
	vector<Param> parameters;
	vector<Param> dependencies;
	bool requires_time = false;

	float scale = 1.0f;

	void render(struct World&, struct Editor&, struct RenderParams&, struct Input&);
};

struct ShaderEditor {
	bool open = false;

	int current_shader = -1;

	ShaderEditor();
	void render(struct World&, struct Editor&, struct RenderParams&, struct Input&);

	struct ImFont* font[10];
};

struct ShaderCompiler {
	StringBuffer contents;
	ShaderGraph& graph;

	ShaderCompiler(ShaderGraph&);

	void begin();
	void end();

	void compile_node(Handle<ShaderNode> handle);
	void compile_input(ShaderNode::InputChannel& input);

	void find_dependencies();

	void compile();
};

void render_input(ShaderGraph& graph, Handle<ShaderNode> handle, unsigned int i, const char** names, bool render_default = true);
void render_output(ShaderGraph& graph, Handle<ShaderNode> handle);
glm::vec2 screenspace_to_position(ShaderGraph& graph, glm::vec2 position);
StringBuffer get_dependency(Handle<ShaderNode> node);

ShaderAsset* create_new_shader(World& world, AssetTab& self, Editor& editor, RenderParams& params);
void asset_properties(struct ShaderAsset* tex, struct Editor& editor, struct World& world, struct AssetTab& self, struct RenderParams& params);

StringView get_param_name(ShaderGraph& graph, unsigned int i);

glm::vec2 to_vec2(ImVec2);
ImVec2 from_vec2(glm::vec2);

void load_Shader_for_graph(ShaderAsset* asset);
void compile_shader_graph(ShaderAsset* asset);
void serialize_shader_asset(struct SerializerBuffer& buffer, ShaderAsset* asset);
void deserialize_shader_asset(struct DeserializerBuffer& buffer, ShaderAsset* asset);

extern StringBuffer node_popup_filter;