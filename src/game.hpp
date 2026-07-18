#pragma once

#include "globals.hpp"
#include "tiles.hpp"

namespace Game {
	static const i32 SCENE_WIDTH_TILES = 17;
	static const i32 SCENE_HEIGHT_TILES = 9;
	static const i32 SCENES_PER_SCREEN = 1;

	struct Controller_Button {
		i32 transitions_count;
		bool is_pressed;
	};

	struct Controller {
		Controller_Button start, back;
		Controller_Button left_shoulder, right_shoulder;
		Controller_Button move_up, move_down, move_left, move_right;
		Controller_Button action_up, action_down, action_left, action_right;
		f32 start_x, start_y;
		f32 end_x, end_y;
		f32 average_x, average_y;
		f32 min_x, min_y;
		f32 max_x, max_y;
		bool is_connected, is_analog;
	};

	struct Mouse {
		Controller_Button left_button;
		Controller_Button right_button;
		i32 x, y;
	};

	struct Input {
		Controller controllers[2];
		Mouse mouse;
		f32 frame_dt;
	};

	struct Sound_Sample {
		i16 left, right;
	};

	struct Sound {
		slice1<Sound_Sample> samples;
		i32 samples_per_second;
	};

	struct Memory {
		bool is_initialized;
		slice1<u8> permanent;
		slice1<u8> transient;
    	Platform::Read_Entire_File* read_entire_file;
    	Platform::Write_Entire_File* write_entire_file;
    	Platform::Free_File_Memory* free_file_memory;
	};

	struct Color {
		f32 red, green, blue;
	};

	struct World {
		Tiles::Map tile_map;
	};

	struct Game_State {
		World world;
		Arena world_arena;
		Tiles::Position player_pos;
		slice2<u32> test_background;
		slice2<u32> test_hero_front_head;
		slice2<u32> test_hero_front_cape;
		slice2<u32> test_hero_front_torso;
		f32 pixels_per_unit;
		f32 sound_t_sin;
	};

	#pragma pack(push, 1)
	struct Bmp_Header {
		// WINBMPFILEHEADER
		u16 file_type;        /* File type, always 4D42h ("BM") */
		u32 file_size;        /* Size of the file in bytes */
		u16 reserved1;        /* Always 0 */
		u16 reserved2;        /* Always 0 */
		u32 bitmap_offset;    /* Starting position of image data in bytes */

		// WIN3XBITMAPHEADER
		u32 size;             /* Size of this header in bytes */
		i32 width;            /* Image width in pixels */
		i32 height;           /* Image height in pixels */
		u16 planes;           /* Number of color planes */
		u16 bits_per_pixel;   /* Number of bits per pixel */
		u32 compression;      /* Compression methods used */
		u32 size_of_bitmap;   /* Size of bitmap in bytes */
		i32 horz_resolution;  /* Horizontal resolution in pixels per meter */
		i32 vert_resolution;  /* Vertical resolution in pixels per meter */
		u32 colors_used;      /* Number of colors in the image */
		u32 colors_important; /* Minimum number of important colors */

		// WINNTBITFIELDSMASKS
		u32 red_mask;         /* Mask identifying bits of red component */
		u32 green_mask;       /* Mask identifying bits of green component */
		u32 blue_mask;        /* Mask identifying bits of blue component */
	};
	#pragma pack(pop)

	extern "C" void update_and_render(const Platform::Thread_Context& thread, const Input& input, Memory& memory, slice2<u32> screen);
	using Update_And_Render = decltype(update_and_render);
	// get_sound_samples должен быть быстрым, не больше 1ms
	extern "C" void get_sound_samples(const Platform::Thread_Context& thread, Memory& memory, Sound& sound_buffer);
	using Get_Sound_Samples = decltype(get_sound_samples);

	static slice2<u32> load_bmp(const Platform::Thread_Context& thread, Platform::Read_Entire_File* read_entire_file, const char* file_name);
	static void draw_pixels(slice2<u32> screen, slice2<const u32> pixels, f32 min_x_f32, f32 min_y_f32);
	static void draw_rectangle(slice2<u32> screen, const Color& color, f32 min_x_f32, f32 min_y_f32, f32 max_x_f32, f32 max_y_f32);
	static f32 get_pixels_per_unit(slice2<const u32> screen);
	static u32 get_hex_color(const Color& color);
	
	static void init_memory(Memory& memory);
	static Game_State& get_game_state(Memory& memory);
}