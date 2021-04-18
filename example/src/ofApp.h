#pragma once

#include "ofMain.h"
#include "ofxTriangleMesh.h"

#include "ofxFontSampler.h"
#include "ofxFontRenderer.h"

class ofApp : public ofBaseApp {
	public:
		void setup();
		void update();
		void draw();

		void keyPressed(int key);
		void mousePressed(int x, int y, int button);
		
  private:
  	ofxFontSampler fontsampler_;

  	uint16_t start_letter_;
  	uint16_t letter_index_;

  	bool bPause_;
    int32_t demo_index_;

  	// ----

  	ofPath path_;

  	// Used to triangulate a glyph.
 		std::vector<ofPoint> vertices_;
		std::vector<glm::ivec2> segments_;
		std::vector<ofPoint> holes_;
		
  	ofxGlyph *glyph_;
		ofxTriangleMesh trimesh_;

		struct {
	 		GlyphPath::Sampling_t sampling; //
	  	ofPolyline polyline;	
		} contour_;

  	// ----

  	ofxFontRenderer *fontrenderer_;
};
