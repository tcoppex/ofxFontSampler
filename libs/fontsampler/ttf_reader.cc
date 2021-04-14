#include "ttf_reader.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <algorithm>

/* -------------------------------------------------------------------------- */

/**
 * TrueType being a BigEndian format we need to 
 * convert the data bytes order on LittleEndian architecture.
 **/
namespace {

#ifdef __ORDER_LITTLE_ENDIAN__

#ifdef __GNUC__
uint32_t SwapBits(uint32_t n) { return __builtin_bswap32(n); }
uint16_t SwapBits(uint16_t n) { return __builtin_bswap16(n); }
#else
uint32_t SwapBits(uint32_t n) { return (n >> 24u) | ((n >> 8u) & 0xff00u) | ((n & 0xff00u) << 8u) | (n << 24u); }
uint16_t SwapBits(uint16_t n) { return (n >> 8u) | (n << 8u); }
#endif

int32_t SwapBits(int32_t n) { return (int32_t)SwapBits((uint32_t)n); }
int16_t SwapBits(int16_t n) { return (int16_t)SwapBits((uint16_t)n); }
uint8_t SwapBits(uint8_t n) { return n; }

#define ENDIANNESS(x)   SwapBits(x)

#else // __ORDER_BIG_ENDIAN__

#define ENDIANNESS(x)   (x)

#endif  // __ORDER_LITTLE_ENDIAN__

template<typename T>
void ConvertEndianness(T &n) {
  n = ENDIANNESS(n);
}

template<typename T>
void ConvertEndiannessArray(T *const v, const size_t size) {
  for (size_t i=0u; i<size; ++i) {
    ConvertEndianness(v[i]);
  }
}

void ConvertHeaderEndianness(Header_t &h) {
  ConvertEndianness(h.filetype);
  ConvertEndianness(h.numTables);
  ConvertEndianness(h.searchRange);
  ConvertEndianness(h.entrySelector);
  ConvertEndianness(h.rangeShift);
}

void ConvertTableHeaderEndianness(TableHeader_t &th) {
  // [the tag field is unaffected]
  ConvertEndianness(th.checksum);
  ConvertEndianness(th.offset);
  ConvertEndianness(th.length);
}

/* Calculate a table checksum. */
uint32_t CalculateTableChecksum(uint32_t *table, const uint32_t table_size) {
  uint32_t sum = 0u;
  uint32_t nLongs = (table_size + 3u) / 4u;
  while (nLongs-- > 0u) {
    sum += *table++;
  }
  return sum;
}

/* Transform a TTF table tag 'word' to a string. */
void TagWordToStr(uint32_t n, char t[5]) {
  union {
    char b[4];
    uint32_t n;
  } word;
  word.n = n;
  sprintf(t, "%s", word.b);
  t[4] = '\0'; 
}

/* Read a file at the given offset */
bool ReadOffset(void *const data, uint32_t offset, uint32_t length, FILE *const fd) {
  fseek(fd, offset, SEEK_SET);
  size_t nread = fread(data, length, 1u, fd);
  return nread == 1u;
}

}  // namespace ""

/* -------------------------------------------------------------------------- */

void TTFReader::clear() {
  for (auto &t : tables_) {
    delete [] t.second.data;
  }
  tables_.clear();

  delete [] cmap_.format4.endCode;
  delete [] cmap_.format4.startCode;
  delete [] cmap_.format4.idDelta;
  delete [] cmap_.format4.idRangeOffset;
  delete [] cmap_.format4.glyphIndexArray;
  memset(&cmap_.format4, sizeof(cmap_.format4), 0);

  if (loca_.offset_u16) {
    delete [] loca_.offset_u16;
    loca_.offset_u16 = nullptr;
  } else if (loca_.offset_u32) {
    delete [] loca_.offset_u32;
    loca_.offset_u32 = nullptr;
  }

  for (auto &g : glyphes_) {
    delete g.second;
  }
  glyphes_.clear();
}

/* -------------------------------------------------------------------------- */

