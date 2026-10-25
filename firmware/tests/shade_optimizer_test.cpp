#include <assert.h>
#include <math.h>

#include "../SOLACE/ShadeOptimizer.h"

int main() {
  ShadeOptimizer optimizer;
  ShadeRequest request{
      35.0f, 35.0f, 15.0f,
      0.0f, 0.0f, 0.0f,
      -17.6f, -12.3f,
      15.0f, 165.0f, 15.0f, 85.0f, 5.0f};

  const ShadeResult result = optimizer.optimize(request);
  assert(result.valid);
  assert(result.coverageTargetMet);
  assert(result.coverage > 0.95f);
  assert(result.azimuthDeg >= request.azimuthMinDeg);
  assert(result.azimuthDeg <= request.azimuthMaxDeg);
  assert(result.elevationDeg >= request.elevationMinDeg);
  assert(result.elevationDeg <= request.elevationMaxDeg);

  request.searchStepDeg = 0.0f;
  assert(!optimizer.optimize(request).valid);
  return 0;
}
