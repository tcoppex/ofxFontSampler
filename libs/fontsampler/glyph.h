#ifndef FONTSAMPLER_GLYPH_H_
#define FONTSAMPLER_GLYPH_H_

#include "ttf_structs.h"

class GlyphPath;

/* -------------------------------------------------------------------------- */

class Glyph {
 public:
  Glyph(const glyph_data_t &glyph, float scale_x, float scale_y);

  explicit 
  Glyph(const glyph_data_t &glyph)
    : Glyph(glyph, 1.0f, 1.0f) 
  {}

  GlyphPath const* const getPath(int index) const {
    return (index < getNumPaths()) ? &paths_[index] : nullptr;
  }

  int getNumPaths() const {
    return static_cast<int>(paths_.size());
  }

  bool isInnerPath(int index) const {
    return is_inner_paths_[index];
  }

 private:
  std::vector<GlyphPath> paths_;
  std::vector<bool> is_inner_paths_;
};

/* -------------------------------------------------------------------------- */

class GlyphPath {
 public:
  /** Discretized sampling of a GlyphPath. 
   * This structure holds a discrete sampling of a path and can be used
   * to reevaluate. 
   * @note No parametrics information of the curve is holds here. */
  struct Sampling_t
  {
    // Vertices coordinates and their distances
    std::vector<vertex_t> vertices;
    std::vector<float> distances;

    void addVertex(const vertex_t &v);
    
    /* Return a precise point on the curve given its absolute position.
     * @note Downsampling a curve from this method can easily bypass 
     * crest vertices. */
    vertex_t evaluate(float dt) const;
    
    int size() const { return vertices.size(); }
    float length() const;
  };

  /* Information flag about the vertex. */
  enum FlagBits : uint32_t {
    NONE      = 0,
    ON_CURVE  = BitMask(0),
    RESERVED1 = BitMask(1),
    RESERVED2 = BitMask(2),
    RESERVED3 = BitMask(3),
    RESERVED4 = BitMask(4),
    RESERVED5 = BitMask(5),
    RESERVED6 = BitMask(6),
    RESERVED7 = BitMask(7)
  };

  /* Default number of samples used per path segments. */
  static constexpr int kDefaultSubSamples = 4;

 public:
  GlyphPath() = default;

  void setup(const vertex_t *vertices,
             const int *flags,
             const int num_vertices,
             const float scale_x,
             const float scale_y);

  /* Create a discretized sampling of the curve. */
  void sample(Sampling_t &out, int subsamples = kDefaultSubSamples, bool enable_segments_sampling = false) const;

  inline int getNumVertices() const { return vertices_.size(); }
  inline const vertex_t& getVertex(int index) const { return vertices_[index]; }
  inline const FlagBits& getFlag(int index) const { return flags_[index]; }
  inline const vertex_t& getMinBound() const { return min_bound_; }
  inline const vertex_t& getMaxBound() const { return max_bound_; }
  
  vertex_t getCentroid() const;

 private:
  /* Add a vertex with given flag to the path. */
  void addVertex(const vertex_t &v, FlagBits flag);

  /* Calculate the path bounding box. */
  void calculateAABB();

  /* Rescale each vertices and calculate the new bounds. */  
  void rescale(float scale_x, float scale_y);

  // Curve parameters.
  std::vector<vertex_t> vertices_;
  std::vector<FlagBits> flags_;
  vertex_t min_bound_;
  vertex_t max_bound_;
};

/* -------------------------------------------------------------------------- */

#endif // FONTSAMPLER_GLYPH_H_