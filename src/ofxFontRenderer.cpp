
#include "ofxFontRenderer.h"


/* -------------------------------------------------------------------------- */

void ofxFontRenderer::update(
  const std::u16string &s,
  ofxGlyph::gradientScaleFunc_t gradientScaling
)
{
  const float dx = ofMap(ofGetMouseX(), 0, ofGetWidth(), 0.0f, 1.0f);
  const float dy = ofMap(ofGetMouseY(), 0, ofGetHeight(), 0.2f, 1.0f);

  string_ = s;

  for (const auto &c : s) {
    // Create the glyph object if needed.
    if (meshes_.cend() == meshes_.find(c)) 
    {
      auto *glyph = fontsampler_.get(c);
      GlyphMesh *gm = new GlyphMesh();
      meshes_[c]= gm;
     
      gm->glyph = glyph;

      // Extract an OFX Path to be rendered.
      glyph->extractPath(gm->path);
    }

    // Update the glyph if found.
    if (auto *glyph = fontsampler_.get(c)) 
    {
      GlyphMesh *gm = meshes_[c];
     
      // Evaluate a glyph path and extract its mesh data for triangulation.
      glyph->extractMeshData(
        8,                           // sub samples count (per curves)
        true,                             // Enable segment subsampling
        vertices_, segments_, holes_, 
        gradientScaling,
        4                               // gradient step
      );

      // Triangulate & generate Voronoi diagram for the glyph.
      gm->mesh.triangulateConstrainedDelaunay(
        vertices_, segments_, holes_, 24, 620
      ); 
      gm->mesh.generateVoronoiDiagram();
    }
  }
}

/* -------------------------------------------------------------------------- */

void ofxFontRenderer::draw()
{
  const float dx = ofMap(ofGetMouseX(), 0, ofGetWidth(), 0.0f, 1.0f);
  ofColor bgcolor(190,10,64);
  ofColor fgcolor(bgcolor);

  fgcolor.setBrightness(fgcolor.getBrightness()*0.94f);

  ofBackground(bgcolor);

  auto *g = (*meshes_[string_[0]]).glyph;
  ofTranslate(
    0.0f,
    (0.5f * ofGetHeight() - g->getCentroid().y), 
    0.0f
  );

  for (auto &c : string_) {
    const auto &ref = meshes_.find(c);
    if (ref != meshes_.end()) {
      auto *gm = ref->second;

      const float alpha = 0.5f*(c-'e'); 
      ofPushMatrix();
        auto *glyph = gm->glyph;
        const auto ct = glyph->getCentroid();
        
        // Rotate letter at its origin.
        ofTranslate( ct.x, ct.y, 0.0f);
        ofRotate(alpha);
        ofTranslate( -ct.x, -ct.y, 0.0f);

        // Rotate letter at its corner.
        ofRotate(alpha);

        // draw its perturbated trianglemesh
        ofSetColor(255, 105, 30);
        gm->mesh.triangulatedMesh.draw();//Wireframe();

        // draw its path.
        gm->path.setFillColor(fgcolor);
        gm->path.draw();
      ofPopMatrix();

      ofTranslate(gm->glyph->getMaxBound().x, 0, 0);    
    }  
  }
}

/* -------------------------------------------------------------------------- */