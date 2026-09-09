#include <falcon-typing/FFIHelpers.hpp>
#include "falcon-routine/math.hpp"
#include <vector>
#include <iostream>
#include <variant>
#include <algorithm>
#include <cstring>
#include <cmath>

using namespace falcon::typing;
using namespace falcon::typing::ffi::wrapper;

static std::shared_ptr<ArrayValue> get_array_from_params(const FalconParamEntry *entries, int32_t count, const char *key) {
  for (int32_t i = 0; i < count; ++i) {
    if (std::strcmp(entries[i].key, key) != 0) {
      continue;
    }
    const FalconParamEntry &e = entries[i];
    if (e.tag != FALCON_TYPE_OPAQUE) {
      throw std::runtime_error(std::string("parameter '") + key + "' is not OPAQUE");
    }
    std::string tn = e.value.opaque.type_name ? e.value.opaque.type_name : "";
    if (tn == "Array") {
      auto sv = *static_cast<std::shared_ptr<void> *>(e.value.opaque.ptr);
      return std::static_pointer_cast<ArrayValue>(sv);
    }
    if (tn == "ArrayValue") {
      return *static_cast<std::shared_ptr<ArrayValue> *>(e.value.opaque.ptr);
    }
  }
  throw std::runtime_error(std::string("parameter '") + key + "' not found");
}

static void pack_array_slot(std::shared_ptr<ArrayValue> arr, FalconResultSlot *slot) {
  slot->tag = FALCON_TYPE_OPAQUE;
  slot->value.opaque.type_name = "Array";
  slot->value.opaque.ptr =
      new std::shared_ptr<void>(std::static_pointer_cast<void>(arr));
  slot->value.opaque.deleter = [](void *p) {
    delete static_cast<std::shared_ptr<void> *>(p);
  };
}

static std::vector<double> extract_1d_vector(const std::shared_ptr<ArrayValue> &arr) {
  std::vector<double> vec;
  vec.reserve(arr->elements.size());
  for (const auto &elem : arr->elements) {
    if (std::holds_alternative<double>(elem)) {
      vec.push_back(std::get<double>(elem));
    } else if (std::holds_alternative<int64_t>(elem)) {
      vec.push_back(static_cast<double>(std::get<int64_t>(elem)));
    } else {
      throw std::runtime_error("Array elements must be numeric (float or int)");
    }
  }
  return vec;
}

static std::vector<std::vector<double>> extract_2d_vector(const std::shared_ptr<ArrayValue> &arr) {
  std::vector<std::vector<double>> mat;
  mat.reserve(arr->elements.size());
  for (const auto &elem : arr->elements) {
    if (std::holds_alternative<std::shared_ptr<ArrayValue>>(elem)) {
      mat.push_back(extract_1d_vector(std::get<std::shared_ptr<ArrayValue>>(elem)));
    } else if (std::holds_alternative<std::shared_ptr<StructInstance>>(elem)) {
      auto inst = std::get<std::shared_ptr<StructInstance>>(elem);
      if (inst && inst->native_handle) {
        auto sub_arr = std::static_pointer_cast<ArrayValue>(inst->native_handle.value());
        mat.push_back(extract_1d_vector(sub_arr));
      }
    } else {
      throw std::runtime_error("2D Array rows must be Array instances");
    }
  }
  return mat;
}

static std::shared_ptr<ArrayValue> make_float_array(const std::vector<double> &vec) {
  std::vector<RuntimeValue> elements;
  elements.reserve(vec.size());
  for (double val : vec) {
    elements.push_back(val);
  }
  return std::make_shared<ArrayValue>("float", std::move(elements));
}

