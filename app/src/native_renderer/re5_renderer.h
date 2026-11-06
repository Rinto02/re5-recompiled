#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace re5::render {

using ResourceId = uint32_t;
inline constexpr ResourceId kInvalidResource = 0;

enum class ShaderStage : uint8_t { kVertex, kFragment };
enum class PrimitiveTopology : uint8_t {
  kPoints, kLines, kLineStrip, kTriangles, kTriangleStrip
};
enum class IndexFormat : uint8_t { kUInt16, kUInt32 };
enum class TextureFormat : uint8_t {
  kR8Unorm, kRG8Unorm, kRGBA8Unorm, kBGRA8Unorm,
  kR16Float, kRG16Float, kRGBA16Float, kD24S8, kD32Float
};
enum class Filter : uint8_t { kNearest, kLinear };
enum class AddressMode : uint8_t { kClamp, kRepeat, kMirror };
enum class CompareOp : uint8_t {
  kNever, kLess, kEqual, kLessEqual, kGreater, kNotEqual, kGreaterEqual, kAlways
};
enum class BlendFactor : uint8_t {
  kZero, kOne, kSrcColor, kOneMinusSrcColor, kDstColor, kOneMinusDstColor,
  kSrcAlpha, kOneMinusSrcAlpha, kDstAlpha, kOneMinusDstAlpha
};
enum class BlendOp : uint8_t { kAdd, kSubtract, kReverseSubtract, kMin, kMax };

struct Extent2D {
  uint32_t width = 0;
  uint32_t height = 0;
};

struct TextureDesc {
  Extent2D extent{};
  uint32_t mip_levels = 1;
  uint32_t layers = 1;
  uint32_t samples = 1;
  TextureFormat format = TextureFormat::kRGBA8Unorm;
  bool sampled = true;
  bool color_attachment = false;
  bool depth_stencil_attachment = false;
};

struct SamplerDesc {
  Filter min_filter = Filter::kLinear;
  Filter mag_filter = Filter::kLinear;
  AddressMode address_u = AddressMode::kClamp;
  AddressMode address_v = AddressMode::kClamp;
  AddressMode address_w = AddressMode::kClamp;
};

struct BlendAttachment {
  bool enabled = false;
  BlendFactor src_color = BlendFactor::kOne;
  BlendFactor dst_color = BlendFactor::kZero;
  BlendOp color_op = BlendOp::kAdd;
  BlendFactor src_alpha = BlendFactor::kOne;
  BlendFactor dst_alpha = BlendFactor::kZero;
  BlendOp alpha_op = BlendOp::kAdd;
  uint8_t write_mask = 0xF;  // RGBA bits.
};

struct DepthStencilState {
  bool depth_test = false;
  bool depth_write = false;
  CompareOp depth_compare = CompareOp::kLessEqual;
  bool stencil_test = false;
};

struct GraphicsPipelineDesc {
  ResourceId vertex_shader = kInvalidResource;
  ResourceId fragment_shader = kInvalidResource;
  PrimitiveTopology topology = PrimitiveTopology::kTriangles;
  std::array<BlendAttachment, 4> blend{};
  DepthStencilState depth_stencil{};
  bool cull_back_faces = false;
  bool front_face_clockwise = true;
};

struct VertexBufferBinding {
  ResourceId buffer = kInvalidResource;
  uint64_t offset = 0;
  uint32_t stride = 0;
};

struct IndexBufferBinding {
  ResourceId buffer = kInvalidResource;
  uint64_t offset = 0;
  IndexFormat format = IndexFormat::kUInt16;
};

struct DrawCall {
  uint32_t vertex_count = 0;
  uint32_t index_count = 0;
  uint32_t instance_count = 1;
  uint32_t first_vertex = 0;
  uint32_t first_index = 0;
  int32_t vertex_offset = 0;
  uint32_t first_instance = 0;
};

struct ShaderModuleDesc {
  ShaderStage stage = ShaderStage::kVertex;
  uint64_t guest_ucode_hash = 0;
  std::span<const uint32_t> spirv_words{};
};

// Adapter boundary for the Xenos-to-SPIR-V translation stage. A future
// implementation can wrap the SDK's SpirvShaderTranslator without exposing
// Xenos parser or translator types to RE5 game hooks.
class IRe5ShaderCompiler {
 public:
  virtual ~IRe5ShaderCompiler() = default;
  virtual bool Compile(ShaderStage stage, uint64_t guest_ucode_hash,
                       std::span<const uint32_t> guest_ucode_dwords,
                       std::vector<uint32_t>& spirv_words,
                       std::string* error) = 0;
};

