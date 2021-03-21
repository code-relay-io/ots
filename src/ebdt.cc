// Copyright (c) 2011-2021 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ebdt.h"
#include "eblc.h"
#include <limits>
#include <vector>

#include "layout.h"
#include "maxp.h"

// EBDT - Embedded Bitmap Data Table
// http://www.microsoft.com/typography/otspec/ebdt.htm

#define TABLE_NAME "EBDT"


namespace {

} // namespace

namespace ots {
bool OpenTypeEBDT::Parse(const uint8_t *data, size_t length) {
  Font *font = GetFont();
  Buffer table(data, length);

  uint16_t version_major = 0, version_minor = 0;
  if (!table.ReadU16(&version_major) ||
    !table.ReadU16(&version_minor)) {
    return Error("Incomplete table");
  }
  if (version_major != 2 || version_minor > 0) {
    return Error("Bad version");
  }
  // The rest of this table is parsed by EBLC
  return true;
}

bool OpenTypeEBDT::Serialize(OTSStream *out) {
  if (!out->Write(this->m_data, this->m_length)) {
    return Error("Failed to write EBDT table");
  }

  return true;
}

}  // namespace ots
#undef TABLE_NAME
