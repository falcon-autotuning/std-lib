#include "falcon-core/communications/messages/MeasurementResponse.hpp"
#include "falcon-core/math/arrays/LabelledArrays.hpp"
#include "falcon-core/math/arrays/LabelledMeasuredArray.hpp"
#include <falcon-core/CerealRegistry.hpp>
#include <falcon-typing/FFIHelpers.hpp>

using namespace falcon::typing;
using namespace falcon::typing::ffi::wrapper;

using LabelledMeasuredArray = falcon_core::math::arrays::LabelledMeasuredArray;
using LabelledMeasuredArraySP = std::shared_ptr<LabelledMeasuredArray>;
using LabelledMeasuredArrays =
    falcon_core::math::arrays::LabelledArrays<LabelledMeasuredArray>;
using LabelledMeasuredArraysSP = std::shared_ptr<LabelledMeasuredArrays>;
using MeasurementResponse =
    falcon_core::communications::messages::MeasurementResponse;
using MeasurementResponseSP = std::shared_ptr<MeasurementResponse>;

static void pack_mr(MeasurementResponseSP resp, FalconResultSlot *out,
                    int32_t *oc) {
  out[0] = {};
  out[0].tag = FALCON_TYPE_OPAQUE;
  out[0].value.opaque.type_name = "MeasurementResponse";
  out[0].value.opaque.ptr = new MeasurementResponseSP(std::move(resp));
  out[0].value.opaque.deleter = [](void *p) {
    delete static_cast<MeasurementResponseSP *>(p);
  };
  *oc = 1;
}

static void pack_array_result(std::shared_ptr<ArrayValue> arr,
                              FalconResultSlot *out, int32_t *oc) {
  out[0] = {};
  out[0].tag = FALCON_TYPE_OPAQUE;
  out[0].value.opaque.type_name = "Array";
  out[0].value.opaque.ptr =
      new std::shared_ptr<void>(std::static_pointer_cast<void>(arr));
  out[0].value.opaque.deleter = [](void *p) {
    delete static_cast<std::shared_ptr<void> *>(p);
  };
  *oc = 1;
}

static std::shared_ptr<ArrayValue>
get_array_from_params(const FalconParamEntry *entries, int32_t count,
                      const char *key) {
  for (int32_t i = 0; i < count; ++i) {
    if (std::strcmp(entries[i].key, key) != 0)
      continue;

    const FalconParamEntry &e = entries[i];
    if (e.tag != FALCON_TYPE_OPAQUE) {
      throw std::runtime_error(
          std::string("parameter '") + key +
          "' is not OPAQUE (tag=" + std::to_string(e.tag) + ")");
    }

    std::string tn = e.value.opaque.type_name ? e.value.opaque.type_name : "";
    if (tn == "Array") {
      auto sv = *static_cast<std::shared_ptr<void> *>(e.value.opaque.ptr);
      return std::static_pointer_cast<ArrayValue>(sv);
    }
    if (tn == "ArrayValue") {
      return *static_cast<std::shared_ptr<ArrayValue> *>(e.value.opaque.ptr);
    }

    throw std::runtime_error(std::string("parameter '") + key +
                             "' has unexpected opaque type_name='" + tn + "'");
  }
  throw std::runtime_error(std::string("parameter '") + key + "' not found");
}

static RuntimeValue wrap_lma_as_struct(LabelledMeasuredArraySP lma) {
  auto inst = std::make_shared<StructInstance>("LabelledMeasuredArray");
  inst->native_handle = std::static_pointer_cast<void>(lma);
  return inst;
}

