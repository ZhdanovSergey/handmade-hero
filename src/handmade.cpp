#include "handmade.hpp"

namespace Game {
	extern "C" void update_and_render(const Input& input, Memory& memory, Screen_Buffer& screen_buffer) {
		assert(sizeof(Game_State) <= (u32)memory.permanent_size);
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
			i32 max_player_x = screen_buffer.width - game_state.player_size;
			i32 max_player_y = screen_buffer.height - game_state.player_size;
			i32 updated_player_x = game_state.player_x;
			i32 updated_player_y = game_state.player_y;

			if (controller.is_analog) {
				game_state.tone_hz += (i32)(128.0f * controller.end_y);
				updated_player_x = game_state.player_x + (i32)((f32)game_state.player_velocity * controller.end_x);
				updated_player_y = game_state.player_y - (i32)((f32)game_state.player_velocity * controller.end_y);
			} else {
				updated_player_x = game_state.player_x;
				if (controller.move_left.is_pressed)  updated_player_x -= game_state.player_velocity;
				if (controller.move_right.is_pressed) updated_player_x += game_state.player_velocity;
				updated_player_y = game_state.player_y;
				if (controller.move_up.is_pressed)   updated_player_y -= game_state.player_velocity;
				if (controller.move_down.is_pressed) updated_player_y += game_state.player_velocity;
			}

			game_state.player_x = updated_player_x < 0 ? 0 : updated_player_x > max_player_x ? max_player_x : updated_player_x;
			game_state.player_y = updated_player_y < 0 ? 0 : updated_player_y > max_player_y ? max_player_y : updated_player_y;
		}

		render_gradient(game_state, screen_buffer);
		render_rectangle(screen_buffer, game_state.player_x, game_state.player_y, game_state.player_size, game_state.player_size, 0x00ffffff);

		if constexpr (DEV_MODE) {
			render_rectangle(screen_buffer, input.dev_mouse.x, input.dev_mouse.y, 10, 10, 0x00ff00ff);
			if (input.dev_mouse.left_button.is_pressed) render_rectangle(screen_buffer, 10, 10, 10, 10, 0x00ff00ff);
			if (input.dev_mouse.right_button.is_pressed) render_rectangle(screen_buffer, 30, 10, 10, 10, 0x00ff00ff);
		}
	};

	extern "C" void get_sound_samples(Memory& memory, Sound_Buffer& sound_buffer) {
		f32 volume = 5000.0f;
		Game_State& game_state = *(Game_State*)memory.permanent_storage;
		f32 samples_per_wave_period = (f32)(sound_buffer.samples_per_second / game_state.tone_hz);
		Sound_Sample* sample = sound_buffer.samples;

		for (i32 i = 0; i < sound_buffer.samples_to_write; i++) {
			i16 value = (i16)(std::sinf(game_state.t_sine) * volume);
			game_state.t_sine += DOUBLE_PI32 / samples_per_wave_period;
			if (game_state.t_sine >= DOUBLE_PI32) game_state.t_sine -= DOUBLE_PI32;
			*sample++ = {
				.left = value,
				.right = value
			};
		}
	}

	static void render_gradient(const Game_State& game_state, Screen_Buffer& screen_buffer) {
		u32* pixel = screen_buffer.pixels;
		for (i32 y = 0; y < screen_buffer.height; y++) {
			for (i32 x = 0; x < screen_buffer.width; x++) {
				*pixel++ = ((u32)y << 8) | (u32)x;
			}
		}
	};

	static void render_rectangle(Screen_Buffer& screen_buffer, i32 x, i32 y, i32 width, i32 height, u32 color) {
		i32 top = y;
		i32 bottom = top + height;
		i32 left = x;
		i32 right = left + width;

		top    = top    < 0 ? 0 : top    > screen_buffer.height ? screen_buffer.height : top;
		bottom = bottom < 0 ? 0 : bottom > screen_buffer.height ? screen_buffer.height : bottom;
		left   = left   < 0 ? 0 : left   > screen_buffer.width  ? screen_buffer.width  : left;
		right  = right  < 0 ? 0 : right  > screen_buffer.width  ? screen_buffer.width  : right;

		for (i32 pixel_y = top; pixel_y < bottom; pixel_y++) {
			for (i32 pixel_x = left; pixel_x < right; pixel_x++) {
				u32* pixel = screen_buffer.pixels + pixel_y * screen_buffer.width + pixel_x;
				*pixel = color;
			}
		}
	};
}