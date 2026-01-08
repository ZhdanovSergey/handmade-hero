#include "handmade.hpp"

namespace Game {
	extern "C" void update_and_render(const Input& input, Memory& memory, Screen_Buffer& screen_buffer) {
		assert(sizeof(Game_State) <= memory.permanent_storage_size);
		if (!memory.is_initialized) memory.is_initialized = true;
		Game_State& game_state = *(Game_State*)memory.permanent_storage;

		for (auto& controller : input.controllers) {
			if (controller.move_up.is_pressed)		game_state.green_offset -= 10;
			if (controller.move_down.is_pressed)	game_state.green_offset += 10;
			if (controller.move_right.is_pressed)	game_state.blue_offset	 += 10;
			if (controller.move_left.is_pressed)	game_state.blue_offset	 -= 10;
			
			game_state.tone_hz = 256;
			if (controller.is_analog) {
				game_state.tone_hz		 += (u32)(128.0f * controller.end_y);
				game_state.green_offset -= (u32)(20.0f  * controller.end_y);
				game_state.blue_offset  += (u32)(20.0f  * controller.end_x);
			}
		}

		render_gradient(game_state, screen_buffer);
	};

	extern "C" void get_sound_samples(Memory& memory, Sound_Buffer& sound_buffer) {
		Game_State& game_state = *(Game_State*)memory.permanent_storage;
		f32 volume = 5000.0f;

		f32 samples_per_wave_period = (f32)(sound_buffer.samples_per_second / game_state.tone_hz);
		Sound_Sample* sound_sample = sound_buffer.samples;

		for (u32 i = 0; i < sound_buffer.samples_to_write; i++) {
			s16 sample_value = (s16)(std::sinf(game_state.t_sine) * volume);
			game_state.t_sine += DOUBLE_PI32 / samples_per_wave_period;
			if (game_state.t_sine >= DOUBLE_PI32) game_state.t_sine -= DOUBLE_PI32;
			*sound_sample++ = {
				.left = sample_value,
				.right = sample_value
			};
		}
	}

	static void render_gradient(const Game_State& game_state, Screen_Buffer& screen_buffer) {
		for (u32 y = 0; y < screen_buffer.height; y++) {
			for (u32 x = 0; x < screen_buffer.width; x++) {
				u32 green = (y + game_state.green_offset) & UINT8_MAX;
				u32 blue  = (x + game_state.blue_offset ) & UINT8_MAX;
				*screen_buffer.memory++ = (green << 8) | blue;
			}
		}
	};
}