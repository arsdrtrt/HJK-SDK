#include "ircam.hpp"
#include <cmath>
#include <limits>

namespace ircam {
namespace measure {

float temperature(const ircam::Frame &f, int x, int y) {
    return f.temp_at(x, y);
}

Stats stats_rect(const ircam::Frame &f, Rect r) {
    Stats s;
    if (r.x < 0) r.x = 0;
    if (r.y < 0) r.y = 0;
    if (r.x + r.w > f.width)  r.w = f.width  - r.x;
    if (r.y + r.h > f.height) r.h = f.height - r.y;
    if (r.w <= 0 || r.h <= 0) return s;

    s.t_min = std::numeric_limits<float>::max();
    s.t_max = -std::numeric_limits<float>::max();
    double sum = 0;

    for (int y = r.y; y < r.y + r.h; y++) {
        for (int x = r.x; x < r.x + r.w; x++) {
            float t = f.temp_at(x, y);
            if (t < s.t_min) { s.t_min = t; s.x_min = x; s.y_min = y; }
            if (t > s.t_max) { s.t_max = t; s.x_max = x; s.y_max = y; }
            sum += t;
            s.count++;
        }
    }
    s.t_mean = (float)(sum / s.count);
    return s;
}

Stats stats_full(const ircam::Frame &f) {
    return stats_rect(f, Rect{0, 0, f.width, f.height});
}

} // namespace measure
} // namespace ircam
