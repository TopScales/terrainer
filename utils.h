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

inline bool t_is_po2(unsigned int p_x) {
	return (p_x & (p_x - 1)) == 0;
}

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

inline uint32_t t_log2i(const uint32_t p_x){
	union { uint32_t u[2]; double d; } t;
#ifdef BIG_ENDIAN_ENABLED
	t.u[0] = 0x43300000;
	t.u[1] = p_x;
	t.d -= 4503599627370496.0;
	return (t.u[0] >> 20) - 0x3FF;
#else
	t.u[1] = 0x43300000;
	t.u[0] = p_x;
	t.d -= 4503599627370496.0;
	return (t.u[1] >> 20) - 0x3FF;
#endif
}

static real_t t_aabb_min_distance_sqrd_from_point(const AABB &p_aabb, const Vector3 &p_point) {
	const Vector3 end = p_aabb.get_end();
	real_t distance_sqrd = 0.0;

	if (p_point.x < p_aabb.position.x) {
		real_t d = p_aabb.position.x - p_point.x;
		distance_sqrd = d * d;
	} else if (p_point.x > end.x) {
		real_t d = p_point.x - end.x;
		distance_sqrd = d * d;
	}

	if (p_point.y < p_aabb.position.y) {
		real_t d = p_aabb.position.y - p_point.y;
		distance_sqrd = d * d;
	} else if (p_point.y > end.y) {
		real_t d = p_point.y - end.y;
		distance_sqrd = d * d;
	}

	if (p_point.z < p_aabb.position.z) {
		real_t d = p_aabb.position.z - p_point.z;
		distance_sqrd = d * d;
	} else if (p_point.z > end.z) {
		real_t d = p_point.z - end.z;
		distance_sqrd = d * d;
	}

	return distance_sqrd;
}

inline bool t_aabb_intersects_sphere(const AABB &p_aabb, const Vector3 &p_center, real_t p_radius) {
	return t_aabb_min_distance_sqrd_from_point(p_aabb, p_center) <= p_radius * p_radius;
}

#endif // TERRAINER_UTILS_H