bool TTFReader::read(const char* ttf_filename) {
  /* Clear previous data. */
  clear();

  /* Try to open the file. */
  FILE *fd = fopen(ttf_filename, "rb");
  if (nullptr == fd) {
    fprintf(stderr, "Error : Invalid filename \"%s\".\n", ttf_filename);
    return false;
  }

  /* Read header */
  size_t nread = fread(&header_, sizeof(header_), 1u, fd);
  assert(nread == 1u);

  /* Convert to the architecture endianness */
  ConvertHeaderEndianness(header_);

  /* Check that the file is correct */
  if ( (header_.filetype != 0x74727565)
    && (header_.filetype != 0x00010000)) {
    fprintf(stderr, "Error : Invalid magic number.\n");
    fclose(fd);
    return false;
  }

  /* Read each tables */
  table_headers_.resize(header_.numTables);
  for (auto& h : table_headers_) {
    // Read table header.
    fread(&h, sizeof(h), 1u, fd);
    // Converts to arch endianness.
    ConvertTableHeaderEndianness(h);
    // Create a table handle.
    Table_t table{};
    // Reference the table through its tag.
    tables_[h.tag] = table;
  }

  /* Sort headers by order in file. */
  std::sort(table_headers_.begin(), table_headers_.end(), 
    [](const TableHeader_t &a, const TableHeader_t &b) {
      return a.offset < b.offset;
    }
  );

  /* Fetch the data from the file. */
  for (uint32_t i=0u; i<table_headers_.size(); ++i) {
    const auto &th = table_headers_[i];
    auto &table = tables_[th.tag];

    table.head_id = i;
    table.data = new uint8_t[th.length]();

    ReadOffset(table.data, th.offset, th.length, fd);
  }

  fclose(fd);

  /* Perform value checking on the data loaded */ 
  check_loaded_data();

  /* Reprocess important data  */
  process_data();

  return true;
}

/* -------------------------------------------------------------------------- */

glyph_data_t const* const TTFReader::get_glyph_data(uint16_t c) {
  auto it = glyphes_.find(c);

  if (glyphes_.end() == it) {
    return create_glyph(c);
  }

  return it->second;
}

/* -------------------------------------------------------------------------- */

void TTFReader::check_loaded_data() const {
  assert(tables_.find(RequiredTableTAG_t::CMAP) != tables_.end());
  assert(tables_.find(RequiredTableTAG_t::GLYF) != tables_.end());
  assert(tables_.find(RequiredTableTAG_t::HEAD) != tables_.end());
  assert(tables_.find(RequiredTableTAG_t::LOCA) != tables_.end());
  assert(tables_.find(RequiredTableTAG_t::MAXP) != tables_.end());
}

/* -------------------------------------------------------------------------- */

namespace {

template<typename T>
void CopyMSBArray(T *dst, T *const src, size_t size) {
  assert(dst != nullptr);
  assert(src != nullptr);
  memcpy(dst, src, size * sizeof(T));
  ConvertEndiannessArray(dst, size);
}

template<typename T>
T* CreateCopyMSBArray(T *src, size_t size) {
  assert(src != nullptr);
  assert(size > 0);

  T *v = new T[size]();
  if (nullptr == v) {
    return nullptr;
  }
  CopyMSBArray(v, src, size);
  
  return v;
}

} // namespace ""

/* -------------------------------------------------------------------------- */

