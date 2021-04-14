#pragma once

#include "ofMain.h"

#include <unordered_map>
#include "fontsampler/ttf_reader.h"

#include "ofxGlyph.h"

/* -------------------------------------------------------------------------- */

// ofxFontSampler is an interface to the fontsampler library used to sample and
// and render font glyphes.
//
// important :
// this whole OpenFramework library is higly "work in progress" and still in
// development.
//
class ofxFontSampler {
 public:
  static const std::u16string kDefaultChars;

  ofxFontSampler() = default;
  ~ofxFontSampler();
  
  void clear();

  /* Load a TrueType File as TypeFace with all default characters. */
  bool setup(const std::string &ttf_filename, float font_size);

  /* Return the ofxGlyph object of the given character. */
  ofxGlyph* get(uint16_t c);

 private:
  TTFReader ttf_;
  float scale_x_;
  float scale_y_;
  std::unordered_map<uint16_t, ofxGlyph*> glyphes_;
};

/* -------------------------------------------------------------------------- */
