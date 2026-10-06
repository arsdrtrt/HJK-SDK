// capture_raw.cpp — захват Y16 с логированием структуры пакетов
// Сохраняет: raw16.bin (кадры) + raw16_meta.jsonl (метаданные)

#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <fstream>
#include <chrono>
#include <thread>
#include <mutex>
#include <atomic>

#include "usbsdk.h"
#include "IRSDK.h"

// ---------------------------------------------------------------
// Глобальное состояние
// ---------------------------------------------------------------
static FILE*             g_data_file     = nullptr;
static std::ofstream     g_meta_file;
static std::atomic<int>  g_frame_count{0};
static std::atomic<bool> g_stopping{false};
static int               g_target_frames = 100;
static std::mutex        g_lock;

// ---------------------------------------------------------------
static void log_frame_structure(const Frame* f, int idx)
{
    char hex[32 * 3 + 1];
    memset(hex, 0, sizeof(hex));
    int nhex = 32;
    if ((int)(f->width * f->height * 2) < nhex)
        nhex = f->width * f->height * 2;
    for (int i = 0; i < nhex; i++)
        sprintf(hex + i * 3, "%02x ", ((const uint8_t*)f->buffer)[i]);

    std::string vals;
    int nval = 8;
    if (f->width * f->height < nval)
        nval = f->width * f->height;
    for (int i = 0; i < nval; i++) {
        vals += std::to_string((unsigned)f->buffer[i]);
        if (i + 1 < nval) vals += " ";
    }

    g_meta_file << "{"
        << "\"idx\":"          << idx
        << ",\"w\":"           << f->width
        << ",\"h\":"           << f->height
        << ",\"bytes\":"       << (f->width * f->height * 2)
        << ",\"head_hex\":\""  << hex << "\""
        << ",\"first8\":\""    << vals << "\""
        << ",\"fpa\":"         << f->u16FpaTemp
        << ",\"env\":"         << f->u16EnvTemp
        << ",\"div\":"         << (int)f->u8TempDiv
        << ",\"sel\":"         << (int)f->u8MeasureSel
        << "}\n";
}

// ---------------------------------------------------------------
// Callback SDK
// ---------------------------------------------------------------
static void on_frame(void* data, void*)
{
    Frame* f = (Frame*)data;
    if (!f || f->width == 0) return;

    std::lock_guard<std::mutex> lk(g_lock);

    // (1) если главный поток уже начал остановку — ничего не делаем
    if (g_stopping.load()) return;

    // (2) если файл закрыт — тоже не пишем
    if (!g_data_file) return;

    int idx = g_frame_count.load();
    if (idx >= g_target_frames) return;

    fwrite(f->buffer, 1, (size_t)f->width * f->height * 2, g_data_file);
    log_frame_structure(f, idx);
    g_frame_count.fetch_add(1);

    if ((idx + 1) % 10 == 0)
        fprintf(stderr, "captured %d/%d\n", idx + 1, g_target_frames);
}

// ---------------------------------------------------------------
int main(int argc, char** argv)
{
    int n = (argc > 1) ? atoi(argv[1]) : 100;
    g_target_frames = n;

    if (USBSDK_Init() != 1) {
        fprintf(stderr, "USBSDK_Init failed\n");
        return 1;
    }

    USB_Camera_Info cam;
    memset(&cam, 0, sizeof(cam));
    cam.dwSize = sizeof(cam);
    if (USBSDK_EnumDevice(&cam) != 0) {
        fprintf(stderr, "EnumDevice failed: %s\n", USBSDK_GetLastError());
        return 1;
    }

    int uid = USBSDK_LoginDevice(&cam);
    if (uid < 0) {
        fprintf(stderr, "Login failed: %s\n", USBSDK_GetLastError());
        return 1;
    }

    USB_SYSTEM_INFO si;
    memset(&si, 0, sizeof(si));
    if (USBSDK_Get_SysInfo(uid, &si) == 0) {
        printf("FW=%s HW=%s SN=%s\n",
               si.byFirmwareVersion,
               si.byHardwareVersion,
               si.bySerialNumber);
    }

    USB_THERMOMETRY_PARAM tp;
    memset(&tp, 0, sizeof(tp));
    if (USBSDK_Get_ThermalParam(uid, &tp) == 0) {
        printf("range=%u emiss=%u dis=%u up=%u lo=%u\n",
               tp.byTemperatureRange,
               tp.dwEmissivity,
               tp.dwDistance,
               tp.dwTemperatureRangeUpperLimit,
               tp.dwTemperatureRangeLowerLimit);
    }

    g_data_file = fopen("raw16.bin", "wb");
    if (!g_data_file) { perror("fopen raw16.bin"); return 1; }

    g_meta_file.open("raw16_meta.jsonl");
    if (!g_meta_file) { perror("open meta"); return 1; }

    if (USBSDK_CreateCallBack(uid, on_frame, nullptr) != 0) {
        fprintf(stderr, "CreateCallBack failed: %s\n", USBSDK_GetLastError());
        return 1;
    }

    printf("Capturing %d frames...\n", n);
    while (g_frame_count.load() < n)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // ---- Мягкая остановка ----
    // 1. под мьютексом сообщаем callback'у "стоп" и закрываем файлы
    {
        std::lock_guard<std::mutex> lk(g_lock);
        g_stopping.store(true);
        if (g_data_file) { fclose(g_data_file); g_data_file = nullptr; }
        if (g_meta_file.is_open()) g_meta_file.close();
    }

    // 2. даём SDK-потоку время увидеть флаг и не зайти в callback
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // 3. логин закрываем последним
    USBSDK_Logout(uid);

    printf("Done: %d frames, %lld bytes\n",
           g_frame_count.load(),
           (long long)g_frame_count.load() * 640 * 512 * 2);
    return 0;
}