#include "re5_renderer.h"

#include <algorithm>
#include <limits>
#include <string>

namespace re5::render {

Re5Renderer::~Re5Renderer() { Shutdown(); }

bool Re5Renderer::Fail(std::string_view message, std::string* error) const {
  if (error) *error = message;
  return false;
}

bool Re5Renderer::Initialize(std::string* error) {
  if (initialized_) return true;
  if (!backend_.Initialize(error)) return false;
  initialized_ = true;
  return true;
}

void Re5Renderer::Shutdown() {
  if (!initialized_) return;
  if (in_frame_) {
    std::string ignored;
    backend_.EndFrame(&ignored);
  }
  backend_.Shutdown();
  initialized_ = false;
  in_frame_ = false;
  bound_pipeline_ = kInvalidResource;
  index_buffer_bound_ = false;
  vertex_buffers_bound_.fill(false);
  textures_bound_.fill(false);
}

bool Re5Renderer::BeginFrame(std::string* error) {
  if (!initialized_) return Fail("renderer is not initialized", error);
  if (in_frame_) return Fail("frame already active", error);
  if (!backend_.BeginFrame(error)) return false;
  in_frame_ = true;
  bound_pipeline_ = kInvalidResource;
  index_buffer_bound_ = false;
  vertex_buffers_bound_.fill(false);
  textures_bound_.fill(false);
  return true;
}

bool Re5Renderer::EndFrame(std::string* error) {
  if (!in_frame_) return Fail("no active frame", error);
  if (!backend_.EndFrame(error)) return false;
  in_frame_ = false;
  return true;
}

ResourceId Re5Renderer::CreateBuffer(std::span<const std::byte> data, bool index_buffer,
                                     std::string* error) {
  if (!initialized_) {
    Fail("renderer is not initialized", error);
    return kInvalidResource;
  }
  if (data.empty()) {
    Fail("buffer initial data is empty", error);
    return kInvalidResource;
  }
  return backend_.CreateBuffer(data, index_buffer, error);
}

ResourceId Re5Renderer::CreateTexture(const TextureDesc& desc,
                                     std::span<const std::byte> data,
                                     std::string* error) {
  if (!initialized_) {
    Fail("renderer is not initialized", error);
    return kInvalidResource;
  }
  if (!desc.extent.width || !desc.extent.height || !desc.mip_levels ||
      !desc.layers || !desc.samples) {
    Fail("texture dimensions, mip count, layers and samples must be non-zero", error);
    return kInvalidResource;
  }
  if (desc.depth_stencil_attachment && desc.color_attachment) {
    Fail("texture cannot be both a color and depth/stencil attachment", error);
    return kInvalidResource;
  }
  return backend_.CreateTexture(desc, data, error);
}

ResourceId Re5Renderer::CreateShader(const ShaderModuleDesc& desc, std::string* error) {
  if (!initialized_) {
    Fail("renderer is not initialized", error);
    return kInvalidResource;
  }
  if (desc.spirv_words.size() < 5 || desc.spirv_words[0] != 0x07230203u) {
    Fail("shader is not a valid SPIR-V module (missing header or magic)", error);
    return kInvalidResource;
  }
  if (!desc.guest_ucode_hash) {
    Fail("guest shader hash must be non-zero", error);
    return kInvalidResource;
  }
  return backend_.CreateShader(desc, error);
}

ResourceId Re5Renderer::CreatePipeline(const GraphicsPipelineDesc& desc,
                                       std::string* error) {
  if (!initialized_) {
    Fail("renderer is not initialized", error);
    return kInvalidResource;
  }
  if (!desc.vertex_shader || !desc.fragment_shader) {
    Fail("graphics pipeline requires vertex and fragment shaders", error);
    return kInvalidResource;
  }
  for (const auto& attachment : desc.blend) {
    if (attachment.write_mask & ~uint8_t{0xF}) {
      Fail("blend write mask contains unsupported bits", error);
      return kInvalidResource;
    }
  }
  return backend_.CreatePipeline(desc, error);
}

void Re5Renderer::DestroyResource(ResourceId id) {
  if (initialized_ && id != kInvalidResource) backend_.DestroyResource(id);
}

bool Re5Renderer::SetRenderTargets(std::span<const ResourceId> colors, ResourceId depth,
                                   std::string* error) {
  if (!in_frame_) return Fail("render targets can only change inside a frame", error);
  if (colors.size() > 4) return Fail("RE5 renderer supports at most four color targets", error);
  if (std::any_of(colors.begin(), colors.end(), [](ResourceId id) { return id == 0; })) {
    return Fail("color target list contains an invalid resource", error);
  }
  return backend_.SetRenderTargets(colors, depth, error);
}

bool Re5Renderer::BindPipeline(ResourceId pipeline, std::string* error) {
  if (!in_frame_) return Fail("pipeline binding requires an active frame", error);
  if (!pipeline) return Fail("invalid pipeline resource", error);
  if (!backend_.BindPipeline(pipeline, error)) return false;
  bound_pipeline_ = pipeline;
  return true;
}

bool Re5Renderer::BindVertexBuffer(uint32_t slot, const VertexBufferBinding& binding,
                                   std::string* error) {
  if (!in_frame_) return Fail("vertex buffer binding requires an active frame", error);
  if (slot >= vertex_buffers_bound_.size()) return Fail("vertex buffer slot out of range", error);
  if (!binding.buffer || !binding.stride) return Fail("vertex buffer and stride must be non-zero", error);
  if (!backend_.BindVertexBuffer(slot, binding, error)) return false;
  vertex_buffers_bound_[slot] = true;
  return true;
}

bool Re5Renderer::BindIndexBuffer(const IndexBufferBinding& binding, std::string* error) {
  if (!in_frame_) return Fail("index buffer binding requires an active frame", error);
  if (!binding.buffer) return Fail("invalid index buffer resource", error);
  if (!backend_.BindIndexBuffer(binding, error)) return false;
  index_buffer_bound_ = true;
  return true;
}

bool Re5Renderer::BindTexture(uint32_t slot, ResourceId texture,
                              const SamplerDesc& sampler, std::string* error) {
  if (!in_frame_) return Fail("texture binding requires an active frame", error);
  if (slot >= textures_bound_.size()) return Fail("texture slot out of range", error);
  if (!texture) return Fail("invalid texture resource", error);
  if (!backend_.BindTexture(slot, texture, sampler, error)) return false;
  textures_bound_[slot] = true;
  return true;
}

bool Re5Renderer::ValidateDraw(const DrawCall& draw, bool indexed, std::string* error) const {
  if (!in_frame_) return Fail("draw requires an active frame", error);
  if (!bound_pipeline_) return Fail("draw requires a bound graphics pipeline", error);
  if (!draw.instance_count) return Fail("instance count must be non-zero", error);
  if (indexed && !index_buffer_bound_) return Fail("indexed draw requires an index buffer", error);
  if (!indexed && !draw.vertex_count) return Fail("vertex count must be non-zero", error);
  if (indexed && !draw.index_count) return Fail("index count must be non-zero", error);
  if (std::none_of(vertex_buffers_bound_.begin(), vertex_buffers_bound_.end(),
                   [](bool bound) { return bound; })) {
    return Fail("draw requires at least one vertex buffer", error);
  }
  return true;
}

bool Re5Renderer::Draw(const DrawCall& draw, std::string* error) {
  if (!ValidateDraw(draw, false, error)) return false;
  return backend_.Draw(draw, false, error);
}

bool Re5Renderer::DrawIndexed(const DrawCall& draw, std::string* error) {
  if (!ValidateDraw(draw, true, error)) return false;
  return backend_.Draw(draw, true, error);
}

}  // namespace re5::render
