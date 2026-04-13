#ifndef __SPHERE_H_
#define __SPHERE_H_

#include "neon/blueprint.hpp"
#include "neon/rendable.hpp"

namespace ne {

class Sphere final : public ne::abstract::Rendable, public std::enable_shared_from_this<Sphere> {
public:
  explicit Sphere(glm::vec3 c, float r, MaterialPointer m = nullptr)
      : ne::abstract::Rendable(m), center_(c), radius_(r) {}

  bool rayIntersect(ne::Ray &ray, Intersection &hit) override;

  glm::vec3 center_;
  float radius_;

  std::shared_ptr<Sphere> getSelf() {
      return shared_from_this();
  }

  std::shared_ptr<ne::abstract::Rendable> getBaseSelf() {
      return shared_from_this();
  }

};

} // namespace ne
#endif // __SPHERE_H_
