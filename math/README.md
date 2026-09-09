# `math` (Falcon Standard Library)

The `math` package provides mathematical model evaluation and robust nonlinear curve fitting routines for quantum device autotuning and transport data analysis.

## Features

- **Sigmoid Model**: Direct evaluation and 1D fitting with initial parameter estimation and bounds.
- **Piecewise Linear Model**: Direct evaluation and 1D fitting for transition/regime identification.
- **2D Channel Accumulation Model**: Direct evaluation and Differential Evolution (DE) optimization for 2D transport maps.

## Usage

```falcon
import (
  "../../std-lib/collections/array/array.fal"
  "../../std-lib/math/math.fal"
)

// Evaluate analytical models
float y = Sigmoid(0.0, 1.0, 0.0, 1.0, 0.0);

// Fit 1D data to a sigmoid curve
Array<float> x = Array.New();
Array<float> y = Array.New();
// Populate data...
float A, float x0, float k, float b, float r2, bool ok = MathUtils.FitSigmoid(x, y);
```
