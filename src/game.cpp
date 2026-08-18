#include "game.hpp"
#include "intrinsics.hpp"
#include "tiles.cpp"

namespace Game {
	extern "C" void get_sound_samples(Thread& thread, Memory& memory, Sound& sound) {
		auto& game_state  = get_game_state(memory);
		auto& sound_t_sin = game_state.sound_t_sin;

		// f32 volume = 5000.0f;
		f32 volume = 0;
		i32 frequency = 261;
		f32 samples_per_wave_period = cast<f32>(sound.samples_per_second / frequency);

		for (auto& sample : sound.samples) {
			i16 value = cast<i16>(std::sinf(sound_t_sin) * volume);
			sample.left  = value;
			sample.right = value;
			sound_t_sin += TWO_PI / samples_per_wave_period;
			if (sound_t_sin >= TWO_PI) sound_t_sin -= TWO_PI;
		}
	}

	extern "C" void update_and_render(Thread& thread, Input& input, Memory& memory, slice2<u32> screen) {
		assert(input.frame_dt > 0);
		if (!memory.is_initialized) {
			init_memory(thread, memory);
		}

		auto& game_state = get_game_state(memory);
		auto& hero_pos = game_state.hero_position;
		auto& camera_pos = game_state.camera_position;
		auto& tile_map   = game_state.world.tile_map;

		auto new_hero_pos = hero_pos;
		for (auto& controller : input.controllers) {
			f32 player_speed = controller.action_down.is_pressed ? 20.0f : 5.0f;
			vec2<f32> player_dpos = {};

			if (controller.move_left.is_pressed) {
				game_state.hero_direction = Hero_Direction::Left;
				player_dpos.x -= player_speed;
			}
			if (controller.move_right.is_pressed) {
				game_state.hero_direction = Hero_Direction::Right;
				player_dpos.x += player_speed;
			}
			if (controller.move_up.is_pressed) {
				game_state.hero_direction = Hero_Direction::Back;
				player_dpos.y += player_speed;
			}
			if (controller.move_down.is_pressed) {
				game_state.hero_direction = Hero_Direction::Front;
				player_dpos.y -= player_speed;
			}

			if (player_dpos.x && player_dpos.y) {
				player_dpos /= SQRT_2;
			}
			
			new_hero_pos.tile_rel_add(player_dpos * input.frame_dt);
		}

		f32 player_width  = 1.0f;

		auto new_hero_pos_left = new_hero_pos;
		new_hero_pos_left.tile_rel_add({ - player_width / 2, 0 });

		auto new_hero_pos_right = new_hero_pos;
		new_hero_pos_left.tile_rel_add({   player_width / 2, 0 });

		if (Tiles::check_walkable_tile(tile_map, new_hero_pos_left) &&
		    Tiles::check_walkable_tile(tile_map, new_hero_pos)      &&
		    Tiles::check_walkable_tile(tile_map, new_hero_pos_right)) {
			if (!Tiles::check_same_tile(hero_pos, new_hero_pos)) {
				auto new_tile = Tiles::get_tile(tile_map, new_hero_pos.abs_xy.x, new_hero_pos.abs_xy.y, new_hero_pos.abs_z);
				if (new_tile == Tiles::Tile::Stairs_Up)   new_hero_pos.abs_z += 1;
				if (new_tile == Tiles::Tile::Stairs_Down) new_hero_pos.abs_z -= 1;
			}
			hero_pos = new_hero_pos;
			camera_pos.abs_z = hero_pos.abs_z;

			for (i32 axis = 0; axis < 2; ++axis) {
				i32 abs_diff = hero_pos.abs_xy(axis) - camera_pos.abs_xy(axis);
				if (hm::abs(abs_diff) > SCENE_DIM_TILES(axis) / 2) {
					camera_pos.abs_xy(axis) += SCENE_DIM_TILES(axis) * hm::sign<i32>(abs_diff);
				}
			}
		}

		draw_rectangle(
			screen, Color{ 1.0f, 0.0f, 1.0f },
			vec2<f32>{0.0f, 0.0f},
			cast<vec2<f32>>(SCENES_PER_SCREEN * SCENE_DIM_TILES) * Tiles::TILE_DIM
		);		
		draw_pixels(screen, game_state.background_bitmap, vec2<f32>{0, 0});

		vec2<i32> half_screen_tiles = SCENES_PER_SCREEN * SCENE_DIM_TILES / 2;
		for (    i32 y = camera_pos.abs_xy.y - half_screen_tiles.y - 1; y <= camera_pos.abs_xy.y + half_screen_tiles.y + 1; ++y) {
			for (i32 x = camera_pos.abs_xy.x - half_screen_tiles.x - 1; x <= camera_pos.abs_xy.x + half_screen_tiles.x + 1; ++x) {
				vec2<i32> xy = {x, y};

				auto tile = Tiles::get_tile(tile_map, x, y, camera_pos.abs_z);
				Color color = {};
				switch (tile) {
					case Tiles::Tile::Not_Initialized: color = { 1.0f, 0.0f, 0.0f };    break;
					case Tiles::Tile::Floor:           color = { 0.5f, 0.5f, 0.5f };    break;
					case Tiles::Tile::Wall:            color = { 1.0f, 1.0f, 1.0f };    break;
					case Tiles::Tile::Stairs_Up:       color = { 0.25f, 0.25f, 0.25f }; break;
					case Tiles::Tile::Stairs_Down:     color = { 0.25f, 0.25f, 0.25f }; break;
				}

				if (xy == hero_pos.abs_xy) {
					color = { 0.0f, 0.0f, 0.0f };
				}

				if (xy == hero_pos.abs_xy || (tile != Tiles::Tile::Not_Initialized && tile != Tiles::Tile::Floor)) {
					vec2<f32> rect_min = cast<vec2<f32>>(xy - camera_pos.abs_xy) * Tiles::TILE_DIM - camera_pos.tile_rel;
					rect_min.y = - rect_min.y;
					rect_min += cast<vec2<f32>>(half_screen_tiles) * Tiles::TILE_DIM;

					vec2<f32> rect_max = rect_min + vec2<f32>{Tiles::TILE_DIM, Tiles::TILE_DIM};
					draw_rectangle(screen, color, rect_min, rect_max);
				}

			}
		}

		vec2<f32> hero_camera_diff = Tiles::subtract_positions(hero_pos, camera_pos);
		vec2<f32> player_ground = hero_camera_diff;
		player_ground.y = Tiles::TILE_DIM - player_ground.y;
		player_ground += cast<vec2<f32>>(half_screen_tiles) * Tiles::TILE_DIM;

		auto hero_bitmap = game_state.hero_bitmaps(game_state.hero_direction);
		draw_pixels(screen, hero_bitmap.torso, player_ground, hero_bitmap.align);
		draw_pixels(screen, hero_bitmap.cape,  player_ground, hero_bitmap.align);
		draw_pixels(screen, hero_bitmap.head,  player_ground, hero_bitmap.align);
	};

