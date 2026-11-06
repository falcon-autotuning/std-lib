#include <cmath>
#include <falcon-typing/FFIHelpers.hpp>

using namespace falcon::typing;
using namespace falcon::typing::ffi::wrapper;

constexpr int FALCON_RESULT_SLOT_COUNT = 16;

extern "C" {

void Sqrt(const FalconParamEntry *params, int32_t param_count,
          FalconResultSlot *result_slots, int32_t *out_count) {
  auto unpacked = unpack_params(params, param_count);

  double value = std::get<double>(unpacked.at("value"));

  pack_results(FunctionResult{std::sqrt(value)}, result_slots,
               FALCON_RESULT_SLOT_COUNT, out_count);
}

void Abs(const FalconParamEntry *params, int32_t param_count,
         FalconResultSlot *result_slots, int32_t *out_count) {
  auto unpacked = unpack_params(params, param_count);

  double value = std::get<double>(unpacked.at("value"));

  pack_results(FunctionResult{std::abs(value)}, result_slots,
               FALCON_RESULT_SLOT_COUNT, out_count);
}

void Min(const FalconParamEntry *params, int32_t param_count,
         FalconResultSlot *result_slots, int32_t *out_count) {
  auto unpacked = unpack_params(params, param_count);

  double lhs = std::get<double>(unpacked.at("lhs"));

  double rhs = std::get<double>(unpacked.at("rhs"));

  pack_results(FunctionResult{std::min(lhs, rhs)}, result_slots,
               FALCON_RESULT_SLOT_COUNT, out_count);
}

void Max(const FalconParamEntry *params, int32_t param_count,
         FalconResultSlot *result_slots, int32_t *out_count) {
  auto unpacked = unpack_params(params, param_count);

  double lhs = std::get<double>(unpacked.at("lhs"));

  double rhs = std::get<double>(unpacked.at("rhs"));

  pack_results(FunctionResult{std::max(lhs, rhs)}, result_slots,
               FALCON_RESULT_SLOT_COUNT, out_count);
}
}
