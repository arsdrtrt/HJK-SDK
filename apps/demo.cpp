// =============================================================
// demo.cpp — витрина проекта libircam для HIK/HJK тепловизора
// =============================================================
// Что делает:
//   Левая панель:  сырой Y16 (16 бит) → линейное растяжение → 8 бит
//   Правая панель: полный пайплайн (destripe + TNR + PE + DDE + палитра)
//   Мышь:          температура под курсором в °C
//   Клавиши:       q выход, p палитра, m режим, d/D DDE, c/C контраст, r сброс
// =============================================================

#include <opencv2/opencv.hpp>   // OpenCV: окна, Mat, imshow
#include <cstdio>               // printf, fprintf, snprintf
#include <cstring>              // memset, memcpy
#include <cmath>                // fabs, pow
#include <mutex>                // std::mutex — защита общего состояния
#include <atomic>               // std::atomic — флаги между потоками
#include <memory>               // std::unique_ptr
#include <vector>               // std::vector
#include <chrono>               // для подсчёта FPS
#include <algorithm>            // std::min, std::max, std::nth_element

#include "ircam.hpp"            // наш публичный API (Camera, Renderer, Frame)

using namespace ircam;          // чтобы не писать ircam:: каждый раз
using cv::Mat;                  // сокращение

// -------------------------------------------------------------
// Глобальное состояние (между SDK-потоком и главным потоком)
// -------------------------------------------------------------
static std::mutex            g_lock;          // защищает g_latest и рендер
static Frame                 g_latest;        // последний полученный кадр
static std::atomic<uint32_t> g_frame_count{0}; // сколько кадров пришло
static std::atomic<int>      g_fps{0};         // текущий FPS
static std::atomic<bool>     g_stop{false};    // флаг выхода

// Под курсором мыши
static std::atomic<int>      g_mx{-1};         // x мыши в кадре (-1 = вне)
static std::atomic<int>      g_my{-1};         // y мыши в кадре
static std::atomic<float>    g_mtemp{0.0f};    // температура под мышью, °C

// Настройки рендера
static RenderParams          g_params;         // параметры пайплайна
static std::atomic<int>      g_palette{2};     // текущая палитра (2=ironbow)
static std::atomic<int>      g_mode{0};        // 0=split, 1=pipeline, 2=raw

// -------------------------------------------------------------
// Коллбэк — вызывается SDK в СВОЁМ потоке на каждый кадр
// -------------------------------------------------------------
static void on_frame(const Frame &f)
{
    std::lock_guard<std::mutex> lk(g_lock);    // блокируем общее состояние
    g_latest = f;                              // сохраняем копию кадра
    g_frame_count.fetch_add(1);                // счётчик кадров +1

    int mx = g_mx.load();                      // читаем координаты мыши
    int my = g_my.load();
    if (mx >= 0 && my >= 0 && mx < f.width && my < f.height)
        g_mtemp.store(f.temp_at(mx, my));      // температура под курсором
}

// -------------------------------------------------------------
// Коллбэк мыши — вызывается OpenCV в главном потоке
// -------------------------------------------------------------
static void on_mouse(int event, int x, int y, int, void *userdata)
{
    if (event != cv::EVENT_MOUSEMOVE && event != cv::EVENT_LBUTTONDOWN)
        return;

    int *off = (int *)userdata;                // [x0,y0,w,h] активной панели
    int fx = x - off[0];                       // координата в кадре
    int fy = y - off[1];
    if (fx >= 0 && fy >= 0 && fx < off[2] && fy < off[3]) {
        g_mx.store(fx);                        // внутри панели — пишем
        g_my.store(fy);
    } else {
        g_mx.store(-1);                        // вне — сбрасываем
        g_my.store(-1);
    }
}