void TTFReader::process_data()
{
  /* HEAD TABLE */
  {
    Table_t &table = tables_[RequiredTableTAG_t::HEAD];

    memcpy(&head_, table.data, sizeof(THead_t));
    ConvertEndianness(head_.version);
    ConvertEndianness(head_.fontRevision);
    ConvertEndianness(head_.checkSumAdjustement);
    ConvertEndianness(head_.magicNumber);
    ConvertEndianness(head_.flags);
    ConvertEndianness(head_.unitsPerEm);
    //ConvertEndianness(created);
    //ConvertEndianness(modified);
    ConvertEndianness(head_.xMin);
    ConvertEndianness(head_.yMin);
    ConvertEndianness(head_.xMax);
    ConvertEndianness(head_.yMax);
    ConvertEndianness(head_.macStyle);
    ConvertEndianness(head_.lowestRecPPEM);
    ConvertEndianness(head_.fontDirectionHint);
    ConvertEndianness(head_.indexToLocFormat);
    ConvertEndianness(head_.glyphDataFormat);

    assert(head_.magicNumber == 0x5F0F3CF5u);
  }

  /* MAXP TABLE */
  {
    Table_t &table = tables_[RequiredTableTAG_t::MAXP];
    uint16_t *data_u16 = reinterpret_cast<uint16_t*>(table.data);
    memcpy(&maxp_, data_u16, sizeof(TMaxp_t));

    ConvertEndianness(maxp_.version);
    const size_t bytesize = (sizeof(TMaxp_t) - sizeof(maxp_.version)) / sizeof(uint16_t);
    ConvertEndiannessArray(&maxp_.numGlyphs, bytesize);
  }

  /* CMAP TABLE */
  {
    Table_t &table = tables_[RequiredTableTAG_t::CMAP];
    uint16_t *data_u16 = reinterpret_cast<uint16_t*>(table.data);

    // CMAP index
    cmap_.index.version = ENDIANNESS(data_u16[0u]);
    cmap_.index.numberSubtables = ENDIANNESS(data_u16[1u]);
    cmap_.subtables.resize(cmap_.index.numberSubtables);

    // CMAP subtable info
    uint16_t chosen_st_index = 0;
    TCmap_subtable_t *st = reinterpret_cast<TCmap_subtable_t*>(&data_u16[2u]);
    for (size_t i=0; i < cmap_.subtables.size(); ++i) {
      auto &v = cmap_.subtables[i];
      v = st[i];
      ConvertEndianness(v.platformID);
      ConvertEndianness(v.platformSpecificID);
      ConvertEndianness(v.offset);

      if (((v.platformID == 0) && (v.platformSpecificID <= 4))    // Unicode
       //|| ((v.platformID == 3) && (v.platformSpecificID == 1))    // Microsoft
      )
      {
        chosen_st_index = i;
        break;
      }
    }

    // CMAP subtable data 
    // 
    // !! retrieve ONLY platformID == 0 (Unicode) with format 4 !!
    //  
    const uint32_t offset = cmap_.subtables[chosen_st_index].offset;
    uint16_t *subtable = data_u16 + offset/2;

    const uint16_t format = ENDIANNESS(subtable[0u]);
    if (format != 4) {
      fprintf(stderr, "Error : CMAP format %u is not handled yet.\n", format);
      exit(EXIT_FAILURE);
    }

    TCmap_format4_t &cmap = cmap_.format4;
    cmap.format = format;
    cmap.length = ENDIANNESS(subtable[1]);
    cmap.language = ENDIANNESS(subtable[2]);
    cmap.segCountX2 = ENDIANNESS(subtable[3]);
    cmap.searchRange = ENDIANNESS(subtable[4]);
    cmap.entrySelector = ENDIANNESS(subtable[5]);
    cmap.rangeShift = ENDIANNESS(subtable[6]);


    const uint16_t segCount = cmap.segCountX2 >> 1;
    const uint16_t off = 8 + segCount;
    
    cmap.endCode         = CreateCopyMSBArray(&subtable[7], segCount);
    cmap.reservedPad     = subtable[7+segCount];
    cmap.startCode       = CreateCopyMSBArray(&subtable[off+0*segCount], segCount);
    cmap.idDelta         = CreateCopyMSBArray((int16_t*)&subtable[off+1*segCount], segCount);
    cmap.idRangeOffset   = CreateCopyMSBArray(&subtable[off+2*segCount], segCount);
    
    /*---------------*/
    uint16_t countGlyphs=0;
    for (int i=0; i<segCount; i++) {
      //fprintf(stderr, "%d\n", cmap.idDelta[i]);
      countGlyphs+=cmap.endCode[i]-cmap.startCode[i]; 
    }
    //fprintf(stderr, "%d // %d\n", countGlyphs, maxp_.numGlyphs);
    countGlyphs = maxp_.numGlyphs;
    /*---------------*/

    // note that indexing operation might account for
    // the memory layout of the whole subtable
    cmap.glyphIndexArray = CreateCopyMSBArray(&subtable[off+3*segCount], countGlyphs);
    
    assert(cmap.reservedPad == 0);
  }

  // LOCA table
  {
    Table_t &table = tables_[RequiredTableTAG_t::LOCA];

    if (head_.indexToLocFormat != 0) {
      uint32_t *data = reinterpret_cast<uint32_t*>(table.data);
      loca_.offset_u32 = CreateCopyMSBArray(data, maxp_.numGlyphs);
    } else  {
      uint16_t *data = reinterpret_cast<uint16_t*>(table.data);
      loca_.offset_u16 = CreateCopyMSBArray(data, maxp_.numGlyphs);
    }
  }
}

/* -------------------------------------------------------------------------- */

