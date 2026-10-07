/* 01_render_test.c
 * Прогон реального y16 кадра через IRSDK_Frame2Gray_DDE_m_v3
 * с разными параметрами.
 *
 * Ключевое:
 *   u8Method = 0 — auto AGC (minT/maxT игнорируются, работают contrast/bright)
 *   u8Method = 1 — manual window (minT/maxT в °C, contrast/bright в manual
 *                  не применяются к окну)
 *
 * Сборка:
 *   gcc -O2 -Wall -o 01_render_test 01_render_test.c -ldl -lm
 * Запуск:
 *   ./01_render_test [путь_к_raw16.bin]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>

typedef struct tagFrame {
    unsigned short width;
    unsigned short height;
    unsigned short u16FpaTemp;
    unsigned short u16EnvTemp;
    unsigned char  u8TempDiv;
    unsigned char  u8DeviceType;
    unsigned char  u8SensorType;
    unsigned char  u8MeasureSel;
    unsigned char  u8Lens;
    unsigned char  u8Fps;
    unsigned char  u8TriggerFrame;
    unsigned char  u8Reversed2;
    unsigned int   u32FrameIndex;
    unsigned short u16MeasureDis;
    unsigned char  Reversed[8];
    unsigned char  u8Handle;
    unsigned char  u8ObjTempFilterSw;
    unsigned short buffer[2048*1536];
} Frame;

typedef int (*fn_v3)(Frame*, unsigned char*, unsigned char*,
    float, float, float, float, unsigned char, unsigned short,
    unsigned char, unsigned char, unsigned char, unsigned char*);

static void save_pgm(const char* fn, unsigned char* img, int w, int h)
{
    FILE* f = fopen(fn, "wb");
    if (!f) { perror(fn); return; }
    fprintf(f, "P5\n%d %d\n255\n", w, h);
    fwrite(img, 1, w*h, f);
    fclose(f);
}

int main(int argc, char** argv)
{
    const char* raw_path = argc > 1 ? argv[1]
        : "/home/master/sdkreverse/mysdk/2.RAW/raw16.bin";
    int w = 640, h = 512;

    if (!dlopen("libz.so.1", RTLD_NOW | RTLD_GLOBAL)) {
        fprintf(stderr, "zlib preload fail: %s\n", dlerror());
        return 1;
    }
    void* lib = dlopen("/home/master/sdkreverse/x86_64/lib/libIRSDK.so", RTLD_NOW);
    if (!lib) {
        fprintf(stderr, "IRSDK dlopen fail: %s\n", dlerror());
        return 1;
    }
    fn_v3 v3 = (fn_v3)dlsym(lib, "IRSDK_Frame2Gray_DDE_m_v3");
    if (!v3) { fprintf(stderr, "no v3\n"); return 1; }

    Frame* fr = calloc(1, sizeof(Frame));
    if (!fr) return 1;
    fr->width = w;
    fr->height = h;
    fr->u16FpaTemp = 12000;
    fr->u16EnvTemp = 12000;
    fr->u8TempDiv = 100;
    fr->u8MeasureSel = 0;
    fr->u8Lens = 0;

    FILE* f = fopen(raw_path, "rb");
    if (!f) { perror(raw_path); return 1; }
    size_t n_read = fread(fr->buffer, 1, w*h*2, f);
    fclose(f);
    fprintf(stderr, "read %zu bytes, first px = %u (%.2f C)\n",
            n_read, fr->buffer[0], (fr->buffer[0] - 10000.0f) / 100.0f);

    int n = w * h;
    unsigned char* gray = malloc(n);
    unsigned char* rgba = malloc(n * 4);
    unsigned char* tbuf = malloc(n * 4);
    if (!gray || !rgba || !tbuf) return 1;

    struct { const char* name; float c, b, minT, maxT;
             unsigned char method, gamma, dde; } tests[] = {
        { "a_c0_b0",      0.0f,   0.0f,  0.0f,  0.0f, 0, 0, 50 },
        { "a_c0_b50",     0.0f,  50.0f,  0.0f,  0.0f, 0, 0, 50 },
        { "a_c0_b100",    0.0f, 100.0f,  0.0f,  0.0f, 0, 0, 50 },
        { "a_c50_b0",    50.0f,   0.0f,  0.0f,  0.0f, 0, 0, 50 },
        { "a_c100_b0",  100.0f,   0.0f,  0.0f,  0.0f, 0, 0, 50 },
        { "a_c50_b50",   50.0f,  50.0f,  0.0f,  0.0f, 0, 0, 50 },
        { "m1_24_26",     0.0f,   0.0f, 24.0f, 26.0f, 1, 0, 50 },
        { "m1_24_26_c50",50.0f,   0.0f, 24.0f, 26.0f, 1, 0, 50 },
    };
    int n_tests = sizeof(tests) / sizeof(tests[0]);

    for (int i = 0; i < n_tests; i++) {
        memset(gray, 0, n);
        memset(rgba, 0, n * 4);
        memset(tbuf, 0, n * 4);
        int rc = v3(fr, gray, rgba,
                    tests[i].c, tests[i].b, tests[i].minT, tests[i].maxT,
                    tests[i].method, 50, tests[i].dde, tests[i].gamma, 0, tbuf);
        char fn[256];
        snprintf(fn, sizeof(fn), "/tmp/out_%s.pgm", tests[i].name);
        save_pgm(fn, gray, w, h);
        fprintf(stderr, "  %-18s rc=%d -> %s\n", tests[i].name, rc, fn);
    }

    free(gray); free(rgba); free(tbuf); free(fr);
    dlclose(lib);
    return 0;
}