#include "ofApp.h"

//------------------------------------------------------------------------------

namespace {

/* Return a linear value in range [0,1] every delay (in seconds). */
float Tick(float delay) {
  return fmodf(ofGetElapsedTimeMillis()/1000.0f, delay) / delay;
}

/* Noise function used by the gradient scaling. */
float Noise(const ofPoint &vertex) {
  return 24.0f * ofNoise(0.005f*vertex + 0.5f*ofGetElapsedTimeMillis()*0.0002f);
}

}  // namespace

//------------------------------------------------------------------------------

void ofApp::setup()
{
  ofSetFrameRate(60);

  const float fontsize = 0.75f * ofGetHeight();
  fontsampler_.setup("FreeSans.ttf", fontsize);

  start_letter_ = 'A';
  letter_index_ = 0;

  bPause_ = false;

  fontrenderer_ = new ofxFontRenderer(fontsampler_); //
}

void ofApp::update()
{
  const float dx = ofMap(ofGetMouseX(), 0, ofGetWidth(), 0.01f, 1.0f);
  const float dy = ofMap(ofGetMouseY(), 0, ofGetHeight(), 0.2f, 1.0f);

  // Functor used to change the glyph vertices along its normals as we sample it.
  ofxGlyph::gradientScaleFunc_t gradientScaling = [dy](int id, const ofPoint &v) { 
    return dy * Noise(dy * v); 
  };

  // -----------------

  if (!bPause_) {
    float delay = 1.15f;
    static float last_tick = Tick(delay);

    float tick = Tick(delay);
    if (tick < last_tick) {
      letter_index_ = (letter_index_ +1) % 26;
    }
    last_tick = tick;
  }

  glyph_ = fontsampler_.get(u'A' + letter_index_);
  if (nullptr == glyph_) {
    ofLog(OF_LOG_FATAL_ERROR, "Glyph not found.");
    ofExit();
  }

  // -----------------

  // Extract an OFX Path to be rendered.
  glyph_->extractPath(path_);

  // Evaluate a glyph path and extract its mesh data for triangulation.
  glyph_->extractMeshData(
    5+dx*5,                           // sub samples count (per curves)
    true,                             // Enable segment subsampling
    vertices_, segments_, holes_, 
    gradientScaling,
    4                                 // gradient step
  );
  contour_.sampling = glyph_->outer_sampling_; //

  // Triangulate & generate Voronoi diagram for the glyph.
  trimesh_.triangulateConstrainedDelaunay(vertices_, segments_, holes_, 24, 620); 
  trimesh_.generateVoronoiDiagram();

  // Sample and transform the glyph to get a contour polyline.
  /// @note : 
  /// You can create the polyline from the extracted mesh data directly, 
  /// but here we will use the GlyphPath::Sampling_t generated to resample it.
  /// This is done internally using constructContourPolyline but you can
  /// retrieve the object to modify it as you see fit.
  glyph_->constructContourPolyline(
    128+dx*256,                      // samples count (for the whole path)
    contour_.polyline, 
    gradientScaling,    
    25.6f                            // gradient step factor
  );

  // -----------------

  fontrenderer_->update(u"fontsampler", gradientScaling);
}

void ofApp::draw()
{
#if 1
  //ofColor bg(190,210,182);
  ofColor bg(50, 50, 50);

  ofBackground(bg);

  // Center the glyph.
  const auto& centroid = glyph_->getCentroid();
  ofTranslate(
    (0.5f * ofGetWidth() - centroid.x),
    (0.5f * ofGetHeight() - centroid.y), 
    0.0f
  );

  // triangle mesh.
  ofSetColor(255, 105, 30);
  trimesh_.draw(false);

  // Contour.
  // ofSetColor(50, 20, 25);
  // contour_.polyline.draw();  

  // Path
  path_.setStrokeWidth(3.0f);
  path_.setStrokeColor(ofColor(255, 155, 30));
  path_.setFillColor(ofColor(190,180,122));
  path_.draw();

  // Voronoi.
  ofSetColor(255, 255, 130);
  trimesh_.drawCleanVoronoi(vertices_);

  // Animated circle around the glyph contour.
  ofSetColor(250, 80, 75);
  const auto v = contour_.sampling.evaluate(Tick(20.0f));
  ofDrawCircle(v.x, v.y, 16);
#else
  fontrenderer_->draw();
#endif
}

void ofApp::keyPressed(int key)
{
  if (key == ' ') {
    bPause_ = !bPause_;
  }
}    

void ofApp::mousePressed(int x, int y, int button)
{
  if (button == 0) {
    letter_index_ = ofWrap(letter_index_+1, 0, 26);
  }
  if (button == 2) {
    letter_index_ = ofWrap(letter_index_-1, 0, 26);
  }
}

/* -------------------------------------------------------------------------- */
