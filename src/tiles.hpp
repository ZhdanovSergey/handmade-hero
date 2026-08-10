#pragma once

#include "globals.hpp"

namespace Tiles {
	static constexpr i32 WORLD_X_CHUNKS = 128;
	static constexpr i32 WORLD_Y_CHUNKS = 128;
	static constexpr i32 WORLD_Z_CHUNKS = 2;

	static constexpr i32 CHUNK_LOOKUP_KEY_SHIFT = 4;
	static constexpr i32 CHUNK_DIM_TILES = 1 << CHUNK_LOOKUP_KEY_SHIFT;
	static constexpr i32 CHUNK_REL_POSITION_MASK = CHUNK_DIM_TILES - 1;
	static constexpr f32 TILE_DIM = 1.4f;

	enum struct Tile {
		Not_Initialized,
		Floor,
		Wall,
		Stairs_Up,
		Stairs_Down
	};

	struct Chunk {
		static_slice<Tile, CHUNK_DIM_TILES, CHUNK_DIM_TILES> tiles;
	};

    struct Map {
		slice3<Chunk> chunks;
    };

	// LATER: сделать сеттеры и конструктор с нормализацией после введения векторов
	struct Position {
		i32 abs_x, abs_y; // нижние CHUNK_LOOKUP_KEY_SHIFT бит это координаты ячейки внутри чанка, верхние биты это координаты чанка в мире
		i32 abs_z;        // просто координата чанка в мире
		f32 tile_rel_x, tile_rel_y;
	};

	struct Positions_Diff {
		f32 dx, dy;
	};

	struct Chunk_Lookup_Key {
		i32 x, y, z;
	};

	struct Chunk_Rel_Position {
		i32 x, y;
	};

	static bool check_same_tile(Position& pos1, Position& pos2);
	static bool check_walkable_tile(Map& map, Position& pos);

	static Tile get_tile(Map& map, i32 abs_x, i32 abs_y, i32 abs_z);
	static void set_tile(Arena& world_arena, Map& map, i32 abs_x, i32 abs_y, i32 abs_z, Tile value);

	static Chunk* get_chunk(Map& map, i32 abs_x, i32 abs_y, i32 abs_z);
	static Chunk_Lookup_Key get_chunk_lookup_key(i32 abs_x, i32 abs_y, i32 abs_z);
	static Chunk_Rel_Position get_chunk_rel_position(i32 abs_x, i32 abs_y);
	
	static void normalize_position(Position& pos);
	static Positions_Diff subtract_positions(Position& a, Position& b);
}