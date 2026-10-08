/**
 * compat_marshalls.h
 * ==================================================================================
 * Copyright (c) 2025-2026 Rafael Martínez Gordillo and the Terrainer contributors.
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 * ==================================================================================
 */

#ifndef TERRAINER_COMPAT_MARSHALLS_H
#define TERRAINER_COMPAT_MARSHALLS_H

#ifdef TERRAINER_MODULE
#include "core/io/marshalls.h"
#endif // TERRAINER_MODULE

#ifdef TERRAINER_GDEXTENSION
#include <godot_cpp/variant/builtin_types.hpp>

using namespace godot;

static inline unsigned int encode_uint16(uint16_t p_uint, uint8_t *p_arr) {
	for (int i = 0; i < 2; i++) {
		*p_arr = p_uint & 0xFF;
		p_arr++;
		p_uint >>= 8;
	}

	return sizeof(uint16_t);
}

static inline uint16_t decode_uint16(const uint8_t *p_arr) {
	uint16_t u = 0;

	for (int i = 0; i < 2; i++) {
		uint16_t b = *p_arr;
		b <<= (i * 8);
		u |= b;
		p_arr++;
	}

	return u;
}

static inline unsigned int encode_half(float p_float, uint8_t *p_arr) {
	encode_uint16(Math::make_half_float(p_float), p_arr);

	return sizeof(uint16_t);
}

static inline unsigned int encode_uint64(uint64_t p_uint, uint8_t *p_arr) {
	for (int i = 0; i < 8; i++) {
		*p_arr = p_uint & 0xFF;
		p_arr++;
		p_uint >>= 8;
	}

	return sizeof(uint64_t);
}
#endif // TERRAINER_GDEXTENSION


#endif // TERRAINER_COMPAT_MARSHALLS_H