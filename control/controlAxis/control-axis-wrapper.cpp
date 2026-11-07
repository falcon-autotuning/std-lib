#include <falcon-typing/FFIHelpers.hpp>
#include <memory>

using namespace falcon::typing;
using namespace falcon::typing::ffi::wrapper;

constexpr int FALCON_RESULT_SLOT_COUNT = 16;

// ---------------------------------------------------------------------------
// Generic helpers
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

// ---------------------------------------------------------------------------
// Direction helpers
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// Point helpers
// ---------------------------------------------------------------------------

static std::shared_ptr<StructInstance>
axis_offset(std::shared_ptr<StructInstance> axis) {
  return get_struct_field(axis, "offset_");
}

static std::shared_ptr<StructInstance>
axis_direction(std::shared_ptr<StructInstance> axis) {
  return get_struct_field(axis, "direction_");
}

static std::shared_ptr<StructInstance>
point_map(std::shared_ptr<StructInstance> point) {
  return get_struct_field(point, "location_");
}

static std::shared_ptr<ArrayValue>
point_keys(std::shared_ptr<StructInstance> point) {
  return get_array_field(point_map(point), "keys_");
}

static std::shared_ptr<ArrayValue>
point_values(std::shared_ptr<StructInstance> point) {
  return get_array_field(point_map(point), "values_");
}

static double voltage_value(std::shared_ptr<StructInstance> voltage) {
  return std::get<double>(voltage->get_field("value_"));
}

static void set_voltage_value(std::shared_ptr<StructInstance> voltage,
                              double value) {
  voltage->set_field("value_", RuntimeValue(value));
}

static std::shared_ptr<StructInstance> make_array_struct() {

  auto arr = std::make_shared<StructInstance>("Array");

  arr->native_handle = std::make_shared<ArrayValue>();

  return arr;
}

// ---------------------------------------------------------------------------
// Deep Clone
// ---------------------------------------------------------------------------

static RuntimeValue clone_runtime_value(const RuntimeValue &value);