uint16_t TTFReader::map_char(uint16_t c) const {
  const auto &fmt = cmap_.format4;

  uint16_t glyph_index = 0;
  for (uint16_t sid=0u; fmt.endCode[sid] != 0xFFFF; ++sid)
  {
    //
    if (fmt.startCode[sid] == 0xFFFF) {
      fprintf(stderr, "Warning : glyph '%u' is missing.\n", c);
      continue;
    }

    // If the character is not in the current segment, continue the search.
    if ((c < fmt.startCode[sid]) || (c > fmt.endCode[sid])) {
      continue;
    }

    //
    if (fmt.idRangeOffset[sid] == 0)
    {
      glyph_index = fmt.idDelta[sid] + c;
    }
    else
    {
      const uint16_t start = fmt.startCode[sid];
      //uint16_t *offset = &fmt.idRangeOffset[sid];
      //glyph_index = *(offset + (c - start) + (*offset)/2);
      glyph_index = *(fmt.glyphIndexArray + (c - start));
      glyph_index += (glyph_index) ? fmt.idDelta[sid] : 0;
    }
    glyph_index &= 0xFFFF;

    // Stop the search when a valid index is found.
    if (glyph_index != 0) {
      break;
    }
  }

  return glyph_index;
}

/* -------------------------------------------------------------------------- */

uint32_t TTFReader::glyph_offset(uint16_t index) const {
  return (loca_.offset_u16) ? loca_.offset_u16[index] * 2u
                            : loca_.offset_u32[index];
}

/* -------------------------------------------------------------------------- */

glyph_data_t const* const  TTFReader::create_glyph(uint16_t charcode) {
  if (glyphes_.find(charcode) != glyphes_.end()) {
    fprintf(stderr, "Warning, glyph '%c' already exists.", charcode);
    return nullptr;
  }

  Table_t &table = tables_[RequiredTableTAG_t::GLYF];

  const uint32_t offset = glyph_offset(map_char(charcode));
  uint8_t *data_ptr = (uint8_t*)table.data + offset;
  TGlyphDesc_t desc = *reinterpret_cast<TGlyphDesc_t*>(data_ptr);
  data_ptr += sizeof(TGlyphDesc_t);

  ConvertEndianness(desc.numberOfContours);
  ConvertEndianness(desc.xMin);
  ConvertEndianness(desc.yMin);
  ConvertEndianness(desc.xMax);
  ConvertEndianness(desc.yMax);

  if (desc.numberOfContours == 0) {
    fprintf(stderr, "Warning : empty glyphes are not handled yet.\n");
    return nullptr;
  }

  glyph_data_t *glyph = nullptr;

  if (desc.numberOfContours > 0) {
    glyph = create_simple_glyph(desc, data_ptr);
  } else {
    //create_compound_glyph(desc, data_ptr);
  }
  glyphes_[charcode] = glyph;

  // [Debug output]
#if 0
  if (glyph == nullptr) {
    return nullptr;
  }
  // Generally the first contour is the exterior edge
  // and the others are interior edges (ie. holes). 
  fprintf(stderr, "'%c' : %d contours\n", charcode, desc.numberOfContours);

  // Display the vertices coordinates of each contour
  unsigned int contour_first_id = 0;
  for (auto const &last_id : glyph->contour_ends) 
  {
    fprintf(stderr, "%u\n", last_id - contour_first_id + 1 );
    for (auto i=contour_first_id; i <= last_id; ++i) 
    {
      auto const &v = glyph->coords[i];
      //bool const isVertex = glyph->on_curve[i];
      //fprintf(stderr, "%c%.3f, %.3f%c, ", isVertex ? '(' : '[', v.x, v.y, isVertex ? ')' : ']');
      fprintf(stderr, "%.3f, %.3f, ", v.x, v.y);
    }
    putc('\n', stderr);

    contour_first_id = last_id + 1;
  }

  contour_first_id = 0;
  for (auto const &last_id : glyph->contour_ends) 
  {
    fprintf(stderr, "%u\n", last_id - contour_first_id + 1 );
    for (auto i=contour_first_id; i <= last_id; ++i) 
    {
      bool const isVertex = glyph->on_curve[i];
      fprintf(stderr, "%d, ", isVertex);
    }
    putc('\n', stderr);

    contour_first_id = last_id + 1;
  }

  // Scaled BBOX
  const float s = 1.0f / head_.unitsPerEm;
  fprintf(stderr, "BBOX (%.3f, %.3f) (%.3f, %.3f)\n",
                  s*desc.xMin, s*desc.yMin, s*desc.xMax, s*desc.yMax);

  fprintf(stderr, "------------------------- \n"); 
#endif

  return glyph;
}

