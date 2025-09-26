/**
 * utils.h
 * =============================================================================
 * Copyright (c) 2025 Rafael Martínez Gordillo and the Terrainer contributors.
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 * =============================================================================
 */

#ifndef TERRAINER_UTILS_H
#define TERRAINER_UTILS_H

#include "core/variant/variant.h"

struct TTerrainData {
	int lod_level_count = 5;
	int quad_tree_leaf_size = 8;
    real_t lod_distance_ratio = 2.0;
	Vector2i raster_size;
	AABB map_dimensions;
};

inline int t_round_po2(int p_in, int p_from) {
    if (p_in > p_from) {
		// Round up to next power of 2.
        int n = p_in - 1;
		n |= n >> 1;
		n |= n >> 2;
		n |= n >> 4;
		n |= n >> 8;
		n |= n >> 16;
		n++;
        return n;
	} else {
		// Round down to previous power of 2.
		int n = p_in;
		n |= n >> 1;
		n |= n >> 2;
		n |= n >> 4;
		n |= n >> 8;
		n |= n >> 16;
		n -= n >> 1;
        return n;
	}
}

#endif // TERRAINER_UTILS_H