#pragma once

#include "globals.hpp"

namespace Game {
	struct Game_State {
		f32 player_x;
		f32 player_y;
	};

	struct Button {
		bool is_pressed;
		i32 transitions_count;
	};

	struct Controller {
		bool is_connected;
		bool is_analog;

		f32 start_x, start_y;
		f32 end_x, end_y;
		f32 average_x, average_y;
		f32 min_x, min_y;
		f32 max_x, max_y;

		Button start, back;
		Button left_shoulder, right_shoulder;
		Button move_up, move_down, move_left, move_right;
		Button action_up, action_down, action_left, action_right;
	};

	struct Dev_Mouse {
		i32 x, y;
		Button left_button;
		Button right_button;
	};

	struct Input {
		Controller controllers[2];
		Dev_Mouse dev_mouse;
		f32 frame_dt;

		void reset_counters() {
			dev_mouse.left_button.transitions_count = 0;
			dev_mouse.right_button.transitions_count = 0;

			for (auto& controller : controllers) {
				controller.start.transitions_count = 0;
				controller.back.transitions_count = 0;
				controller.left_shoulder.transitions_count = 0;
				controller.right_shoulder.transitions_count = 0;

				controller.move_up.transitions_count = 0;
				controller.move_down.transitions_count = 0;
				controller.move_left.transitions_count = 0;
				controller.move_right.transitions_count = 0;

				controller.action_up.transitions_count = 0;
				controller.action_down.transitions_count = 0;
				controller.action_left.transitions_count = 0;
				controller.action_right.transitions_count = 0;
			}
		}
	};

	struct Memory {
		bool is_initialized;
		i64 permanent_size;
		i64 transient_size;
		u8* permanent_storage;
		u8* transient_storage;
    	Platform::Read_File_Sync* read_file_sync;
    	Platform::Write_File_Sync* write_file_sync;
    	Platform::Free_File_Memory* free_file_memory;

		i64 get_total_size() const { return permanent_size + transient_size; }
	};

	struct Screen_Buffer {
		i32 width, height;
		u32* pixels;
	};

	struct Sound_Sample {
		i16 left, right;
	};

	struct Sound_Buffer {
		i32 samples_per_second;
		i32 samples_to_write;
		Sound_Sample* samples;
	};

	struct Color {
		f32 red, green, blue;
		u32 to_hex() {
			return ((u32)hm::round(red   * 255.0f) << 16)
				 | ((u32)hm::round(green * 255.0f) << 8)
				 | ((u32)hm::round(blue  * 255.0f));
		}
	};

	extern "C" void update_and_render(const Input& input, Memory& memory, Screen_Buffer& screen_buffer);
	using Update_And_Render = decltype(update_and_render);

	// get_sound_samples должен быть быстрым, не больше 1ms
	extern "C" void get_sound_samples(Memory& memory, Sound_Buffer& sound_buffer);
	using Get_Sound_Samples = decltype(get_sound_samples);

	static void draw_rectangle(Screen_Buffer& screen_buffer, f32 min_x_f32, f32 max_x_f32, f32 min_y_f32, f32 max_y_f32, Color color);
	static void dev_render_mouse_test(const Input& input, Screen_Buffer& screen_buffer);
}