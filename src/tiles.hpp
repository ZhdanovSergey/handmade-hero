#pragma once

#include "globals.hpp"

namespace Tiles {
	// временные значения для проверки перехода через границы чанков
	// static const i32 WORLD_SIZE_CHUNKS = 4;
	// static const i32 CHUNK_LOOKUP_KEY_SHIFT = 8;
	static const i32 WORLD_SIZE_CHUNKS = 128;
	static const i32 CHUNK_LOOKUP_KEY_SHIFT = 4;

	static const i32 CHUNK_SIZE_TILES = 1 << CHUNK_LOOKUP_KEY_SHIFT;
	static const u32 CHUNK_REL_TILE_POSITION_MASK = CHUNK_SIZE_TILES - 1;
	static const f32 TILE_SIZE = 1.4f;

	using Tile = u32;

	struct Chunk {
		slice2<Tile> tiles;
	};

    struct Map {
		slice2<Chunk> chunks;
    };

	// LATER: сделать сеттеры с нормализацией после введения векторов (без конструктора)
	struct Position {
		u32 abs_x, abs_y; // нижние CHUNK_LOOKUP_KEY_SHIFT бит это координаты ячейки внутри чанка, верхние биты это координаты чанка в мире
		f32 tile_rel_x, tile_rel_y;
	};

	struct Chunk_Lookup_Key {
		i32 x, y;
	};

	struct Chunk_Rel_Position {
		i32 x, y;
	};

	// LATER: появляются первые признаки const-poisoning, подумать над отказом от const
	static bool check_empty_tile(Map& map, u32 abs_x, u32 abs_y);
	static Tile get_tile(Map& map, u32 abs_x, u32 abs_y);
	static void set_tile(Map& map, u32 abs_x, u32 abs_y, Tile value);
	static Chunk* get_chunk(Map& map, u32 abs_x, u32 abs_y);
	static Chunk_Lookup_Key get_chunk_lookup_key(u32 abs_x, u32 abs_y);
	static Chunk_Rel_Position get_chunk_rel_position(u32 abs_x, u32 abs_y);
	static void normalize_position(Position& position);
}