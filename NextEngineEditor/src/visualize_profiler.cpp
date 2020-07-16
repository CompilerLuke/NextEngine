#include <imgui/imgui.h>
#include "visualize_profiler.h"
#include "core/profiler.h"
#include "core/memory/linear_allocator.h"
#include "core/container/hash_map.h"
#include <stdio.h>
#include <algorithm>

int is_max(int a, int b) {
	return a > b ? a : b;
}

#define ms(value) (value / 1000)

__forceinline
void drawRectFilled(ImDrawList* drawList, ImVec2 ref, glm::vec2 pos, glm::vec2 size, ImColor color) {
	drawList->AddRectFilled(ImVec2(ref.x + pos.x, ref.y + pos.y), ImVec2(ref.x + pos.x + size.x, ref.y + pos.y + size.y), color);
}

struct ProfileDrawData {
	ImColor color;
	glm::vec2 pos;
	glm::vec2 size;
	int depth;
};

struct ProfileCtx {
	hash_set<sstring, 103>& name_to_color_idx;
	ImDrawList* drawList;
	glm::vec2 p;
	float width, height;
	uint frame_count;
	float frame_max;
};

ImColor profile_colors[11] = {
	ImColor(9, 189, 75),
	ImColor(250, 179, 15),
	ImColor(187, 250, 15),
	ImColor(17, 209, 199),
	ImColor(190, 14, 235),
	ImColor(81, 76, 237),
	ImColor(76, 237, 181),
	ImColor(181, 13, 24),
	ImColor(53, 79, 130),
	ImColor(58, 148, 97),
	ImColor(227, 146, 48),
};

ProfileDrawData profile_draw_data(ProfileCtx& ctx, uint frame, const ProfileData& profile) {
	ImColor color = profile_colors[ctx.name_to_color_idx.add(profile.name) % 11]; //(100, 255, 100);
	double duration_ratio_height = profile.duration / ctx.frame_max;
	double start_ratio = profile.start / ctx.frame_max;
	double duration_ratio_width = 1.0 / ctx.frame_count;
	double in_frame_ratio = (double)frame / ctx.frame_count;

	glm::vec2 pos(in_frame_ratio * ctx.width, (1.00f - duration_ratio_height - start_ratio) * ctx.height);
	glm::vec2 size((duration_ratio_width) * ctx.width, duration_ratio_height * ctx.height);

	return { color, pos, size, profile.profile_depth };
}

void draw_profile(ProfileCtx& ctx, const ProfileDrawData& data) {
	ctx.drawList->ChannelsSetCurrent(data.depth);
	drawRectFilled(ctx.drawList, ctx.p, data.pos, data.size, data.color);
}

ProfileDrawData draw_profile(ProfileCtx& ctx, uint frame, const ProfileData& profile) {
	ProfileDrawData data = profile_draw_data(ctx, frame, profile);
	draw_profile(ctx, data);
	return data;
}

