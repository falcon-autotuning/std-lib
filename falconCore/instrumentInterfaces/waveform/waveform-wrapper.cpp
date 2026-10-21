#include "falcon-core/instrument_interfaces/Waveform.hpp"
#include "falcon-core/instrument_interfaces/port_transforms/PortTransform.hpp"
#include "falcon-core/math/discrete_spaces/DiscreteSpace.hpp"
#include <falcon-core/CerealRegistry.hpp>
#include <falcon-typing/FFIHelpers.hpp>
#include <stdexcept>

using namespace falcon::typing;
using namespace falcon::typing::ffi::wrapper;

using Waveform = falcon_core::instrument_interfaces::Waveform;
using WaveformSP = std::shared_ptr<Waveform>;
using PortTransform =
    falcon_core::instrument_interfaces::port_transforms::PortTransform;
using PortTransformSP = std::shared_ptr<PortTransform>;
using DiscreteSpace = falcon_core::math::discrete_spaces::DiscreteSpace;
using DiscreteSpaceSP = std::shared_ptr<DiscreteSpace>;

static void pack_opaque_waveform(WaveformSP w, FalconResultSlot *out,
                                 int32_t *oc) {
  out[0] = {};
  out[0].tag = FALCON_TYPE_OPAQUE;
  out[0].value.opaque.type_name = "Waveform";
  out[0].value.opaque.ptr = new WaveformSP(std::move(w));
  out[0].value.opaque.deleter = [](void *p) {
    delete static_cast<WaveformSP *>(p);
  };
  *oc = 1;
}

static void pack_opaque_pt(PortTransformSP pt, FalconResultSlot *out,
                           int32_t *oc) {
  out[0] = {};
  out[0].tag = FALCON_TYPE_OPAQUE;
  out[0].value.opaque.type_name = "PortTransform";
  out[0].value.opaque.ptr = new PortTransformSP(std::move(pt));
  out[0].value.opaque.deleter = [](void *p) {
    delete static_cast<PortTransformSP *>(p);
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
    if (e.tag != FALCON_TYPE_OPAQUE)
      throw std::runtime_error(std::string("get_array_from_params: '") + key +
                               "' not OPAQUE");
    std::string tn = e.value.opaque.type_name ? e.value.opaque.type_name : "";
    if (tn == "Array") {
      auto sv = *static_cast<std::shared_ptr<void> *>(e.value.opaque.ptr);
      return std::static_pointer_cast<ArrayValue>(sv);
    }
    if (tn == "ArrayValue")
      return *static_cast<std::shared_ptr<ArrayValue> *>(e.value.opaque.ptr);
    throw std::runtime_error(std::string("get_array_from_params: '") + key +
                             "' unexpected type '" + tn + "'");
  }
  throw std::runtime_error(std::string("get_array_from_params: '") + key +
                           "' not found");
}

static std::shared_ptr<falcon_core::math::Axes<int>>
get_int_axes(const ParameterMap &pm, const char *key) {
  auto inst = std::get<std::shared_ptr<StructInstance>>(pm.at(key));

  if (!inst) {
    throw std::runtime_error(
        std::string("get_int_axes: null StructInstance for '") + key + "'");
  }

  auto fields = inst->fields;

  auto arr_it = fields->find("arr_");
  if (arr_it == fields->end()) {
    throw std::runtime_error(
        std::string("get_int_axes: missing arr_ field for '") + key + "'");
  }

  auto arr_inst = std::get<std::shared_ptr<StructInstance>>(arr_it->second);

  if (!arr_inst) {
    throw std::runtime_error(std::string("get_int_axes: arr_ is null for '") +
                             key + "'");
  }

  if (!arr_inst->native_handle.has_value()) {
    throw std::runtime_error(
        std::string("get_int_axes: Array has no native_handle for '") + key +
        "'");
  }

  auto arr =
      std::static_pointer_cast<ArrayValue>(arr_inst->native_handle.value());

  auto native = std::make_shared<falcon_core::math::Axes<int>>();

  for (const auto &v : arr->elements) {
    native->push_back(static_cast<int>(std::get<int64_t>(v)));
  }

  return native;
}

