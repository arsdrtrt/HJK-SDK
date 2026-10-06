#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

struct tagUSB_THERMOMETRY_PARAM;
typedef struct tagUSB_THERMOMETRY_PARAM USB_THERMOMETRY_PARAM;

namespace ircam {

struct Frame {
    int width = 0;
    int height = 0;
    std::vector<uint16_t> data;
    uint32_t index = 0;

    uint16_t fpa_temp_raw = 0;
    uint16_t env_temp_raw = 0;
    uint8_t  temp_div    = 100;
    uint8_t  measure_sel = 1;
    uint8_t  lens        = 0;
    uint8_t  fps         = 0;

    float temp_at(int x, int y) const {
        if (x < 0 || x >= width || y < 0 || y >= height) return 0.0f;
        return (data[y * width + x] - 10000.0f) / (float)temp_div;
    }
};

struct RenderParams {
    float plateau_k  = 4.0f;
    float contrast   = 1.0f;
    float dde_gain   = 1.5f;
    int   dde_radius = 4;
    bool  destripe   = true;
    float tnr_alpha  = 0.4f;
    float gamma      = 1.0f;
    int   palette    = 0;
};

struct Rect { int x, y, w, h; };

struct Stats {
    float t_min = 0, t_max = 0, t_mean = 0;
    int   x_min = 0, y_min = 0;
    int   x_max = 0, y_max = 0;
    int   count = 0;
};

class Camera {
public:
    Camera();
    ~Camera();

    bool open(int device_index = 0);
    void close();
    bool is_open() const;

    std::string serial() const;
    std::string firmware() const;
    std::string model() const;

    using FrameCallback = std::function<void(const Frame&)>;
    void set_callback(FrameCallback cb);
    bool get_frame(Frame &out, int timeout_ms = 1000);

    bool set_brightness(int v);
    bool set_contrast(int v);
    bool set_noise_reduce(int temporal, int spatial);
    bool set_flip(int mode);
    int  get_flip();

    bool get_thermal_param(USB_THERMOMETRY_PARAM *out);
    bool set_thermal_param(const USB_THERMOMETRY_PARAM *in);

    bool manual_correct();
    bool reset();

    bool upgrade_start(const std::string &firmware_path);
    int  upgrade_state();
    bool upgrade_close();

    Camera(const Camera&) = delete;
    Camera& operator=(const Camera&) = delete;

    struct Impl;

private:
    std::unique_ptr<Impl> p;
};

class Renderer {
public:
    Renderer(int width, int height);
    ~Renderer();
    void set_params(const RenderParams &p);
    RenderParams params() const;
    void render(const Frame &src,
                std::vector<uint8_t> &gray_out,
                std::vector<uint8_t> &rgb_out);
    void reset();
private:
    struct Impl;
    std::unique_ptr<Impl> p;
};

namespace measure {
    float temperature(const Frame &f, int x, int y);
    Stats stats_rect(const Frame &f, Rect r);
    Stats stats_full(const Frame &f);
}

const char* version();

} // namespace ircam

