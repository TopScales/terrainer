#pragma once

#include "scene/3d/node_3d.h"

class TTerrain : public Node3D {
    GDCLASS(TTerrain, Node3D);

private:
    const int MAX_CHUNK_SIZE = 2048;

    int chunk_size = 32;
    Vector2i chunk_world_offset;
    bool centered = false;
    real_t unit_size = 1.0;
    bool chunk_manual_update = false;
    Vector2i chunk_active;

protected:
    static void _bind_methods();

public:
    void set_chunk_size(int p_size);
    int get_chunk_size() const;
    void set_chunk_world_offset(Vector2i p_offset);
    Vector2i get_chunk_world_offset() const;
    void set_centered(bool p_centered);
    bool is_centered() const;
    void set_unit_size(real_t p_size);
    real_t get_unit_size() const;
    void set_chunk_manual_update(bool p_manual);
    bool is_chunk_manual_update() const;
    void set_chunk_active(Vector2i p_active);
    Vector2i get_chunk_active() const;

	TTerrain();
    ~TTerrain();
};