extern "C" {

// ── Top-level Analytical Routines ──────────────────────────────────────────

void Sigmoid(const FalconParamEntry *params, int32_t param_count, FalconResultSlot *out, int32_t *oc) {
  auto pm = unpack_params(params, param_count);
  double x  = std::get<double>(pm.at("x"));
  double A  = std::get<double>(pm.at("A"));
  double x0 = std::get<double>(pm.at("x0"));
  double k  = std::get<double>(pm.at("k"));
  double b  = std::get<double>(pm.at("b"));
  double y = falcon::routine::sigmoid(x, A, x0, k, b);
  pack_results(FunctionResult{y}, out, 16, oc);
}

void PiecewiseLinear(const FalconParamEntry *params, int32_t param_count, FalconResultSlot *out, int32_t *oc) {
  auto pm = unpack_params(params, param_count);
  double x  = std::get<double>(pm.at("x"));
  double x0 = std::get<double>(pm.at("x0"));
  double x1 = std::get<double>(pm.at("x1"));
  double m1 = std::get<double>(pm.at("m1"));
  double m2 = std::get<double>(pm.at("m2"));
  double y0 = std::get<double>(pm.at("y0"));
  double y = falcon::routine::piecewise_linear(x, x0, x1, m1, m2, y0);
  pack_results(FunctionResult{y}, out, 16, oc);
}

void ChannelAccumulation2D(const FalconParamEntry *params, int32_t param_count, FalconResultSlot *out, int32_t *oc) {
  auto pm = unpack_params(params, param_count);
  double x  = std::get<double>(pm.at("x"));
  double y  = std::get<double>(pm.at("y"));
  double cx = std::get<double>(pm.at("cx"));
  double cy = std::get<double>(pm.at("cy"));
  double cm = std::get<double>(pm.at("cm"));
  double cr = std::get<double>(pm.at("cr"));
  double m1 = std::get<double>(pm.at("m1"));
  double m2 = std::get<double>(pm.at("m2"));
  double m3 = std::get<double>(pm.at("m3"));
  double bx = std::get<double>(pm.at("bx"));
  double dx = std::get<double>(pm.at("dx"));
  double dy = std::get<double>(pm.at("dy"));
  double dm = std::get<double>(pm.at("dm"));
  double dr = std::get<double>(pm.at("dr"));
  double z = falcon::routine::channel_accumulation_2d(x, y, cx, cy, cm, cr, m1, m2, m3, bx, dx, dy, dm, dr);
  pack_results(FunctionResult{z}, out, 16, oc);
}

// ── MathUtils Fitting Routines ──────────────────────────────────────────────

void STRUCTMathUtilsFitSigmoid(const FalconParamEntry *params, int32_t param_count, FalconResultSlot *out, int32_t *oc) {
  auto x_arr = get_array_from_params(params, param_count, "x");
  auto y_arr = get_array_from_params(params, param_count, "y");

  auto x_vec = extract_1d_vector(x_arr);
  auto y_vec = extract_1d_vector(y_arr);

  if (x_vec.empty() || y_vec.empty() || x_vec.size() != y_vec.size()) {
    pack_results(FunctionResult{0.0, 0.0, 0.0, 0.0, 0.0, false}, out, 16, oc);
    return;
  }

  auto model = [](std::vector<double> xv, std::vector<double> pv) {
    std::vector<double> yv(xv.size());
    for (size_t i = 0; i < xv.size(); ++i) {
      yv[i] = falcon::routine::sigmoid(xv[i], pv[0], pv[1], pv[2], pv[3]);
    }
    return yv;
  };

  falcon::routine::fitting_parameters fit_params;
  // Estimate baseline and max
  double min_y = *std::min_element(y_vec.begin(), y_vec.end());
  double max_y = *std::max_element(y_vec.begin(), y_vec.end());
  double min_x = *std::min_element(x_vec.begin(), x_vec.end());
  double max_x = *std::max_element(x_vec.begin(), x_vec.end());
  double mid_x = 0.5 * (min_x + max_x);

  fit_params.initial_guess = { max_y - min_y, mid_x, 10.0, min_y };

  auto res = falcon::routine::curvefit1D(model, x_vec, y_vec, fit_params);
  if (res.success && res.coefficients.size() >= 4) {
    pack_results(FunctionResult{res.coefficients[0], res.coefficients[1], res.coefficients[2], res.coefficients[3], res.r_squared, true}, out, 16, oc);
  } else {
    pack_results(FunctionResult{0.0, 0.0, 0.0, 0.0, 0.0, false}, out, 16, oc);
  }
}

void STRUCTMathUtilsFitSigmoidWithGuess(const FalconParamEntry *params, int32_t param_count, FalconResultSlot *out, int32_t *oc) {
  auto pm = unpack_params(params, param_count);
  auto x_arr = get_array_from_params(params, param_count, "x");
  auto y_arr = get_array_from_params(params, param_count, "y");

  double g_A  = std::get<double>(pm.at("guess_A"));
  double g_x0 = std::get<double>(pm.at("guess_x0"));
  double g_k  = std::get<double>(pm.at("guess_k"));
  double g_b  = std::get<double>(pm.at("guess_b"));

  auto x_vec = extract_1d_vector(x_arr);
  auto y_vec = extract_1d_vector(y_arr);

  if (x_vec.empty() || y_vec.empty() || x_vec.size() != y_vec.size()) {
    pack_results(FunctionResult{0.0, 0.0, 0.0, 0.0, 0.0, false}, out, 16, oc);
    return;
  }

  auto model = [](std::vector<double> xv, std::vector<double> pv) {
    std::vector<double> yv(xv.size());
    for (size_t i = 0; i < xv.size(); ++i) {
      yv[i] = falcon::routine::sigmoid(xv[i], pv[0], pv[1], pv[2], pv[3]);
    }
    return yv;
  };

  falcon::routine::fitting_parameters fit_params;
  fit_params.initial_guess = { g_A, g_x0, g_k, g_b };

  auto res = falcon::routine::curvefit1D(model, x_vec, y_vec, fit_params);
  if (res.success && res.coefficients.size() >= 4) {
    pack_results(FunctionResult{res.coefficients[0], res.coefficients[1], res.coefficients[2], res.coefficients[3], res.r_squared, true}, out, 16, oc);
  } else {
    pack_results(FunctionResult{0.0, 0.0, 0.0, 0.0, 0.0, false}, out, 16, oc);
  }
}

void STRUCTMathUtilsFitPiecewiseLinear(const FalconParamEntry *params, int32_t param_count, FalconResultSlot *out, int32_t *oc) {
  auto x_arr = get_array_from_params(params, param_count, "x");
  auto y_arr = get_array_from_params(params, param_count, "y");

  auto x_vec = extract_1d_vector(x_arr);
  auto y_vec = extract_1d_vector(y_arr);

  if (x_vec.empty() || y_vec.empty() || x_vec.size() != y_vec.size()) {
    pack_results(FunctionResult{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, false}, out, 16, oc);
    return;
  }

  auto model = [](std::vector<double> xv, std::vector<double> pv) {
    std::vector<double> yv(xv.size());
    for (size_t i = 0; i < xv.size(); ++i) {
      yv[i] = falcon::routine::piecewise_linear(xv[i], pv[0], pv[1], pv[2], pv[3], pv[4]);
    }
    return yv;
  };

  double min_y = *std::min_element(y_vec.begin(), y_vec.end());
  double max_y = *std::max_element(y_vec.begin(), y_vec.end());
  double min_x = *std::min_element(x_vec.begin(), x_vec.end());
  double max_x = *std::max_element(x_vec.begin(), x_vec.end());
  double span_x = std::max(max_x - min_x, 1e-6);

  falcon::routine::fitting_parameters fit_params;
  fit_params.initial_guess = {
    min_x + span_x * 0.33,
    min_x + span_x * 0.66,
    0.0,
    (max_y - min_y) / span_x,
    min_y
  };

  auto res = falcon::routine::curvefit1D(model, x_vec, y_vec, fit_params);
  if (res.success && res.coefficients.size() >= 5) {
    pack_results(FunctionResult{
      res.coefficients[0], res.coefficients[1], res.coefficients[2],
      res.coefficients[3], res.coefficients[4], res.r_squared, true
    }, out, 16, oc);
  } else {
    pack_results(FunctionResult{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, false}, out, 16, oc);
  }
}

void STRUCTMathUtilsFitPiecewiseLinearWithGuess(const FalconParamEntry *params, int32_t param_count, FalconResultSlot *out, int32_t *oc) {
  auto pm = unpack_params(params, param_count);
  auto x_arr = get_array_from_params(params, param_count, "x");
  auto y_arr = get_array_from_params(params, param_count, "y");

  double g_x0 = std::get<double>(pm.at("guess_x0"));
  double g_x1 = std::get<double>(pm.at("guess_x1"));
  double g_m1 = std::get<double>(pm.at("guess_m1"));
  double g_m2 = std::get<double>(pm.at("guess_m2"));
  double g_y0 = std::get<double>(pm.at("guess_y0"));

  auto x_vec = extract_1d_vector(x_arr);
  auto y_vec = extract_1d_vector(y_arr);

  if (x_vec.empty() || y_vec.empty() || x_vec.size() != y_vec.size()) {
    pack_results(FunctionResult{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, false}, out, 16, oc);
    return;
  }

  auto model = [](std::vector<double> xv, std::vector<double> pv) {
    std::vector<double> yv(xv.size());
    for (size_t i = 0; i < xv.size(); ++i) {
      yv[i] = falcon::routine::piecewise_linear(xv[i], pv[0], pv[1], pv[2], pv[3], pv[4]);
    }
    return yv;
  };

  falcon::routine::fitting_parameters fit_params;
  fit_params.initial_guess = { g_x0, g_x1, g_m1, g_m2, g_y0 };

  auto res = falcon::routine::curvefit1D(model, x_vec, y_vec, fit_params);
  if (res.success && res.coefficients.size() >= 5) {
    pack_results(FunctionResult{
      res.coefficients[0], res.coefficients[1], res.coefficients[2],
      res.coefficients[3], res.coefficients[4], res.r_squared, true
    }, out, 16, oc);
  } else {
    pack_results(FunctionResult{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, false}, out, 16, oc);
  }
}

void STRUCTMathUtilsFitChannelAccumulation2D(const FalconParamEntry *params, int32_t param_count, FalconResultSlot *out, int32_t *oc) {
  auto x_arr = get_array_from_params(params, param_count, "x");
  auto y_arr = get_array_from_params(params, param_count, "y");
  auto z_arr = get_array_from_params(params, param_count, "z");

  auto x_mat = extract_2d_vector(x_arr);
  auto y_mat = extract_2d_vector(y_arr);
  auto z_mat = extract_2d_vector(z_arr);

  if (x_mat.empty() || y_mat.empty() || z_mat.empty() ||
      x_mat.size() != y_mat.size() || x_mat.size() != z_mat.size()) {
    auto empty_arr = std::make_shared<ArrayValue>("float");
    pack_array_slot(empty_arr, &out[0]);
    out[1] = {};
    out[1].tag = FALCON_TYPE_FLOAT;
    out[1].value.float_val = 0.0;
    out[2] = {};
    out[2].tag = FALCON_TYPE_BOOL;
    out[2].value.bool_val = 0;
    *oc = 3;
    return;
  }

  auto model = [](std::vector<std::vector<double>> xv,
                  std::vector<std::vector<double>> yv,
                  std::vector<std::vector<double>> pv) {
    std::vector<double> p = pv[0];
    std::vector<std::vector<double>> zv(xv.size(), std::vector<double>(xv[0].size()));
    for (size_t i = 0; i < xv.size(); ++i) {
      for (size_t j = 0; j < xv[i].size(); ++j) {
        zv[i][j] = falcon::routine::channel_accumulation_2d(
            xv[i][j], yv[i][j], p[0], p[1], p[2], p[3],
            p[4], p[5], p[6], p[7], p[8], p[9],
            p[10], p[11]);
      }
    }
    return zv;
  };

  falcon::routine::fitting_parameters fit_params;
  fit_params.initial_guess = {
      0.5, 0.5,  1.0, 10.0, // cx, cy, cm, cr
      1.0, -1.0, 0.5,       // m1, m2, m3
      0.2, 0.8,  0.5,       // bx, dx, dy
      0.5, 5.0              // dm, dr
  };
  std::vector<std::pair<double, double>> bounds;
  for (double val : *fit_params.initial_guess) {
    bounds.push_back({val - 2.0, val + 2.0});
  }
  fit_params.bounds = bounds;

  auto res = falcon::routine::curvefit2D(model, x_mat, y_mat, z_mat, fit_params);
  if (res.success && res.coefficients.size() >= 12) {
    auto res_arr = make_float_array(res.coefficients);
    pack_array_slot(res_arr, &out[0]);
    out[1] = {};
    out[1].tag = FALCON_TYPE_FLOAT;
    out[1].value.float_val = res.r_squared;
    out[2] = {};
    out[2].tag = FALCON_TYPE_BOOL;
    out[2].value.bool_val = 1;
    *oc = 3;
  } else {
    auto empty_arr = std::make_shared<ArrayValue>("float");
    pack_array_slot(empty_arr, &out[0]);
    out[1] = {};
    out[1].tag = FALCON_TYPE_FLOAT;
    out[1].value.float_val = 0.0;
    out[2] = {};
    out[2].tag = FALCON_TYPE_BOOL;
    out[2].value.bool_val = 0;
    *oc = 3;
  }
}

} // extern "C"
