#include "neon/material.hpp"
#include "neon/scene.hpp"
#include "neon/sphere.hpp"

#include <glm/gtx/string_cast.hpp>
#include <glm/gtc/random.hpp>
#include <iostream>

namespace ne {

void Scene::add(ne::RendablePointer object) {
  objects_.push_back(object);
  if (glm::length(object->material_->emitted()) > 0.0f) {
    lights_.push_back(object);
  }
}

bool Scene::rayIntersect(ne::Ray &ray, ne::Intersection &inter) {
  bool foundIntersection = false;

  // Do not change order between a || b.
  // sometimes result(a || b) != result (b || a) and this is the case!
  // this is because b will not evaluated if a is true in this statment.
  // e.g. a = a || b (b will not be evaluated because a is true)
  for (const auto o : objects_) {
    foundIntersection = o->rayIntersect(ray, inter) || foundIntersection;
  }

  return foundIntersection;
}

glm::vec3 Scene::sampleBackgroundLight(const glm::vec3 &dir) const {
  glm::vec3 unit = glm::normalize(dir);
  float t = 0.5f * (unit.y + 1.0f);
  return ((1.0f - t) * glm::vec3(1.0f) + t * glm::vec3(0.5, 0.5, 0.9));
}

glm::vec3 Scene::sampleDirectLight(const ne::Ray& r_in, ne::Intersection &hit, std::vector<ne::RendablePointer> &dls_objects) const {
    glm::vec3 L{ 0.0f };
    ne::Ray shadow_ray;
    ne::Intersection shadow_hit;

    for (const auto li : lights_) {
        auto l_sphere = std::dynamic_pointer_cast<ne::Sphere>(li);
        glm::vec3 lightDir = l_sphere->center_ - hit.p;
        float d2 = glm::dot(lightDir, lightDir);
        float d = std::sqrt(d2);

        float sinThetaMax = l_sphere->radius_ / d;
        float cosThetaMax = std::sqrt(glm::max(0.0f, 1.0f - sinThetaMax * sinThetaMax));
        float oneMinusCosThetaMax = 1.0f - cosThetaMax;

        float u1 = glm::linearRand(0.0f, 1.0f);
        float u2 = glm::linearRand(0.0f, 1.0f);

        float cosTheta = 1.0f - u1 * (1.0f - cosThetaMax);
        float sinTheta = std::sqrt(std::max(0.0f, 1.0f - cosTheta * cosTheta));
        float phi = 2.0f * glm::pi<float>() * u2;
        
        glm::vec3 dirLocal(
            sinTheta * cos(phi), 
            sinTheta * sin(phi), 
            cosTheta);

        glm::vec3 wc = glm::normalize(lightDir);
        glm::vec3 wi = glm::transpose(changeToLocal(wc)) * dirLocal;

        shadow_ray.dir = glm::normalize(wi);
        shadow_ray.o = hit.p + li->eps_ * shadow_ray.dir;
        
        if (!li->rayIntersect(shadow_ray, shadow_hit)) continue;

        bool foundIntersection = false;
        for (const auto o : objects_) {
            if (o == li)
                continue;
            if (o->rayIntersect(shadow_ray, shadow_hit)) {
                foundIntersection = true;
                break;
            }
        }
        if (!foundIntersection) {
            const glm::vec3 albedo = hit.material->attenuation(r_in, hit, shadow_ray, false);
            if (albedo == glm::vec3(0.0f)) continue;

            float pdf = 1.0f / (2.0f * glm::pi<float>() * oneMinusCosThetaMax);
            L += albedo * li->material_->emitted() / pdf;
            dls_objects.push_back(li);
        }
    }
    
    return L;
}

} // namespace ne
