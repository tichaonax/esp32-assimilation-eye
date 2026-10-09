#include "Gamma.h"

#include <math.h>

namespace eye {
namespace gamma {

float correct(float level) {
  if (!(level > 0.0f)) {  // also catches NaN
    return 0.0f;
  }
  if (level >= 1.0f) {
    return 1.0f;
  }
  return powf(level, EXPONENT);
}

}  // namespace gamma
}  // namespace eye
