# RE5 native rendering foundation

This module establishes a game-facing rendering contract that does not expose
Xenos registers, PM4 packets, or ReXGlue GPU-plugin types to RE5-specific code.

## Current implementation

- Resource descriptors for buffers, textures, samplers, render targets, and
  depth/stencil state.
- Pipeline and draw descriptions for indexed and non-indexed submissions.
- `IRe5ShaderCompiler`, the translation boundary between captured Xenos
  microcode and SPIR-V modules.
- `IRenderBackend`, the backend boundary that a Vulkan implementation must
  satisfy.
- `Re5Renderer`, which validates frame state, resource handles, bindings, and
  draw preconditions before forwarding commands.
- `tools/validate_re5_shader_dump.py`, which validates the supplied captured
  microcode/disassembly pairs and emits a JSON inventory.

## Deliberate limitation

This patch is the interface and validation foundation, not a functioning
Vulkan renderer. It does not create a VkInstance/VkDevice, translate Xenos
microcode, allocate GPU resources, or intercept RE5's original graphics calls.
Those require a Vulkan device/lifetime integration point and verified RE5
guest entry points. The current app still uses the existing `rexgpu-xenos`
plugin until those pieces are implemented and tested.

## Shader pipeline status

The SDK already contains `Shader`, `ShaderTranslator`, and
`SpirvShaderTranslator`; the Xenos Vulkan backend already uses that translator.
The dump audit tool verifies the captured inputs before a dedicated offline
translation harness is added. A shader dump passing this audit is not proof
that it compiles to valid SPIR-V or renders correctly.

## Integration contract

Game-side hooks should translate RE5 state into `Re5Renderer` calls. Keep
guest-memory decoding, Xbox 360 service compatibility, and presentation
separate from this interface. Do not disable the Xenos plugin until the native
backend passes scene-by-scene comparison tests.
