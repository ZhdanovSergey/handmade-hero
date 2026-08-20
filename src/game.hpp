#pragma once

#include "globals.hpp"
#include "random.hpp"
#include "tiles.hpp"

namespace Game {
	static constexpr v2<i32> SCENE_DIM_TILES = { 17, 9 };
	static constexpr i32 SCENES_PER_SCREEN = 1;

	struct Controller_Button {
		i32 transitions_count;
		bool is_pressed;
	};

	struct Controller {
		Controller_Button start_btn, back_btn;
		Controller_Button left_shoulder, right_shoulder;
		Controller_Button move_up, move_down, move_left, move_right;
		Controller_Button action_up, action_down, action_left, action_right;
		v2<f32> start;
		v2<f32> end;
		v2<f32> average;
		v2<f32> min;
		v2<f32> max;
		bool is_connected, is_analog;
	};

	struct Mouse {
		Controller_Button left_button;
		Controller_Button right_button;
		v2<i32> coord;
	};

	struct Input {
		Array<Controller, 2> controllers;
		Mouse mouse;
		f32 frame_dt;
	};

	struct Sound_Sample {
		i16 left, right;
	};

	struct Sound {
		slice<Sound_Sample> samples;
		i32 samples_per_second;
	};

	
    struct Thread {};

    static slice<u8> read_entire_file(Thread& thread, cstr file_name);
    using Read_Entire_File = decltype(read_entire_file);

    static void write_entire_file(Thread& thread, cstr file_name, slice<u8> file);
    using Write_Entire_File = decltype(write_entire_file);

    static void free_file_memory(Thread& thread, void*& memory);
    using Free_File_Memory = decltype(free_file_memory);

	struct Memory {
		bool is_initialized;
		slice<u8> permanent;
		slice<u8> transient;
    	Read_Entire_File* read_entire_file;
    	Write_Entire_File* write_entire_file;
    	Free_File_Memory* free_file_memory;
	};

	struct Color {
		f32 red, green, blue;
	};

	struct World {
		Arena arena;
		Tiles::Map tile_map;
	};

	struct Hero_Side_Bitmap {
		slice2<u32> head, cape, torso;
		v2<i32> align;
	};

	namespace Hero_Direction {
		enum Type {
			Front,
			Back,
			Left,
			Right,
			Count
		};
	}

	struct Game_State {
		World world;
		slice2<u32> background_bitmap;
		Array<Hero_Side_Bitmap, Hero_Direction::Count> hero_bitmaps;
		Hero_Direction::Type hero_dir;
		Tiles::Position camera_pos;
		Tiles::Position hero_pos;
		v2<f32> d_hero_pos;
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

	extern "C" void update_and_render(Thread& thread, Input& input, Memory& memory, slice2<u32> screen);
	using Update_And_Render = decltype(update_and_render);
	// get_sound_samples должен быть быстрым, не больше 1ms
	extern "C" void get_sound_samples(Thread& thread, Memory& memory, Sound& sound);
	using Get_Sound_Samples = decltype(get_sound_samples);

	static slice2<u32> load_bmp(Thread& thread, Read_Entire_File* read_entire_file, cstr file_name);
	static void draw_pixels(slice2<u32> dst, slice2<u32> src, v2<f32> min_f32, v2<i32> align = {0, 0});
	static void draw_rectangle(slice2<u32> dst, Color color, v2<f32> min_f32, v2<f32> max_f32);
	static f32 get_pixels_per_unit(slice2<u32> screen);
	static u32 get_hex_color(Color color);
	
	static void init_memory(Thread& thread, Memory& memory);
	static Game_State& get_game_state(Memory& memory);
}