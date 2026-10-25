
#pragma once

struct ShadeResult {
  bool valid = false;
  bool coverageTargetMet = false;

  float azimuthDeg = 0.0f;
  float elevationDeg = 0.0f;
  float coverage = 0.0f;
  float powerProxy = -1.0f;
};

struct ShadeRequest {
  float sunAzimuthDeg;
  float sunElevationDeg;
  float pivotHeightCm;
  float offsetUcm;
  float offsetVcm;
  float offsetNcm;
  float targetCenterXcm;
  float targetCenterYcm;
  float azimuthMinDeg;
  float azimuthMaxDeg;
  float elevationMinDeg;
  float elevationMaxDeg;
  float searchStepDeg;
};

class ShadeOptimizer {
 public:
  ShadeResult optimize(const ShadeRequest &request);

 private:
  static constexpr float PANEL_W_CM = 11.5f;
  static constexpr float PANEL_H_CM = 6.5f;
  static constexpr float TARGET_W_CM = 11.5f;
  static constexpr float TARGET_D_CM = 6.5f;
  static constexpr float MIN_COVERAGE = 0.70f;

  static constexpr float EPS = 1e-5f;

  struct Vec3 {
    float x, y, z;
  };

  struct Vec2 {
    float x, y;
  };

  struct Candidate {
    bool valid = false;
    float azimuthDeg = 0.0f;
    float elevationDeg = 0.0f;
    float coverage = 0.0f;
    float powerProxy = -1.0f;
  };

  static Vec3 add(Vec3 a, Vec3 b);
  static Vec3 scale(Vec3 v, float s);
  static float dot(Vec3 a, Vec3 b);
  static Vec3 cross(Vec3 a, Vec3 b);
  static Vec3 normalize(Vec3 v);

  static void makePanel(
      float azDeg,
      float elDeg,
      float pivotHeightCm,
      float offsetUcm,
      float offsetVcm,
      float offsetNcm,
      Vec3 vertices[4],
      Vec3& normal
  );

  static float computeCoverage(
      const Vec3 vertices[4],
      Vec3 sunDirection,
      float targetCenterXcm,
      float targetCenterYcm
  );

  static float clippedArea(
      const Vec2 polygon[],
      int count
  );

  static bool insideTarget(Vec2 p, int edge);
  static Vec2 intersectTargetEdge(
      Vec2 a,
      Vec2 b,
      int edge
  );

  static float clamp01(float value);
};
