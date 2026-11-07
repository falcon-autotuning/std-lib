#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>

#include <falcon-typing/FFIHelpers.hpp>

using namespace falcon::typing;
using namespace falcon::typing::ffi::wrapper;

constexpr int FALCON_RESULT_SLOT_COUNT = 16;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static std::shared_ptr<StructInstance>
get_struct_field(std::shared_ptr<StructInstance> obj,
                 const std::string &field_name) {
  return std::get<std::shared_ptr<StructInstance>>(obj->get_field(field_name));
}

static std::shared_ptr<ArrayValue>
get_array_field(std::shared_ptr<StructInstance> obj,
                const std::string &field_name) {
  auto arr_struct = get_struct_field(obj, field_name);

  return std::static_pointer_cast<ArrayValue>(
      arr_struct->native_handle.value());
}

static std::shared_ptr<StructInstance>
get_direction_map(std::shared_ptr<StructInstance> direction) {
  return get_struct_field(direction, "dir_");
}

static std::shared_ptr<ArrayValue>
direction_keys(std::shared_ptr<StructInstance> direction) {
  auto map = get_direction_map(direction);

  return get_array_field(map, "keys_");
}

static std::shared_ptr<ArrayValue>
direction_values(std::shared_ptr<StructInstance> direction) {
  auto map = get_direction_map(direction);

  return get_array_field(map, "values_");
}

static double coefficient_at(std::shared_ptr<StructInstance> direction,
                             const RuntimeValue &gate, bool *found) {
  auto keys = direction_keys(direction);

  auto values = direction_values(direction);

  for (size_t i = 0; i < keys->elements.size(); ++i) {
    if (keys->elements[i] == gate) {
      *found = true;

      return std::get<double>(values->elements[i]);
    }
  }

  *found = false;
  return 0.0;
}

static double norm_squared(std::shared_ptr<StructInstance> direction) {
  auto values = direction_values(direction);

  double result = 0.0;

  for (size_t i = 0; i < values->elements.size(); ++i) {
    double coeff = std::get<double>(values->elements[i]);

    result += coeff * coeff;
  }

  return result;
}

// ---------------------------------------------------------------------------
// Deep Clone
// ---------------------------------------------------------------------------

static RuntimeValue clone_runtime_value(const RuntimeValue &value);

static std::shared_ptr<StructInstance>
clone_struct(const std::shared_ptr<StructInstance> &src) {

  if (!src) {
    return nullptr;
  }

  auto result = std::make_shared<StructInstance>(src->type_name);

  result->native_handle = src->native_handle;

  for (const auto &[name, field] : *src->fields) {
    result->set_field(name, clone_runtime_value(field));
  }

  return result;
}

static std::shared_ptr<ArrayValue>
clone_array(const std::shared_ptr<ArrayValue> &src) {

  if (!src) {
    return nullptr;
  }

  auto result = std::make_shared<ArrayValue>();

  result->element_type_name = src->element_type_name;

  for (const auto &value : src->elements) {
    result->elements.push_back(clone_runtime_value(value));
  }

  return result;
}

static std::shared_ptr<TupleValue>
clone_tuple(const std::shared_ptr<TupleValue> &src) {

  if (!src) {
    return nullptr;
  }

  auto result = std::make_shared<TupleValue>();

  for (const auto &value : src->values) {
    result->values.push_back(clone_runtime_value(value));
  }

  return result;
}

static RuntimeValue clone_runtime_value(const RuntimeValue &value) {

  return std::visit(
      [](auto const &v) -> RuntimeValue {
        using T = std::decay_t<decltype(v)>;

        if constexpr (std::is_same_v<T, std::shared_ptr<StructInstance>>) {

          return clone_struct(v);

        } else if constexpr (std::is_same_v<T, std::shared_ptr<ArrayValue>>) {

          return clone_array(v);

        } else if constexpr (std::is_same_v<T, std::shared_ptr<TupleValue>>) {

          return clone_tuple(v);

        } else {
          return v;
        }
      },
      value);
}

// ---------------------------------------------------------------------------
// Dot
// ---------------------------------------------------------------------------