	static void draw_rectangle(slice2<u32> dst, Color color, vec2<f32> min_f32, vec2<f32> max_f32) {
		f32 pixels_per_unit = get_pixels_per_unit(dst);

		vec2<i32> min = hm::round<vec2<i32>>(min_f32 * pixels_per_unit);
		vec2<i32> max = hm::round<vec2<i32>>(max_f32 * pixels_per_unit);

		min = hm::max(min, vec2<i32>{0, 0});
		max = hm::min(max, dst.count);

		u32 hex_color = get_hex_color(color);
		for (i32 y = min.y; y < max.y; ++y) {
			for (i32 x = min.x; x < max.x; ++x) {
				dst(x, y) = hex_color;
			}
		}
	};

	static void draw_pixels(slice2<u32> dst, slice2<u32> src, vec2<f32> min_f32, vec2<i32> align) {
		f32 pixels_per_unit = get_pixels_per_unit(dst);

		vec2<i32> src_min = hm::round<vec2<i32>>(min_f32 * pixels_per_unit - cast<vec2<f32>>(align));
		vec2<i32> src_max = src_min + src.count;

		vec2<i32> dst_min = hm::max(src_min, vec2<i32>{0, 0});
		vec2<i32> dst_max = hm::min(src_max, dst.count);

		for (    i32 dst_y = dst_min.y; dst_y < dst_max.y; ++dst_y) {
			for (i32 dst_x = dst_min.x; dst_x < dst_max.x; ++dst_x) {
				u32 src_pixel = src(dst_x - src_min.x, src_max.y - 1 - dst_y); // bmp загружается bottom-up
				u32 dst_pixel = dst(dst_x, dst_y);

				// linear alpha blend
				f32 alpha     = cast<f32>((src_pixel >> 24) & UINT8_MAX) / UINT8_MAX;
				f32 src_red   = cast<f32>((src_pixel >> 16) & UINT8_MAX);
				f32 src_green = cast<f32>((src_pixel >> 8)  & UINT8_MAX);
				f32 src_blue  = cast<f32>((src_pixel >> 0)  & UINT8_MAX);

				f32 dst_red   = cast<f32>((dst_pixel >> 16) & UINT8_MAX);
				f32 dst_green = cast<f32>((dst_pixel >> 8)  & UINT8_MAX);
				f32 dst_blue  = cast<f32>((dst_pixel >> 0)  & UINT8_MAX);

				// LATER: vec3?
				f32 result_red   = (1 - alpha) * dst_red   + alpha * src_red;
				f32 result_green = (1 - alpha) * dst_green + alpha * src_green;
				f32 result_blue  = (1 - alpha) * dst_blue  + alpha * src_blue;

				assert(result_red   >= 0 && result_red   <= UINT8_MAX);
				assert(result_green >= 0 && result_green <= UINT8_MAX);
				assert(result_blue  >= 0 && result_blue  <= UINT8_MAX);
				
				dst(dst_x, dst_y) = (hm::round_positive<u32>(result_red)   << 16) |
							        (hm::round_positive<u32>(result_green) << 8)  |
							        (hm::round_positive<u32>(result_blue)  << 0);
			}
		}
	}

