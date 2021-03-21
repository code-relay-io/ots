// Copyright (c) 2011-2021 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "eblc.h"
#include "ebdt.h"

#include <limits>
#include <vector>


// EBLC - Embedded Bitmap Location Table
// http://www.microsoft.com/typography/otspec/eblc.htm

#define TABLE_NAME "EBLC"


namespace {


bool ParseIndexSubTable(const ots::Font* font,
  const ots::OpenTypeEBDT* ebdt,
  uint16_t first_glyph_index,
  uint16_t last_glyph_index,
  const uint8_t* data, size_t length) {
  ots::Buffer table(data, length);
  uint16_t index_format = 0;
  uint16_t image_format = 0;
  uint32_t image_data_offset = 0;
  if (!table.ReadU16(&index_format) ||
    !table.ReadU16(&image_format) ||
    !table.ReadU32(&image_data_offset)) {
    return OTS_FAILURE_MSG("Failed to read IndexSubTable");
  }
  //TODO index_format and image_format can be contradictory
  // (They can both provide metrics) is this allowed?
  switch (index_format)
  {
  case 1:
  
    last_glyph_index - first_glyph_index + 1 + 1
    break;
  case 2:
  case 3:
  case 4:
  case 5:
  default:
    return OTS_FAILURE_MSG("Invalid format %d", index_format);
  }

  return true;
};



bool ParseIndexSubTableArray(
  const ots::Font* font,
  const ots::OpenTypeEBDT* ebdt,
  const uint8_t* eblc_data,
  size_t eblc_length,
  uint32_t index_sub_table_array_offset,
  const uint8_t* data,
  size_t length) {

  ots::Buffer table(data, length);
  uint16_t first_glyph_index = 0;
  uint16_t last_glyph_index = 0;
  uint32_t additional_offset_to_index_subtable = 0;
  if (
    //The firstGlyphIndex and the lastglyphIndex
    !table.ReadU16(&first_glyph_index) ||
    !table.ReadU16(&last_glyph_index) ||
    !table.ReadU32(&additional_offset_to_index_subtable)) {
    return OTS_FAILURE_MSG("Failed to read IndexSubTableArray");
  }
  if (last_glyph_index < first_glyph_index) {
    return OTS_FAILURE_MSG("Invalid glyph indicies, first index %d > than last index %d", first_glyph_index, last_glyph_index);
  }
  uint32_t offset = index_sub_table_array_offset + additional_offset_to_index_subtable;
  if (offset >= eblc_length) {
    // Don't need to check the lower bound because we already checked
    // index_sub_table_array_offset
    return OTS_FAILURE_MSG("Bad index sub table offset %d", offset);
  }
  if (!ParseIndexSubTable(font, ebdt, first_glyph_index, last_glyph_index, eblc_data + offset, eblc_length - offset)) {
    return OTS_FAILURE_MSG("Bad index sub table ");
  }
  return true;

}
} // namespace

namespace ots {




bool OpenTypeEBLC::Parse(const uint8_t* data, size_t length) {
  Font* font = GetFont();
  Buffer table(data, length);

  uint16_t version_major = 0, version_minor = 0;
  uint32_t num_sizes = 0;
  if (!table.ReadU16(&version_major) ||
    !table.ReadU16(&version_minor) ||
    !table.ReadU32(&num_sizes)) {
    return Error("Incomplete table");
  }
  if (version_major != 2 || version_minor != 0) {
    return Error("Bad version");
  }
  std::vector<uint32_t> index_subtable_arrays;
  index_subtable_arrays.reserve(num_sizes);
  const unsigned bitmap_size_end = 48 * static_cast<unsigned>(num_sizes) + 8;
  OpenTypeEBDT* ebdt = static_cast<OpenTypeEBDT*>(
      font->GetTypedTable(OTS_TAG_EBDT));
  
  if(!ebdt){
    return OTS_FAILURE_MSG("Missing required table EBDT");
  }
  for (uint32_t i = 0; i < num_sizes; i++) {
    uint32_t index_sub_table_array_offset = 0;
    uint32_t index_table_size = 0;
    uint32_t number_of_index_sub_tables = 0;
    uint32_t color_ref = 0;
    uint16_t start_glyph_index = 0;
    uint16_t end_glyph_index = 0;
    uint8_t bit_depth = 0;
    uint8_t flags = 0;
    //BitmapSize Record
    if (!table.ReadU32(&index_sub_table_array_offset) ||
      !table.ReadU32(&index_table_size) ||
      !table.ReadU32(&number_of_index_sub_tables) ||
      !table.ReadU32(&color_ref) ||
      //Skip Horizontal and vertical SbitLineMetrics
      !table.Skip(24) ||
      !table.ReadU16(&start_glyph_index) ||
      !table.ReadU16(&end_glyph_index) ||
      //Skip ppemX and ppemY
      !table.Skip(2) ||
      !table.ReadU8(&bit_depth) ||
      !table.ReadU8(&flags)
      ) {
      return Error("Incomplete table");
    }
    if (end_glyph_index < start_glyph_index) {
      return Error("start glyph is greater than end glyph");
    }
    if (color_ref != 0) {
      return Error("Color ref should be 0");
    }
    if (index_sub_table_array_offset < bitmap_size_end ||
      index_sub_table_array_offset >= length) {
      return OTS_FAILURE_MSG("Bad index sub table array offset %d for BitmapSize %d", index_sub_table_array_offset, i);
    }
    index_subtable_arrays.push_back(index_sub_table_array_offset);
  }
  if (index_subtable_arrays.size() != num_sizes) {
    return OTS_FAILURE_MSG("Bad subtable size %ld", index_subtable_arrays.size());
  }

  uint16_t lowest_glyph = UINT16_MAX;
  uint16_t highest_glyph = 0;

  for (unsigned i = 0; i < num_sizes; ++i) {
    if (!ParseIndexSubTableArray(
      font,
      ebdt,
      /* eblc_data */ data,
      /* eblc_length */ length,
      /*index_sub_table_array_offset*/  index_subtable_arrays[i],
      data + index_subtable_arrays[i],
      length - index_subtable_arrays[i]
    )) {
      return OTS_FAILURE_MSG("Failed to parse IndexSubTableArray %d", i);
    }
  }
  if (lowest_glyph)

    return true;
}

bool OpenTypeEBLC::Serialize(OTSStream* out) {
  if (!out->Write(this->m_data, this->m_length)) {
    return Error("Failed to write EBLC table");
  }

  return true;
}

}  // namespace ots
#undef TABLE_NAME
