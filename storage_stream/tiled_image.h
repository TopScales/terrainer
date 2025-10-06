/**
 * tailed_image.h
 * =============================================================================
 * Copyright (c) 2025 Rafael Martínez Gordillo and the Terrainer contributors.
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 * =============================================================================
 */

#ifndef TERRAINER_TILED_IMAGE_H
#define TERRAINER_TILED_IMAGE_H

#include "core/io/file_access.h"
#include "core/io/image.h"

class TTiledImage : public Resource {
    GDCLASS(TTiledImage, Resource);

private:
    static const int FORMAT_VERSION = 1;
    Ref<FileAccess> file;
    Image::Format format = Image::FORMAT_L8;
    int width = 0;
    int height = 0;
    int block_size = 128;
    int blocks_x = 0;
    int blocks_y = 0;
    int stored_blocks = 0;

public:
    Error create(const String p_path, Image::Format p_format, int p_blocks_x, int p_blocks_y, int p_block_size);
    Error load(const String p_path);

    int get_width() const;
	int get_height() const;
	Size2i get_size() const;

    TTiledImage();
    ~TTiledImage();
};

#endif // TERRAINER_TILED_IMAGE_H