	static slice2<u32> load_bmp(Thread& thread, Read_Entire_File* read_entire_file, cstr file_name) {
		slice<u8> read_result = read_entire_file(thread, file_name);
		if (!read_result.ptr) return {};

		auto& header = cast<Bmp_Header&>(*read_result.ptr);
		assert(header.compression == 3);

		slice2<u32> pixels = {};
		pixels.count = { header.width, header.height };
		pixels.ptr = cast<u32*>(read_result.ptr + header.bitmap_offset);
		assert(pixels.get_size() == read_result.get_size() - header.bitmap_offset);

		u32 alpha_mask = ~(header.red_mask | header.green_mask | header.blue_mask);
		result<i32> alpha_shift = hm::find_set_bit_right(alpha_mask);
		result<i32> red_shift   = hm::find_set_bit_right(header.red_mask);
		result<i32> green_shift = hm::find_set_bit_right(header.green_mask);
		result<i32> blue_shift  = hm::find_set_bit_right(header.blue_mask);
		assert(alpha_shift.ok && red_shift.ok && green_shift.ok && blue_shift.ok);

		for (u32& pixel : pixels) {
			pixel = ((pixel & alpha_mask)        >> alpha_shift.value << 24) |
			        ((pixel & header.red_mask)   >> red_shift.value   << 16) |
					((pixel & header.green_mask) >> green_shift.value << 8)  |
					((pixel & header.blue_mask)  >> blue_shift.value  << 0);
		}

		return pixels; // bottom-up
	}

