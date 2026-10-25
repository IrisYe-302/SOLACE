
#include "ShadeOptimizer.h"
#include <math.h>

namespace {
  constexpr int MAX_POLY_VERTICES = 12;
  constexpr float kDegreesToRadians = 0.017453292519943295f;
}

ShadeOptimizer::Vec3 ShadeOptimizer::add(Vec3 a, Vec3 b) {
  return {a.x + b.x, a.y + b.y, a.z + b.z};
}

ShadeOptimizer::Vec3 ShadeOptimizer::scale(Vec3 v, float s) {
  return {v.x * s, v.y * s, v.z * s};
}

float ShadeOptimizer::dot(Vec3 a, Vec3 b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

ShadeOptimizer::Vec3 ShadeOptimizer::cross(Vec3 a, Vec3 b) {
  return {
    a.y * b.z - a.z * b.y,
    a.z * b.x - a.x * b.z,
    a.x * b.y - a.y * b.x
  };
}

ShadeOptimizer::Vec3 ShadeOptimizer::normalize(Vec3 v) {
  float length = sqrtf(dot(v, v));

  if (length < EPS) {
    return {0.0f, 0.0f, 0.0f};
  }

  return scale(v, 1.0f / length);
}

float ShadeOptimizer::clamp01(float value) {
  if (value < 0.0f) return 0.0f;
  if (value > 1.0f) return 1.0f;
  return value;
}

// azDeg: azimuth of panel normal in the ground plane.
// elDeg: elevation of panel normal above the ground.
//
// Local u = panel width direction.
// Local v = panel height direction.
// Local n = panel normal.
//
// The pivot is (0, 0, pivotHeightCm).
// Offsets are expressed in the rotating local panel frame.
void ShadeOptimizer::makePanel(
    float azDeg,
    float elDeg,
    float pivotHeightCm,
    float offsetUcm,
    float offsetVcm,
    float offsetNcm,
    Vec3 vertices[4],
    Vec3& normal
) {
  float az = azDeg * kDegreesToRadians;
  float el = elDeg * kDegreesToRadians;

  normal = normalize({
    cosf(el) * cosf(az),
    cosf(el) * sinf(az),
    sinf(el)
  });

  Vec3 u = normalize({
    -sinf(az),
     cosf(az),
     0.0f
  });

  Vec3 v = normalize(cross(normal, u));

  Vec3 pivot = {0.0f, 0.0f, pivotHeightCm};

  Vec3 center = add(
    add(
      add(pivot, scale(u, offsetUcm)),
      scale(v, offsetVcm)
    ),
    scale(normal, offsetNcm)
  );

  const float halfW = PANEL_W_CM * 0.5f;
  const float halfH = PANEL_H_CM * 0.5f;

  const float signsU[4] = {-1.0f, 1.0f, 1.0f, -1.0f};
  const float signsV[4] = {-1.0f, -1.0f, 1.0f, 1.0f};

  for (int i = 0; i < 4; ++i) {
    vertices[i] = add(
      add(center, scale(u, signsU[i] * halfW)),
      scale(v, signsV[i] * halfH)
    );
  }
}

bool ShadeOptimizer::insideTarget(Vec2 p, int edge) {
  const float halfW = TARGET_W_CM * 0.5f;
  const float halfD = TARGET_D_CM * 0.5f;

  switch (edge) {
    case 0: return p.x >= -halfW; // Left
    case 1: return p.x <=  halfW; // Right
    case 2: return p.y >= -halfD; // Bottom
    default:return p.y <=  halfD; // Top
  }
}

ShadeOptimizer::Vec2 ShadeOptimizer::intersectTargetEdge(
    Vec2 a,
    Vec2 b,
    int edge
) {
  const float halfW = TARGET_W_CM * 0.5f;
  const float halfD = TARGET_D_CM * 0.5f;

  Vec2 result = a;

  if (edge <= 1) {
    float xBoundary = (edge == 0) ? -halfW : halfW;
    float dx = b.x - a.x;

    if (fabsf(dx) < EPS) {
      result.x = xBoundary;
      result.y = a.y;
    } else {
      float t = (xBoundary - a.x) / dx;
      result.x = xBoundary;
      result.y = a.y + t * (b.y - a.y);
    }
  } else {
    float yBoundary = (edge == 2) ? -halfD : halfD;
    float dy = b.y - a.y;

    if (fabsf(dy) < EPS) {
      result.x = a.x;
      result.y = yBoundary;
    } else {
      float t = (yBoundary - a.y) / dy;
      result.x = a.x + t * (b.x - a.x);
      result.y = yBoundary;
    }
  }

  return result;
}

// Shoelace formula for polygon area.
float ShadeOptimizer::clippedArea(
    const Vec2 polygon[],
    int count
) {
  if (count < 3) return 0.0f;

  float twiceArea = 0.0f;

  for (int i = 0; i < count; ++i) {
    int j = (i + 1) % count;
    twiceArea += polygon[i].x * polygon[j].y
               - polygon[j].x * polygon[i].y;
  }

  return fabsf(twiceArea) * 0.5f;
}

float ShadeOptimizer::computeCoverage(
    const Vec3 vertices[4],
    Vec3 sunDirection,
    float targetCenterXcm,
    float targetCenterYcm
) {
  // sunDirection points from the panel toward the sun.
  // Light rays travel in the opposite direction.
  if (sunDirection.z <= EPS) {
    return 0.0f;
  }

  Vec2 projected[4];

  for (int i = 0; i < 4; ++i) {
    if (vertices[i].z <= EPS) {
      return 0.0f;
    }

    float t = vertices[i].z / sunDirection.z;

    // Translate into the target rectangle's local frame before clipping.
    projected[i].x =
        vertices[i].x - t * sunDirection.x - targetCenterXcm;
    projected[i].y =
        vertices[i].y - t * sunDirection.y - targetCenterYcm;
  }

  // Clip the projected panel polygon against the target rectangle.
  Vec2 bufferA[MAX_POLY_VERTICES];
  Vec2 bufferB[MAX_POLY_VERTICES];

  int countA = 4;
  for (int i = 0; i < 4; ++i) {
    bufferA[i] = projected[i];
  }

  for (int edge = 0; edge < 4; ++edge) {
    if (countA == 0) return 0.0f;

    int countB = 0;

    Vec2 previous = bufferA[countA - 1];
    bool previousInside = insideTarget(previous, edge);

    for (int i = 0; i < countA; ++i) {
      Vec2 current = bufferA[i];
      bool currentInside = insideTarget(current, edge);

      if (currentInside != previousInside &&
          countB < MAX_POLY_VERTICES) {
        bufferB[countB++] =
            intersectTargetEdge(previous, current, edge);
      }

      if (currentInside && countB < MAX_POLY_VERTICES) {
        bufferB[countB++] = current;
      }

      previous = current;
      previousInside = currentInside;
    }

    countA = countB;

    for (int i = 0; i < countA; ++i) {
      bufferA[i] = bufferB[i];
    }
  }

  float overlap = clippedArea(bufferA, countA);
  float targetArea = TARGET_W_CM * TARGET_D_CM;

  return clamp01(overlap / targetArea);
}

ShadeResult ShadeOptimizer::optimize(const ShadeRequest &request) {
  ShadeResult result;

  // Reject invalid or below-horizon sun estimates.
  if (!isfinite(request.sunAzimuthDeg) ||
      !isfinite(request.sunElevationDeg) ||
      !isfinite(request.pivotHeightCm) ||
      !isfinite(request.offsetUcm) ||
      !isfinite(request.offsetVcm) ||
      !isfinite(request.offsetNcm) ||
      !isfinite(request.targetCenterXcm) ||
      !isfinite(request.targetCenterYcm) ||
      !isfinite(request.azimuthMinDeg) ||
      !isfinite(request.azimuthMaxDeg) ||
      !isfinite(request.elevationMinDeg) ||
      !isfinite(request.elevationMaxDeg) ||
      !isfinite(request.searchStepDeg) ||
      request.sunElevationDeg <= 0.0f ||
      request.sunElevationDeg > 90.0f ||
      request.pivotHeightCm <= 0.0f ||
      request.azimuthMinDeg > request.azimuthMaxDeg ||
      request.elevationMinDeg > request.elevationMaxDeg ||
      request.searchStepDeg <= 0.0f) {
    return result;
  }

  const float sunAz = request.sunAzimuthDeg * kDegreesToRadians;
  const float sunEl = request.sunElevationDeg * kDegreesToRadians;

  Vec3 sunDirection = normalize({
    cosf(sunEl) * cosf(sunAz),
    cosf(sunEl) * sinf(sunAz),
    sinf(sunEl)
  });

  Candidate bestFeasible;
  Candidate bestShade;

  // Keep the best-shade fallback separate from feasible poses.
  bestShade.valid = false;
  bestShade.coverage = -1.0f;
  bestShade.powerProxy = -1.0f;

  bestFeasible.valid = false;
  bestFeasible.powerProxy = -1.0f;

  for (float az = request.azimuthMinDeg;
       az <= request.azimuthMaxDeg + EPS;
       az += request.searchStepDeg) {

    for (float el = request.elevationMinDeg;
         el <= request.elevationMaxDeg + EPS;
         el += request.searchStepDeg) {

      Vec3 vertices[4];
      Vec3 normal;

      makePanel(
        az, el,
        request.pivotHeightCm,
        request.offsetUcm, request.offsetVcm, request.offsetNcm,
        vertices, normal
      );

      float coverage = computeCoverage(
        vertices, sunDirection,
        request.targetCenterXcm, request.targetCenterYcm
      );

      // Direct-sun alignment proxy.
      float powerProxy = dot(normal, sunDirection);

      Candidate candidate;
      candidate.valid = true;
      candidate.azimuthDeg = az;
      candidate.elevationDeg = el;
      candidate.coverage = coverage;
      candidate.powerProxy = powerProxy;

      // Fallback objective: maximize coverage first.
      if (!bestShade.valid ||
          candidate.coverage > bestShade.coverage + EPS ||
          (fabsf(candidate.coverage - bestShade.coverage) <= EPS &&
           candidate.powerProxy > bestShade.powerProxy)) {
        bestShade = candidate;
      }

      // Feasible objective: among poses reaching 70%,
      // maximize the direct-sun alignment proxy.
      if (candidate.coverage + EPS >= MIN_COVERAGE) {
        if (!bestFeasible.valid ||
            candidate.powerProxy > bestFeasible.powerProxy) {
          bestFeasible = candidate;
        }
      }
    }
  }

  Candidate chosen;

  if (bestFeasible.valid) {
    chosen = bestFeasible;
    result.coverageTargetMet = true;
  } else if (bestShade.valid) {
    chosen = bestShade;
    result.coverageTargetMet = false;
  } else {
    return result;
  }

  result.valid = true;
  result.azimuthDeg = chosen.azimuthDeg;
  result.elevationDeg = chosen.elevationDeg;
  result.coverage = chosen.coverage;
  result.powerProxy = chosen.powerProxy;

  return result;
}
