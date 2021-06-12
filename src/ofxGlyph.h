#pragma once

#include <functional>
#include "fontsampler/glyph.h"

#include "ofMain.h"

/* -------------------------------------------------------------------------- */

class ofxGlyph {
 public:
  using updateVertexFunc_t = std::function<void(ofPoint&, int, const ofPoint&)>; 

  explicit ofxGlyph(Glyph *glyph);
  ~ofxGlyph();

  /* Getter for the bounding box. */
  ofPoint getMinBound() const;
  ofPoint getMaxBound() const;

  /* Centroid of the glyph. */
  ofPoint getCentroid() const;

  /* Transform the glyph into an openframework path object.*/
  void extractPath(ofPath &path) const;

  /* Extract the data needed to triangulate the mesh of the glyph.
   * @param subsamples : number of subsample to use per curves.
   * @param enable_segments_sampling : if true, subsample both curve and segments,
   *        otherwise only the curve are and segment only keep their two vertices.
   * @param vertices : output buffer for the sampled 2d vertices.
   * @param segments : output buffer for the vertex indices of each sub-segments.
   * @param holes : output buffer for the centroid of each holes in the resulting polygon. */
  void extractMeshData(
    int                     subsamples,
    bool                    enable_segments_sampling,
    std::vector<ofPoint>    &vertices,
    std::vector<glm::ivec2> &segments,
    std::vector<ofPoint>    &holes
  );

  /* Construct a polyline from the glyph sampling using samples. 
   * @param samples : number of sample to use.
   * @param pl : polyline to construct. */
  void constructContourPolyline(int samples, ofPolyline &pl) const;
  

  // ---------------------------------------------------------------
  // Helpers methods that act like the previous ones but allows
  // the user to specify a functor to modify the vertice along the
  // vertex normal.
  // @param gradientScaling : functor taking a vertex index, a vertex and a contour normal as parameters 
  //  and returning a new vertex.
  // @param gradient_step : distance used to calculated the vertex normal.
  // ---------------------------------------------------------------
  
  void extractMeshData(
    int                     subsamples,
    bool                    enable_segments_sampling,
    std::vector<ofPoint>    &vertices,
    std::vector<glm::ivec2> &segments,
    std::vector<ofPoint>    &holes,
    int                     gradient_step,
    updateVertexFunc_t      updateVertex
  );

  void constructContourPolyline(
    int                   samples,
    ofPolyline            &pl,
    float                 gradient_step_factor,
    updateVertexFunc_t    updateVertex
  );

 public:
  GlyphPath const* outer_path;
  GlyphPath::Sampling_t outer_sampling_; //

 private:
   Glyph const* fs_glyph_;
};

/* -------------------------------------------------------------------------- */