	static void init_memory(Thread& thread, Memory& memory) {
		auto& game_state  = get_game_state(memory);
		auto& hero_pos    = game_state.hero_position;
		auto& camera_pos  = game_state.camera_position;
		auto& tile_map    = game_state.world.tile_map;
		auto& tile_chunks = game_state.world.tile_map.chunks;
		auto& world_arena = game_state.world.arena;

		world_arena.ptr  = memory.permanent.ptr + size_of(Game_State);
		world_arena.size = memory.permanent.get_size() - size_of(Game_State);

		tile_chunks.count_x = Tiles::WORLD_X_CHUNKS;
		tile_chunks.count_y = Tiles::WORLD_Y_CHUNKS;
		tile_chunks.count_z = Tiles::WORLD_Z_CHUNKS;
		tile_chunks.ptr = world_arena.push<Tiles::Chunk>(tile_chunks.get_size());

		i32 abs_tile_z = 0;
		vec2<i32> scene = {};
		bool is_door_left = false, is_door_right  = false;
		bool is_door_top  = false, is_door_bottom = false;
		bool is_stairs_up = false, is_stairs_down = false;

		i32 scenes_count = 100;
		for (i32 scene_index = 0; scene_index < scenes_count; ++scene_index) {
			i32 random_choice_3 = is_stairs_up || is_stairs_down
				? RANDOM_NUMBERS_TABLE(scene_index) % 2
				: RANDOM_NUMBERS_TABLE(scene_index) % 3;

			switch (random_choice_3) {
				case 0: is_door_right = true; break;
				case 1: is_door_top   = true; break;
				case 2: {
					if (abs_tile_z == 0) is_stairs_up   = true;
					else                 is_stairs_down = true;
				} break;
			}

			if constexpr (SLOW_MODE) {
				i32 fact_doors_count = is_door_left + is_door_right + is_door_top + is_door_bottom + is_stairs_up + is_stairs_down;
				i32 correct_doors_count = scene_index == 0 ? 1 : 2;
				assert(fact_doors_count == correct_doors_count);
			}

			for (    i32 tile_y = 0; tile_y < SCENE_DIM_TILES.y; ++tile_y) {
				for (i32 tile_x = 0; tile_x < SCENE_DIM_TILES.x; ++tile_x) {
					i32 abs_tile_x = scene.x * SCENE_DIM_TILES.x + tile_x;
					i32 abs_tile_y = scene.y * SCENE_DIM_TILES.y + tile_y;

					auto tile_value = Tiles::Tile::Floor;
					if (tile_x == 0 || tile_x == SCENE_DIM_TILES.x - 1 ||
					    tile_y == 0 || tile_y == SCENE_DIM_TILES.y - 1) {
						tile_value = Tiles::Tile::Wall;
					}

					if ((is_door_left   && tile_x == 0                     && tile_y == SCENE_DIM_TILES.y / 2) ||
				        (is_door_right  && tile_x == SCENE_DIM_TILES.x - 1 && tile_y == SCENE_DIM_TILES.y / 2) ||
					    (is_door_top    && tile_x == SCENE_DIM_TILES.x / 2 && tile_y == SCENE_DIM_TILES.y - 1) ||
					    (is_door_bottom && tile_x == SCENE_DIM_TILES.x / 2 && tile_y == 0                     )) {
						tile_value = Tiles::Tile::Floor;
					}

					if (tile_x == SCENE_DIM_TILES.x / 2 && tile_y == SCENE_DIM_TILES.y / 2) {
						if (is_stairs_up)   tile_value = Tiles::Tile::Stairs_Up;
						if (is_stairs_down) tile_value = Tiles::Tile::Stairs_Down;
					}
					
					Tiles::set_tile(world_arena, tile_map, abs_tile_x, abs_tile_y, abs_tile_z, tile_value);
				}
			}

			if (random_choice_3 == 2) {
				abs_tile_z     = !abs_tile_z;
				is_stairs_up   = !is_stairs_up;
				is_stairs_down = !is_stairs_down;
			} else {
				is_stairs_up   = false;
				is_stairs_down = false;
				if (random_choice_3 == 0) scene.x += 1;
				if (random_choice_3 == 1) scene.y += 1;
			}

			is_door_left = is_door_right;
			is_door_bottom = is_door_top;
			is_door_right = false;
			is_door_top = false;
		}

		hero_pos.abs_xy = { 1, 1 };
		hero_pos.tile_rel_add({ Tiles::TILE_DIM / 2, Tiles::TILE_DIM / 2 });
		assert(Tiles::check_walkable_tile(tile_map, hero_pos));

		camera_pos.abs_xy = SCENE_DIM_TILES / 2;
		camera_pos.abs_z = hero_pos.abs_z;
		camera_pos.tile_rel.x = Tiles::TILE_DIM / 2;

		game_state.background_bitmap = load_bmp(thread, memory.read_entire_file, "test/test_background.bmp");

		game_state.hero_bitmaps(Hero_Direction::Front).head  = load_bmp(thread, memory.read_entire_file, "test/test_hero_front_head.bmp");
		game_state.hero_bitmaps(Hero_Direction::Front).cape  = load_bmp(thread, memory.read_entire_file, "test/test_hero_front_cape.bmp");
		game_state.hero_bitmaps(Hero_Direction::Front).torso = load_bmp(thread, memory.read_entire_file, "test/test_hero_front_torso.bmp");
		game_state.hero_bitmaps(Hero_Direction::Front).align = {72, 182};

		game_state.hero_bitmaps(Hero_Direction::Back).head   = load_bmp(thread, memory.read_entire_file, "test/test_hero_back_head.bmp");
		game_state.hero_bitmaps(Hero_Direction::Back).cape   = load_bmp(thread, memory.read_entire_file, "test/test_hero_back_cape.bmp");
		game_state.hero_bitmaps(Hero_Direction::Back).torso  = load_bmp(thread, memory.read_entire_file, "test/test_hero_back_torso.bmp");
		game_state.hero_bitmaps(Hero_Direction::Back).align  = {72, 182};

		game_state.hero_bitmaps(Hero_Direction::Left).head   = load_bmp(thread, memory.read_entire_file, "test/test_hero_left_head.bmp");
		game_state.hero_bitmaps(Hero_Direction::Left).cape   = load_bmp(thread, memory.read_entire_file, "test/test_hero_left_cape.bmp");
		game_state.hero_bitmaps(Hero_Direction::Left).torso  = load_bmp(thread, memory.read_entire_file, "test/test_hero_left_torso.bmp");
		game_state.hero_bitmaps(Hero_Direction::Left).align  = {72, 182};

		game_state.hero_bitmaps(Hero_Direction::Right).head  = load_bmp(thread, memory.read_entire_file, "test/test_hero_right_head.bmp");
		game_state.hero_bitmaps(Hero_Direction::Right).cape  = load_bmp(thread, memory.read_entire_file, "test/test_hero_right_cape.bmp");
		game_state.hero_bitmaps(Hero_Direction::Right).torso = load_bmp(thread, memory.read_entire_file, "test/test_hero_right_torso.bmp");
		game_state.hero_bitmaps(Hero_Direction::Right).align = {72, 182};
		
		memory.is_initialized = true;
	}

	static u32 get_hex_color(Color color) {
		assert(color.red   >= 0 && color.red   <= 1);
		assert(color.green >= 0 && color.green <= 1);
		assert(color.blue  >= 0 && color.blue  <= 1);

		return (hm::round_positive<u32>(color.red   * UINT8_MAX) << 16) |
			   (hm::round_positive<u32>(color.green * UINT8_MAX) << 8)  |
			   (hm::round_positive<u32>(color.blue  * UINT8_MAX));
	}

	static f32 get_pixels_per_unit(slice2<u32> screen) {
		return cast<f32>(screen.count.y) / (SCENES_PER_SCREEN * SCENE_DIM_TILES.y * Tiles::TILE_DIM);
	}

	static Game_State& get_game_state(Memory& memory) {
		return cast<Game_State&>(*memory.permanent.ptr);
	}
}