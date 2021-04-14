#ifndef FONTSAMPLER_TTF_STRUCTS_H_
#define FONTSAMPLER_TTF_STRUCTS_H_

#include <cstdint>
#include <vector>

/* -------------------------------------------------------------------------- */

/* Type description from Apple TrueType Reference. */
typedef int32_t   Fixed;
typedef int16_t   FWord;
typedef uint16_t  UFWord;
typedef int16_t   F2Dot14;
typedef int64_t   longDateTime;

/* -------------------------------------------------------------------------- */

/* File Header */
struct Header_t {
  uint32_t filetype;
  uint16_t numTables;
  uint16_t searchRange;
  uint16_t entrySelector;
  uint16_t rangeShift;
};

/* Table sub-header */
struct TableHeader_t {
  uint32_t tag;
  uint32_t checksum;
  uint32_t offset;
  uint32_t length;
};

/* -------------------------------------------------------------------------- */

static constexpr uint32_t TrueTypeFontTag(const char s[4]) {
  return (s[3]<<24) | (s[2] << 16) | (s[1] << 8) | (s[0] << 0);
}

enum RequiredTableTAG_t : uint32_t {
  CMAP = TrueTypeFontTag("cmap"), // Character to glyph mapping
  GLYF = TrueTypeFontTag("glyf"), // Glyph data
  HEAD = TrueTypeFontTag("head"), // Font header
  HHEA = TrueTypeFontTag("hhea"), // Horizontal head
  HMTX = TrueTypeFontTag("hmtx"), // Horizontal metrics
  LOCA = TrueTypeFontTag("loca"), // Index to location
  MAXP = TrueTypeFontTag("maxp"), // Maximum profile
  NAME = TrueTypeFontTag("name"), // naming
  POST = TrueTypeFontTag("post"), // PostScript

  kNumRequiredTableTAGs = 9u
};

/* -------------------------------------------------------------------------- */

static constexpr uint32_t BitMask(const uint8_t n) { return 1 << n; }

enum MacStyleBits {
  STYLE_BOLD_BIT        = BitMask(0),
  STYLE_ITALIC_BIT      = BitMask(1),
  STYLE_UNDERLINE_BIT   = BitMask(2),
  STYLE_OUTLINE_BIT     = BitMask(3),
  STYLE_SHADOW_BIT      = BitMask(4),
  STYLE_CONDENSED_BIT   = BitMask(5),
  STYLE_EXTENDED_BIT    = BitMask(6)
};

/* -------------------------------------------------------------------------- */

#pragma pack(push, 1)
struct THead_t {
  Fixed version;
  Fixed fontRevision;
  uint32_t checkSumAdjustement;
  uint32_t magicNumber;
  uint16_t flags;
  uint16_t unitsPerEm;
  longDateTime created;
  longDateTime modified;
  FWord xMin;
  FWord yMin;
  FWord xMax;
  FWord yMax;
  uint16_t macStyle;
  uint16_t lowestRecPPEM;
  int16_t fontDirectionHint;
  int16_t indexToLocFormat;
  int16_t glyphDataFormat;
};

struct THhead_t {
  Fixed version;
  FWord ascender;
  FWord descender;
  FWord linegap;
  UFWord advanceWidthMax;
  FWord minLeftSideBearing;
  FWord minRightSideBearing;
  FWord xMaxExtent;
  int16_t caretSlipeRise;
  int16_t caretSlopeRun;
  FWord caretOffset;
  int16_t reserved[4u];
  int16_t metricDataFormat;
  uint16_t numOfLongHorMetrics; 
};

struct TMaxp_t {
  Fixed version;
  uint16_t numGlyphs;
  uint16_t maxPoints;
  uint16_t maxContours;
  uint16_t maxComponentPoints;
  uint16_t maxComponentContours;
  uint16_t maxZones;
  uint16_t maxTwilightPoints;
  uint16_t maxStorage;
  uint16_t maxFunctionDefs;
  uint16_t maxInstructionDefs;
  uint16_t maxStackElements;
  uint16_t maxSizeOfInstructions;
  uint16_t maxComponentElements;
  uint16_t maxComponentDepth;
};

struct TCmap_index_t {
  uint16_t version;
  uint16_t numberSubtables;
};

struct TCmap_subtable_t {
  uint16_t platformID;
  uint16_t platformSpecificID;
  uint32_t offset;
};

struct TCmap_format4_t {
  uint16_t format;
  uint16_t length;
  uint16_t language;
  uint16_t segCountX2;
  uint16_t searchRange;
  uint16_t entrySelector;
  uint16_t rangeShift;
  uint16_t *endCode;
  uint16_t reservedPad;
  uint16_t *startCode;
  int16_t *idDelta;
  uint16_t *idRangeOffset;
  uint16_t *glyphIndexArray;
};

struct TLoca16_t {
  uint16_t offset;
};

struct TLoca32_t {
  uint32_t offset;
};

struct TGlyphDesc_t {
  int16_t numberOfContours;
  FWord xMin;
  FWord yMin;
  FWord xMax;
  FWord yMax;
};
#pragma pack(pop)

/* -------------------------------------------------------------------------- */

enum SimpleGlyphBitFlag {
  ON_CURVE_POINT  = BitMask(0),
  X_SHORT_VECTOR  = BitMask(1),
  Y_SHORT_VECTOR  = BitMask(2),
  REPEAT_FLAG     = BitMask(3),
  X_IS_SAME       = BitMask(4),
  X_IS_POSITIVE   = X_IS_SAME,
  Y_IS_SAME       = BitMask(5),
  Y_IS_POSITIVE   = Y_IS_SAME,
  RESERVED6       = BitMask(6),
  RESERVED7       = BitMask(7)
};

enum CompoundGlyphBitFlag {
  ARG_1_AND_2_ARE_WORDS       = BitMask(0),
  ARGS_ARE_XY_VALUES          = BitMask(1),
  ROUND_XY_TO_GRID            = BitMask(2),
  HAVE_A_SCALE                = BitMask(3),
  MORE_COMPONENTS             = BitMask(4),
  HAVE_AN_X_AND_Y_SCALE       = BitMask(5),
  HAVE_A_TWO_BY_TWO           = BitMask(6),
  HAVE_INSTRUCTIONS           = BitMask(7),
  USE_METRICS                 = BitMask(8),
  OVERLAP_COMPOUND            = BitMask(9),
  SCALED_COMPONENT_OFFSET     = BitMask(10),
  UNSCALED_COMPONENT_OFFSET   = BitMask(11)
};

/* -------------------------------------------------------------------------- */

struct vertex_t {
  float x;
  float y;
  vertex_t()=default;
  vertex_t(float x_, float y_) : x(x_), y(y_) {}
  void set(float x_, float y_) { x = x_; y = y_; }
};

struct glyph_data_t {
  std::vector<vertex_t> coords;       // vertices / vector coords.
  std::vector<int> on_curve;          // non zero if vertex, zero otherwise.
  std::vector<uint16_t> contour_ends; // indices of vertex for each contour.
};

/* -------------------------------------------------------------------------- */

#endif  // FONTSAMPLER_TTF_STRUCTS_H_
