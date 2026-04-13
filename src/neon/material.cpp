#include "neon/material.hpp"
#include "neon/scene.hpp"
#include <glm/gtc/constants.hpp>
#include <glm/gtc/random.hpp>
#include <random>

namespace ne {

// Transfrom world coord to shading coord. 
glm::mat3 changeToLocal(glm::vec3 normal) {
    glm::vec3 up = std::abs(normal.y) < 0.999f ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
    glm::vec3 t = glm::normalize(glm::cross(up, normal));
    glm::vec3 b = glm::cross(normal, t);
    glm::mat3 TBN(t, b, normal);

    return glm::transpose(TBN);
}

bool DiffuseLight::scatter(const ne::Ray &r_in, const ne::Intersection &hit,
                           ne::Ray &r_out) const {
  return false;
}

glm::vec3 DiffuseLight::emitted() const {
    return this->color_;
}

glm::vec3 DiffuseLight::attenuation(const ne::Ray& r_in, const ne::Intersection& hit, const ne::Ray& r_out, bool is_scatterd) const {
  return glm::vec3(0.0f);
}

bool Dielectric::scatter(const ne::Ray &r_in, const ne::Intersection &hit,
                         ne::Ray &r_out) const {

    // Calculate Fresnel Equation.
    glm::vec3 normal = glm::normalize(hit.n);
    float cosTheta_i = glm::dot(-r_in.dir, normal);
    cosTheta_i = glm::clamp(cosTheta_i, -1.0f, 1.0f);

    float IOR_in = 1.0f;
    float IOR_out = this->IOR_;

    if (cosTheta_i < 0.0f) {
        std::swap(IOR_in, IOR_out);
        cosTheta_i = -cosTheta_i;
        normal = -normal;
    }

    float eta = IOR_in / IOR_out;

    float sin2Theta_i = 1.0f - cosTheta_i * cosTheta_i;
    float sin2Theta_t = sin2Theta_i * (eta * eta);
    // Total Reflection
    if (sin2Theta_t >= 1.0f) {
        r_out.dir = glm::normalize(glm::reflect(r_in.dir, normal));
        r_out.o = hit.p + 1e-4f * r_out.dir;
        return true;
    }
    float cosTheta_t = std::sqrt(std::max(0.0f, 1.0f - sin2Theta_t));

    float r_s = (IOR_in * cosTheta_i - IOR_out * cosTheta_t) / (IOR_in * cosTheta_i + IOR_out * cosTheta_t);
    float r_p = (IOR_out * cosTheta_i - IOR_in * cosTheta_t) / (IOR_out * cosTheta_i + IOR_in * cosTheta_t);

    float R = (r_s * r_s + r_p * r_p) / 2.0f;

    // Reflection
    if (glm::linearRand(0.0f, 1.0f) <= R) {
        r_out.dir = glm::normalize(glm::reflect(r_in.dir, normal));
        r_out.o = hit.p + 1e-4f * r_out.dir;
    } 
    // Transmission
    else {
        r_out.dir = glm::normalize(glm::refract(r_in.dir, normal, eta));
        r_out.o = hit.p + 1e-4f * r_out.dir;
    }
    return true;
}

glm::vec3 Dielectric::attenuation(const ne::Ray& r_in, const ne::Intersection& hit, const ne::Ray& r_out, bool is_scatterd) const {
    if (!is_scatterd)
        return glm::vec3(0.0f);

    return glm::vec3(1.0f);
}

float Dielectric::pdf(const ne::Ray& r_in, const ne::Intersection& hit, const ne::Ray& r_out, bool is_scatterd) const {
    if (!is_scatterd)
        return 0.0f;

    return 1.0f;
}

bool Lambertian::scatter(const ne::Ray& r_in, const ne::Intersection& hit,
    ne::Ray& r_out) const {

    // Sample Disk Uniform Concentric.
    float u1 = glm::linearRand(0.0f, 1.0f);
    float u2 = glm::linearRand(0.0f, 1.0f);

    float uOffsetx = 2 * u1 - 1;
    float uOffsety = 2 * u2 - 1;

    float r, theta;

    if (uOffsetx == 0 && uOffsety == 0) {
        r = 0;
        theta = 0;
    }
    else if (std::abs(uOffsetx) > std::abs(uOffsety)) {
        r = uOffsetx;
        theta = (glm::pi<float>() / 4) * (uOffsety / uOffsetx);
    }
    else {
        r = uOffsety;
        theta = (glm::pi<float>() / 2) - (glm::pi<float>() / 4)* (uOffsetx / uOffsety);
    }

    // Project random sample points on uniform disk up to the unit hemisphere.
    float x = r * cos(theta);
    float y = r * sin(theta);
    float z = sqrt(std::max(0.0f, 1.0f - x * x - y * y));

    // Change to world coordinate system.
    glm::vec3 rand_dir(x, y, z);
    rand_dir = glm::transpose(changeToLocal(hit.n))* rand_dir;

    r_out.dir = glm::normalize(rand_dir);
    r_out.o = hit.p + 1e-4f * r_out.dir;

    return true;
}

glm::vec3 Lambertian::attenuation(const ne::Ray& r_in, const ne::Intersection& hit, const ne::Ray& r_out, bool is_scatterd) const {
    return color_ / glm::pi<float>() * glm::dot(hit.n, r_out.dir);
}

float Lambertian::pdf(const ne::Ray& r_in, const ne::Intersection& hit, const ne::Ray& r_out, bool is_scatterd) const {
    return glm::dot(hit.n, r_out.dir) / glm::pi<float>();
}

bool Metal::scatter(const ne::Ray& r_in, const ne::Intersection& hit,
    ne::Ray& r_out) const {

    // Sampling the Distribution of Visible Normals.
    float alpha = roughness_ * this->roughness_;
    glm::vec3 wo = changeToLocal(hit.n) * -r_in.dir;
    glm::vec3 wh = glm::normalize(glm::vec3(alpha * wo.x, alpha * wo.y, wo.z));
    if (wh.z < 0) wh = -wh;

    glm::vec3 T1 = (wh.z < 0.999f) ? glm::normalize(glm::cross(glm::vec3(0, 0, 1), wh)) : glm::vec3(1, 0, 0);
    glm::vec3 T2 = glm::cross(wh, T1);

    float u1 = glm::linearRand(0.0f, 1.0f);
    float u2 = glm::linearRand(0.0f, 1.0f);

    float r = std::sqrt(u1);
    float theta = 2.0f * glm::pi<float>() * u2;
    float x = r * cos(theta);
    float y = r * sin(theta);

    float h = std::sqrt(1 - x * x);
    float s = (1.0f + wh.z) / 2.0f;
    y = (1.0f - s) * h + s * y;
    float z = std::sqrt(std::max(0.0f, 1.0f - x * x - y * y));

    glm::vec3 nh = x * T1 + y * T2 + z * wh;
    nh = glm::normalize(glm::vec3(alpha * nh.x, alpha * nh.y, std::max(0.0f, nh.z)));

    glm::vec3 wi = glm::reflect(-wo, nh);
    if (wo.z * wi.z <= 0)
        return false;

    r_out.dir = glm::normalize(glm::transpose(changeToLocal(hit.n)) * wi);
    r_out.o = hit.p + 1e-4f * r_out.dir;

    return true;
}

glm::vec3 Metal::attenuation(const ne::Ray& r_in, const ne::Intersection& hit, const ne::Ray& r_out, bool is_scatterd) const {

    glm::vec3 wo = changeToLocal(hit.n) * -r_in.dir;
    glm::vec3 wi = changeToLocal(hit.n) * r_out.dir;

    float cosTheta_o = std::abs(wo.z);
    float cosTheta_i = std::abs(wi.z);
    if (cosTheta_i == 0 || cosTheta_o == 0)
        return glm::vec3(0.0f);

    // Get half vector h.
    glm::vec3 h = wo + wi;
    if (h == glm::vec3(0.0f))
        return glm::vec3(0.0f);
    h = glm::normalize(h);

    // Term calculate factors.
    float alpha = this->roughness_ * this->roughness_;;
    float Cos2Theta;
    float Tan2Theta;

    // D term calculate. (Trowbridge Reitz Distribution)
    float D;
    Cos2Theta = h.z * h.z;
    Tan2Theta = std::max(0.0f, 1.0f - Cos2Theta) / Cos2Theta;
    if (std::isinf(Tan2Theta))
        D = 0.0f;
    else {
        float Cos4Theta = Cos2Theta * Cos2Theta;
        float e = Tan2Theta / (alpha * alpha);
        D = 1.0f / (glm::pi<float>() * alpha * alpha * Cos4Theta * (1 + e) * (1 + e));
    }

    // F term calculate. (Schlick Approximation)
    glm::vec3 F;
    glm::vec3 F0 = this->color_;
    float cos = glm::clamp(glm::dot(wo, h), 0.0f, 1.0f);
    F = F0 + (1.0f - F0) * std::powf(1.0f - cos, 5);

    // G term calculate. (Masking Function)
    float G;
    float lambda1, lambda2;
    Cos2Theta = wo.z * wo.z;
    Tan2Theta = std::max(0.0f, 1.0f - Cos2Theta) / Cos2Theta;
    if (std::isinf(Tan2Theta)) lambda1 = 0.0f;
    else lambda1 = (sqrt(1 + alpha * alpha * Tan2Theta) - 1.0f) / 2.0f;
    Cos2Theta = wi.z * wi.z;
    Tan2Theta = std::max(0.0f, 1.0f - Cos2Theta) / Cos2Theta;
    if (std::isinf(Tan2Theta)) lambda2 = 0.0f;
    else lambda2 = (sqrt(1 + alpha * alpha * Tan2Theta) - 1.0f) / 2.0f;
    G = 1.0f / (1.0f + lambda1 + lambda2);

    // BRDF * cos(wi)
    return D * F * G / (4.0f * cosTheta_o);
}

float Metal::pdf(const ne::Ray& r_in, const ne::Intersection& hit, const ne::Ray& r_out, bool is_scatterd) const {

    glm::vec3 wo = changeToLocal(hit.n) * -r_in.dir;
    glm::vec3 wi = changeToLocal(hit.n) * r_out.dir;

    float cosTheta_o = std::abs(wo.z);
    float cosTheta_i = std::abs(wi.z);
    if (cosTheta_i == 0 || cosTheta_o == 0)
        return 0.0f;

    // Get half vector h.
    glm::vec3 h = wo + wi;
    if (h == glm::vec3(0.0f))
        return 0.0f;
    h = glm::normalize(h);

    // Term calculate factors.
    float alpha = this->roughness_ * this->roughness_;;
    float Cos2Theta;
    float Tan2Theta;

    // D term calculate.
    float D;
    Cos2Theta = h.z * h.z;
    Tan2Theta = std::max(0.0f, 1.0f - Cos2Theta) / Cos2Theta;
    if (std::isinf(Tan2Theta))
        D = 0;
    else {
        float Cos4Theta = Cos2Theta * Cos2Theta;
        float e = Tan2Theta / (alpha * alpha);
        D = 1.0f / (glm::pi<float>() * alpha * alpha * Cos4Theta * (1 + e) * (1 + e));
    }

    // G1 term calculate.
    float G1;
    float lambda1;
    Cos2Theta = wo.z * wo.z;
    Tan2Theta = std::max(0.0f, 1.0f - Cos2Theta) / Cos2Theta;
    if (std::isinf(Tan2Theta)) lambda1 = 0.0f;
    else lambda1 = (sqrt(1 + alpha * alpha * Tan2Theta) - 1.0f) / 2.0f;
    G1 = 1.0f / (1.0f + lambda1);

    return D * G1 * std::max(0.0f, glm::dot(wo,h)) / (4.0f * glm::dot(wo, h) * cosTheta_o);
}

} // namespace ne
