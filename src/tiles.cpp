#include "tiles.hpp"
#include "random.hpp"

namespace Tiles {
	static void normalize_position(Position& map_pos) {
		if (map_pos.tile_rel_x < 0 || map_pos.tile_rel_x >= TILE_DIM) {
			i32 tiles_diff = hm::floor(map_pos.tile_rel_x / TILE_DIM);
			map_pos.abs_x += cast_ignore_sign<u32>(tiles_diff);
			map_pos.tile_rel_x  -= tiles_diff * TILE_DIM;
		}

		if (map_pos.tile_rel_y < 0 || map_pos.tile_rel_y >= TILE_DIM) {
			i32 tiles_diff = hm::floor(map_pos.tile_rel_y / TILE_DIM);
			map_pos.abs_y += cast_ignore_sign<u32>(tiles_diff);
			map_pos.tile_rel_y  -= tiles_diff * TILE_DIM;
		}

		// == TILE_DIM пока допускается, потому что float иногда округляется вверх
		assert(map_pos.tile_rel_x >= 0 && map_pos.tile_rel_x <= TILE_DIM);
		assert(map_pos.tile_rel_y >= 0 && map_pos.tile_rel_y <= TILE_DIM);
	}

	static bool check_empty_tile(Map& map, const Position& pos) {
		return get_tile(map, pos.abs_x, pos.abs_y, pos.abs_z) == Tile::Floor;
	}

	static Tile get_tile(Map& map, u32 abs_x, u32 abs_y, u32 abs_z) {
		auto* chunk_ptr = get_chunk(map, abs_x, abs_y, abs_z);
		if (!chunk_ptr || !chunk_ptr->tiles.base) return {};
		
		auto chunk_rel_pos = get_chunk_rel_position(abs_x, abs_y);
		return chunk_ptr->tiles.get(chunk_rel_pos.x, chunk_rel_pos.y);
	};

	static void set_tile(Arena& world_arena, Map& map, u32 abs_x, u32 abs_y, u32 abs_z, Tile value) {
		auto* chunk_ptr = get_chunk(map, abs_x, abs_y, abs_z);
		if (!chunk_ptr) {
			assert(false);
			return;
		}

		auto& chunk = *chunk_ptr;
		if (!chunk.tiles.base) {
			chunk.tiles.base = world_arena.push<Tiles::Tile>(chunk.tiles.get_size());
			for (auto& tile : chunk.tiles) {
				tile = Tiles::Tile::Floor;
			}
		}
		auto chunk_rel_pos = get_chunk_rel_position(abs_x, abs_y);
		chunk.tiles.get(chunk_rel_pos.x, chunk_rel_pos.y) = value;
	}
	
	static Chunk_Rel_Position get_chunk_rel_position(u32 abs_x, u32 abs_y) {
		Chunk_Rel_Position result = {};
		result.x = cast<i32>(abs_x & CHUNK_REL_POSITION_MASK);
		result.y = cast<i32>(abs_y & CHUNK_REL_POSITION_MASK);
		return result;
	}

	static Chunk* get_chunk(Map& map, u32 abs_x, u32 abs_y, u32 abs_z) {
		auto lookup_key = get_chunk_lookup_key(abs_x, abs_y, abs_z);

		if (lookup_key.x < 0 || lookup_key.x >= map.chunks.count_x
		 || lookup_key.y < 0 || lookup_key.y >= map.chunks.count_y
		 || lookup_key.z < 0 || lookup_key.z >= map.chunks.count_z) {
			return nullptr;
		}

		return &map.chunks.get(lookup_key.x, lookup_key.y, lookup_key.z);
	};

	static Chunk_Lookup_Key get_chunk_lookup_key(u32 abs_x, u32 abs_y, u32 abs_z) {
		Chunk_Lookup_Key result = {};
		result.x = cast<i32>(abs_x >> CHUNK_LOOKUP_KEY_SHIFT);
		result.y = cast<i32>(abs_y >> CHUNK_LOOKUP_KEY_SHIFT);
		result.z = cast<i32>(abs_z); // shift не нужен
		return result;
	}
}