/**
 * @file        codegen/phase_gapfill.cpp
 * @brief       GapFill phase: find uncovered code regions and register them as functions
 *
 * @copyright   Copyright (c) 2026 Tom Clay <tomc@tctechstuff.com>
 *              All rights reserved.
 *
 * @license     BSD 3-Clause License
 *              See LICENSE file in the project root for full license text.
 */

#include "ppc/instruction.h"

#include <unordered_set>

#include <rex/codegen/phases.h>
#include "phase_helpers.h"

#include <rex/logging.h>

#include "codegen_logging.h"
#include <rex/memory/utils.h>

#include <ppc.h>

using rex::codegen::ppc::decode_instruction;
using rex::codegen::ppc::Opcode;
using rex::memory::load_and_swap;

namespace rex::codegen {

namespace {

//=============================================================================
// GapFill to register uncovered code regions
//=============================================================================

// bctr preceded by "lwz rX,N(rY); mtctr rX" is a vtable tail call (a switch jump table uses lwzx).
bool isVtableTailCall(const BinaryView& binary, uint32_t addr) {
  const uint8_t* p1 = binary.translate(addr - 4);
  const uint8_t* p2 = binary.translate(addr - 8);
  if (!p1 || !p2)
    return false;
  auto i1 = decode_instruction(addr - 4, load_and_swap<uint32_t>(p1));
  auto i2 = decode_instruction(addr - 8, load_and_swap<uint32_t>(p2));
  return i1.opcode == Opcode::mtctr && i2.opcode == Opcode::lwz;
}

// Split a code region into function segments based on terminators (blr, tail calls).
std::vector<CodeRegion> splitRegionOnTerminators(
    const CodeRegion& region, const BinaryView& binary,
    const std::unordered_set<uint32_t>& knownCallables) {
  std::vector<CodeRegion> segments;
  uint32_t segmentStart = region.start;

  for (uint32_t addr = region.start; addr < region.end; addr += 4) {
    const uint8_t* data = binary.translate(addr);
    if (!data)
      break;

    uint32_t raw = load_and_swap<uint32_t>(data);
    auto decoded = decode_instruction(addr, raw);
    bool shouldSplit = false;
    const char* reason = nullptr;

    // Check for terminators
    if (decoded.is_return()) {
      shouldSplit = true;
      reason = "blr";
    } else if (decoded.opcode == Opcode::bcctr && decoded.code == 0x4E800420 &&
               addr >= region.start + 8 && isVtableTailCall(binary, addr)) {
      // bctr: unconditional indirect branch (vtable tail call), never falls through
      shouldSplit = true;
      reason = "bctr";
    } else if (decoded.opcode == Opcode::b && decoded.branch_target.has_value()) {
      uint32_t target = decoded.branch_target.value();
      // Don't split on tail recursion (branch to own segment start)
      if (target != segmentStart && knownCallables.contains(target)) {
        shouldSplit = true;
        reason = "tail call";
      }
    }

    if (shouldSplit) {
      uint32_t segmentEnd = addr + 4;
      if (segmentEnd > segmentStart) {
        segments.push_back({segmentStart, segmentEnd});
        REXCODEGEN_TRACE("GapFill: split segment 0x{:08X}-0x{:08X} ({} at 0x{:08X})", segmentStart,
                         segmentEnd, reason, addr);
      }
      segmentStart = segmentEnd;
    }
  }

  // Handle remaining code after last terminator
  if (segmentStart < region.end) {
    segments.push_back({segmentStart, region.end});
  }

  return segments;
}

// Check if address looks like exception handler data (handler ptr + rdata ptr)
bool looksLikeExceptionData(const BinaryView& binary, const FunctionGraph& graph, uint32_t addr) {
  const uint8_t* data = binary.translate(addr);
  if (!data)
    return false;

  // Exception handler data pattern:
  // [addr+0]: pointer to __C_specific_handler (entry point)
  // [addr+4]: pointer to scope table in .rdata
  uint32_t firstDword = load_and_swap<uint32_t>(data);
  uint32_t secondDword = load_and_swap<uint32_t>(data + 4);

  // Check if first dword is a known entry point (like __C_specific_handler)
  if (!graph.isEntryPoint(firstDword)) {
    return false;
  }

  // Check if second dword points to .rdata section
  auto* rdataSection = binary.findSectionByName(".rdata");
  if (!rdataSection)
    return false;

  uint32_t rdataStart = rdataSection->baseAddress;
  uint32_t rdataEnd = rdataStart + rdataSection->size;

  if (secondDword >= rdataStart && secondDword < rdataEnd) {
    REXCODEGEN_TRACE(
        "GapFill: 0x{:08X} looks like exception data (handler=0x{:08X}, scope=0x{:08X}), skipping",
        addr, firstDword, secondDword);
    return true;
  }

  return false;
}

// Register code that is not part of any known function and directly follows a known function's
// final unconditional branch (e.g. vtable-only thunks that GapFill's segment walk skips).
void fillHolesAfterKnownFunctions(CodegenContext& ctx) {
  auto& graph = ctx.graph;
  auto& binary = ctx.binary();
  auto& scan = ctx.scan;

  std::unordered_set<uint32_t> knownCallables;
  for (const auto& [addr, node] : graph.functions()) {
    knownCallables.insert(addr);
  }

  // Covered = inside a known function's discovered blocks. A function's declared size can extend
  // past its reachable code (e.g. a vtable-only thunk packed right after it).
  auto covered = [&](uint32_t a) {
    if (graph.isEntryPoint(a))
      return true;
    const auto* n = graph.getFunctionContaining(a);
    if (!n)
      return false;
    if (n->blocks().empty())
      return true;
    return n->containsAddress(a);
  };

  size_t filled = 0;
  for (const auto& region : scan.codeRegions) {
    uint32_t addr = region.start;
    while (addr < region.end) {
      if (covered(addr)) {
        addr += 4;
        continue;
      }
      uint32_t runStart = addr;
      while (addr < region.end && !covered(addr)) {
        addr += 4;
      }
      uint32_t runEnd = addr;
      if (runStart == region.start)
        continue;

      const uint8_t* pd = binary.translate(runStart - 4);
      if (!pd)
        continue;
      auto prev = decode_instruction(runStart - 4, load_and_swap<uint32_t>(pd));
      bool terminated = prev.is_return() || prev.opcode == Opcode::b ||
                        (prev.opcode == Opcode::bcctr && prev.code == 0x4E800420 &&
                         isVtableTailCall(binary, runStart - 4));
      if (!terminated)
        continue;

      uint32_t start = runStart;
      while (start < runEnd) {
        const uint8_t* d = binary.translate(start);
        if (!d || load_and_swap<uint32_t>(d) != 0x60000000)
          break;
        start += 4;
      }
      if (start >= runEnd)
        continue;

      auto segments = splitRegionOnTerminators({start, runEnd}, binary, knownCallables);
      for (const auto& segment : segments) {
        if (covered(segment.start))
          continue;
        if (looksLikeExceptionData(binary, graph, segment.start))
          continue;
        graph.addFunction(segment.start, segment.size(), FunctionAuthority::GAP_FILL, false);
        knownCallables.insert(segment.start);
        filled++;
        REXCODEGEN_WARN("GapFill hole: sub_{:08X} {} bytes", segment.start, segment.size());
      }
    }
  }

  if (filled > 0) {
    REXCODEGEN_WARN("GapFill: registered {} functions in holes after known functions", filled);
  }
}

void gapFillCodeRegions(CodegenContext& ctx) {
  REXCODEGEN_TRACE("Analyze: checking for uncovered code regions...");

  auto& graph = ctx.graph;
  auto& binary = ctx.binary();
  auto& scan = ctx.scan;

  // Build set of known callables for tail call detection
  std::unordered_set<uint32_t> knownCallables;
  for (const auto& [addr, node] : graph.functions()) {
    knownCallables.insert(addr);
  }

  size_t gapsFound = 0;
  size_t segmentsCreated = 0;

  for (const auto& region : scan.codeRegions) {
    // Split region on terminators (blr, tail calls), then check each segment
    auto segments = splitRegionOnTerminators(region, binary, knownCallables);

    for (const auto& segment : segments) {
      // Skip if this segment's start is already a registered function entry
      if (graph.isEntryPoint(segment.start))
        continue;

      // Skip if this segment's start is inside another function
      if (auto* containingFunc = graph.getFunctionContaining(segment.start)) {
        continue;
      }

      // Skip if this looks like exception handler data (handler ptr + rdata ptr)
      if (looksLikeExceptionData(binary, graph, segment.start))
        continue;

      uint32_t segmentSize = segment.size();
      graph.addFunction(segment.start, segmentSize, FunctionAuthority::GAP_FILL, false);

      REXCODEGEN_TRACE("GapFill: registered sub_{:08X} (0x{:08X}-0x{:08X}, {} bytes)",
                       segment.start, segment.start, segment.end, segmentSize);
      segmentsCreated++;
    }

    gapsFound++;
  }

  if (segmentsCreated > 0) {
    REXCODEGEN_TRACE("Analyze: registered {} gap functions from {} regions", segmentsCreated,
                     gapsFound);
  } else {
    REXCODEGEN_TRACE("Analyze: no uncovered regions found");
  }

  fillHolesAfterKnownFunctions(ctx);
}

//=============================================================================
// Cleanup absorbed GAP_FILL functions
//=============================================================================

void cleanupAbsorbedGapFills(CodegenContext& ctx) {
  auto& graph = ctx.graph;
  std::vector<uint32_t> toRemove;

  for (const auto& [addr, node] : graph.functions()) {
    if (node->authority() != FunctionAuthority::GAP_FILL)
      continue;

    for (const auto& [otherAddr, otherNode] : graph.functions()) {
      if (otherAddr == addr)
        continue;
      if (!otherNode->containsAddress(addr))
        continue;

      // This GAP_FILL is inside another function's blocks
      if (otherNode->authority() != FunctionAuthority::GAP_FILL) {
        // Absorbed by higher authority - remove
        toRemove.push_back(addr);
        break;
      } else if (otherAddr < addr) {
        // Both GAP_FILL, other has lower address - it survives
        toRemove.push_back(addr);
        break;
      }
    }
  }

  for (uint32_t addr : toRemove) {
    graph.removeFunction(addr);
  }

  if (!toRemove.empty()) {
    REXCODEGEN_TRACE("Analyze: removed {} absorbed GAP_FILL functions", toRemove.size());
  }
}

}  // anonymous namespace

namespace phases {

VoidResult GapFill(CodegenContext& ctx, ProgressReporter* reporter) {
  (void)reporter;
  gapFillCodeRegions(ctx);

  // Discover blocks for gap-filled functions
  auto known = buildKnownFunctions(ctx.graph, /*excludeGapFill=*/true);
  size_t discovered = discoverPendingFunctions(ctx, known);
  REXCODEGEN_TRACE("Analyze: discovered blocks for {} gap-filled functions", discovered);

  cleanupAbsorbedGapFills(ctx);

  return Ok();
}

}  // namespace phases

}  // namespace rex::codegen
