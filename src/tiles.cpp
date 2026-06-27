#include "tiles.hpp"

namespace Tiles {
	static void normalize_position(Position& map_pos) {
		if (map_pos.tile_x < 0 || map_pos.tile_x >= TILE_SIZE) {
			i32 tiles_diff = hm::floor(map_pos.tile_x / TILE_SIZE);
			map_pos.world_x += cast_ignore_sign<u32>(tiles_diff);
			map_pos.tile_x  -= tiles_diff * TILE_SIZE;
		}

		if (map_pos.tile_y < 0 || map_pos.tile_y >= TILE_SIZE) {
			i32 tiles_diff = hm::floor(map_pos.tile_y / TILE_SIZE);
			map_pos.world_y += cast_ignore_sign<u32>(tiles_diff);
			map_pos.tile_y  -= tiles_diff * TILE_SIZE;
		}

		assert(map_pos.tile_x >= 0 && map_pos.tile_x < TILE_SIZE);
		assert(map_pos.tile_y >= 0 && map_pos.tile_y < TILE_SIZE);
	}

	static bool check_empty_tile(Map& map, u32 world_x, u32 world_y) {
		return get_tile(map, world_x, world_y) == 0;
	}

	static Tile get_tile(Map& map, u32 world_x, u32 world_y) {
		auto* chunk = get_chunk(map, world_x, world_y);
		if (!chunk) {
			// TODO: обработать null
			assert(false);
			return {};
		}
		auto chunk_rel_tile_pos = get_chunk_rel_tile_position(world_x, world_y);
		return chunk->tiles.get(chunk_rel_tile_pos.x, chunk_rel_tile_pos.y);
	};

	static void set_tile(Map& map, u32 world_x, u32 world_y, Tile value) {
		auto* chunk = get_chunk(map, world_x, world_y);
		if (!chunk) {
			// TODO: обработать null
			assert(false);
			return;
		}
		auto chunk_rel_tile_pos = get_chunk_rel_tile_position(world_x, world_y);
		chunk->tiles.get(chunk_rel_tile_pos.x, chunk_rel_tile_pos.y) = value;
	}
	
	static Chunk_Rel_Tile_Position get_chunk_rel_tile_position(u32 world_x, u32 world_y) {
		Chunk_Rel_Tile_Position result = {};
		result.x = cast<i32>(world_x & CHUNK_REL_TILE_POSITION_MASK);
		result.y = cast<i32>(world_y & CHUNK_REL_TILE_POSITION_MASK);
		return result;
	}

	static Chunk_Lookup_Key get_chunk_lookup_key(u32 world_x, u32 world_y) {
		Chunk_Lookup_Key result = {};
		result.x = cast<i32>(world_x >> CHUNK_LOOKUP_KEY_SHIFT);
		result.y = cast<i32>(world_y >> CHUNK_LOOKUP_KEY_SHIFT);
		return result;
	}

	static Chunk* get_chunk(Map& map, u32 world_x, u32 world_y) {
		// TODO: реализовать, сейчас все время отдаем один и тот же чанк
		return map.chunks.base;
	};
}