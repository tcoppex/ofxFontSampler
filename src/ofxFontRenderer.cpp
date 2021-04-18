
#include "ofxFontRenderer.h"


/* -------------------------------------------------------------------------- */

void ofxFontRenderer::update(
  const std::u16string &str,
  ofxGlyph::gradientScaleFunc_t gradientScaling
)
{
  const float dx = ofMap(ofGetMouseX(), 0, ofGetWidth(), 0.0f, 1.0f);
  const float dy = ofMap(ofGetMouseY(), 0, ofGetHeight(), 0.2f, 1.0f);

  string_ = str;

  for (const auto &glyph_car : string_) {
    ofxGlyph* glyph = nullptr;

    // Create the glyph object if needed.
    if (meshes_.cend() == meshes_.find(glyph_car)) {
      auto glyph_mesh = std::make_shared<ofxGlyphMesh>();
      meshes_[glyph_car] = glyph_mesh;

      if (glyph = fontsampler_.get(glyph_car); glyph) {
        glyph_mesh->glyph_ptr = glyph;
        glyph->extractPath(glyph_mesh->path);
      }
    } else {
      glyph = meshes_[glyph_car]->glyph_ptr;
    }

    // Update the glyph if found.
    if (nullptr != glyph) {
      auto glyph_mesh = meshes_[glyph_car];
     
      // Evaluate a glyph path and extract its mesh data for triangulation.
      glyph->extractMeshData(
        8,                                // sub samples count (per curves)
        true,                             // Enable segment subsampling
        vertices_, segments_, holes_, 
        gradientScaling,
        4                                 // gradient step
      );

      // Triangulate & generate Voronoi diagram for the glyph.
      {
        auto &mesh = glyph_mesh->face;
        mesh.triangulateConstrainedDelaunay(
          vertices_, segments_, holes_, 24, 620
        ); 
        mesh.generateVoronoiDiagram();
      }

      // Generate a tristrip for its extruded edge.
      {
        auto &mesh = glyph_mesh->edge;

        const float edge_width{ 1.0f }; //
        
        mesh.clearVertices();
        for (const auto& v : vertices_) {
          mesh.addVertex(ofPoint(v.x, v.y, v.z));
          mesh.addVertex(ofPoint(v.x, v.y, v.z - edge_width));
        }

        if (!mesh.hasIndices()) {
          mesh.setMode(OF_PRIMITIVE_TRIANGLES);
          for (const auto& s : segments_) {
            const auto i1 = 2*s.x;
            const auto i2 = 2*s.y;
            
            mesh.addIndex(i1 + 1);
            mesh.addIndex(i1);
            mesh.addIndex(i2 + 1);

            mesh.addIndex(i2 + 1);
            mesh.addIndex(i1);
            mesh.addIndex(i2);
          }
        }
      }

    }
  }
}

/* -------------------------------------------------------------------------- */

void ofxFontRenderer::draw()
{
  //const float dx = ofMap(ofGetMouseX(), 0, ofGetWidth(), 0.0f, 1.0f);
  
  const ofColor bgcolor(190, 10, 64);
  const ofColor fgcolor(bgcolor.getBrightness()*0.94f);

  ofBackground(bgcolor);

  // Center to first glyph pivot.
  if (!string_.empty()) {
    auto *g = meshes_[string_[0]]->glyph_ptr;
    ofTranslate(
      0.0f,
      0.5f * ofGetHeight() - g->getCentroid().y, 
      0.0f
    );
  }

  for (auto &glyph_car : string_) {
    const auto &ref = meshes_.find(glyph_car);

    if (ref != meshes_.end()) {
      auto gm = ref->second;
      const auto center    = gm->glyph_ptr->getCentroid();
      const auto max_bound = gm->glyph_ptr->getMaxBound();
      const float alpha    = 0.5f * (glyph_car - 'e'); 

      ofPushMatrix();
      {
        // Rotate letter at its origin.
        ofTranslate( center.x, center.y, 0.0f);
        ofRotate(alpha);
        ofTranslate( -center.x, -center.y, 0.0f);

        // Rotate letter at its corner.
        ofRotate(alpha);

        // Draw its perturbated trianglemesh.
        {
          // front side
          ofSetColor(255, 105, 130);
          gm->face.triangulatedMesh.drawWireframe();

          //if (extrusion_scale_ > glm::epsilon<float>()) 
          {
            // edge
            ofPushMatrix();
              ofScale( 1.0f, 1.0f, extrusion_scale_);
              ofSetColor(50, 55, 70);
              gm->edge.draw();
            ofPopMatrix();

            // back side
            ofTranslate( 0.0f, 0.0f, -extrusion_scale_); //
            ofSetColor(255, 175, 130);
            gm->face.triangulatedMesh.draw();
          }
        }

        // draw its path.
        // gm->path.setFillColor(fgcolor);
        // gm->path.draw();
      }
      ofPopMatrix();

      ofTranslate(max_bound.x, 0, 0);    
    }  
  }
}

/* -------------------------------------------------------------------------- */