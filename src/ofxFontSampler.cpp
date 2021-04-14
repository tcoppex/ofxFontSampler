
#include "ofxFontSampler.h"

/* -------------------------------------------------------------------------- */

const std::u16string ofxFontSampler::kDefaultChars = std::u16string(
  u" !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~"   
);

/* -------------------------------------------------------------------------- */

ofxFontSampler::~ofxFontSampler()
{
  clear();
}

/* -------------------------------------------------------------------------- */

bool ofxFontSampler::setup(const std::string &ttf_filename, float fontsize)
{
  scale_x_ = +fontsize;
  scale_y_ = -fontsize;

  const auto &path = ofToDataPath(ttf_filename);
  if (!ttf_.read(path.c_str())) {
    ofLog(OF_LOG_FATAL_ERROR, "Unable to read the TTF file " + path);
    return false;
  }

  // preload default characters.
  for (auto c : kDefaultChars) {
    get(c);
  }

  return true;
}

/* -------------------------------------------------------------------------- */

void ofxFontSampler::clear()
{
  for (auto &it : glyphes_) {
    delete it.second;
  }
  glyphes_.clear();
}

/* -------------------------------------------------------------------------- */

ofxGlyph* ofxFontSampler::get(uint16_t c)
{
  if (glyphes_.cend() == glyphes_.find(c)) {
    if (auto *ttf_glyph = ttf_.get_glyph_data(c)) { 
      Glyph *glyph = new Glyph(*ttf_glyph, scale_x_, scale_y_);
      glyphes_[c] = new ofxGlyph(glyph);
    }
  }
  return glyphes_[c];
}

/* -------------------------------------------------------------------------- */
