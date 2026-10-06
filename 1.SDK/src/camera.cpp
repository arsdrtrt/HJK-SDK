#include "ircam.hpp"
#include <cstdio>
#include <cstring>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

// SDK-заголовки: usbsdk.h для функций, IRSDK.h для struct Frame (Y16 буфер)
#include "usbsdk.h"
#include "IRSDK.h"

namespace ircam {

struct Camera::Impl {
    int  uid = -1;
    bool opened = false;
    std::string fw, hw, model, sn;

    FrameCallback user_cb;
    std::mutex cb_mtx;

    Frame latest;
    std::mutex frame_mtx;
    std::condition_variable frame_cv;
    std::atomic<uint32_t> frame_count{0};
};

static Camera::Impl* g_active = nullptr;

// SDK callback
static void sdk_frame_cb(void *data, void *user) {
    (void)user;
    if (!g_active) return;

    // ::Frame — это SDK-шный Frame из IRSDK.h (глобальный namespace)
    ::Frame *sf = (::Frame*)data;
    if (!sf || !sf->width || !sf->height) return;

    ircam::Frame out;
    out.width  = sf->width;
    out.height = sf->height;
    out.index  = sf->u32FrameIndex;
    out.fpa_temp_raw = sf->u16FpaTemp;
    out.env_temp_raw = sf->u16EnvTemp;
    out.temp_div    = sf->u8TempDiv;
    out.measure_sel = sf->u8MeasureSel;
    out.lens        = sf->u8Lens;
    out.fps         = sf->u8Fps;

    size_t n = (size_t)out.width * out.height;
    out.data.assign(sf->buffer, sf->buffer + n);

    Camera::Impl *impl = g_active;
    {
        std::lock_guard<std::mutex> lk(impl->frame_mtx);
        impl->latest = out;
        impl->frame_count.fetch_add(1);
    }
    impl->frame_cv.notify_all();

    std::lock_guard<std::mutex> lk(impl->cb_mtx);
    if (impl->user_cb) impl->user_cb(out);
}

Camera::Camera() : p(new Impl) {}
Camera::~Camera() { close(); }

bool Camera::open(int idx) {
    if (p->opened) return true;

    USBSDK_dlopen(DLL_LIBUSB, "/usr/lib/x86_64-linux-gnu");
    USBSDK_dlopen(DLL_LIBUVC, "/home/master/sdkreverse/x86_64/lib");

    if (USBSDK_Init() != 1) return false;

    USB_Camera_Info cam;
    std::memset(&cam, 0, sizeof(cam));
    cam.dwSize = sizeof(cam);
    if (USBSDK_EnumDevice(&cam) != 0) return false;
    (void)idx;

    p->uid = USBSDK_LoginDevice(&cam);
    if (p->uid < 0) return false;

    USB_SYSTEM_INFO si;
    std::memset(&si, 0, sizeof(si));
    if (USBSDK_Get_SysInfo(p->uid, &si) == 0) {
        p->fw    = (char*)si.byFirmwareVersion;
        p->hw    = (char*)si.byHardwareVersion;
        p->model = (char*)si.byDeviceType;
        p->sn    = (char*)si.bySerialNumber;
    }

    g_active = p.get();
    if (USBSDK_CreateCallBack(p->uid, sdk_frame_cb, nullptr) != 0) {
        USBSDK_Logout(p->uid);
        p->uid = -1;
        g_active = nullptr;
        return false;
    }

    p->opened = true;
    return true;
}

void Camera::close() {
    if (!p->opened) return;
    g_active = nullptr;
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    if (p->uid >= 0) {
        USBSDK_Logout(p->uid);
        p->uid = -1;
    }
    p->opened = false;
}

bool Camera::is_open() const { return p->opened; }

std::string Camera::serial()   const { return p->sn; }
std::string Camera::firmware() const { return p->fw; }
std::string Camera::model()    const { return p->model; }

void Camera::set_callback(FrameCallback cb) {
    std::lock_guard<std::mutex> lk(p->cb_mtx);
    p->user_cb = cb;
}

bool Camera::get_frame(ircam::Frame &out, int timeout_ms) {
    std::unique_lock<std::mutex> lk(p->frame_mtx);
    uint32_t start = p->frame_count.load();
    auto deadline = std::chrono::steady_clock::now() +
                    std::chrono::milliseconds(timeout_ms);
    while (p->frame_count.load() == start) {
        if (p->frame_cv.wait_until(lk, deadline) == std::cv_status::timeout)
            return false;
    }
    out = p->latest;
    return true;
}

bool Camera::set_brightness(int v) {
    return USBSDK_Set_CameraBright(p->uid, v) == 0;
}

bool Camera::set_contrast(int v) {
    return USBSDK_Set_CameraContrast(p->uid, v) == 0;
}

bool Camera::set_noise_reduce(int t, int s) {
    USB_CAMERA_PARAM cp;
    std::memset(&cp, 0, sizeof(cp));
    cp.dwInterFrameNoiseReduceLevel = t;
    cp.dwFrameNoiseReduceLevel      = s;
    cp.byLSEDetailEnabled = 1;
    cp.dwLSEDetailLevel = 50;
    return USBSDK_Set_CameraNoiseReduce(p->uid, &cp) == 0;
}

bool Camera::set_flip(int mode) {
    if (p->uid < 0) return false;
    return USBSDK_Set_CameraFlip(p->uid, mode) == 0;
}

int Camera::get_flip() {
    if (p->uid < 0) return -1;
    return USBSDK_Get_CameraFlip(p->uid);
}

bool Camera::get_thermal_param(USB_THERMOMETRY_PARAM *out) {
    if (p->uid < 0 || !out) return false;
    std::memset(out, 0, sizeof(*out));
    return USBSDK_Get_ThermalParam(p->uid, out) == 0;
}

bool Camera::set_thermal_param(const USB_THERMOMETRY_PARAM *in) {
    if (p->uid < 0 || !in) return false;
    return USBSDK_Set_ThermalParam(p->uid,
                                   const_cast<USB_THERMOMETRY_PARAM*>(in)) == 0;
}

bool Camera::manual_correct() {
    if (p->uid < 0) return false;
    return USBSDK_Manual_Correct(p->uid) == 0;
}

bool Camera::reset() {
    if (p->uid < 0) return false;
    return USBSDK_Reset(p->uid) == 0;
}

bool Camera::upgrade_start(const std::string &firmware_path) {
    if (p->uid < 0) return false;
    return USBSDK_Upgrade(p->uid,
                          const_cast<char*>(firmware_path.c_str())) == 0;
}

int Camera::upgrade_state() {
    if (p->uid < 0) return -1;
    return USBSDK_Get_Upgrade_State(p->uid);
}

bool Camera::upgrade_close() {
    return true;
}

const char* version() { return "ircam 1.0"; }

} // namespace ircam

