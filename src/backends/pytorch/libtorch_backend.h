// Copyright (c) 2019, NVIDIA CORPORATION. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//  * Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//  * Neither the name of NVIDIA CORPORATION nor the names of its
//    contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
// OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#pragma once

#include <torch/script.h>  // One-stop header.
#include <set>
#include <string>
#include <unordered_map>
#include <vector>
#include "src/core/backend.h"
#include "src/core/backend_context.h"
#include "src/core/model_config.h"
#include "src/core/model_config.pb.h"
#include "src/core/scheduler.h"
#include "src/core/status.h"

namespace nvidia { namespace inferenceserver {

class AllocatedMemory;

class LibTorchBackend : public InferenceBackend {
 public:
  explicit LibTorchBackend(const double min_compute_capability)
      : InferenceBackend(min_compute_capability)
  {
  }
  LibTorchBackend(LibTorchBackend&&) = default;

  // Create a context for execution for each instance for the
  // serialized .pt models specified in 'paths'.
  Status CreateExecutionContexts(
      const std::unordered_map<std::string, std::string>& paths);
  Status CreateExecutionContext(
      const std::string& instance_name, const int gpu_device,
      const std::unordered_map<std::string, std::string>& paths);

 private:
 private:
  DISALLOW_COPY_AND_ASSIGN(LibTorchBackend);
  friend std::ostream& operator<<(std::ostream&, const LibTorchBackend&);

  // For each model instance there is a context.
  struct Context : BackendContext {
    struct InputMetaData;

    Context(
        const std::string& name, const int gpu_device, const int max_batch_size,
        const bool enable_pinned_input, const bool enable_pinned_output);
    ~Context();

    DISALLOW_COPY_AND_ASSIGN(Context);
    DISALLOW_MOVE(Context);

    Status ValidateInputs(
        const ::google::protobuf::RepeatedPtrField<ModelInput>& ios);
    Status ValidateOutputs(
        const ::google::protobuf::RepeatedPtrField<ModelOutput>& ios);

    // Set the meta data of an input from payloads.
    Status SetInputMetaData(
        const std::string& name, const DataType datatype,
        const std::vector<int64_t>& dims, const size_t total_batch_size,
        std::vector<Scheduler::Payload>* payloads,
        std::vector<InputInfo>* inputs, InputMetaData* meta_data,
        bool* cuda_copy);

    // See BackendContext::Run()
    Status Run(
        const InferenceBackend* base,
        std::vector<Scheduler::Payload>* payloads) override;

    // Helper function to set an input buffer from one or more payloads.
    Status SetFixedSizedInputBuffer(
        const std::string& name, const size_t batch1_byte_size,
        const size_t total_byte_size, std::vector<Scheduler::Payload>* payloads,
        InputInfo* input, InputMetaData* meta_data, bool* cuda_copy);

    // Read an output tensor into one or more payloads.
    Status ReadFixedSizedOutputTensor(
        std::vector<torch::Tensor>* outputs_, const std::string& name,
        const int& op_index, const DataType dtype, const size_t dtype_byte_size,
        const size_t total_batch_size, const DimsList& dims,
        std::vector<Scheduler::Payload>* payloads, OutputInfo* output,
        bool* cuda_copy);

    // Set the input tensor given the meta data of the input.
    Status SetInputTensor(
        const InputMetaData& meta_data, torch::jit::IValue* tensor);

    Status GetOutputTensor(
        std::vector<torch::Tensor>* outputs_, const int& op_index,
        const std::string& name, const DataType dtype, const char** content,
        size_t* byte_size, std::vector<int64_t>* content_shape);

    Status Execute(
        std::vector<torch::jit::IValue>* inputs_,
        std::vector<torch::Tensor>* outputs_);

    std::shared_ptr<torch::jit::script::Module> torch_model_;
    torch::Device device_;
    std::unordered_map<std::string, int> input_index_map_;
    std::unordered_map<std::string, int> output_index_map_;
  };
};

std::ostream& operator<<(std::ostream& out, const LibTorchBackend& pb);

}}  // namespace nvidia::inferenceserver