extern "C" {

void SampleJSON(const FalconParamEntry *, int32_t, FalconResultSlot *out,
                int32_t *oc) {
  auto arr = std::make_shared<falcon_core::math::arrays::LabelledArrays<
      falcon_core::math::arrays::LabelledMeasuredArray>>();
  MeasurementResponse resp(arr);
  pack_results(FunctionResult{resp.to_json_string()}, out, 16, oc);
}

// New(arrays: Array<LabelledMeasuredArray>) -> (MeasurementResponse response)
void STRUCTMeasurementResponseNew(const FalconParamEntry *params,
                                  int32_t param_count, FalconResultSlot *out,
                                  int32_t *oc) {
  auto arr_val = get_array_from_params(params, param_count, "arrays");

  std::vector<LabelledMeasuredArraySP> vec;
  vec.reserve(arr_val->elements.size());

  for (const auto &elem : arr_val->elements) {
    if (!std::holds_alternative<std::shared_ptr<StructInstance>>(elem)) {
      throw std::runtime_error(
          "STRUCTMeasurementResponseNew: array element is not a StructInstance");
    }
    auto inst = std::get<std::shared_ptr<StructInstance>>(elem);
    if (!inst || !inst->native_handle.has_value()) {
      throw std::runtime_error(
          "STRUCTMeasurementResponseNew: StructInstance has no native_handle");
    }
    auto lma =
        std::static_pointer_cast<LabelledMeasuredArray>(inst->native_handle.value());
    if (!lma) {
      throw std::runtime_error(
          "STRUCTMeasurementResponseNew: null LabelledMeasuredArray in native_handle");
    }
    vec.push_back(std::move(lma));
  }

  auto labelled_arrays =
      std::make_shared<LabelledMeasuredArrays>(vec);
  pack_mr(std::make_shared<MeasurementResponse>(labelled_arrays), out, oc);
}

// Arrays(this: MeasurementResponse) -> (Array<LabelledMeasuredArray> arrays)
void STRUCTMeasurementResponseArrays(const FalconParamEntry *params,
                                     int32_t param_count, FalconResultSlot *out,
                                     int32_t *oc) {
  auto self = get_opaque<MeasurementResponse>(params, param_count, "this");
  auto arr = self->arrays();

  std::vector<RuntimeValue> elements;
  if (arr) {
    elements.reserve(arr->size());
    for (const auto &lma : arr->items()) {
      elements.push_back(wrap_lma_as_struct(lma));
    }
  }

  auto arr_val =
      std::make_shared<ArrayValue>("LabelledMeasuredArray", std::move(elements));
  pack_array_result(arr_val, out, oc);
}

// Message(this: MeasurementResponse) -> (string message)
void STRUCTMeasurementResponseMessage(const FalconParamEntry *params,
                                      int32_t param_count,
                                      FalconResultSlot *out, int32_t *oc) {
  auto self = get_opaque<MeasurementResponse>(params, param_count, "this");
  pack_results(FunctionResult{self->message()}, out, 16, oc);
}

// Equal(this: MeasurementResponse, other: MeasurementResponse) -> (bool equal)
void STRUCTMeasurementResponseEqual(const FalconParamEntry *params,
                                    int32_t param_count, FalconResultSlot *out,
                                    int32_t *oc) {
  auto self = get_opaque<MeasurementResponse>(params, param_count, "this");
  auto other = get_opaque<MeasurementResponse>(params, param_count, "other");
  pack_results(FunctionResult{*self == *other}, out, 16, oc);
}

// NotEqual(this: MeasurementResponse, other: MeasurementResponse) -> (bool
// notequal)
void STRUCTMeasurementResponseNotEqual(const FalconParamEntry *params,
                                       int32_t param_count,
                                       FalconResultSlot *out, int32_t *oc) {
  auto self = get_opaque<MeasurementResponse>(params, param_count, "this");
  auto other = get_opaque<MeasurementResponse>(params, param_count, "other");
  pack_results(FunctionResult{*self != *other}, out, 16, oc);
}

// ToJSON(this: MeasurementResponse) -> (string json)
void STRUCTMeasurementResponseToJSON(const FalconParamEntry *params,
                                     int32_t param_count, FalconResultSlot *out,
                                     int32_t *oc) {
  auto self = get_opaque<MeasurementResponse>(params, param_count, "this");
  pack_results(FunctionResult{self->to_json_string()}, out, 16, oc);
}

// FromJSON(json: string) -> (MeasurementResponse response)
void STRUCTMeasurementResponseFromJSON(const FalconParamEntry *params,
                                       int32_t param_count,
                                       FalconResultSlot *out, int32_t *oc) {
  auto pm = unpack_params(params, param_count);
  auto json = std::get<std::string>(pm.at("json"));
  auto resp = MeasurementResponse::from_json_string<MeasurementResponse>(json);
  pack_mr(std::move(resp), out, oc);
}

} // extern "C"