// -------------------------------------------------------------
// 16 бит → 8 бит: линейное растяжение по перцентилям 0.5% и 99.5%
// -------------------------------------------------------------
static void stretch16to8(const std::vector<uint16_t> &src, Mat &dst, int w, int h)
{
    std::vector<uint16_t> tmp(src);            // копия для nth_element
    size_t lo_i = tmp.size() *   5 / 1000;     // индекс 0.5%
    size_t hi_i = tmp.size() * 995 / 1000;     // индекс 99.5%
    std::nth_element(tmp.begin(), tmp.begin() + lo_i, tmp.end());
    uint16_t lo = tmp[lo_i];                   // нижняя граница
    std::nth_element(tmp.begin(), tmp.begin() + hi_i, tmp.end());
    uint16_t hi = tmp[hi_i];                   // верхняя граница
    if (hi <= lo) hi = lo + 1;                 // защита от деления на 0

    double scale = 255.0 / (hi - lo);          // коэффициент растяжения
    dst.create(h, w, CV_8UC1);                 // 8-бит, 1 канал
    for (int i = 0; i < w * h; i++) {          // по всем пикселям
        int v = (int)((src[i] - lo) * scale);  // линейная формула
        dst.data[i] = (uint8_t)(v < 0 ? 0 : (v > 255 ? 255 : v)); // клип
    }
}

