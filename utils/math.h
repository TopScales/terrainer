/**
 * math.h
 * ==================================================================================
 * Copyright (c) 2025-2026 Rafael Martínez Gordillo and the Terrainer contributors.
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 * ==================================================================================
 */

#ifndef TERRAINER_MATH_H
#define TERRAINER_MATH_H

#ifdef TERRAINER_MODULE
#include "core/variant/variant.h"
#endif // TERRAINER_MODULE

#ifdef TERRAINER_GDEXTENSION
#include <godot_cpp/variant/builtin_types.hpp>

using namespace godot;
#endif // TERRAINER_GDEXTENSION

namespace Terrainer {

inline bool is_po2(unsigned int p_x) {
	return (p_x & (p_x - 1)) == 0;
}

inline int round_po2(int p_in, int p_from) {
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

static real_t aabb_min_distance_sqrd_from_point(const AABB &p_aabb, const Vector3 &p_point) {
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
		distance_sqrd += d * d;
	} else if (p_point.y > end.y) {
		real_t d = p_point.y - end.y;
		distance_sqrd += d * d;
	}

	if (p_point.z < p_aabb.position.z) {
		real_t d = p_aabb.position.z - p_point.z;
		distance_sqrd += d * d;
	} else if (p_point.z > end.z) {
		real_t d = p_point.z - end.z;
		distance_sqrd += d * d;
	}

	return distance_sqrd;
}

inline bool aabb_intersects_sphere(const AABB &p_aabb, const Vector3 &p_center, real_t p_radius) {
	return aabb_min_distance_sqrd_from_point(p_aabb, p_center) <= p_radius * p_radius;
}

inline int lod_expand(int p_size, int p_lods) {
	return int(4.0 * p_size * (1.0 - 1.0 / float(1 << (2 * p_lods))) / 3.0);
}

#ifdef TERRAINER_MODULE
#define MAKE_HALF_FLOAT(v) Math::make_half_float(v)
#elif TERRAINER_GDEXTENSION
_ALWAYS_INLINE_ uint16_t make_half_float(float p_value) {
	union {
		float fv;
		uint32_t ui;
	} ci;
	ci.fv = p_value;

	uint32_t x = ci.ui;
	uint32_t sign = (unsigned short)(x >> 31);
	uint32_t mantissa;
	uint32_t exponent;
	uint16_t hf;

	// get mantissa
	mantissa = x & ((1 << 23) - 1);
	// get exponent bits
	exponent = x & (0xFF << 23);
	if (exponent >= 0x47800000) {
		// check if the original single precision float number is a NaN
		if (mantissa && (exponent == (0xFF << 23))) {
			// we have a single precision NaN
			mantissa = (1 << 23) - 1;
		} else {
			// 16-bit half-float representation stores number as Inf
			mantissa = 0;
		}
		hf = (((uint16_t)sign) << 15) | (uint16_t)((0x1F << 10)) |
				(uint16_t)(mantissa >> 13);
	}
	// check if exponent is <= -15
	else if (exponent <= 0x38000000) {
		/*
		// store a denorm half-float value or zero
		exponent = (0x38000000 - exponent) >> 23;
		mantissa >>= (14 + exponent);

		hf = (((uint16_t)sign) << 15) | (uint16_t)(mantissa);
		*/
		hf = 0; //denormals do not work for 3D, convert to zero
	} else {
		hf = (((uint16_t)sign) << 15) |
				(uint16_t)((exponent - 0x38000000) >> 13) |
				(uint16_t)(mantissa >> 13);
	}

	return hf;
}

#define MAKE_HALF_FLOAT(v) make_half_float(v)
#endif

} // namespace Terrainer

#endif // TERRAINER_MATH_H