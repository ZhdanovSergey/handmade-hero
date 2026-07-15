#include "tiles.hpp"
#include "random.hpp"

namespace Tiles {
	static bool check_same_tile(const Position& pos1, const Position& pos2) {
		return pos1.abs_x == pos2.abs_x &&
		       pos1.abs_y == pos2.abs_y &&
			   pos1.abs_z == pos2.abs_z;
	}

	static bool check_walkable_tile(Map& map, const Position& pos) {
		switch (get_tile(map, pos.abs_x, pos.abs_y, pos.abs_z)) {
			case Tile::Floor:
			case Tile::Stairs_Up:
			case Tile::Stairs_Down: return true;
			default:                return false;
		}
	}

	static Tile get_tile(Map& map, i32 abs_x, i32 abs_y, i32 abs_z) {
		auto* chunk_ptr = get_chunk(map, abs_x, abs_y, abs_z);
		if (!chunk_ptr || !chunk_ptr->tiles.base) return {};
		
		auto chunk_rel_pos = get_chunk_rel_position(abs_x, abs_y);
		return chunk_ptr->tiles.get(chunk_rel_pos.x, chunk_rel_pos.y);
	};

	static void set_tile(Arena& world_arena, Map& map, i32 abs_x, i32 abs_y, i32 abs_z, Tile value) {
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

	static Chunk* get_chunk(Map& map, i32 abs_x, i32 abs_y, i32 abs_z) {
		auto lookup_key = get_chunk_lookup_key(abs_x, abs_y, abs_z);

		if (lookup_key.x < 0 || lookup_key.x >= map.chunks.count_x ||
		    lookup_key.y < 0 || lookup_key.y >= map.chunks.count_y ||
		    lookup_key.z < 0 || lookup_key.z >= map.chunks.count_z) {
			return nullptr;
		}

		return &map.chunks.get(lookup_key.x, lookup_key.y, lookup_key.z);
	};

	static Chunk_Lookup_Key get_chunk_lookup_key(i32 abs_x, i32 abs_y, i32 abs_z) {
		Chunk_Lookup_Key result = {};
		result.x = cast<i32>(cast<u32, IGNORE_SIGN>(abs_x) >> CHUNK_LOOKUP_KEY_SHIFT);
		result.y = cast<i32>(cast<u32, IGNORE_SIGN>(abs_y) >> CHUNK_LOOKUP_KEY_SHIFT);
		result.z = abs_z; // сдвиг не нужен
		return result;
	}
	
	static Chunk_Rel_Position get_chunk_rel_position(i32 abs_x, i32 abs_y) {
		Chunk_Rel_Position result = {};
		result.x = abs_x & CHUNK_REL_POSITION_MASK;
		result.y = abs_y & CHUNK_REL_POSITION_MASK;
		return result;
	}

	static void normalize_position(Position& pos) {
		if (pos.tile_rel_x < 0 || pos.tile_rel_x >= TILE_DIM) {
			i32 tiles_diff = hm::floor(pos.tile_rel_x / TILE_DIM);
			pos.abs_x += tiles_diff;
			pos.tile_rel_x  -= tiles_diff * TILE_DIM;
		}

		if (pos.tile_rel_y < 0 || pos.tile_rel_y >= TILE_DIM) {
			i32 tiles_diff = hm::floor(pos.tile_rel_y / TILE_DIM);
			pos.abs_y += tiles_diff;
			pos.tile_rel_y  -= tiles_diff * TILE_DIM;
		}

		// == TILE_DIM пока допускается, потому что float иногда округляется вверх
		assert(pos.tile_rel_x >= 0 && pos.tile_rel_x <= TILE_DIM);
		assert(pos.tile_rel_y >= 0 && pos.tile_rel_y <= TILE_DIM);
	}
}