/**************************************************************************/
/*  test_storage.cpp                                                      */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#pragma once

#include "tests/test_macros.h"

#include "../map_storage/map_storage.h"
#include "core/io/image.h"
#include "modules/noise/fastnoise_lite.h"

TEST_FORCE_LINK(test_storage)

namespace TestStorage {

using namespace Terrainer;

TEST_CASE("[Modules][Terrainer] Storage") {
	Ref<MapStorage> storage;
	storage.instantiate();

	SUBCASE("Test hmap data storage") {
		const String dir_path = "res://terrainer_map_test/";

		if (DirAccess::dir_exists_absolute(dir_path)) {
			Ref<DirAccess> dir = DirAccess::open(dir_path);
    		dir->list_dir_begin();
			String file_name = dir->get_next();

			while (!file_name.is_empty()) {
				if (!dir->current_is_dir()) {
					DirAccess::remove_absolute(dir_path + file_name);
				}

				file_name = dir->get_next();
			}

			dir->list_dir_end();
			DirAccess::remove_absolute(dir_path);
		}

		const int chunk_size = 16;
		const int region_size = 8;
		DirAccess::make_dir_absolute(dir_path);
		storage->set_directory_path(dir_path);
		storage->set_chunk_size(chunk_size);
		storage->set_region_size(region_size);
		const Vector2i regions = Vector2i(2, 2);
		Ref<FastNoiseLite> noise;
		noise.instantiate();
		const int w = regions.x * chunk_size * region_size;
		const int h = regions.y * chunk_size * region_size;
		Ref<Image> image = noise->get_image(w, h);
		PackedByteArray data = image->get_data();
		storage->store_heightmap_data(data, Vector2i(w, h));
		CHECK(storage->get_num_regions() == regions.x * regions.y);
		String file1 = dir_path + vformat(MapStorage::REGION_FILE_FORMAT, 0, 0);
		String file2 = dir_path + vformat(MapStorage::REGION_FILE_FORMAT, 1, 0);
		String file3 = dir_path + vformat(MapStorage::REGION_FILE_FORMAT, 0, 1);
		String file4 = dir_path + vformat(MapStorage::REGION_FILE_FORMAT, 1, 1);
		CHECK(FileAccess::exists(file1));
		CHECK(FileAccess::exists(file2));
		CHECK(FileAccess::exists(file3));
		CHECK(FileAccess::exists(file4));
		const uint8_t *data_ptr = data.ptr();

		for (size_t irz = 0; irz < 2; ++irz) {
			for (size_t irx = 0; irx < 2; ++irx) {
				bool equal = true;

				for (size_t icz = 0; icz < region_size; ++icz) {
					for (size_t icx = 0; icx < region_size; ++icx) {
						PackedInt32Array hmap = storage->get_chunk_hmap(Vector2i(irx, irz), Vector2i(icx, icz));
						const int *hmap_ptr = hmap.ptr();

						for (size_t j = 0; j < chunk_size; ++j) {
							for (size_t i = 0; i < chunk_size; ++i) {
								size_t ix = i + icx * chunk_size + irx * region_size * chunk_size;
								size_t iz = j + icz * chunk_size + irz * region_size * chunk_size;

								if ((int)data_ptr[ix + w * iz] != *hmap_ptr) {
									equal = false;
									goto end_check_chunk;
								}

								hmap_ptr++;
							}
						}
					}
				}

				end_check_chunk:
				CHECK_MESSAGE(equal, vformat("Checking data for region [%d, %d].", irx, irz));
			}
		}

		storage->clear();
		DirAccess::remove_absolute(file1);
		DirAccess::remove_absolute(file2);
		DirAccess::remove_absolute(file3);
		DirAccess::remove_absolute(file4);
		DirAccess::remove_absolute(dir_path);
	}
}

} // namespace TestStorage
