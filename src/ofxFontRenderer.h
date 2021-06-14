#pragma once

#include <unordered_map>
#include <memory>

#include "ofMain.h"
#include "ofxFontSampler.h"
#include "ofxTriangleMesh.h"



struct ofxGlyphMesh {
  ofxGlyph *glyph_ptr = nullptr;

  ofPath path;
  ofxTriangleMesh face;
  ofMesh edge;
};

///
/// ofxFontRenderer simplify how to draw a given style to a text. [WiP]
///
class ofxFontRenderer {
 public:
  static constexpr float kDefaultExtrusionScale = 1.0f;

 public:
  ofxFontRenderer(ofxFontSampler& fontsampler)
    : fontsampler_(fontsampler)
    , extrusion_scale_(kDefaultExtrusionScale)
  {}

  void update(const std::u16string &s, 
              ofxGlyph::updateVertexFunc_t updateVertex);

  void draw();

  float getExtrusionScale() const {
    return extrusion_scale_;
  }

  void setExtrusionScale(float scale) {
    extrusion_scale_ = scale;
  }

 private:
  ofxFontSampler& fontsampler_;

  std::u16string string_;
  std::unordered_map<uint16_t, std::shared_ptr<ofxGlyphMesh>> meshes_;

  // Use to generate mesh data.
  ofxTriangleMesh::Polygon_t polygon_;
  float extrusion_scale_;
};