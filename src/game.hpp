#pragma once

#include "globals.hpp"

namespace Game {
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
		u32 to_hex() const {
			return ((u32)hm::round(red   * 255.0f) << 16)
				 | ((u32)hm::round(green * 255.0f) << 8)
				 | ((u32)hm::round(blue  * 255.0f));
		}
	};

	struct Position {
		// TODO: сделать сеттеры с автоматической нормализацией после введения векторов
		i32 scene_x, scene_y;
		f32 point_x, point_y;

		i32 get_tile_x(f32 tile_size) const { return hm::floor(point_x / tile_size); }
		i32 get_tile_y(f32 tile_size) const { return hm::floor(point_y / tile_size); }
		void normalize(f32 tile_size);
	};

	struct Scene {
		static const i32 WIDTH = 17;
		static const i32 HEIGHT = 9;
		static f32 get_width_pixels (f32 tile_size) { return Scene::WIDTH  * tile_size; };
		static f32 get_height_pixels(f32 tile_size) { return Scene::HEIGHT * tile_size; };

		const i32 (*tiles)[Scene::WIDTH];
	};

	struct World {
		static const i32 WIDTH = 2;
		static const i32 HEIGHT = 2;

		f32 tile_size;
		const Scene* scenes;
		
		Scene get_scene(const Position& player_pos) const { return scenes[player_pos.scene_y * World::WIDTH + player_pos.scene_x]; };
	};

	struct Game_State {
		// TODO: const для TILES и SCENES? компилятор удаляет конструктор по умолчанию
		i32 TILES_00[Scene::HEIGHT][Scene::WIDTH];
		i32 TILES_01[Scene::HEIGHT][Scene::WIDTH];
		i32 TILES_10[Scene::HEIGHT][Scene::WIDTH];
		i32 TILES_11[Scene::HEIGHT][Scene::WIDTH];
		Scene SCENES[World::HEIGHT][World::WIDTH];
		World world;
		Position player_pos;
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

		Game_State& get_game_state();
		i64 get_total_size() const { return permanent_size + transient_size; }
	};

	extern "C" void update_and_render(const Input& input, Memory& memory, Screen_Buffer& screen_buffer);
	using Update_And_Render = decltype(update_and_render);
	// get_sound_samples должен быть быстрым, не больше 1ms
	extern "C" void get_sound_samples(Memory& memory, Sound_Buffer& sound_buffer);
	using Get_Sound_Samples = decltype(get_sound_samples);

	static bool check_empty_tile(const World& world, const Position& position);
	static void draw_rectangle(Screen_Buffer& screen_buffer, const Color& color, f32 min_x_f32, f32 max_x_f32, f32 min_y_f32, f32 max_y_f32);
	static void dev_render_mouse_test(const Input& input, Screen_Buffer& screen_buffer);
}