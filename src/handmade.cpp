#include "handmade.hpp"

namespace Game {
	extern "C" void update_and_render(const Input& input, Memory& memory, Screen_Buffer& screen_buffer) {
		assert(sizeof(Game_State) <= memory.permanent_storage_size);
		Game_State& game_state = *(Game_State*)memory.permanent_storage;

		if (!memory.is_initialized) {
			memory.is_initialized = true;
			game_state.player_x = 100;
			game_state.player_y = 100;
			game_state.player_size = 20;
			game_state.player_velocity = 20;
		}

		for (auto& controller : input.controllers) {
			game_state.tone_hz = 256;
			u32 max_player_x = screen_buffer.width - game_state.player_size;
			u32 max_player_y = screen_buffer.height - game_state.player_size;

			if (controller.is_analog) {
				game_state.tone_hz += (u32)(128.0f * controller.end_y);
				u32 updated_player_y = game_state.player_y - (u32)((f32)game_state.player_velocity * controller.end_y);
				game_state.player_y = updated_player_y <= max_player_y ? updated_player_y : max_player_y * (controller.end_y < 0);
				u32 updated_player_x = game_state.player_x + (u32)((f32)game_state.player_velocity * controller.end_x);
				game_state.player_x = updated_player_x <= max_player_x ? updated_player_x : max_player_x * (controller.end_x > 0);
			}

			if (controller.move_up.is_pressed) 		game_state.player_y = game_state.player_y - game_state.player_velocity <= max_player_y ? game_state.player_y - game_state.player_velocity : 0;
			if (controller.move_left.is_pressed)	game_state.player_x	= game_state.player_x - game_state.player_velocity <= max_player_x ? game_state.player_x - game_state.player_velocity : 0;
			if (controller.move_down.is_pressed)	game_state.player_y = game_state.player_y + game_state.player_velocity <= max_player_y ? game_state.player_y + game_state.player_velocity : max_player_y;
			if (controller.move_right.is_pressed)	game_state.player_x	= game_state.player_x + game_state.player_velocity <= max_player_x ? game_state.player_x + game_state.player_velocity : max_player_x;
		}

		render_gradient(game_state, screen_buffer);
		render_player(game_state, screen_buffer);
	};

	extern "C" void get_sound_samples(Memory& memory, Sound_Buffer& sound_buffer) {
		// f32 volume = 5000.0f;
		f32 volume = 0;
		Game_State& game_state = *(Game_State*)memory.permanent_storage;
		f32 samples_per_wave_period = (f32)(sound_buffer.samples_per_second / game_state.tone_hz);
		Sound_Sample* sample = sound_buffer.samples;

		for (u32 i = 0; i < sound_buffer.samples_to_write; i++) {
			s16 value = (s16)(std::sinf(game_state.t_sine) * volume);
			game_state.t_sine += DOUBLE_PI32 / samples_per_wave_period;
			if (game_state.t_sine >= DOUBLE_PI32) game_state.t_sine -= DOUBLE_PI32;
			*sample++ = {
				.left = value,
				.right = value
			};
		}
	}

	static void render_gradient(const Game_State& game_state, Screen_Buffer& screen_buffer) {
		u32* pixel = screen_buffer.memory;
		for (u32 y = 0; y < screen_buffer.height; y++) {
			for (u32 x = 0; x < screen_buffer.width; x++) {
				*pixel++ = (y << 8) | x;
			}
		}
	};

	static void render_player(const Game_State& game_state, Screen_Buffer& screen_buffer) {
		u32 top = game_state.player_y;
		u32 bottom = game_state.player_y + game_state.player_size;
		u32 left = game_state.player_x;
		u32 right = game_state.player_x + game_state.player_size;

		for (u32 y = top; y < bottom; y++) {
			for (u32 x = left; x < right; x++) {
				u32* pixel = screen_buffer.memory + y * screen_buffer.width + x;
				*pixel = 0x00ffffff;
			}
		}
	};
}