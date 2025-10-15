/**
 * tailed_image.cpp
 * =============================================================================
 * Copyright (c) 2025 Rafael Martínez Gordillo and the Terrainer contributors.
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 * =============================================================================
 */

 #include "tiled_image.h"
 #include "utils.h"

Error TTiledImage::create(const String p_path, Image::Format p_format, int p_blocks_x, int p_blocks_y, int p_block_size) {
    ERR_FAIL_COND_V_EDMSG(!t_is_po2(p_block_size), ERR_INVALID_PARAMETER, "Block size must a power of 2.");
    format = p_format;
    block_size = p_block_size;
    blocks_x = p_blocks_x;
    blocks_y = p_blocks_y;
    width = p_blocks_x * block_size;
    height = p_blocks_y * block_size;
    stored_blocks = 0;

    if (FileAccess::exists(p_path)) {
        WARN_PRINT_ED(vformat("File %s already exist. It will be overwritten."));
    }

    Error error;
    file = FileAccess::open(p_path, FileAccess::WRITE, &error);
    ERR_FAIL_COND_V_EDMSG(error != OK, error, "An error occurred while opening Tiled Image file.");
    file->store_32(FORMAT_VERSION);
    file->store_32(format);
    file->store_32(block_size);
    file->store_32(blocks_x);
    file->store_32(blocks_y);
    file->store_32(stored_blocks);
}

Error TTiledImage::load(const String p_path) {

}

int TTiledImage::get_width() const {
    return width;
}

int TTiledImage::get_height() const {
    return height;
}

Size2i TTiledImage::get_size() const {
    return Size2i(width, height);
}

TTiledImage::TTiledImage() {
}

TTiledImage::~TTiledImage() {
}