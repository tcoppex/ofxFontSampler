
#include "ofxGlyph.h"

/* -------------------------------------------------------------------------- */

namespace {

/* Return the normal of a segment. */
ofPoint CalculateNormal(const ofPoint &vertex0, const ofPoint &vertex1) {
  return (vertex1 - vertex0).rotate(90.0f, ofVec3f(0,0,-1)).normalize();
}

};  // namespace

/* -------------------------------------------------------------------------- */

ofxGlyph::ofxGlyph(Glyph *glyph)
  : fs_glyph_(glyph)
  , outer_path(nullptr)
{
  // Find first outer path. 
  for (int i = 0; i < fs_glyph_->getNumPaths(); ++i) {
    if (!fs_glyph_->isInnerPath(i)) {
      outer_path = fs_glyph_->getPath(i);
      break;
    }
  }
}

/* -------------------------------------------------------------------------- */

ofxGlyph::~ofxGlyph()
{
  if (fs_glyph_) {
    delete fs_glyph_;
  }
}

/* -------------------------------------------------------------------------- */

ofPoint ofxGlyph::getMinBound() const
{
  const auto &v = outer_path->getMinBound();
  return ofPoint(v.x, v.y);
}

/* -------------------------------------------------------------------------- */

ofPoint ofxGlyph::getMaxBound() const
{
  const auto &v = outer_path->getMaxBound();
  return ofPoint(v.x, v.y);
}

/* -------------------------------------------------------------------------- */

ofPoint ofxGlyph::getCentroid() const
{
  const auto &v = outer_path->getCentroid();
  return ofPoint(v.x, v.y);
}

/* -------------------------------------------------------------------------- */

void ofxGlyph::extractPath(ofPath &path) const
{
  path.clear();

  for (int i = 0; i < fs_glyph_->getNumPaths(); ++i) {
    const auto &gp = fs_glyph_->getPath(i);
    const bool is_inner = fs_glyph_->isInnerPath(i);
    const int inc = (is_inner) ? -1 : +1;

    // first vertex
    {
      vertex_t v(gp->getVertex(0));
      GlyphPath::FlagBits flag(gp->getFlag(0));
      if (flag & GlyphPath::ON_CURVE) {
        path.moveTo(v.x, v.y, 0.0f);
      }
    }

    // rest of the curve
    bool next_point_on_curve = false;
    for (int i = 0; i < gp->getNumVertices(); i += (next_point_on_curve) ? 1 : 2) {
      const int i0 = ofWrap(inc*i, 0, gp->getNumVertices());
      const int i1 = ofWrap(i0 + inc, 0, gp->getNumVertices());
      const int i2 = ofWrap(i1 + inc, 0, gp->getNumVertices()); 
      
      const vertex_t &v0(gp->getVertex(i0)); 
      const vertex_t &v1(gp->getVertex(i1));
      const vertex_t &v2(gp->getVertex(i2));

      const GlyphPath::FlagBits &f1(gp->getFlag(i1));
      if (f1 & GlyphPath::ON_CURVE) {
        path.lineTo(v1.x, v1.y, 0.0f);
        next_point_on_curve = true;
      } else {
        path.quadBezierTo(
          v0.x, v0.y, 0.0f,
          v1.x, v1.y, 0.0f,
          v2.x, v2.y, 0.0f
        );
        next_point_on_curve = false;
      }
    }

    path.close();
  }
}

/* -------------------------------------------------------------------------- */

