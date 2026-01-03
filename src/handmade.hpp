#pragma once

#include "globals.hpp"

namespace Game {
	struct Game_State {
		f32 t_sine;
		u32 tone_hz;
		u32 green_offset, blue_offset;
	};

	struct Button_State {
		bool is_pressed;
		u32 transitions_count;
	};

	struct Controller {
		bool is_analog;

		f32 start_x, start_y;
		f32 end_x, end_y;
		f32 min_x, min_y;
		f32 max_x, max_y;

		Button_State start, back;
		Button_State left_shoulder, right_shoulder;
		Button_State move_up, move_down, move_left, move_right;
		Button_State action_up, action_down, action_left, action_right;
	};

	struct Input {
		Controller controllers[2];

		void reset_transitions_count() {
			for (auto& controller : controllers) {
				controller.start.transitions_count 			= 0;
				controller.back.transitions_count  			= 0;
				controller.left_shoulder.transitions_count  = 0;
				controller.right_shoulder.transitions_count = 0;

				controller.move_up.transitions_count    	= 0;
				controller.move_down.transitions_count  	= 0;
				controller.move_left.transitions_count  	= 0;
				controller.move_right.transitions_count 	= 0;

				controller.action_up.transitions_count		= 0;
				controller.action_down.transitions_count	= 0;
				controller.action_left.transitions_count	= 0;
				controller.action_right.transitions_count	= 0;
			}
		}
	};

	struct Memory {
		bool is_initialized;
		uptr permanent_storage_size;
		uptr transient_storage_size;
		u8* permanent_storage;
		u8* transient_storage;

    	Platform::Read_File_Sync* read_file_sync;
    	Platform::Write_File_Sync* write_file_sync;
    	Platform::Free_File_Memory* free_file_memory;
	};

	struct Screen_Buffer {
		u32 width, height;
		u32* memory;
	};

	struct Sound_Sample {
		s16 left, right;
	};

	struct Sound_Buffer {
		u32 samples_per_second;
		u32 samples_to_write;
		Sound_Sample* samples;
	};

	typedef 	void Update_And_Render		(const Input& input, Memory& memory, Screen_Buffer& screen_buffer);
	extern "C"	void update_and_render		(const Input& input, Memory& memory, Screen_Buffer& screen_buffer);
				void update_and_render_stub	(const Input& input, Memory& memory, Screen_Buffer& screen_buffer){};

	// get_sound_samples должен быть быстрым, не больше 1ms
	typedef		void Get_Sound_Samples		(Memory& memory, Sound_Buffer& sound_buffer);
	extern "C"	void get_sound_samples		(Memory& memory, Sound_Buffer& sound_buffer);
				void get_sound_samples_stub	(Memory& memory, Sound_Buffer& sound_buffer){};

	static void render_gradient(const Game_State* game_state, Screen_Buffer& screen_buffer);
}