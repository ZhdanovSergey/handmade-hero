#pragma once

#include "globals.hpp"

namespace Tiles {
	// временные значения для проверки перехода через границы чанков
	static const i32 WORLD_SIZE_CHUNKS = 128;
	static const i32 CHUNK_POSITION_SHIFT = 4;

	// TODO: отказаться от таких констант после написания многомерного slice
	// static const i32 WORLD_SIZE_CHUNKS = 4;
	// static const i32 CHUNK_POSITION_SHIFT = 8;

	static const i32 CHUNK_SIZE_TILES = 1 << CHUNK_POSITION_SHIFT;
	static const u32 CHUNK_POSITION_MASK = CHUNK_SIZE_TILES - 1;
	static const f32 TILE_SIZE = 1.4f;

	using tile = u32;

	struct Chunk {
		slice<tile> tiles;
	};

    struct Map {
		slice<Chunk> chunks;
    };

	// TODO: переименовать в Position
	// TODO: сделать сеттеры с автоматической нормализацией после введения векторов + конструктор с нормализацией
	struct Map_Position {
		// TODO: поменять названия на tile_abs_x/tile_rel_x
		u32 world_x, world_y; // верхние биты это координаты чанка в мире, нижние биты это координаты ячейки внутри чанка
		f32 tile_x, tile_y;
	};

	// TODO: разбить эту структуру на 2
	struct Chunk_Position {
		// TODO: поменять названия на chunk_lookup_x/chunk_rel_x
		u32 lookup_x, lookup_y;
		i32 chunk_x, chunk_y;
	};

	// TODO: заменить структуры координат на числа x/y в функциях где возможно
	static bool check_empty_tile(Map& map, const Map_Position& map_pos);
	static tile get_tile(Map& map, u32 abs_x, u32 abs_y);
	static Chunk* get_chunk(Map& map, const Chunk_Position& map_pos);
	static Chunk_Position get_chunk_position(u32 tile_abs_x, u32 tile_abs_y);
	static void normalize_position(Map_Position& position);
	static void set_tile(Map& map, u32 abs_x, u32 abs_y, tile value);
}