// The backend owns GPU objects. The RE5 frontend owns game-facing state and
// validates all resource references before dispatching commands.
class IRenderBackend {
 public:
  virtual ~IRenderBackend() = default;
  virtual bool Initialize(std::string* error) = 0;
  virtual void Shutdown() = 0;
  virtual bool BeginFrame(std::string* error) = 0;
  virtual bool EndFrame(std::string* error) = 0;

  virtual ResourceId CreateBuffer(std::span<const std::byte> initial_data,
                                  bool index_buffer, std::string* error) = 0;
  virtual ResourceId CreateTexture(const TextureDesc& desc,
                                  std::span<const std::byte> initial_data,
                                  std::string* error) = 0;
  virtual ResourceId CreateShader(const ShaderModuleDesc& desc,
                                  std::string* error) = 0;
  virtual ResourceId CreatePipeline(const GraphicsPipelineDesc& desc,
                                    std::string* error) = 0;
  virtual void DestroyResource(ResourceId id) = 0;

  virtual bool SetRenderTargets(std::span<const ResourceId> colors,
                                ResourceId depth, std::string* error) = 0;
  virtual bool BindPipeline(ResourceId pipeline, std::string* error) = 0;
  virtual bool BindVertexBuffer(uint32_t slot, const VertexBufferBinding& binding,
                                std::string* error) = 0;
  virtual bool BindIndexBuffer(const IndexBufferBinding& binding,
                               std::string* error) = 0;
  virtual bool BindTexture(uint32_t slot, ResourceId texture,
                           const SamplerDesc& sampler, std::string* error) = 0;
  virtual bool Draw(const DrawCall& draw, bool indexed, std::string* error) = 0;
};

// Explicit RE5-facing interface. Game hooks should translate RE5's draw state
// into these calls; no Xbox 360 PM4 packets or Xenos registers cross this API.
class Re5Renderer final {
 public:
  explicit Re5Renderer(IRenderBackend& backend) : backend_(backend) {}
  ~Re5Renderer();

  Re5Renderer(const Re5Renderer&) = delete;
  Re5Renderer& operator=(const Re5Renderer&) = delete;

  bool Initialize(std::string* error = nullptr);
  void Shutdown();
  bool BeginFrame(std::string* error = nullptr);
  bool EndFrame(std::string* error = nullptr);

  ResourceId CreateBuffer(std::span<const std::byte> data, bool index_buffer,
                          std::string* error = nullptr);
  ResourceId CreateTexture(const TextureDesc& desc,
                           std::span<const std::byte> data = {},
                           std::string* error = nullptr);
  ResourceId CreateShader(const ShaderModuleDesc& desc,
                          std::string* error = nullptr);
  ResourceId CreatePipeline(const GraphicsPipelineDesc& desc,
                            std::string* error = nullptr);
  void DestroyResource(ResourceId id);

  bool SetRenderTargets(std::span<const ResourceId> colors, ResourceId depth = 0,
                        std::string* error = nullptr);
  bool BindPipeline(ResourceId pipeline, std::string* error = nullptr);
  bool BindVertexBuffer(uint32_t slot, const VertexBufferBinding& binding,
                        std::string* error = nullptr);
  bool BindIndexBuffer(const IndexBufferBinding& binding,
                       std::string* error = nullptr);
  bool BindTexture(uint32_t slot, ResourceId texture,
                   const SamplerDesc& sampler = {},
                   std::string* error = nullptr);
  bool Draw(const DrawCall& draw, std::string* error = nullptr);
  bool DrawIndexed(const DrawCall& draw, std::string* error = nullptr);

  bool initialized() const { return initialized_; }
  bool in_frame() const { return in_frame_; }

 private:
  bool Fail(std::string_view message, std::string* error) const;
  bool ValidateDraw(const DrawCall& draw, bool indexed, std::string* error) const;

  IRenderBackend& backend_;
  bool initialized_ = false;
  bool in_frame_ = false;
  ResourceId bound_pipeline_ = kInvalidResource;
  bool index_buffer_bound_ = false;
  std::array<bool, 16> vertex_buffers_bound_{};
  std::array<bool, 32> textures_bound_{};
};

}  // namespace re5::render
