#include "terrain.h"

void TTerrain::set_chunk_size(int p_size) {
	ERR_FAIL_COND_EDMSG(p_size <= 0, "Terrain chunk size must be greater than zero.");

	if (p_size > MAX_CHUNK_SIZE || p_size == chunk_size) {
		return;
	}

	if (p_size > chunk_size) {
		// Round up to next power of 2.
		chunk_size = p_size - 1;
		chunk_size |= chunk_size >> 1;
		chunk_size |= chunk_size >> 2;
		chunk_size |= chunk_size >> 4;
		chunk_size |= chunk_size >> 8;
		chunk_size |= chunk_size >> 16;
		chunk_size++;
	} else {
		// Round down to previous power of 2.
		chunk_size = p_size;
		chunk_size |= chunk_size >> 1;
		chunk_size |= chunk_size >> 2;
		chunk_size |= chunk_size >> 4;
		chunk_size |= chunk_size >> 8;
		chunk_size |= chunk_size >> 16;
		chunk_size -= chunk_size >> 1;
	}
}

int TTerrain::get_chunk_size() const {
	return chunk_size;
}

void TTerrain::set_chunk_world_offset(Vector2i p_offset) {
	chunk_world_offset = p_offset;
}

Vector2i TTerrain::get_chunk_world_offset() const {
	return chunk_world_offset;
}

void TTerrain::set_centered(bool p_centered) {
	centered = p_centered;
}

bool TTerrain::is_centered() const {
	return centered;
}

void TTerrain::set_unit_size(real_t p_size) {
	unit_size = p_size;
}

real_t TTerrain::get_unit_size() const {
	return unit_size;
}

void TTerrain::set_chunk_manual_update(bool p_manual) {
	chunk_manual_update = p_manual;
}

bool TTerrain::is_chunk_manual_update() const {
	return chunk_manual_update;
}

void TTerrain::set_chunk_active(Vector2i p_active) {
	chunk_active = p_active;
}

Vector2i TTerrain::get_chunk_active() const {
	return chunk_active;
}

void TTerrain::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_chunk_size", "size"), &TTerrain::set_chunk_size);
	ClassDB::bind_method(D_METHOD("get_chunk_size"), &TTerrain::get_chunk_size);
	ClassDB::bind_method(D_METHOD("set_chunk_world_offset", "offset"), &TTerrain::set_chunk_world_offset);
	ClassDB::bind_method(D_METHOD("get_chunk_world_offset"), &TTerrain::get_chunk_world_offset);
	ClassDB::bind_method(D_METHOD("set_centered", "centered"), &TTerrain::set_centered);
	ClassDB::bind_method(D_METHOD("is_centered"), &TTerrain::is_centered);
	ClassDB::bind_method(D_METHOD("set_unit_size", "size"), &TTerrain::set_unit_size);
	ClassDB::bind_method(D_METHOD("get_unit_size"), &TTerrain::get_unit_size);
	ClassDB::bind_method(D_METHOD("set_chunk_manual_update", "manual"), &TTerrain::set_chunk_manual_update);
	ClassDB::bind_method(D_METHOD("is_chunk_manual_update"), &TTerrain::is_chunk_manual_update);
	ClassDB::bind_method(D_METHOD("set_chunk_active", "active"), &TTerrain::set_chunk_active);
	ClassDB::bind_method(D_METHOD("get_chunk_active"), &TTerrain::get_chunk_active);

	ADD_GROUP("Chunk", "chunk_");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "chunk_size", PROPERTY_HINT_RANGE, "4,2048"), "set_chunk_size", "get_chunk_size");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2I, "chunk_world_offset"), "set_chunk_world_offset", "get_chunk_world_offset");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "centered"), "set_centered", "is_centered");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "unit_size", PROPERTY_HINT_RANGE, "0.1,10.0,0.1,or_greater"), "set_unit_size", "get_unit_size");
	ADD_SUBGROUP("Chunk Manual Update", "chunk_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "chunk_manual_update", PROPERTY_HINT_GROUP_ENABLE), "set_chunk_manual_update", "is_chunk_manual_update");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2I, "chunk_active"), "set_chunk_active", "get_chunk_active");
}

TTerrain::TTerrain() {
}

TTerrain::~TTerrain() {
}