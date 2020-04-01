#include "stdafx.h"
#include <imgui/imgui.h>
#include "visualize_profiler.h"
#include "ecs/system.h"
#include "core/profiler.h"
#include "core/memory/linear_allocator.h"

void drawRectFilled(ImDrawList* drawList, ImVec2 ref, glm::vec2 pos, glm::vec2 size, ImColor color) {
	drawList->AddRectFilled(ImVec2(ref.x + pos.x, ref.y + pos.y), ImVec2(ref.x + pos.x + size.x, ref.y + pos.y + size.y), color);
}

void VisualizeProfiler::render(struct World& world, struct Editor& editor, struct RenderCtx& ctx) {

	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
	if (ImGui::Begin("Profiler", NULL)) {
		vector<float> fps_times;
		fps_times.allocator = &temporary_allocator;


		auto drawList = ImGui::GetWindowDrawList();

		if (Profiler::paused) {
			if (ImGui::Button("Resume")) Profiler::paused = false;
		} else {
			if (ImGui::Button("Pause")) Profiler::paused = true;
		}

		ImVec2 p = ImGui::GetCursorScreenPos();
		p.x += 500;
		p.y += 10;

		float width = 1000;
		float height = 500;

		drawRectFilled(drawList, p, glm::vec2(0,0), glm::vec2(width, height), ImColor(255, 255, 255));

		int num_frames = Profiler::frames.length;

		for (int i = 0; i < num_frames - 1; i++) {
			Frame& frame = Profiler::frames[i];
			fps_times.append(1.0 / frame.frame_swap_duration * 1000);
		}

		int max_depth = 10;

		drawList->ChannelsSplit(max_depth);

		if (Profiler::frames.length > 1) {
			Frame& frame = Profiler::frames[Profiler::frames.length - 2];
			for (int i = 0; i < frame.profiles.length; i++) {
				ProfileData& profile = frame.profiles[i];

				assert(profile.profile_depth < max_depth);

				float in_frame_ratio = profile.start.count() / frame.frame_duration;
				float duration_ratio = profile.duration.count() / frame.frame_duration;
				float offset_profile_depth = profile.profile_depth * 60;

				drawList->ChannelsSetCurrent(profile.profile_depth);

				ImColor color(255 * (i + 1) / frame.profiles.length, 255 * profile.profile_depth / max_depth, 0);

				drawRectFilled(drawList, p, glm::vec2(in_frame_ratio * width, offset_profile_depth), glm::vec2(duration_ratio * 1000, height - offset_profile_depth), color);

				char buffer[100];
				sprintf_s(buffer, "%s - %3.f ms", profile.name, profile.duration.count());

				drawList->AddText(ImVec2(p.x + in_frame_ratio * width, p.y + 10 + offset_profile_depth), ImColor(255,255,255), buffer);
			}
		}

		drawList->ChannelsMerge();

		ImGui::PlotLines("FPS", fps_times.data, fps_times.length, 0, NULL, 0, 70, ImVec2(300, 240));
	}

	ImGui::PopStyleVar();
	ImGui::End();
}