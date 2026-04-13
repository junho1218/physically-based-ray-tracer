#ifndef __INTEGRATOR_H_
#define __INTEGRATOR_H_

#include "neon/blueprint.hpp"

#include <glm/glm.hpp>
#include <memory>
#include <vector>

namespace ne {

namespace core {

class Integrator {
public:
  // integration part of rendering equation
  virtual glm::vec3 integrate(const ne::Ray &ray,
                              std::shared_ptr<ne::Scene> scene, int ray_depth, std::vector<ne::RendablePointer> &dls_objects);
};

} // namespace core

} // namespace ne

#endif // __INTEGRATOR_H_