extern "C" {

void SampleJSON(const FalconParamEntry *, int32_t, FalconResultSlot *out,
                int32_t *oc) {
  auto conn =
      falcon_core::physics::device_structures::Connection::PlungerGate("P1");
  auto port = falcon_core::instrument_interfaces::names::InstrumentPort::Knob(
      "P1", "", conn);
  auto pt = falcon_core::instrument_interfaces::port_transforms::PortTransform::
      IdentityTransform(port);
  auto list = std::make_shared<falcon_core::generic::List<PortTransform>>();
  list->push_back(pt);

  auto ld = falcon_core::math::domains::LabelledDomain::from_port(
      std::make_pair(0.0, 1.0), port);
  auto cld =
      std::make_shared<falcon_core::math::domains::CoupledLabelledDomain>(
          std::vector<falcon_core::math::domains::LabelledDomainSP>{ld});
  auto cld_axes = std::make_shared<falcon_core::math::Axes<
      falcon_core::math::domains::CoupledLabelledDomain>>();
  cld_axes->push_back(cld);

  auto space = falcon_core::math::discrete_spaces::DiscreteSpace::
      CartesianDiscreteSpace1D(
          10, cld,
          std::make_shared<falcon_core::generic::Map<std::string, bool>>(),
          std::make_shared<falcon_core::math::domains::Domain>(0.0, 1.0));
  Waveform w(space, list);
  pack_results(FunctionResult{w.to_json_string()}, out, 16, oc);
}

// New(space: DiscreteSpace, transforms: Array<PortTransform>) -> (Waveform
// waveform)
void STRUCTWaveformNew(const FalconParamEntry *params, int32_t param_count,
                       FalconResultSlot *out, int32_t *oc) {
  auto space = get_opaque<DiscreteSpace>(params, param_count, "space");
  auto arr = get_array_from_params(params, param_count, "transforms");

  auto list = std::make_shared<falcon_core::generic::List<PortTransform>>();
  for (const auto &elem : arr->elements) {
    if (!std::holds_alternative<std::shared_ptr<StructInstance>>(elem))
      throw std::runtime_error(
          "STRUCTWaveformNew: element is not a StructInstance");
    auto inst = std::get<std::shared_ptr<StructInstance>>(elem);
    if (!inst || !inst->native_handle.has_value())
      throw std::runtime_error(
          "STRUCTWaveformNew: StructInstance has no native_handle");
    auto pt =
        std::static_pointer_cast<PortTransform>(inst->native_handle.value());
    list->push_back(pt);
  }
  pack_opaque_waveform(std::make_shared<Waveform>(space, list), out, oc);
}

// NewCartesianWaveform1D(
//   division,
//   axes,
//   increasing,
//   transforms,
//   domain)
// -> Waveform
void STRUCTWaveformNewCartesianWaveform1D(const FalconParamEntry *params,
                                          int32_t param_count,
                                          FalconResultSlot *out, int32_t *oc) {

  auto pm = unpack_params(params, param_count);

  int division = static_cast<int>(std::get<int64_t>(pm.at("division")));

  auto axes = get_opaque<falcon_core::math::domains::CoupledLabelledDomain>(
      params, param_count, "sharedDomain");

  auto increasing = get_opaque<falcon_core::generic::Map<std::string, bool>>(
      params, param_count, "increasing");

  auto domain = get_opaque<falcon_core::math::domains::Domain>(
      params, param_count, "domain");

  auto transforms_array =
      get_array_from_params(params, param_count, "transforms");

  auto transforms =
      std::make_shared<falcon_core::generic::List<PortTransform>>();

  for (const auto &elem : transforms_array->elements) {
    auto inst = std::get<std::shared_ptr<StructInstance>>(elem);

    auto pt =
        std::static_pointer_cast<PortTransform>(inst->native_handle.value());

    transforms->push_back(pt);
  }

  auto wf = Waveform::CartesianWaveform1D(division, axes, increasing,
                                          transforms, domain);

  pack_opaque_waveform(wf, out, oc);
}

// NewCartesianIdentityWaveform1D(...)
void STRUCTWaveformNewCartesianIdentityWaveform1D(
    const FalconParamEntry *params, int32_t param_count, FalconResultSlot *out,
    int32_t *oc) {

  auto pm = unpack_params(params, param_count);

  int division = static_cast<int>(std::get<int64_t>(pm.at("division")));

  auto axes = get_opaque<falcon_core::math::domains::CoupledLabelledDomain>(
      params, param_count, "axes");

  auto increasing = get_opaque<falcon_core::generic::Map<std::string, bool>>(
      params, param_count, "increasing");

  auto domain = get_opaque<falcon_core::math::domains::Domain>(
      params, param_count, "domain");

  auto wf =
      Waveform::CartesianIdentityWaveform1D(division, axes, increasing, domain);

  pack_opaque_waveform(wf, out, oc);
}

// NewCartesianWaveform(...)
void STRUCTWaveformNewCartesianWaveform(const FalconParamEntry *params,
                                        int32_t param_count,
                                        FalconResultSlot *out, int32_t *oc) {

  auto pm = unpack_params(params, param_count);
  auto divisions = get_int_axes(pm, "divisions");

  auto axes = get_opaque<falcon_core::math::Axes<
      falcon_core::math::domains::CoupledLabelledDomain>>(params, param_count,
                                                          "axes");

  auto increasing = get_opaque<
      falcon_core::math::Axes<falcon_core::generic::Map<std::string, bool>>>(
      params, param_count, "increasing");

  auto domain = get_opaque<falcon_core::math::domains::Domain>(
      params, param_count, "domain");

  auto arr = get_array_from_params(params, param_count, "transforms");

  auto transforms =
      std::make_shared<falcon_core::generic::List<PortTransform>>();

  for (const auto &elem : arr->elements) {
    auto inst = std::get<std::shared_ptr<StructInstance>>(elem);

    auto pt =
        std::static_pointer_cast<PortTransform>(inst->native_handle.value());

    transforms->push_back(pt);
  }

  auto wf = Waveform::CartesianWaveform(divisions, axes, increasing, transforms,
                                        domain);

  pack_opaque_waveform(wf, out, oc);
}

// NewCartesianIdentityWaveform(...)
void STRUCTWaveformNewCartesianIdentityWaveform(const FalconParamEntry *params,
                                                int32_t param_count,
                                                FalconResultSlot *out,
                                                int32_t *oc) {

  auto pm = unpack_params(params, param_count);
  auto divisions = get_int_axes(pm, "divisions");

  auto axes = get_opaque<falcon_core::math::Axes<
      falcon_core::math::domains::CoupledLabelledDomain>>(params, param_count,
                                                          "axes");

  auto increasing = get_opaque<
      falcon_core::math::Axes<falcon_core::generic::Map<std::string, bool>>>(
      params, param_count, "increasing");

  auto domain = get_opaque<falcon_core::math::domains::Domain>(
      params, param_count, "domain");

  auto wf =
      Waveform::CartesianIdentityWaveform(divisions, axes, increasing, domain);

  pack_opaque_waveform(wf, out, oc);
}

// NewCartesianWaveform2D(...)
void STRUCTWaveformNewCartesianWaveform2D(const FalconParamEntry *params,
                                          int32_t param_count,
                                          FalconResultSlot *out, int32_t *oc) {

  auto pm = unpack_params(params, param_count);
  auto divisions = get_int_axes(pm, "divisions");

  auto axes = get_opaque<falcon_core::math::Axes<
      falcon_core::math::domains::CoupledLabelledDomain>>(params, param_count,
                                                          "axes");

  auto increasing = get_opaque<
      falcon_core::math::Axes<falcon_core::generic::Map<std::string, bool>>>(
      params, param_count, "increasing");

  auto domain = get_opaque<falcon_core::math::domains::Domain>(
      params, param_count, "domain");

  auto arr = get_array_from_params(params, param_count, "transforms");

  auto transforms =
      std::make_shared<falcon_core::generic::List<PortTransform>>();

  for (const auto &elem : arr->elements) {
    auto inst = std::get<std::shared_ptr<StructInstance>>(elem);

    auto pt =
        std::static_pointer_cast<PortTransform>(inst->native_handle.value());

    transforms->push_back(pt);
  }

  auto wf = Waveform::CartesianWaveform2D(divisions, axes, increasing,
                                          transforms, domain);

  pack_opaque_waveform(wf, out, oc);
}

// NewCartesianIdentityWaveform2D(...)
void STRUCTWaveformNewCartesianIdentityWaveform2D(
    const FalconParamEntry *params, int32_t param_count, FalconResultSlot *out,
    int32_t *oc) {

  auto pm = unpack_params(params, param_count);
  auto divisions = get_int_axes(pm, "divisions");

  auto axes = get_opaque<falcon_core::math::Axes<
      falcon_core::math::domains::CoupledLabelledDomain>>(params, param_count,
                                                          "axes");

  auto increasing = get_opaque<
      falcon_core::math::Axes<falcon_core::generic::Map<std::string, bool>>>(
      params, param_count, "increasing");

  auto domain = get_opaque<falcon_core::math::domains::Domain>(
      params, param_count, "domain");

  auto wf = Waveform::CartesianIdentityWaveform2D(divisions, axes, increasing,
                                                  domain);

  pack_opaque_waveform(wf, out, oc);
}

// Space(this: Waveform) -> (DiscreteSpace space)
void STRUCTWaveformSpace(const FalconParamEntry *params, int32_t param_count,
                         FalconResultSlot *out, int32_t *oc) {
  auto self = get_opaque<Waveform>(params, param_count, "this");
  auto sp = self->space();
  out[0] = {};
  out[0].tag = FALCON_TYPE_OPAQUE;
  out[0].value.opaque.type_name = "DiscreteSpace";
  out[0].value.opaque.ptr = new DiscreteSpaceSP(sp);
  out[0].value.opaque.deleter = [](void *p) {
    delete static_cast<DiscreteSpaceSP *>(p);
  };
  *oc = 1;
}

// Size(this: Waveform) -> (int size)
void STRUCTWaveformSize(const FalconParamEntry *params, int32_t param_count,
                        FalconResultSlot *out, int32_t *oc) {
  auto self = get_opaque<Waveform>(params, param_count, "this");
  pack_results(FunctionResult{static_cast<int64_t>(self->size())}, out, 16, oc);
}

// IsEmpty(this: Waveform) -> (bool empty)
void STRUCTWaveformIsEmpty(const FalconParamEntry *params, int32_t param_count,
                           FalconResultSlot *out, int32_t *oc) {
  auto self = get_opaque<Waveform>(params, param_count, "this");
  pack_results(FunctionResult{self->empty()}, out, 16, oc);
}

// GetIndex(this: Waveform, index: int) -> (PortTransform value)
void STRUCTWaveformGetIndex(const FalconParamEntry *params, int32_t param_count,
                            FalconResultSlot *out, int32_t *oc) {
  auto self = get_opaque<Waveform>(params, param_count, "this");
  auto pm = unpack_params(params, param_count);
  int32_t idx = static_cast<int32_t>(std::get<int64_t>(pm.at("index")));
  auto list = self->transforms();
  auto it = list->begin();
  std::advance(it, idx);
  pack_opaque_pt(std::make_shared<PortTransform>(**it), out, oc);
}

// SetIndex(this: Waveform, index: int, value: PortTransform) -> ()
void STRUCTWaveformSetIndex(const FalconParamEntry *params, int32_t param_count,
                            FalconResultSlot *out, int32_t *oc) {
  auto self = get_opaque<Waveform>(params, param_count, "this");
  auto pm = unpack_params(params, param_count);
  int32_t idx = static_cast<int32_t>(std::get<int64_t>(pm.at("index")));
  auto val = get_opaque<PortTransform>(params, param_count, "value");
  auto list = self->transforms();
  auto it = list->begin();
  std::advance(it, idx);
  **it = *val;
  *oc = 0;
}

// PushBack(this: Waveform, value: PortTransform) -> ()
void STRUCTWaveformPushBack(const FalconParamEntry *params, int32_t param_count,
                            FalconResultSlot *out, int32_t *oc) {
  auto self = get_opaque<Waveform>(params, param_count, "this");
  auto val = get_opaque<PortTransform>(params, param_count, "value");
  self->push_back(std::make_shared<PortTransform>(*val));
  *oc = 0;
}

// PopBack(this: Waveform) -> (PortTransform value)
void STRUCTWaveformPopBack(const FalconParamEntry *params, int32_t param_count,
                           FalconResultSlot *out, int32_t *oc) {
  auto self = get_opaque<Waveform>(params, param_count, "this");
  if (self->empty())
    throw std::runtime_error("STRUCTWaveformPopBack: container is empty");
  auto last = self->back();
  self->erase_at(self->size() - 1);
  pack_opaque_pt(last, out, oc);
}

// Insert(this: Waveform, index: int, value: PortTransform) -> ()
void STRUCTWaveformInsert(const FalconParamEntry *params, int32_t param_count,
                          FalconResultSlot *out, int32_t *oc) {
  auto self = get_opaque<Waveform>(params, param_count, "this");
  auto pm = unpack_params(params, param_count);
  int32_t idx = static_cast<int32_t>(std::get<int64_t>(pm.at("index")));
  auto val = get_opaque<PortTransform>(params, param_count, "value");
  auto it = self->begin() + idx;
  std::vector<std::shared_ptr<PortTransform>> temp{
      std::make_shared<PortTransform>(*val)};
  self->insert(it, temp.begin(), temp.end());
  *oc = 0;
}

// Erase(this: Waveform, index: int) -> ()
void STRUCTWaveformErase(const FalconParamEntry *params, int32_t param_count,
                         FalconResultSlot *out, int32_t *oc) {
  auto self = get_opaque<Waveform>(params, param_count, "this");
  auto pm = unpack_params(params, param_count);
  int32_t idx = static_cast<int32_t>(std::get<int64_t>(pm.at("index")));
  self->erase_at(idx);
  *oc = 0;
}

// Clear(this: Waveform) -> ()
void STRUCTWaveformClear(const FalconParamEntry *params, int32_t param_count,
                         FalconResultSlot *out, int32_t *oc) {
  auto self = get_opaque<Waveform>(params, param_count, "this");
  self->clear();
  *oc = 0;
}

// Contains(this: Waveform, value: PortTransform) -> (bool found)
void STRUCTWaveformContains(const FalconParamEntry *params, int32_t param_count,
                            FalconResultSlot *out, int32_t *oc) {
  auto self = get_opaque<Waveform>(params, param_count, "this");
  auto val = get_opaque<PortTransform>(params, param_count, "value");
  pack_results(FunctionResult{self->contains(val)}, out, 16, oc);
}

// IndexOf(this: Waveform, value: PortTransform) -> (int index)
void STRUCTWaveformIndexOf(const FalconParamEntry *params, int32_t param_count,
                           FalconResultSlot *out, int32_t *oc) {
  auto self = get_opaque<Waveform>(params, param_count, "this");
  auto val = get_opaque<PortTransform>(params, param_count, "value");
  pack_results(FunctionResult{static_cast<int64_t>(self->index(val))}, out, 16,
               oc);
}

// Equal(this: Waveform, other: Waveform) -> (bool equal)
void STRUCTWaveformEqual(const FalconParamEntry *params, int32_t param_count,
                         FalconResultSlot *out, int32_t *oc) {
  auto self = get_opaque<Waveform>(params, param_count, "this");
  auto other = get_opaque<Waveform>(params, param_count, "other");
  pack_results(FunctionResult{*self == *other}, out, 16, oc);
}

// NotEqual(this: Waveform, other: Waveform) -> (bool notequal)
void STRUCTWaveformNotEqual(const FalconParamEntry *params, int32_t param_count,
                            FalconResultSlot *out, int32_t *oc) {
  auto self = get_opaque<Waveform>(params, param_count, "this");
  auto other = get_opaque<Waveform>(params, param_count, "other");
  pack_results(FunctionResult{*self != *other}, out, 16, oc);
}

// ToJSON(this: Waveform) -> (string json)
void STRUCTWaveformToJSON(const FalconParamEntry *params, int32_t param_count,
                          FalconResultSlot *out, int32_t *oc) {
  auto self = get_opaque<Waveform>(params, param_count, "this");
  pack_results(FunctionResult{self->to_json_string()}, out, 16, oc);
}

// FromJSON(json: string) -> (Waveform waveform)
void STRUCTWaveformFromJSON(const FalconParamEntry *params, int32_t param_count,
                            FalconResultSlot *out, int32_t *oc) {
  auto pm = unpack_params(params, param_count);
  auto json = std::get<std::string>(pm.at("json"));
  auto w = Waveform::from_json_string<Waveform>(json);
  pack_opaque_waveform(std::make_shared<Waveform>(*w), out, oc);
}

} // extern "C"