/* -------------------------------------------------------------------------- */

namespace {

/* Return the actual pointer value of data and move it forward. */
template<typename T>
T* read_ptr(const uint8_t**data, const size_t count=1ul) {
  T *p = ((T*)*data);
  *data += count * sizeof(T);
  return p; 
}

}  // namespace ""

/* -------------------------------------------------------------------------- */

glyph_data_t* TTFReader::create_simple_glyph(const TGlyphDesc_t &desc, const uint8_t *data) {
  glyph_data_t *glyph = new glyph_data_t();

  if (nullptr == glyph) {
    return nullptr;
  }

  // Contour endpoints.
  glyph->contour_ends.resize(desc.numberOfContours);
  CopyMSBArray(
    glyph->contour_ends.data(), 
    read_ptr<uint16_t>(&data, desc.numberOfContours), 
    desc.numberOfContours
  );

  // Number of points.
  const uint16_t num_points = glyph->contour_ends.back() + 1u;

  // Instructions.
  const uint16_t instructionLength = ENDIANNESS(*read_ptr<uint16_t>(&data));
  if (instructionLength > 0u) {
    read_ptr<uint8_t>(&data, instructionLength);
  }

  /* Collect FLAGs */
  std::vector<uint8_t> flags;
  flags.reserve(num_points);

  uint16_t y_offset = 0u;
  for(uint16_t i = 0u; i < num_points; ++i) {
    const uint8_t flag = *read_ptr<uint8_t>(&data);

    flags.push_back(flag);

    // add copy of flags if it repeats
    // (and hijack the loop increment)
    uint8_t nrepeats = 0u;
    if (flag & REPEAT_FLAG) {
      const uint8_t copy_flag = flag & ~REPEAT_FLAG;

      nrepeats = *read_ptr<uint8_t>(&data);
      for (uint16_t j = 0u; j < nrepeats; ++j) {
        flags.push_back(copy_flag);
        ++i;
      }
    }

    // calculate offset to pass the x coordinates
    uint16_t datasize = (flag & X_SHORT_VECTOR) ? sizeof(uint8_t) 
                      : (flag & X_IS_SAME)      ? 0 
                                                : sizeof(int16_t)
                                                ;
    y_offset += (1u + nrepeats) * datasize;
  }

  /* Collect Coordinates */
  glyph->coords.resize(num_points);
  glyph->on_curve.resize(num_points);

  const uint8_t *x_data = data;
  const uint8_t *y_data = data + y_offset;

  const float coord_scale = 1.0f / head_.unitsPerEm;

  /// Each point is stored relative to the previous one.
  int32_t current_x = 0;
  int32_t current_y = 0;
 
  uint16_t contour_id = 0;
  for (uint16_t i = 0u; i < num_points; ++i) {
    const uint8_t flag = flags[i];

    // READ X Data
    if (flag & X_SHORT_VECTOR) {
      uint8_t value = *read_ptr<uint8_t>(&x_data);
      const int32_t sign = ((flag & X_IS_POSITIVE) ? 1 : -1);
      current_x += sign * static_cast<int32_t>(value);
    } else if (!(flag & X_IS_SAME)) {
      int16_t value = *read_ptr<int16_t>(&x_data);
      current_x += ENDIANNESS(value);
    }

    // READ Y Data
    if (flag & Y_SHORT_VECTOR) {
      uint8_t value = *read_ptr<uint8_t>(&y_data);
      const int32_t sign = ((flag & Y_IS_POSITIVE) ? 1 : -1);
      current_y += sign * static_cast<int32_t>(value);
    } else if (!(flag & Y_IS_SAME)) {
      int16_t value = *read_ptr<int16_t>(&y_data);
      current_y += ENDIANNESS(value);
    }

    // Set glyph data
    const float fx = current_x * coord_scale;
    const float fy = current_y * coord_scale;
    glyph->coords[i].set(fx, fy);
    glyph->on_curve[i] = (flag & ON_CURVE_POINT);

    // Detects the last index of contour.
    if (i == glyph->contour_ends[contour_id]) {
      ++contour_id;
    }
  }

  return glyph;
}

/* -------------------------------------------------------------------------- */
/*
void TTFReader::create_compound_glyph(const TGlyphDesc_t &desc, const uint8_t *data) {
  //todo
}
*/
/* -------------------------------------------------------------------------- */
