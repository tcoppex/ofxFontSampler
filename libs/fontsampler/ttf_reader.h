#ifndef FONTSAMPLER_TTF_READER_H_
#define FONTSAMPLER_TTF_READER_H_

#include <cstdio>
#include <unordered_map>
#include <vector>

#include "ttf_structs.h"

/* -------------------------------------------------------------------------- */

class TTFReader {
 public:
  ~TTFReader() {
    clear();
  }

  /* Clear internal data. */
  void clear();
  
  /* Parse and store internal data of the given ttf file.
   * @return true if it succeeds. */ 
  bool read(const char* ttf_filename);

  /* Returns a pointer to an internal glyph data if it exists, nullptr otherwise.
   * The pointer should only be used for postprocessing. 
   **/
  glyph_data_t const* const get_glyph_data(uint16_t c);

 private:
  typedef uint32_t TAG_t;
  
  struct Table_t {
    uint32_t head_id;
    uint8_t *data = nullptr;
  };

  /* Check that the required TTF's tables tag were correctly loaded. */
  void check_loaded_data() const;

  /* Convert TTF data to internal structure for further use. */
  void process_data();

  /* Find character glyph location from its code. */
  uint16_t map_char(uint16_t c) const;
  uint32_t glyph_offset(uint16_t index) const;

  /* Create a glyph and store it in an internal map. */
  glyph_data_t const* const create_glyph(uint16_t charcode);

  /* Create a simple glyph. */
  glyph_data_t* create_simple_glyph(const TGlyphDesc_t &desc, const uint8_t *data);

  /* Header of the TTF, mapped to the platform's byte order. */
  Header_t header_;

  /* Raw data from the TTF file, kept in BIG-ENDIAN order. */
  std::vector<TableHeader_t> table_headers_;
  std::unordered_map<TAG_t, Table_t> tables_;

  /* Common / required tags specific values */
  THead_t head_;
  TMaxp_t maxp_;
  struct {
    TCmap_index_t index;
    std::vector<TCmap_subtable_t> subtables;
    TCmap_format4_t format4;
  } cmap_;

  struct {
    uint16_t *offset_u16 = nullptr;
    uint32_t *offset_u32 = nullptr;
  } loca_;

  /* glyphes data cache */
  std::unordered_map<uint16_t, glyph_data_t*> glyphes_;
};

/* -------------------------------------------------------------------------- */

#endif // FONTSAMPLER_TTF_READER_H_