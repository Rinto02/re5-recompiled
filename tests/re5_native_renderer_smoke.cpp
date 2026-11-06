#include "../app/src/native_renderer/re5_renderer.h"

#include <array>
#include <cassert>
#include <cstddef>
#include <span>
#include <string>

using namespace re5::render;

class TestBackend final : public IRenderBackend {
 public:
  bool Initialize(std::string*) override { initialized = true; return true; }
  void Shutdown() override { initialized = false; }
  bool BeginFrame(std::string*) override { return true; }
  bool EndFrame(std::string*) override { return true; }
  ResourceId CreateBuffer(std::span<const std::byte>, bool, std::string*) override { return 1; }
  ResourceId CreateTexture(const TextureDesc&, std::span<const std::byte>, std::string*) override { return 2; }
  ResourceId CreateShader(const ShaderModuleDesc&, std::string*) override { return 3; }
  ResourceId CreatePipeline(const GraphicsPipelineDesc&, std::string*) override { return 4; }
  void DestroyResource(ResourceId) override {}
  bool SetRenderTargets(std::span<const ResourceId>, ResourceId, std::string*) override { return true; }
  bool BindPipeline(ResourceId, std::string*) override { return true; }
  bool BindVertexBuffer(uint32_t, const VertexBufferBinding&, std::string*) override { return true; }
  bool BindIndexBuffer(const IndexBufferBinding&, std::string*) override { return true; }
  bool BindTexture(uint32_t, ResourceId, const SamplerDesc&, std::string*) override { return true; }
  bool Draw(const DrawCall&, bool is_indexed, std::string*) override { last_indexed = is_indexed; return true; }
  bool initialized = false;
  bool last_indexed = false;
};

int main() {
  TestBackend backend;
  Re5Renderer renderer(backend);
  std::string error;
  assert(renderer.Initialize(&error));
  assert(renderer.BeginFrame(&error));

  std::array<uint32_t, 5> spirv{0x07230203u, 0x00010000u, 0u, 1u, 0u};
  ShaderModuleDesc vs{ShaderStage::kVertex, 0x1234, spirv};
  ShaderModuleDesc fs{ShaderStage::kFragment, 0x5678, spirv};
  assert(renderer.CreateShader(vs, &error) == 3);
  assert(renderer.CreateShader(fs, &error) == 3);

  GraphicsPipelineDesc pipeline{};
  pipeline.vertex_shader = 3;
  pipeline.fragment_shader = 3;
  assert(renderer.CreatePipeline(pipeline, &error) == 4);
  assert(renderer.BindPipeline(4, &error));

  std::array<std::byte, 16> vertex_data{};
  assert(renderer.CreateBuffer(vertex_data, false, &error) == 1);
  assert(renderer.BindVertexBuffer(0, VertexBufferBinding{1, 0, 16}, &error));

  DrawCall draw{};
  draw.vertex_count = 3;
  assert(renderer.Draw(draw, &error));
  assert(!backend.last_indexed);

  assert(!renderer.DrawIndexed(draw, &error));
  assert(error == "indexed draw requires an index buffer");

  assert(renderer.BindIndexBuffer(IndexBufferBinding{1, 0, IndexFormat::kUInt16}, &error));
  draw.index_count = 3;
  assert(renderer.DrawIndexed(draw, &error));
  assert(backend.last_indexed);
  assert(renderer.EndFrame(&error));
  renderer.Shutdown();
  assert(!backend.initialized);
}
