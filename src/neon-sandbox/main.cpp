#include "test.hpp"

#include "neon/camera.hpp"
#include "neon/image.hpp"
#include "neon/integrator.hpp"
#include "neon/ray.hpp"
#include "neon/scene.hpp"
#include "neon/sphere.hpp"
#include "neon/utils.hpp"

#include <glm/gtx/string_cast.hpp>
#include <iostream>
#include <memory>
#include <taskflow/taskflow.hpp>

int main(int argc, char *argv[]) {
  int nx = 1024;
  int ny = 512;
  int spp = 32;

  // create output image
  ne::Image canvas(nx, ny);
  glm::uvec2 tilesize(32, 32);

  // Split images into set of tiles.
  // Each thread render its corresponding tile.
  std::vector<ne::TileIterator> tiles = canvas.toTiles(tilesize);

  // create scene
  std::shared_ptr<ne::Scene> scene = testScene2();

  // spawn camera
  static ne::Camera camera;
  float distToFocus = 4;
  float aperture = 0.1f;
  glm::vec3 lookfrom(0, 0, 3);
  glm::vec3 lookat(0, 0, 0);
  camera = ne::Camera(lookfrom, lookat, glm::vec3(0, 1, 0), 60,
                      float(canvas.width()) / float(canvas.height()), aperture,
                      distToFocus);

  // summon progress bar. this is just eye candy.
  // you can use timer class instead
  ne::utils::Progressbar progressbar(canvas.numPixels());

  // prep to build task graph
  tf::Taskflow tf;
  tf::Task taskRenderStart =
      tf.emplace([&progressbar]() { progressbar.start(); });
  tf::Task taskRenderEnd = tf.emplace([&progressbar]() { progressbar.end(); });

  // build integrator
  ne::core::Integrator Li;

  // ########## antialiasing ##########
  // decide grid size of each pixels.
  int sample = static_cast<int>(std::sqrt(spp));
  for (sample; sample >= 1; sample--) {
      if (spp % sample == 0)
          break;
  }
  int ray_x = spp / sample;
  int ray_y = sample;
  // ########## antialiasing ##########

  // build rendering task graph
  for (auto &tile : tiles) {
      tf::Task taskTileRender = tf.emplace([&]() {
          // Iterate pixels in tile
          for (auto& index : tile) {
              glm::vec3 color{ 0.0f };

              // construct direct light sampling objects vector
              std::vector<ne::RendablePointer> dls_objects;

              for (int i = 0; i < ray_x; i++) {
                  for (int j = 0; j < ray_y; j++) {
                      float u = float(index.x * ray_x + i + 0.5f) / float(canvas.width() * ray_x);
                      float v = float(index.y * ray_y + j + 0.5f) / float(canvas.height() * ray_y);

                      // construct ray and clear direct light sampling objects vector
                      ne::Ray r = camera.sample(u, v);
                      dls_objects.clear();

                      // compute color of ray sample and then add to pixel
                      color += Li.integrate(r, scene, 15, dls_objects);
                  }
              }

              color = color / float(spp);
              color = glm::clamp(color, 0.0f, 1.0f);

              // record to canvas
              canvas(index) = glm::u8vec4(color * 255.99f, 255.0f);

              // update progressbar and draw it every 10 progress
              if (++progressbar % 20 == 0)
                  progressbar.display();
          }
          });

    taskRenderStart.precede(taskTileRender);
    taskTileRender.precede(taskRenderEnd);
  }

  // start rendering
  tf.wait_for_all();

  canvas.save("result.png");
  return 0;
}
