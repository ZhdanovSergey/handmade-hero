#include "tiles.hpp"

namespace Tiles {
	static bool check_same_tile(Position& pos1, Position& pos2) {
		return pos1.abs_xy == pos2.abs_xy &&
			   pos1.abs_z  == pos2.abs_z;
	}

	static bool check_walkable_tile(Map& map, Position& pos) {
		switch (get_tile(map, pos.abs_xy.x, pos.abs_xy.y, pos.abs_z)) {
			case Tile::Floor:
			case Tile::Stairs_Up:
			case Tile::Stairs_Down: return true;
			default:                return false;
		}
	}

	static Tile get_tile(Map& map, i32 abs_x, i32 abs_y, i32 abs_z) {
		auto* chunk_ptr = get_chunk(map, abs_x, abs_y, abs_z);
		if (!chunk_ptr || !chunk_ptr->tiles.ptr) return {};
		
		vec2<i32> chunk_rel_pos = get_chunk_rel_position(abs_x, abs_y);
		return chunk_ptr->tiles(chunk_rel_pos.x, chunk_rel_pos.y);
	};

	static void set_tile(Arena& world_arena, Map& map, i32 abs_x, i32 abs_y, i32 abs_z, Tile value) {
		auto* chunk_ptr = get_chunk(map, abs_x, abs_y, abs_z);
		if (!chunk_ptr) {
			assert(false);
			return;
		}

		auto& chunk = *chunk_ptr;
		if (!chunk.tiles.ptr) {
			chunk.tiles.ptr = world_arena.push<Tiles::Tile>(chunk.tiles.get_size());
			for (auto& tile : chunk.tiles) {
				tile = Tiles::Tile::Floor;
			}
		}
		vec2<i32> chunk_rel_pos = get_chunk_rel_position(abs_x, abs_y);
		chunk.tiles(chunk_rel_pos.x, chunk_rel_pos.y) = value;
	}

	static Chunk* get_chunk(Map& map, i32 abs_x, i32 abs_y, i32 abs_z) {
		auto lookup_key = get_chunk_lookup_key(abs_x, abs_y, abs_z);

		if (lookup_key.x < 0 || lookup_key.x >= map.chunks.count_x ||
		    lookup_key.y < 0 || lookup_key.y >= map.chunks.count_y ||
		    lookup_key.z < 0 || lookup_key.z >= map.chunks.count_z) {
			return nullptr;
		}

		return &map.chunks(lookup_key.x, lookup_key.y, lookup_key.z);
	};

	static Chunk_Lookup_Key get_chunk_lookup_key(i32 abs_x, i32 abs_y, i32 abs_z) {
		Chunk_Lookup_Key result = {};
		result.x = cast<i32>(cast<u32>(abs_x) >> CHUNK_LOOKUP_KEY_SHIFT);
		result.y = cast<i32>(cast<u32>(abs_y) >> CHUNK_LOOKUP_KEY_SHIFT);
		result.z = abs_z; // сдвиг не нужен
		return result;
	}
	
	static vec2<i32> get_chunk_rel_position(i32 abs_x, i32 abs_y) {
		vec2<i32> result = {};
		result.x = abs_x & CHUNK_REL_POSITION_MASK;
		result.y = abs_y & CHUNK_REL_POSITION_MASK;
		return result;
	}

	void Position::normalize() {
		auto& pos = *this;

		for (i32 axis = 0; axis < 2; ++axis) {
			if (pos.tile_rel(axis) < 0 || pos.tile_rel(axis) >= TILE_DIM) {
				i32 tiles_diff = hm::floor(pos.tile_rel(axis) / TILE_DIM);
				pos.abs_xy(axis)   += tiles_diff;
				pos.tile_rel(axis) -= tiles_diff * TILE_DIM;
			}
		}

		// value == TILE_DIM пока допускается, потому что float иногда округляется вверх
		assert((pos.tile_rel.x >= 0 && pos.tile_rel.x <= TILE_DIM));
		assert((pos.tile_rel.y >= 0 && pos.tile_rel.y <= TILE_DIM));
	}

	static vec2<f32> subtract_positions(Position& a, Position& b) {
		return cast<vec2<f32>>(a.abs_xy - b.abs_xy) * TILE_DIM + (a.tile_rel - b.tile_rel);
	}
}