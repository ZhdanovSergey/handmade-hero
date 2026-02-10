#include "handmade.hpp"

namespace Game {
	extern "C" void update_and_render(const Input& input, Memory& memory, Screen_Buffer& screen_buffer) {
		assert(sizeof(Game_State) <= (size_t)memory.permanent_size);
		if (!memory.is_initialized) memory.is_initialized = true;
		
		draw_rectangle(screen_buffer, 0.f, (f32)screen_buffer.width, 0.f, (f32)screen_buffer.height, 0);
		draw_rectangle(screen_buffer, 10.f, 30.f, 10.f, 30.f, 0xffffff);
	};

	extern "C" void get_sound_samples(Memory& memory, Sound_Buffer& sound_buffer) {
		Sound_Sample* sample = sound_buffer.samples;

		for (i32 i = 0; i < sound_buffer.samples_to_write; i++) {
			i16 value = 0;
			sample->left = value;
			sample->right = value;
			sample++;
		}
	}

	static void draw_rectangle(Screen_Buffer& screen_buffer, f32 min_x_f32, f32 max_x_f32, f32 min_y_f32, f32 max_y_f32, u32 color) {
		i32 min_x = (i32)hm::round(min_x_f32);
		i32 max_x = (i32)hm::round(max_x_f32);
		i32 min_y = (i32)hm::round(min_y_f32);
		i32 max_y = (i32)hm::round(max_y_f32);

		min_x = hm::max(min_x, 0);
		min_y = hm::max(min_y, 0);
		max_x = hm::min(max_x, screen_buffer.width);
		max_y = hm::min(max_y, screen_buffer.height);

		u32* row = screen_buffer.pixels + min_y * screen_buffer.width + min_x;
		for (i32 y = min_y; y < max_y; y++) {
			u32* pixel = row;
			for (i32 x = min_x; x < max_x; x++) {
				*pixel++ = color;
			}
			row += screen_buffer.width;
		}
	};

	static void dev_render_gradient(const Game_State& game_state, Screen_Buffer& screen_buffer) {
		if constexpr (!DEV_MODE) return;
		u32* pixel = screen_buffer.pixels;
		for (i32 y = 0; y < screen_buffer.height; y++) {
			for (i32 x = 0; x < screen_buffer.width; x++) {
				*pixel++ = ((u32)y << 8) | (u32)x;
			}
		}
	};

	static void dev_render_mouse_test(const Input& input, Screen_Buffer& screen_buffer) {
		if constexpr (!DEV_MODE) return;
		draw_rectangle(screen_buffer, (f32)input.dev_mouse.x, (f32)input.dev_mouse.x + 10.f, (f32)input.dev_mouse.y, (f32)input.dev_mouse.y + 10.f, 0xff00ff);
		if (input.dev_mouse.left_button.is_pressed) draw_rectangle(screen_buffer, 10.f, 20.f, 10.f, 20.f, 0xff00ff);
		if (input.dev_mouse.right_button.is_pressed) draw_rectangle(screen_buffer, 30.f, 40.f, 10.f, 20.f, 0xff00ff);
	};
}