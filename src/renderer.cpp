#include "ircam.hpp"
#include <cstring>
#include <cmath>
#include <algorithm>

namespace ircam {

static void build_palette(int id, uint8_t lut[256][3]) {
    for (int i = 0; i < 256; i++) {
        float x = i / 255.0f;
        float r = 0, g = 0, b = 0;
        switch (id) {
        case 0: r = g = b = x; break;
        case 1: r = g = b = 1.0f - x; break;
        case 2: {
            r = 1.5f - fabsf(3*x - 2.2f); r = r < 0 ? 0 : (r > 1 ? 1 : r);
            g = 1.5f - fabsf(3*x - 1.5f) - 0.3f; g = g < 0 ? 0 : (g > 1 ? 1 : g);
            b = 1.5f - fabsf(3*x - 0.4f); b = b < 0 ? 0 : (b > 1 ? 1 : b);
            break;
        }
        case 3: {
            float h = (1.0f - x) * 0.75f;
            float c = 1.0f;
            float xx = c * (1 - fabsf(fmodf(h * 6, 2) - 1));
            int seg = (int)(h * 6);
            switch (seg) {
                case 0: r = c; g = xx; break;
                case 1: r = xx; g = c; break;
                case 2: g = c; b = xx; break;
                case 3: g = xx; b = c; break;
                case 4: r = xx; b = c; break;
                default: r = c; b = xx; break;
            }
            break;
        }
        case 4: {
            r = 1.5f - fabsf(4*x - 3); r = r < 0 ? 0 : (r > 1 ? 1 : r);
            g = 1.5f - fabsf(4*x - 2); g = g < 0 ? 0 : (g > 1 ? 1 : g);
            b = 1.5f - fabsf(4*x - 1); b = b < 0 ? 0 : (b > 1 ? 1 : b);
            break;
        }
        }
        lut[i][0] = (uint8_t)(b * 255);
        lut[i][1] = (uint8_t)(g * 255);
        lut[i][2] = (uint8_t)(r * 255);
    }
}

static void destripe_1d(uint16_t *img, int w, int h, bool vertical) {
    int n = vertical ? w : h;
    int m = vertical ? h : w;

    static thread_local std::vector<double> mean, smooth, kw;
    mean.resize(n);
    smooth.resize(n);

    for (int i = 0; i < n; i++) {
        double s = 0;
        for (int j = 0; j < m; j++) {
            int v = vertical ? img[j*w + i] : img[i*w + j];
            s += v;
        }
        mean[i] = s / m;
    }

    const int R = 32;
    const double sigma = 16.0;
    if (kw.empty()) {
        kw.resize(2*R + 1);
        for (int k = -R; k <= R; k++)
            kw[k+R] = exp(-(double)(k*k) / (2*sigma*sigma));
    }
    double wsum = 0;
    for (int k = -R; k <= R; k++) wsum += kw[k+R];

    for (int i = 0; i < n; i++) {
        double s = 0;
        for (int k = -R; k <= R; k++) {
            int ii = i + k;
            if (ii < 0) ii = 0;
            if (ii >= n) ii = n - 1;
            s += mean[ii] * kw[k+R];
        }
        smooth[i] = s / wsum;
    }

    for (int i = 0; i < n; i++) {
        int corr = (int)std::lround(mean[i] - smooth[i]);
        for (int j = 0; j < m; j++) {
            int idx = vertical ? (j*w + i) : (i*w + j);
            int v = (int)img[idx] - corr;
            v = v < 0 ? 0 : (v > 65535 ? 65535 : v);
            img[idx] = (uint16_t)v;
        }
    }
}

struct Renderer::Impl {
    int W, H;
    RenderParams params;
    std::vector<uint16_t> work, prev;
    std::vector<uint8_t>  gray, gray2;
    std::vector<float>    base;
    std::vector<uint32_t> hist;
    std::vector<uint8_t>  lut_pe;
    uint8_t pal[256][3];