extern "C" {

void STRUCTDirectionDot(const FalconParamEntry *p, int32_t pc,
                        FalconResultSlot *out, int32_t *oc) {
  auto pm = unpack_params(p, pc);

  auto self = std::get<std::shared_ptr<StructInstance>>(pm.at("this"));
  auto other = std::get<std::shared_ptr<StructInstance>>(pm.at("other"));

  auto self_keys = direction_keys(self);
  auto self_values = direction_values(self);

  double dot = 0.0;
  for (size_t i = 0; i < self_keys->elements.size(); ++i) {
    bool found = false;
    double rhs = coefficient_at(other, self_keys->elements[i], &found);
    if (found) {
      double lhs = std::get<double>(self_values->elements[i]);
      dot += lhs * rhs;
    }
  }

  pack_results(FunctionResult{dot}, out, FALCON_RESULT_SLOT_COUNT, oc);
}

// ---------------------------------------------------------------------------
// Scale
// ---------------------------------------------------------------------------

void STRUCTDirectionScale(const FalconParamEntry *p, int32_t pc,
                          FalconResultSlot *out, int32_t *oc) {
  auto pm = unpack_params(p, pc);
  auto result =
      clone_struct(std::get<std::shared_ptr<StructInstance>>(pm.at("this")));
  double factor = std::get<double>(pm.at("factor"));
  auto values = direction_values(result);

  for (size_t i = 0; i < values->elements.size(); ++i) {
    double coeff = std::get<double>(values->elements[i]);

    values->elements[i] = coeff * factor;
  }

  pack_results(FunctionResult{result}, out, FALCON_RESULT_SLOT_COUNT, oc);
}

// ---------------------------------------------------------------------------
// Add
// ---------------------------------------------------------------------------

void STRUCTDirectionAdd(const FalconParamEntry *p, int32_t pc,
                        FalconResultSlot *out, int32_t *oc) {
  auto pm = unpack_params(p, pc);

  auto result =
      clone_struct(std::get<std::shared_ptr<StructInstance>>(pm.at("this")));
  auto other = std::get<std::shared_ptr<StructInstance>>(pm.at("other"));

  auto result_keys = direction_keys(result);
  auto result_values = direction_values(result);
  auto other_keys = direction_keys(other);
  auto other_values = direction_values(other);

  for (size_t i = 0; i < other_keys->elements.size(); ++i) {
    const RuntimeValue &gate = other_keys->elements[i];

    double rhs = std::get<double>(other_values->elements[i]);
    bool found = false;
    for (size_t j = 0; j < result_keys->elements.size(); ++j) {
      if (result_keys->elements[j] == gate) {
        double lhs = std::get<double>(result_values->elements[j]);
        result_values->elements[j] = lhs + rhs;
        found = true;
        break;
      }
    }

    if (!found) {
      result_keys->elements.push_back(gate);
      result_values->elements.push_back(rhs);
    }
  }

  pack_results(FunctionResult{result}, out, FALCON_RESULT_SLOT_COUNT, oc);
}

// ---------------------------------------------------------------------------
// Subtract
// ---------------------------------------------------------------------------

void STRUCTDirectionSubtract(const FalconParamEntry *p, int32_t pc,
                             FalconResultSlot *out, int32_t *oc) {
  auto pm = unpack_params(p, pc);

  auto result =
      clone_struct(std::get<std::shared_ptr<StructInstance>>(pm.at("this")));
  auto other = std::get<std::shared_ptr<StructInstance>>(pm.at("other"));

  auto result_keys = direction_keys(result);
  auto result_values = direction_values(result);
  auto other_keys = direction_keys(other);
  auto other_values = direction_values(other);

  for (size_t i = 0; i < other_keys->elements.size(); ++i) {
    const RuntimeValue &gate = other_keys->elements[i];

    double rhs = std::get<double>(other_values->elements[i]);
    bool found = false;

    for (size_t j = 0; j < result_keys->elements.size(); ++j) {
      if (result_keys->elements[j] == gate) {
        double lhs = std::get<double>(result_values->elements[j]);
        result_values->elements[j] = lhs - rhs;
        found = true;
        break;
      }
    }

    if (!found) {
      result_keys->elements.push_back(gate);
      result_values->elements.push_back(-rhs);
    }
  }

  pack_results(FunctionResult{result}, out, FALCON_RESULT_SLOT_COUNT, oc);
}

// ---------------------------------------------------------------------------
// Normalize
// ---------------------------------------------------------------------------

void STRUCTDirectionNormalize(const FalconParamEntry *p, int32_t pc,
                              FalconResultSlot *out, int32_t *oc) {
  auto pm = unpack_params(p, pc);

  auto self = std::get<std::shared_ptr<StructInstance>>(pm.at("this"));
  double norm = std::sqrt(norm_squared(self));

  if (norm != 0.0) {
    auto values = direction_values(self);
    for (size_t i = 0; i < values->elements.size(); ++i) {
      double coeff = std::get<double>(values->elements[i]);
      values->elements[i] = coeff / norm;
    }
  }

  pack_results(FunctionResult{}, out, FALCON_RESULT_SLOT_COUNT, oc);
}

} // extern "C"
