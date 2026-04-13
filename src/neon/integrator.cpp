#include "integrator.hpp"
#include "neon/intersection.hpp"
#include "neon/material.hpp"
#include "neon/scene.hpp"

namespace ne {

namespace core {

glm::vec3 Integrator::integrate(const ne::Ray& ray,
    std::shared_ptr<ne::Scene> scene, int ray_depth, std::vector<ne::RendablePointer> &dls_objects) {
    glm::vec3 L{ 0.0f };
    ne::Ray r{ ray };
    ne::Intersection hit;

    if (ray_depth <= 0)
        return L;

    // if ray hit object in scene, apply shading
    if (scene->rayIntersect(r, hit)) {
        RendablePointer object = hit.object;
        MaterialPointer material = hit.material;

        // avoid double counting
        if (std::find(dls_objects.begin(), dls_objects.end(), object) == dls_objects.end())
            L += material->emitted();

        ne::Ray out_ray;
        if (hit.material->scatter(r, hit, out_ray)) {

            // Direct Light Sampling
            dls_objects.clear();
            glm::vec3 d_L = scene->sampleDirectLight(r, hit, dls_objects);
            if (d_L != glm::vec3(0.0f))
                L += d_L;

            const float pdf = material->pdf(r, hit, out_ray, true);
            if (pdf < 1e-6f) 
                return L;
            const glm::vec3 albedo = material->attenuation(r, hit, out_ray, true);
            //L += albedo * scene->sampleBackgroundLight(out_ray.dir) / pdf;
            L += albedo * this->integrate(out_ray, scene, ray_depth - 1, dls_objects) / pdf;
        }
    }

    // background color
    else
        L += scene->sampleBackgroundLight(r.dir);

    return L;
}

} // namespace core
} // namespace ne
