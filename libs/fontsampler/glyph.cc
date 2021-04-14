#include "glyph.h"

#include <cassert>
#include <cmath>
#include <algorithm>

/* -------------------------------------------------------------------------- */

namespace {

vertex_t Lerp(const vertex_t &v1, const vertex_t &v2, float t)
{
  return vertex_t(
    (1.0f-t) * v1.x + t * v2.x,
    (1.0f-t) * v1.y + t * v2.y
  );
}

vertex_t EvaluateQuadraticBezier(const vertex_t &p0, 
                                 const vertex_t &p1, 
                                 const vertex_t &p2, 
                                 float t)
{
  return Lerp(
    Lerp(p0, p1, t),
    Lerp(p1, p2, t),
    t
  );
}

float CalculateDistance(const vertex_t &p0, const vertex_t &p1) 
{
  float x = p1.x - p0.x;
  float y = p1.y - p0.y;
  return sqrtf( x*x + y*y );
}

} // namespace ""

/* -------------------------------------------------------------------------- */

//namespace fontsampler {

Glyph::Glyph(const glyph_data_t &glyph, float scale_x, float scale_y)
{
  // Reconstruct curve paths.
  paths_.resize(glyph.contour_ends.size());
  const int num_paths = paths_.size();
  int first_index = 0;
  for (int i=0; i < num_paths; ++i) {
    const auto next_first_index = glyph.contour_ends[i] + 1;
    const auto num_vertices = next_first_index - first_index;
    paths_[i].setup(
      &(glyph.coords[first_index]), 
      &(glyph.on_curve[first_index]), 
      num_vertices,
      scale_x,
      scale_y
    );
    first_index = next_first_index;
  }

  // Detects simple inner paths.
  // complex nested imbrications cannot be found that way.
  is_inner_paths_.resize(num_paths);
  for (int i=0; i < num_paths; ++i) {
    const auto &min_a = paths_[i].getMinBound();
    const auto &max_a = paths_[i].getMaxBound();
    bool is_inner = false;
    for (int j=0; j < num_paths; ++j) {
      if (i == j) {
        continue;
      }
      const auto &min_b = paths_[j].getMinBound();
      const auto &max_b = paths_[j].getMaxBound();
      is_inner |= (min_a.x > min_b.x) && (min_a.y > min_b.y)
               && (max_a.x < max_b.x) && (max_a.y < max_b.y); 
    }
    is_inner_paths_[i] = is_inner;
  }
}


/* -------------------------------------------------------------------------- */

void GlyphPath::setup(const vertex_t *vertices,
                      const int *flags,
                      const int num_vertices,
                      const float scale_x,
                      const float scale_y)
{
  /// Recreate in-between anchor points from the compressed
  /// curved representation.
  /// @note : points might need to be scaled up to avoid precision error
  ///         when downsampling.

  // Calculate the number of points to add.
  int num_sequential_cp = 0;
  for (int i=1; i<num_vertices; ++i) {
    num_sequential_cp += int((flags[i-1]&ON_CURVE) && (flags[i]&ON_CURVE));
  }
  vertices_.reserve(num_vertices + num_sequential_cp);
  flags_.reserve(vertices_.size());

  // Reconstruct mid anchor points from control points.
  for (int i=0; i<num_vertices; ++i)
  {
    // add the current vertex
    const int i0 = i % num_vertices;
    const auto &p0 = vertices[i0];
    addVertex( p0, FlagBits(flags[i0]));

    const int i1 = (i0+1) % num_vertices;

    // if both current and next are on the curve, continue.
    if ((flags[i0]&ON_CURVE) || (flags[i1]&ON_CURVE)) {
      continue;
    }

    // otherwise both are control points, create mid anchor point.
    const auto &p1 = vertices[i1];
    const auto &anchor_point = Lerp(p0, p1, 0.5f);
    addVertex( anchor_point, ON_CURVE);
  }

  // Rescale the path and calculate its bounding box.
  rescale(scale_x, scale_y);
}

/* -------------------------------------------------------------------------- */

