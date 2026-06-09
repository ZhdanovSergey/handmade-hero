#include "tiles.hpp"

namespace Tiles {
	// TODO: учесть возможность смещения больше чем на 1 клетку
	static void normalize_position(Map_Position& map_pos) {
		if (map_pos.tile_x < 0) {
			map_pos.world_x -= 1;
			map_pos.tile_x += TILE_SIZE;
		} else if (map_pos.tile_x >= TILE_SIZE) {
			map_pos.world_x += 1;
			map_pos.tile_x -= TILE_SIZE;
		}

		if (map_pos.tile_y < 0) {
			map_pos.world_y -= 1;
			map_pos.tile_y += TILE_SIZE;
		} else if (map_pos.tile_y >= TILE_SIZE) {
			map_pos.world_y += 1;
			map_pos.tile_y -= TILE_SIZE;
		}

		// TODO: при получении дробной части через деление можно получить ненормализованное значение из-за округления
		// можно на время сделать <= TILE_SIZE
		assert(map_pos.tile_x >= 0 && map_pos.tile_x < TILE_SIZE);
		assert(map_pos.tile_y >= 0 && map_pos.tile_y < TILE_SIZE);
	}

	// TODO: сделать 2 отдельные функции/структуры
	static Chunk_Position get_chunk_position(u32 tile_abs_x, u32 tile_abs_y) {
		Chunk_Position result = {};
		result.lookup_x = tile_abs_x >> CHUNK_POSITION_SHIFT;
		result.lookup_y = tile_abs_y >> CHUNK_POSITION_SHIFT;
		result.chunk_x = (i32)(tile_abs_x & CHUNK_POSITION_MASK);
		result.chunk_y = (i32)(tile_abs_y & CHUNK_POSITION_MASK);
		return result;
	}

	static Chunk* get_chunk(Map& map, const Chunk_Position& chunk_pos) {
		// TODO: finish this
		return map.chunks.base;
	};

	static tile get_tile(Map& map, u32 abs_x, u32 abs_y) {
		Chunk_Position chunk_pos = get_chunk_position(abs_x, abs_y);
		Chunk* chunk = get_chunk(map, chunk_pos);
		assert(chunk); // TODO: обработать null
		return chunk->tiles[chunk_pos.chunk_y * CHUNK_SIZE_TILES + chunk_pos.chunk_x];
	};

	static void set_tile(Map& map, u32 abs_x, u32 abs_y, tile value) {
		Chunk_Position chunk_pos = get_chunk_position(abs_x, abs_y);
		Chunk* chunk = get_chunk(map, chunk_pos);
		assert(chunk); // TODO: обработать null
		chunk->tiles[chunk_pos.chunk_y * CHUNK_SIZE_TILES + chunk_pos.chunk_x] = value;
	}
	
	static bool check_empty_tile(Map& map, const Map_Position& map_pos) {
		return get_tile(map, map_pos.world_x, map_pos.world_y) == 0;
	}
}