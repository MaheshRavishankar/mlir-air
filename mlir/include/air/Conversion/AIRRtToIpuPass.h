//===- AIRRtToIpuPass.h -----------------------------------------*- C++ -*-===//
//
// Copyright (C) 2023, Advanced Micro Devices, Inc. All rights reserved.
// SPDX-License-Identifier: MIT
//
//===----------------------------------------------------------------------===//

#ifndef AIRRT_TO_IPU_H
#define AIRRT_TO_IPU_H

#include "mlir/Pass/Pass.h"
#include <memory>

namespace xilinx {
namespace airrt {

std::unique_ptr<mlir::Pass> createAIRRtToIpuPass();

} // namespace airrt
} // namespace xilinx

#endif // AIRRT_TO_IPU_H
