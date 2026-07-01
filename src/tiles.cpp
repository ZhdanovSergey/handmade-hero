#include "tiles.hpp"

namespace Tiles {
	static void normalize_position(Position& map_pos) {
		if (map_pos.tile_rel_x < 0 || map_pos.tile_rel_x >= TILE_SIZE) {
			i32 tiles_diff = hm::floor(map_pos.tile_rel_x / TILE_SIZE);
			map_pos.abs_x += cast_ignore_sign<u32>(tiles_diff);
			map_pos.tile_rel_x  -= tiles_diff * TILE_SIZE;
		}

		if (map_pos.tile_rel_y < 0 || map_pos.tile_rel_y >= TILE_SIZE) {
			i32 tiles_diff = hm::floor(map_pos.tile_rel_y / TILE_SIZE);
			map_pos.abs_y += cast_ignore_sign<u32>(tiles_diff);
			map_pos.tile_rel_y  -= tiles_diff * TILE_SIZE;
		}

		assert(map_pos.tile_rel_x >= 0 && map_pos.tile_rel_x < TILE_SIZE);
		assert(map_pos.tile_rel_y >= 0 && map_pos.tile_rel_y < TILE_SIZE);
	}

	static bool check_empty_tile(Map& map, u32 abs_x, u32 abs_y) {
		return get_tile(map, abs_x, abs_y) == 0;
	}

	static Tile get_tile(Map& map, u32 abs_x, u32 abs_y) {
		auto* chunk = get_chunk(map, abs_x, abs_y);
		if (!chunk) {
			// TODO: обработать null
			assert(false);
			return {};
		}
		auto chunk_rel_pos = get_chunk_rel_position(abs_x, abs_y);
		return chunk->tiles.get(chunk_rel_pos.x, chunk_rel_pos.y);
	};

	static void set_tile(Map& map, u32 abs_x, u32 abs_y, Tile value) {
		auto* chunk = get_chunk(map, abs_x, abs_y);
		if (!chunk) {
			// TODO: обработать null
			assert(false);
			return;
		}
		auto chunk_rel_pos = get_chunk_rel_position(abs_x, abs_y);
		chunk->tiles.get(chunk_rel_pos.x, chunk_rel_pos.y) = value;
	}
	
	static Chunk_Rel_Position get_chunk_rel_position(u32 abs_x, u32 abs_y) {
		Chunk_Rel_Position result = {};
		result.x = cast<i32>(abs_x & CHUNK_REL_TILE_POSITION_MASK);
		result.y = cast<i32>(abs_y & CHUNK_REL_TILE_POSITION_MASK);
		return result;
	}

	static Chunk_Lookup_Key get_chunk_lookup_key(u32 abs_x, u32 abs_y) {
		Chunk_Lookup_Key result = {};
		result.x = cast<i32>(abs_x >> CHUNK_LOOKUP_KEY_SHIFT);
		result.y = cast<i32>(abs_y >> CHUNK_LOOKUP_KEY_SHIFT);
		return result;
	}

	static Chunk* get_chunk(Map& map, u32 abs_x, u32 abs_y) {
		// TODO: реализовать, сейчас все время отдаем один и тот же чанк
		return map.chunks.base;
	};
}