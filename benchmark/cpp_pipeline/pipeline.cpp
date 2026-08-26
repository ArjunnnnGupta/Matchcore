// Same conceptual 3-stage pipeline as python_pipeline/pipeline.py's
// row_loop(): parse tick -> update best-bid/ask -> emit a signal (mid vs its
// own moving average). Deliberately mirrors the Python row-loop's logic
// exactly (not the vectorized version) so the C++/Python comparison isolates
// interpreter/allocation overhead rather than algorithmic differences.
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>

#include <cmath>
#include <deque>

namespace py = pybind11;

// sides: int8 array, 0 = buy, 1 = sell. prices: float64 array. Same length.
py::array_t<int8_t> row_loop(py::array_t<int8_t> sides, py::array_t<double> prices, int window) {
    auto sides_buf = sides.request();
    auto prices_buf = prices.request();
    const int8_t* sides_ptr = static_cast<const int8_t*>(sides_buf.ptr);
    const double* prices_ptr = static_cast<const double*>(prices_buf.ptr);
    const size_t n = static_cast<size_t>(prices_buf.shape[0]);

    py::array_t<int8_t> signals(n);
    int8_t* signals_ptr = static_cast<int8_t*>(signals.request().ptr);

    double best_bid = std::nan("");
    double best_ask = std::nan("");
    std::deque<double> window_vals;
    double window_sum = 0.0;

    for (size_t i = 0; i < n; ++i) {
        if (sides_ptr[i] == 0) {
            best_bid = prices_ptr[i];
        } else {
            best_ask = prices_ptr[i];
        }

        signals_ptr[i] = 0;
        if (!std::isnan(best_bid) && !std::isnan(best_ask)) {
            const double mid = (best_bid + best_ask) / 2.0;
            if (static_cast<int>(window_vals.size()) == window) {
                window_sum -= window_vals.front();
                window_vals.pop_front();
            }
            window_vals.push_back(mid);
            window_sum += mid;
            const double moving_avg = window_sum / static_cast<double>(window_vals.size());
            if (mid > moving_avg) {
                signals_ptr[i] = 1;
            } else if (mid < moving_avg) {
                signals_ptr[i] = -1;
            }
        }
    }
    return signals;
}

PYBIND11_MODULE(cpp_pipeline, m) {
    m.def("row_loop", &row_loop, "Row-by-row tick pipeline: book update + signal (C++)");
}