// -------------------------------------------------------------
// main — точка входа
// -------------------------------------------------------------
int main(int argc, char **argv)
{
    int cam_idx = (argc > 1) ? atoi(argv[1]) : 0;  // индекс камеры из argv

    Camera cam;                                     // объект камеры
    if (!cam.open(cam_idx)) {                       // открыть
        fprintf(stderr, "camera open failed\n");
        return 1;                                   // ошибка → выход
    }
    printf("Model: %s\n", cam.model().c_str());     // модель в консоль
    printf("FW:    %s\n", cam.firmware().c_str());  // прошивка
    printf("SN:    %s\n\n", cam.serial().c_str());  // серийник

    cam.set_callback(on_frame);                     // подписываемся на кадры

    g_params.plateau_k  = 2.0f;                     // AGC: мягче → ironbow ок
    g_params.contrast   = 1.0f;                     // глобальный контраст
    g_params.dde_gain   = 1.2f;                     // усиление деталей
    g_params.dde_radius = 4;                        // радиус DDE
    g_params.destripe   = true;                     // убрать полосы
    g_params.tnr_alpha  = 0.4f;                     // сила TNR
    g_params.gamma      = 1.0f;                     // гамма
    g_params.palette    = g_palette.load();         // текущая палитра

    std::unique_ptr<Renderer> renderer;             // создадим после 1-го кадра

    const char *win = "libircam demo";              // имя окна
    cv::namedWindow(win, cv::WINDOW_AUTOSIZE);      // создаём окно

    int mouse_off[4] = {0, 0, 0, 0};                // смещение активной панели
    cv::setMouseCallback(win, on_mouse, mouse_off); // подписываемся на мышь

    printf("Keys: q=quit p=palette m=mode d/D=dde c/C=contrast r=reset\n");

    auto t_last = std::chrono::steady_clock::now(); // для FPS
    int  fps_frames = 0;                            // счётчик кадров за секунду

    while (!g_stop.load()) {                        // главный цикл
        Frame f;
        {
            std::lock_guard<std::mutex> lk(g_lock); // копируем кадр под мьютексом
            f = g_latest;
        }
        if (f.width == 0 || f.height == 0) {        // нет кадра — ждём
            cv::waitKey(10);
            continue;
        }

        if (!renderer) {                            // первый кадр — знаем размер
            renderer = std::make_unique<Renderer>(f.width, f.height);
            renderer->set_params(g_params);
            printf("Renderer init: %dx%d\n", f.width, f.height);
        }

        // ---- Левая панель: сырой Y16 ----
        Mat raw8;
        stretch16to8(f.data, raw8, f.width, f.height); // 16→8 линейно
        Mat raw_bgr;
        cv::cvtColor(raw8, raw_bgr, cv::COLOR_GRAY2BGR); // gray→BGR

        // ---- Правая панель: полный пайплайн ----
        std::vector<uint8_t> gray(f.width * f.height);     // выход gray
        std::vector<uint8_t> rgb (f.width * f.height * 3); // выход rgb
        {
            std::lock_guard<std::mutex> lk(g_lock);        // рендер под мьютексом
            g_params.palette = g_palette.load();           // обновляем палитру
            renderer->set_params(g_params);                // применяем параметры
            renderer->render(f, gray, rgb);                // рендерим
        }
        Mat proc_bgr(f.height, f.width, CV_8UC3, rgb.data()); // обёртка над rgb
        Mat proc_copy = proc_bgr.clone();                     // копия для рисования

        // ---- Overlay под курсором ----
        int mx = g_mx.load(), my = g_my.load();
        if (mx >= 0 && my >= 0) {                             // мышь в кадре
            char buf[64];
            snprintf(buf, sizeof(buf), "%.2f C", g_mtemp.load()); // текст T°
            cv::circle(raw_bgr,  {mx, my}, 3, {0, 255, 0}, 1);    // круг слева
            cv::circle(proc_copy,{mx, my}, 3, {0, 255, 0}, 1);    // круг справа
            cv::putText(proc_copy, buf, {mx + 8, my - 8},         // подпись
                        cv::FONT_HERSHEY_SIMPLEX, 0.5, {0, 255, 0}, 1);
        }

        // ---- FPS ----
        fps_frames++;                                          // +1 кадр
        auto t_now = std::chrono::steady_clock::now();         // текущее время
        if (std::chrono::duration_cast<std::chrono::milliseconds>(t_now - t_last).count() >= 1000) {
            g_fps.store(fps_frames);                           // записали FPS
            fps_frames = 0;                                    // сброс счётчика
            t_last = t_now;                                    // сдвиг времени
        }

        // ---- Сборка окна ----
        Mat canvas;
        int mode = g_mode.load();
        if (mode == 0) {                                       // split: обе панели
            cv::hconcat(raw_bgr, proc_copy, canvas);           // склейка по X
        } else if (mode == 1) {                                // только pipeline
            canvas = proc_copy;
        } else {                                               // только raw
            canvas = raw_bgr;
        }
        mouse_off[0] = 0;               // активная панель = вся ширина кадра
        mouse_off[1] = 0;
        mouse_off[2] = f.width;
        mouse_off[3] = f.height;

        // ---- Инфо-строка внизу ----
        char info[256];
        snprintf(info, sizeof(info),
                 "mode=%s pal=%d dde=%.2f contrast=%.2f tnr=%.2f fps=%d frames=%u",
                 mode == 0 ? "split" : (mode == 1 ? "pipeline" : "raw"),
                 g_palette.load(), g_params.dde_gain, g_params.contrast,
                 g_params.tnr_alpha, g_fps.load(), g_frame_count.load());
        cv::putText(canvas, info, {10, canvas.rows - 10},
                    cv::FONT_HERSHEY_SIMPLEX, 0.45, {0, 255, 0}, 1);

        cv::imshow(win, canvas);                               // показать окно

        // ---- Клавиши ----
        int key = cv::waitKey(1) & 0xFF;                       // неблокирующий ввод
        if (key == 'q' || key == 27) break;                    // выход
        else if (key == 'p') { int p = g_palette.load(); g_palette.store((p + 1) % 19); }
        else if (key == 'm') { int m = g_mode.load();    g_mode.store((m + 1) % 3); }
        else if (key == 'd') { g_params.dde_gain = std::min(4.0f, g_params.dde_gain + 0.25f); }
        else if (key == 'D') { g_params.dde_gain = std::max(0.0f, g_params.dde_gain - 0.25f); }
        else if (key == 'c') { g_params.contrast = std::min(3.0f, g_params.contrast + 0.1f); }
        else if (key == 'C') { g_params.contrast = std::max(0.1f, g_params.contrast - 0.1f); }
        else if (key == 'r') {                                 // сброс параметров
            g_params = RenderParams{};
            g_params.plateau_k = 2.0f; g_params.contrast = 1.0f;
            g_params.dde_gain  = 1.2f; g_params.dde_radius = 4;
            g_params.destripe  = true; g_params.tnr_alpha = 0.4f; g_params.gamma = 1.0f;
        }
    }

    cam.close();                                   // закрыть камеру
    cv::destroyAllWindows();                       // закрыть окна
    printf("done. frames=%u\n", g_frame_count.load());
    return 0;
}