void GlyphPath::sample(Sampling_t &out, int subsamples, bool enable_segments_sampling) const
{
  assert(subsamples > 0);

  // Allocate memory for the sampled vertices.
  int on_curve_vertices = 0;
  for (auto & flag : flags_) {
    on_curve_vertices += int(flag & ON_CURVE);
  }
  const size_t approximate_size = on_curve_vertices * subsamples;
  out.vertices.reserve(approximate_size);
  out.distances.reserve(approximate_size);
  out.vertices.clear();
  out.distances.clear();

  // Sample the curve !
  const int num_vertices = vertices_.size();  
  const float inv_subsamples = 1.0f / subsamples;
  const int first_index = (flags_[0] & ON_CURVE) ? 0 : 1;
  bool next_point_on_curve = false;
  for (int i = first_index; i < num_vertices; i += (next_point_on_curve) ? 1 : 2)
  {
    const int i0 = (i+0) % num_vertices;
    const int i1 = (i+1) % num_vertices;

    // this point is on the curve.
    const auto &p0 = vertices_[i0];
    out.addVertex(p0);

    // this point is either on the curve or not.
    const auto &p1 = vertices_[i1];
    next_point_on_curve = flags_[i1] & ON_CURVE;

    // special case : we subsample segment only if specified.
    if (next_point_on_curve && !enable_segments_sampling) {
      continue;
    }

    // Samples intermediate points.
    const int i2 = (i+2) % num_vertices;
    const auto &p2 = vertices_[i2];
    for (int s = 1; s < subsamples; ++s)
    {
      const float t = s * inv_subsamples;
      const auto &sampled_point = (next_point_on_curve) ? Lerp(p0, p1, t)
                                                        : EvaluateQuadraticBezier(p0, p1, p2, t)
                                                        ;
      out.addVertex(sampled_point);
    }
  }
}

/* -------------------------------------------------------------------------- */

vertex_t GlyphPath::getCentroid() const 
{ 
  return Lerp(min_bound_, max_bound_, 0.5f); 
}

/* -------------------------------------------------------------------------- */

void GlyphPath::addVertex(const vertex_t &v, FlagBits flag)
{
  vertices_.push_back(v);
  flags_.push_back(flag);
}

/* -------------------------------------------------------------------------- */

void GlyphPath::rescale(float scale_x, float scale_y)
{
  std::for_each(vertices_.begin(), vertices_.end(), 
    [scale_x, scale_y](auto &v) {
      v.x *= scale_x;
      v.y *= scale_y;
  });
  calculateAABB();
}

/* -------------------------------------------------------------------------- */

void GlyphPath::calculateAABB()
{
  auto resultX = std::minmax_element(vertices_.begin(), vertices_.end(), 
    [](const auto &a, const auto &b) { return a.x < b.x; }
  );
  auto resultY = std::minmax_element(vertices_.begin(), vertices_.end(), 
    [](const auto &a, const auto &b) { return a.y < b.y; }
  );
  min_bound_.set(resultX.first->x, resultY.first->y);
  max_bound_.set(resultX.second->x, resultY.second->y);
}

/* -------------------------------------------------------------------------- */

void GlyphPath::Sampling_t::addVertex(const vertex_t &v)
{
  float distance = 0.0f;
  if (!vertices.empty()) {
    const auto &last_vertex = vertices.back();
    distance = distances.back() + CalculateDistance(last_vertex, v);
  }
  vertices.push_back(v);
  distances.push_back(distance);
}

/* -------------------------------------------------------------------------- */

vertex_t GlyphPath::Sampling_t::evaluate(float delta) const
{
  // mirror delta to [0, 1] 
  delta = fmod(delta, 1.0f);
  delta = (delta < 0.0f) ? 1.0f + delta : delta;

  const float dist = delta * length();
  const auto upper = std::upper_bound(distances.begin(), distances.end(), dist);
  const size_t i1 = (std::distance(distances.begin(), upper)-1) % vertices.size(); 
  const size_t i2 = (i1+1) % vertices.size();
  const auto &v1 = vertices[i1];
  const auto &v2 = vertices[i2];
  const auto d1 = distances[i1];
  const auto d2 = (i2 < i1) ? length() : distances[i2];

  const auto t = (dist - d1) / (d2 - d1);

  return Lerp(v1, v2, t);
}

/* -------------------------------------------------------------------------- */

float GlyphPath::Sampling_t::length() const
{
  return distances.back() + CalculateDistance(vertices.back(), vertices[0]);
}

/* -------------------------------------------------------------------------- */

//} // namespace fontsampler