    Impl(int w, int h) : W(w), H(h) {
        size_t n = (size_t)w * h;
        work.resize(n);
        prev.resize(n);
        gray.resize(n);
        gray2.resize(n);
        base.resize(n);
        hist.resize(65536);
        lut_pe.resize(65536);
        build_palette(0, pal);
    }
};

Renderer::Renderer(int w, int h) : p(new Impl(w, h)) {}
Renderer::~Renderer() {}

void Renderer::set_params(const RenderParams &pp) {
    p->params = pp;
    build_palette(pp.palette, p->pal);
}

RenderParams Renderer::params() const { return p->params; }

void Renderer::reset() {
    std::fill(p->prev.begin(), p->prev.end(), 0);
}

static void tnr(uint16_t *cur, uint16_t *prev, int n, float alpha, float range) {
    for (int i = 0; i < n; i++) {
        int diff = std::abs((int)cur[i] - (int)prev[i]);
        float k = alpha * (1.0f - (float)diff / range);
        if (k < 0) k = 0;
        if (k > 1) k = 1;
        float v = cur[i] + k * (prev[i] - cur[i]);
        cur[i] = (uint16_t)(v < 0 ? 0 : (v > 65535 ? 65535 : v));
    }
    std::memcpy(prev, cur, n * sizeof(uint16_t));
}

static void guided_base(const uint8_t *src, float *base, int w, int h, int r) {
    static thread_local std::vector<float> ii;
    ii.assign((size_t)(w+1) * (h+1), 0.0f);

    for (int y = 1; y <= h; y++) {
        float row = 0;
        for (int x = 1; x <= w; x++) {
            row += (float)src[(y-1)*w + (x-1)];
            ii[y*(w+1)+x] = ii[(y-1)*(w+1)+x] + row;
        }
    }
    for (int y = 0; y < h; y++) {
        int y0 = y - r; if (y0 < 0) y0 = 0;
        int y1 = y + r; if (y1 > h-1) y1 = h-1;
        for (int x = 0; x < w; x++) {
            int x0 = x - r; if (x0 < 0) x0 = 0;
            int x1 = x + r; if (x1 > w-1) x1 = w-1;
            float s = ii[(y1+1)*(w+1)+(x1+1)] - ii[y0*(w+1)+(x1+1)]
                    - ii[(y1+1)*(w+1)+x0] + ii[y0*(w+1)+x0];
            float area = (float)((x1-x0+1) * (y1-y0+1));
            base[y*w+x] = s / area;
        }
    }
}

void Renderer::render(const ircam::Frame &src,
                      std::vector<uint8_t> &gray_out,
                      std::vector<uint8_t> &rgb_out)
{
    int w = p->W, h = p->H, n = w * h;
    const RenderParams &pp = p->params;

    if ((int)src.data.size() < n) return;

    gray_out.resize(n);
    rgb_out.resize((size_t)n * 3);

    std::memcpy(p->work.data(), src.data.data(), n * sizeof(uint16_t));

    if (pp.destripe) {
        destripe_1d(p->work.data(), w, h, true);
        destripe_1d(p->work.data(), w, h, false);
    }

    if (pp.tnr_alpha > 0.01f) {
        tnr(p->work.data(), p->prev.data(), n, pp.tnr_alpha, 2048.0f);
    } else {
        std::memcpy(p->prev.data(), p->work.data(), n * sizeof(uint16_t));
    }

    std::fill(p->hist.begin(), p->hist.end(), 0);
    for (int i = 0; i < n; i++) p->hist[p->work[i]]++;

    uint32_t occupied = 0;
    for (int i = 0; i < 65536; i++) if (p->hist[i]) occupied++;
    if (!occupied) occupied = 1;
    uint32_t plateau = (uint32_t)((double)n / occupied * pp.plateau_k);
    if (plateau < 1) plateau = 1;

    {
        uint32_t s = 0;
        double scale = (double)pp.contrast * 255.0 / (double)plateau;
        for (int i = 0; i < 65536; i++) {
            uint32_t hv = p->hist[i];
            if (hv > plateau) hv = plateau;
            s += hv;
            int v = (int)std::ceil((double)s * scale);
            v = v < 0 ? 0 : (v > 255 ? 255 : v);
            p->lut_pe[i] = (uint8_t)v;
        }
    }

    for (int i = 0; i < n; i++) p->gray[i] = p->lut_pe[p->work[i]];

    if (pp.dde_gain > 0.01f) {
        guided_base(p->gray.data(), p->base.data(), w, h, pp.dde_radius);
        for (int i = 0; i < n; i++) {
            float b = p->base[i];
            float d = (float)p->gray[i] - b;
            float g = pp.dde_gain * (1.0f + fabsf(d) / 128.0f);
            float v = b + d * g;
            p->gray2[i] = (uint8_t)(v < 0 ? 0 : (v > 255 ? 255 : v));
        }
    } else {
        std::memcpy(p->gray2.data(), p->gray.data(), n);
    }

    if (pp.gamma > 0.01f && fabsf(pp.gamma - 1.0f) > 0.01f) {
        static thread_local uint8_t gl[256];
        for (int i = 0; i < 256; i++) {
            float v = 255.0f * powf(i / 255.0f, 1.0f / pp.gamma);
            gl[i] = (uint8_t)(v < 0 ? 0 : (v > 255 ? 255 : v));
        }
        for (int i = 0; i < n; i++) p->gray2[i] = gl[p->gray2[i]];
    }

    std::memcpy(gray_out.data(), p->gray2.data(), n);

    for (int i = 0; i < n; i++) {
        const uint8_t *c = p->pal[p->gray2[i]];
        rgb_out[i*3+0] = c[0];
        rgb_out[i*3+1] = c[1];
        rgb_out[i*3+2] = c[2];
    }
}

} // namespace ircam