void ofxGlyph::extractMeshData(
  int                     subsamples,
  bool                    enable_segments_sampling,
  std::vector<ofPoint>    &vertices,
  std::vector<glm::ivec2> &segments,
  std::vector<ofPoint>    &holes
)
{
  vertices.clear();
  segments.clear();
  holes.clear();

  int first_index = 0;
  int vertex_index = first_index;
  for (int i = 0; i < fs_glyph_->getNumPaths(); ++i) {
    GlyphPath::Sampling_t sampling;
    auto const *path(fs_glyph_->getPath(i));

    path->sample(
      sampling, 
      subsamples, 
      enable_segments_sampling
    );
    
    ofPoint centroid(0.0f, 0.0f);
    const int num_vertices = sampling.vertices.size();
    for (const auto& v : sampling.vertices) 
    {
      // Vertex.
      ofPoint vertex(v.x, v.y);
      vertices.push_back(vertex);
      centroid += vertex;

      // Segment indices.
      const int next_index = first_index + (vertex_index+1 - first_index) % num_vertices;
      segments.push_back(glm::ivec2(vertex_index++, next_index));
    }
    centroid *= 1.0f / num_vertices;
    first_index = vertex_index;

    // Holes.
    if (fs_glyph_->isInnerPath(i)) {
      holes.push_back(centroid);
    } else {
      outer_sampling_ = sampling;
    }
  }
}

/* -------------------------------------------------------------------------- */

void ofxGlyph::constructContourPolyline(
  int samples,
  ofPolyline &pl
) const
{
  const float sampling_step = 1.0f / samples;

  pl.clear();
  for (int i = 0; i < samples; ++i) {
    const float t = i * sampling_step;
    const auto v  = outer_sampling_.evaluate(t);
    ofPoint vertex(v.x, v.y);
    pl.addVertex(vertex);
  }
  pl.addVertex(pl.getVertices()[0]);
}

/* -------------------------------------------------------------------------- */

void ofxGlyph::extractMeshData(
  int                     subsamples,
  bool                    enable_segments_sampling,
  std::vector<ofPoint>    &vertices,
  std::vector<glm::ivec2> &segments,
  std::vector<ofPoint>    &holes,

  gradientScaleFunc_t     gradientScaling,
  int                     gradient_step
)
{
  extractMeshData(subsamples, enable_segments_sampling, vertices, segments, holes);

  // Postprocess the vertex data using noise.
  std::vector<ofPoint> points(vertices.size());

  int prev_vertex_index = -1;
  int first_vertex_index = -1;
  int last_vertex_index = -1;
  
  const int num_segments = static_cast<int>(segments.size());
  for (int i=0; i < num_segments; ++i) {
    const auto &s = segments[i];

    // if new path : look for the last segment looping the path.
    if (prev_vertex_index != s.x) {
      first_vertex_index = s.x;
      int j;
      for (j=i+1; segments[j].y != first_vertex_index; ++j);
      last_vertex_index = segments[j].x;
    }
    prev_vertex_index = s.y;
    
    auto vertex = vertices[s.x];

    // Calculate normal.
    const int i0 = ofWrap(s.x-gradient_step, first_vertex_index, last_vertex_index);
    const int i1 = ofWrap(s.x+gradient_step, first_vertex_index, last_vertex_index);
    const auto normal = CalculateNormal(vertices[i0], vertices[i1]);
    vertex += gradientScaling(s.x, vertex) * normal;

    points[i] = (vertex);
  }

  vertices = points;
}

/* -------------------------------------------------------------------------- */

void ofxGlyph::constructContourPolyline(
  int                 samples, 
  ofPolyline          &pl,

  gradientScaleFunc_t gradientScaling,
  float               gradient_step_factor
)
{
  const float sampling_step = 1.0f / samples;
  const float gradient_sampling_step = gradient_step_factor * sampling_step;
  
  pl.clear();
  for (int i = 0; i < samples; ++i) {
    const float t = i * sampling_step;

    const vertex_t& v = outer_sampling_.evaluate(t);
    ofPoint vertex(v.x, v.y);

    // Calculate normal.
    const auto v0 = outer_sampling_.evaluate(t - gradient_sampling_step);
    const auto v1 = outer_sampling_.evaluate(t + gradient_sampling_step);
    const ofPoint vertex0(v0.x, v0.y);
    const ofPoint vertex1(v1.x, v1.y);
    const ofPoint normal = CalculateNormal(vertex0, vertex1);
    vertex += gradientScaling(i, vertex) * normal;

    pl.addVertex(vertex);
  }
  // Close the polyline.
  pl.addVertex(pl.getVertices()[0]);
}

/* -------------------------------------------------------------------------- */
