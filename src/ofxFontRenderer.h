#pragma once

#include "ofMain.h"

#include <unordered_map>

#include "ofxFontSampler.h"
#include "ofxTriangleMesh.h"

///
/// ofxFontRenderer simplify how to draw a given style to a text. [WiP]
///
class ofxFontRenderer {
 public:
  struct GlyphMesh {
    ofxGlyph *glyph;
    ofxTriangleMesh mesh;
    ofPath path;
  };

  ofxFontRenderer(ofxFontSampler& fontsampler)
    : fontsampler_(fontsampler)
  {}

  void update(const std::u16string &s, 
              ofxGlyph::gradientScaleFunc_t gradientScaling);

  void draw();

 private:
  ofxFontSampler& fontsampler_;

  std::u16string string_;
  std::unordered_map<uint16_t, GlyphMesh*> meshes_;

  // Use to generated mesh data.
  std::vector<ofPoint> vertices_;
  std::vector<glm::ivec2> segments_;
  std::vector<ofPoint> holes_;
};