void VisualizeProfiler::render(struct World& world, struct Editor& editor, struct RenderPass& ctx) {
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
	if (ImGui::Begin("Profiler", NULL)) {
		vector<float> fps_times;
		fps_times.allocator = &temporary_allocator;


		auto drawList = ImGui::GetWindowDrawList();


		{

			if (Profiler::paused) {
				if (ImGui::Button("Resume")) Profiler::paused = false;
			}
			else {
				if (ImGui::Button("Pause")) Profiler::paused = true;
			}

			ImGui::SameLine(500);
		}

		{
			float frame_max = 0.0f;
			float frame_min = FLT_MAX;
			float avg_duration = 0.0f;

			for (Frame& frame : Profiler::frames) {
				if (frame.frame_duration < 0) continue;
				frame_max = fmaxf(frame_max, frame.frame_duration);
				frame_min = fminf(frame_min, frame.frame_duration);
				avg_duration += frame.frame_duration;
			}

			avg_duration /= (float)Profiler::frames.length;

			ImGui::Text("avg [%.1f ms %.1f fps]", avg_duration * 1000.0f, 1.0f / avg_duration);
			ImGui::SameLine();
			ImGui::Text("max [%.1f ms %.1f fps]", frame_min * 1000, 1.0f / frame_min);
			ImGui::SameLine();
			ImGui::Text("min[%.1f ms %.1f fps]", frame_max * 1000, 1.0f / frame_max);
		}

		{
			ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 1100);

			ImGui::SetNextItemWidth(300);
			ImGui::SliderFloat("Max frame time", &frame_max_time, 0, 0.1);
			ImGui::SameLine();

			int frame_sample_count = Profiler::frame_sample_count;
			ImGui::SetNextItemWidth(300);
			ImGui::SliderInt("Frame sample count", &frame_sample_count, 10, 1000);
			Profiler::set_frame_sample_count(frame_sample_count);

		}

		ProfileCtx ctx = {name_to_color_idx, drawList, ImGui::GetCursorScreenPos()};
		ctx.width = ImGui::GetContentRegionMax().x * 0.8;
		ctx.height = ImGui::GetContentRegionMax().y - ImGui::GetCursorPos().y;
		ctx.frame_count = Profiler::frames.length - 1;
		ctx.frame_max = frame_max_time;

		const float slow_frame = 1.0f / 55.0f;
		const int max_depth = 10;

		drawList->ChannelsSplit(max_depth);

		
		ImGui::PushClipRect(ctx.p, ctx.p + glm::vec2(ctx.width, ctx.height), true);

		glm::vec2 mouse_pos = ImGui::GetMousePos();
		mouse_pos -= ctx.p;

		uint frames_length = ctx.frame_count;
		for (int frame_i = 0; frame_i < frames_length; frame_i++) {
			Frame& frame = Profiler::frames[frame_i];

			drawList->ChannelsSetCurrent(0);

			ProfileData frame_profile = {};
			frame_profile.duration = frame.frame_duration;
			frame_profile.name = "Frame";

			ProfileDrawData frame_draw = profile_draw_data(ctx, frame_i, frame_profile);
			frame_draw.color = frame.frame_duration > slow_frame ? ImColor(255, 100, 100) : ImColor(100, 255, 100);
			draw_profile(ctx, frame_i, frame_profile);

			for (int i = 0; i < frame.profiles.length; i++) {
				ProfileData& profile = frame.profiles[i];
				if (profile.duration < 0.001) continue;

				assert(profile.profile_depth < max_depth);

				ProfileDrawData data = draw_profile(ctx, frame_i, profile);

				if (data.pos.x < mouse_pos.x && data.pos.y < mouse_pos.y && data.pos.x + data.size.x > mouse_pos.x && data.pos.y + data.size.y > mouse_pos.y) {
					ImGui::BeginTooltip();
					ImGui::Text("%s - [%.4fms]", profile.name, profile.duration * 1000);
					ImGui::EndTooltip();
				}

				if (Profiler::paused && Profiler::frame_sample_count < 100) {

				}
			}

		}

		drawList->ChannelsMerge();
		ImGui::PopClipRect();


		if (frames_length >= 1) {
			Frame& frame = Profiler::frames[frames_length - 1];

			vector<ProfileData> sorted_profiles = frame.profiles;

			std::sort(frame.profiles.begin(), frame.profiles.end(), [](ProfileData& a, ProfileData& b) { 
				float mid_a = (a.start + a.duration) / 2.0f;
				float mid_b = (b.start + b.duration) / 2.0f;

				return mid_a > mid_b; 
			});

			ImGui::Dummy(ImVec2(0,50));

			for (ProfileData& profile : frame.profiles) {
				ProfileDrawData data = profile_draw_data(ctx, frames_length, profile);
				data.pos += ctx.p;

				ImVec2 points[4];
				points[0] = glm::vec2(data.pos.x - 1.0f, data.pos.y);
				points[1] = glm::vec2(data.pos.x + 101.0f, ImGui::GetCursorScreenPos().y);
				points[2] = glm::vec2(data.pos.x + 101.0f, ImGui::GetCursorScreenPos().y + ImGui::GetTextLineHeightWithSpacing());
				points[3] = glm::vec2(data.pos.x - 1.0f, data.pos.y + data.size.y);

				//drawList->AddRectFilled(ImVec2(points[0].x - tip, points[0].y), ImVec2(points[0].x, points[3].y), data.color);
				drawList->AddConvexPolyFilled(points, 4, data.color);
				drawList->AddRectFilled(ImVec2(points[1].x - 1.0f, points[1].y), ImVec2(points[2].x + 50.0f, points[2].y), data.color);
				
				ImGui::SetCursorPosX(ctx.width + 250.f);
				ImGui::TextColored(data.color, "%s - %.2f ms", profile.name, profile.duration * 1000);
			}
		}
		
		/*
		for (int i = start_index; i < Profiler::frames.length; i++) {
			Frame& frame = Profiler::frames[i];
			for (int i = 0; i < frame.profiles.length; i++) {
				ProfileData& profile = frame.profiles[i];
				if (profile.duration < 0.001) continue;

				double frame_offset = frame.start_of_frame - total_frame_offset + profile.start;

				assert(profile.profile_depth < max_depth);

				//log("In frame ratio ", (float)frame_offset.count(), " ", total_frame_duration, "\n");

				float in_frame_ratio = frame_offset / total_frame_duration;
				float duration_ratio = profile.duration / total_frame_duration;
				float offset_profile_depth = profile.profile_depth * 60;

				drawList->ChannelsSetCurrent(profile.profile_depth);

				ImColor color(255 * (i + 1) / frame.profiles.length, 255 * profile.profile_depth / max_depth, 0);

				drawRectFilled(drawList, p, glm::vec2(in_frame_ratio * width, offset_profile_depth), glm::vec2(duration_ratio * width, height - offset_profile_depth), color);

				char buffer[100];
				sprintf_s(buffer, "%s - %3.f ms", profile.name, profile.duration * 1000);

				drawList->AddText(ImVec2(p.x + in_frame_ratio * width, p.y + 10 + offset_profile_depth), ImColor(255,255,255), buffer);
			}
		}*/



		//ImGui::PlotLines("FPS", fps_times.data, fps_times.length, 0, NULL, 0, 70, ImVec2(300, 240));
	}

	ImGui::PopStyleVar();
	ImGui::End();
}