static bool is_array_type(const std::string &type_name) {
  return type_name == "Array" || type_name.starts_with("Array<");
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

static std::shared_ptr<StructInstance>
clone_struct(const std::shared_ptr<StructInstance> &src) {

  if (!src) {
    return nullptr;
  }

  auto result = std::make_shared<StructInstance>(src->type_name);

  //
  // Clone native storage.
  //
  if (src->native_handle.has_value()) {

    if (is_array_type(src->type_name)) {

      auto array =
          std::static_pointer_cast<ArrayValue>(src->native_handle.value());

      result->native_handle = clone_array(array);

    } else {

      result->native_handle = src->native_handle;
    }
  }

  for (const auto &[name, field] : *src->fields) {
    result->set_field(name, clone_runtime_value(field));
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

static std::shared_ptr<ArrayValue>
array_items(std::shared_ptr<StructInstance> arr) {

  return std::static_pointer_cast<ArrayValue>(arr->native_handle.value());
}

static bool runtime_equal(const RuntimeValue &lhs, const RuntimeValue &rhs) {

  if (std::holds_alternative<std::shared_ptr<StructInstance>>(lhs) &&
      std::holds_alternative<std::shared_ptr<StructInstance>>(rhs)) {

    auto lhs_struct = std::get<std::shared_ptr<StructInstance>>(lhs);

    auto rhs_struct = std::get<std::shared_ptr<StructInstance>>(rhs);

    if (!lhs_struct || !rhs_struct) {
      return lhs_struct == rhs_struct;
    }

    return *lhs_struct == *rhs_struct;
  }

  return lhs == rhs;
}

extern "C" {
void STRUCTControlAxisNewFromPoints(const FalconParamEntry *p, int32_t pc,
                                    FalconResultSlot *out, int32_t *oc) {

  auto pm = unpack_params(p, pc);
  auto offset = std::get<std::shared_ptr<StructInstance>>(pm.at("offset"));
  auto point = std::get<std::shared_ptr<StructInstance>>(pm.at("point"));
  auto axis = std::make_shared<StructInstance>("ControlAxis");

  axis->set_field("offset_", clone_struct(offset));

  auto direction = std::make_shared<StructInstance>("Direction");
  auto direction_map = std::make_shared<StructInstance>("Map");
  auto keys = make_array_struct();
  auto values = make_array_struct();
  direction_map->set_field("keys_", keys);
  direction_map->set_field("values_", values);
  direction->set_field("dir_", direction_map);

  auto key_items = array_items(keys);
  auto value_items = array_items(values);

  auto offset_keys = point_keys(offset);
  auto offset_values = point_values(offset);
  auto point_values_arr = point_values(point);

  for (size_t i = 0; i < offset_keys->elements.size(); ++i) {

    auto gate = offset_keys->elements[i];
    auto off_voltage =
        std::get<std::shared_ptr<StructInstance>>(offset_values->elements[i]);
    auto point_voltage = std::get<std::shared_ptr<StructInstance>>(
        point_values_arr->elements[i]);
    double off = voltage_value(off_voltage);
    double pt = voltage_value(point_voltage);
    key_items->elements.push_back(gate);
    value_items->elements.push_back(pt - off);
  }

  axis->set_field("direction_", direction);

  pack_results(FunctionResult{axis}, out, FALCON_RESULT_SLOT_COUNT, oc);
}

void STRUCTControlAxisShifted(const FalconParamEntry *p, int32_t pc,
                              FalconResultSlot *out, int32_t *oc) {

  auto pm = unpack_params(p, pc);
  auto self = std::get<std::shared_ptr<StructInstance>>(pm.at("this"));
  auto shift = std::get<std::shared_ptr<StructInstance>>(pm.at("dir"));

  auto axis = clone_struct(self);
  auto offset = axis_offset(axis);
  auto offset_keys = point_keys(offset);
  auto offset_values = point_values(offset);
  auto shift_keys = direction_keys(shift);
  auto shift_values = direction_values(shift);

  for (size_t i = 0; i < shift_keys->elements.size(); ++i) {
    for (size_t j = 0; j < offset_keys->elements.size(); ++j) {

      if (runtime_equal(shift_keys->elements[i], offset_keys->elements[j])) {

        auto voltage = std::get<std::shared_ptr<StructInstance>>(
            offset_values->elements[j]);
        double current = voltage_value(voltage);
        double delta = std::get<double>(shift_values->elements[i]);
        set_voltage_value(voltage, current + delta);
        break;
      }
    }
  }
  auto debug_voltage = std::get<std::shared_ptr<StructInstance>>(
      point_values(axis_offset(axis))->elements[0]);

  pack_results(FunctionResult{axis}, out, FALCON_RESULT_SLOT_COUNT, oc);
}

void STRUCTControlAxisZeroOffsets(const FalconParamEntry *p, int32_t pc,
                                  FalconResultSlot *out, int32_t *oc) {

  auto pm = unpack_params(p, pc);
  auto self = std::get<std::shared_ptr<StructInstance>>(pm.at("this"));

  auto values = point_values(axis_offset(self));
  for (auto &entry : values->elements) {
    auto voltage = std::get<std::shared_ptr<StructInstance>>(entry);
    set_voltage_value(voltage, 0.0);
  }

  pack_results(FunctionResult{}, out, FALCON_RESULT_SLOT_COUNT, oc);
}

void STRUCTControlAxisToPoint(const FalconParamEntry *p, int32_t pc,
                              FalconResultSlot *out, int32_t *oc) {

  auto pm = unpack_params(p, pc);
  auto self = std::get<std::shared_ptr<StructInstance>>(pm.at("this"));
  auto coordinate =
      std::get<std::shared_ptr<StructInstance>>(pm.at("coordinate"));

  double coord = voltage_value(coordinate);
  auto point = clone_struct(axis_offset(self));
  auto keys = point_keys(point);
  auto values = point_values(point);
  auto dir = axis_direction(self);
  auto dir_keys = direction_keys(dir);
  auto dir_values = direction_values(dir);

  for (size_t i = 0; i < keys->elements.size(); ++i) {
    auto voltage =
        std::get<std::shared_ptr<StructInstance>>(values->elements[i]);
    double offset = voltage_value(voltage);
    double coeff = 0.0;

    for (size_t j = 0; j < dir_keys->elements.size(); ++j) {
      if (runtime_equal(dir_keys->elements[i], dir_keys->elements[j])) {
        coeff = std::get<double>(dir_values->elements[j]);
        break;
      }
    }
    set_voltage_value(voltage, offset + coord * coeff);
  }
  auto debug_voltage = std::get<std::shared_ptr<StructInstance>>(
      point_values(point)->elements[0]);

  pack_results(FunctionResult{point}, out, FALCON_RESULT_SLOT_COUNT, oc);
}
}
