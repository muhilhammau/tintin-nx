/* tintin_stage0.c -- stage 1: link + load both libs, no constructors yet. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <switch.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include "dcr_boost.h"
#include "rt_applet.h"
#include "rt_window.h"
#include <math.h>
#include <ctype.h>
#include "rt_pad.h"
#include <stdlib.h>
#include "rt_audout.h"
#include "config.h"
#include "dcr_path.h"
#include "dcr_setup.h"
#include "error.h"
#include "gl_layer.h"
#include "imports.h"
#include "jni.h"
#include "so_util.h"
#include "util.h"

/* Custom JNI handlers for Game class */
JNI_H_DECL(jni_h_getSDFolder) {
    return jv_l(jni_str("/sdcard/gameloft/games/GloftTTSS"));
}

JNI_H_DECL(jni_h_GetCurrentTime) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return jv_j((jlong)ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

#define TT_LIB_STORM  "libStormGLOFT.so"
#define TT_LIB_TINTIN "libtintin.so"
#define TT_JNI(cls, fn) "Java_com_gameloft_android_GAND_GloftTTSS_" cls "_" fn

static so_module g_storm;
static so_module g_tintin;

/* ---- android.media.AudioTrack on audout (48 kHz stereo s16) ---- */
typedef struct {
  int rate, ch, bits8, play;
  uint32_t pos, in_step;
  int pl, pr;
  int n;
  uint64_t frames_in;
  int16_t out[RT_AUDOUT_FRAMES * 2];
} tt_at;
static Mutex g_at_lock;

static void tt_at_free(JObj *o) { free(o->p); o->p = NULL; }

static void tt_at_push(tt_at *t, int l, int r) {
  while (t->pos < 65536u) {
    int ol = t->pl + (int)(((int64_t)(l - t->pl) * (int64_t)t->pos) >> 16);
    int orr = t->pr + (int)(((int64_t)(r - t->pr) * (int64_t)t->pos) >> 16);
    t->out[t->n * 2] = (int16_t)ol;
    t->out[t->n * 2 + 1] = (int16_t)orr;
    if (++t->n == RT_AUDOUT_FRAMES) {
      if (rt_audout_submit(t->out) != 0) svcSleepThread(5000000ULL);
      t->n = 0;
    }
    t->pos += t->in_step;
  }
  t->pos -= 65536u;
  t->pl = l;
  t->pr = r;
}

JNI_H_DECL(tt_at_init) {
  int rate = 48000, ch = 2, fmt = 2;
  if (!strncmp(m->sig, "(IIII", 5)) {
    rate = a[1].i;
    ch = (a[2].i == 4 || a[2].i == 2) ? 1 : 2;
    fmt = a[3].i;
  }
  if (rate < 4000 || rate > 192000) rate = 48000;
  tt_at *t = (tt_at *)calloc(1, sizeof *t);
  if (!t) return jv_none();
  t->rate = rate;
  t->ch = ch;
  t->bits8 = (fmt == 3);
  t->in_step = (uint32_t)(((uint64_t)rate << 16) / 48000u);
  rt_audout_open();
  self->p = t;
  self->finalize = tt_at_free;
  debugPrintf("[audio] AudioTrack %d Hz, %d ch, %s, sig %s\n", rate, ch, t->bits8 ? "8-bit" : "16-bit", m->sig);
  return jv_none();
}

JNI_H_DECL(tt_at_minbuf) {
  int rate = a[0].i > 0 ? a[0].i : 48000;
  int ch = (a[1].i == 4 || a[1].i == 2) ? 1 : 2;
  int bytes = (rate * ch * 2 / 10) & ~15;
  if (bytes < 4096) bytes = 4096;
  return jv_i(bytes);
}
JNI_H_DECL(tt_at_play) { tt_at *t = self ? (tt_at *)self->p : NULL; if (t) t->play = 1; return jv_none(); }
JNI_H_DECL(tt_at_stop) { tt_at *t = self ? (tt_at *)self->p : NULL; if (t) t->play = 0; return jv_none(); }
JNI_H_DECL(tt_at_flush) {
  tt_at *t = self ? (tt_at *)self->p : NULL;
  if (t) { mutexLock(&g_at_lock); t->n = 0; t->pos = 0; mutexUnlock(&g_at_lock); }
  return jv_none();
}
JNI_H_DECL(tt_at_playstate) { tt_at *t = self ? (tt_at *)self->p : NULL; return jv_i(t && t->play ? 3 : 1); }
JNI_H_DECL(tt_at_state) { return jv_i(1); }
JNI_H_DECL(tt_at_head) { tt_at *t = self ? (tt_at *)self->p : NULL; return jv_i(t ? (jint)t->frames_in : 0); }
JNI_H_DECL(tt_at_rate) { tt_at *t = self ? (tt_at *)self->p : NULL; return jv_i(t ? t->rate : 48000); }
JNI_H_DECL(tt_at_nativerate) { return jv_i(48000); }

JNI_H_DECL(tt_at_write_b) {
  tt_at *t = self ? (tt_at *)self->p : NULL;
  JObj *arr = (JObj *)a[0].l;
  int off = a[1].i, size = a[2].i;
  if (!t || !arr || arr->kind != JK_ARRAY || off < 0 || size <= 0 || off + size > arr->a.len) {
    svcSleepThread(10000000ULL);
    return jv_i(size > 0 ? size : 0);
  }
  const uint8_t *p = (const uint8_t *)arr->a.data + off;
  int fb = t->ch * (t->bits8 ? 1 : 2);
  int nf = size / fb;
  mutexLock(&g_at_lock);
  for (int i = 0; i < nf; i++, p += fb) {
    int l, r;
    if (t->bits8) {
      l = ((int)p[0] - 128) << 8;
      r = t->ch == 2 ? ((int)p[1] - 128) << 8 : l;
    } else {
      l = (int16_t)(p[0] | (p[1] << 8));
      r = t->ch == 2 ? (int16_t)(p[2] | (p[3] << 8)) : l;
    }
    tt_at_push(t, l, r);
  }
  t->frames_in += (uint64_t)nf;
  mutexUnlock(&g_at_lock);
  return jv_i(size);
}

JNI_H_DECL(tt_at_write_s) {
  tt_at *t = self ? (tt_at *)self->p : NULL;
  JObj *arr = (JObj *)a[0].l;
  int off = a[1].i, size = a[2].i;
  if (!t || !arr || arr->kind != JK_ARRAY || off < 0 || size <= 0 || off + size > arr->a.len) {
    svcSleepThread(10000000ULL);
    return jv_i(size > 0 ? size : 0);
  }
  const int16_t *p = (const int16_t *)arr->a.data + off;
  int nf = size / t->ch;
  mutexLock(&g_at_lock);
  for (int i = 0; i < nf; i++, p += t->ch)
    tt_at_push(t, p[0], t->ch == 2 ? p[1] : p[0]);
  t->frames_in += (uint64_t)nf;
  mutexUnlock(&g_at_lock);
  return jv_i(size);
}
/* android32 requires these port-owned JNI tables. */
const JMethodDef jni_method_defs[] = {
    /* com/gameloft/android/GAND/GloftTTSS/Game - specific handlers */
    {"com/gameloft/android/GAND/GloftTTSS/Game", "getSDFolder", NULL, jni_h_getSDFolder},
    {"com/gameloft/android/GAND/GloftTTSS/Game", "IsDemo", NULL, jni_h_false},
    {"com/gameloft/android/GAND/GloftTTSS/Game", "ShowLoadingInterface", NULL, jni_h_void},

    /* Game.nativeInit lookup list - neutral handlers */
    {"com/gameloft/android/GAND/GloftTTSS/Game", "sendAppToBackground", NULL, jni_h_void},
    {"com/gameloft/android/GAND/GloftTTSS/Game", "Exit", NULL, jni_h_void},
    {"com/gameloft/android/GAND/GloftTTSS/Game", "Pause", NULL, jni_h_void},
    {"com/gameloft/android/GAND/GloftTTSS/Game", "openBrowser", NULL, jni_h_void},
    {"com/gameloft/android/GAND/GloftTTSS/Game", "isWifiEnabled", NULL, jni_h_false},
    {"com/gameloft/android/GAND/GloftTTSS/Game", "PrintDebug", NULL, jni_h_void},
    {"com/gameloft/android/GAND/GloftTTSS/Game", "GetCurrentTime", NULL, jni_h_GetCurrentTime},
    {"com/gameloft/android/GAND/GloftTTSS/Game", "GetTextureFormat", NULL, jni_h_zero},
    {"com/gameloft/android/GAND/GloftTTSS/Game", "GetPhoneManufacturer", NULL, jni_h_empty_string},
    {"com/gameloft/android/GAND/GloftTTSS/Game", "GetPhoneModel", NULL, jni_h_empty_string},
    {"com/gameloft/android/GAND/GloftTTSS/Game", "GetPhoneCPUName", NULL, jni_h_empty_string},
    {"com/gameloft/android/GAND/GloftTTSS/Game", "GetPhoneCPUFreq", NULL, jni_h_zero},
    {"com/gameloft/android/GAND/GloftTTSS/Game", "GetPhoneGPUName", NULL, jni_h_empty_string},
    {"com/gameloft/android/GAND/GloftTTSS/Game", "GetPhoneLanguage", NULL, jni_h_zero},
    {"com/gameloft/android/GAND/GloftTTSS/Game", "getManufacture", NULL, jni_h_empty_string},
    {"com/gameloft/android/GAND/GloftTTSS/Game", "getSDFolder", NULL, jni_h_getSDFolder},
    {"com/gameloft/android/GAND/GloftTTSS/Game", "PlayVideo", NULL, jni_h_void},
    {"com/gameloft/android/GAND/GloftTTSS/Game", "IsPlayingVideoDone", NULL, jni_h_true},
	{"com/gameloft/android/GAND/GloftTTSS/Game", "SetVideoFile", NULL, jni_h_void},
    {"com/gameloft/android/GAND/GloftTTSS/Game", "setCurrentContext", NULL, jni_h_void},
    {"com/gameloft/android/GAND/GloftTTSS/Game", "showProcessInfo", NULL, jni_h_void},
    {"com/gameloft/android/GAND/GloftTTSS/Game", "SetScreenOrientation", NULL, jni_h_void},
    {"com/gameloft/android/GAND/GloftTTSS/Game", "keepScreenOn", NULL, jni_h_void},
    {"com/gameloft/android/GAND/GloftTTSS/Game", "launchIGP", NULL, jni_h_void},
    {"com/gameloft/android/GAND/GloftTTSS/Game", "FullScreenToggleHideBar", NULL, jni_h_void},
    {"com/gameloft/android/GAND/GloftTTSS/Game", "FullScreenToggleShowBar", NULL, jni_h_void},
    {"com/gameloft/android/GAND/GloftTTSS/Game", "LaunchBilling", NULL, jni_h_void},
    {"com/gameloft/android/GAND/GloftTTSS/Game", "IsDemo", NULL, jni_h_false},
    {"com/gameloft/android/GAND/GloftTTSS/Game", "ShowLoadingInterface", NULL, jni_h_void},
    {"com/gameloft/android/GAND/GloftTTSS/Game", "HideLoadingInterface", NULL, jni_h_void},

    {"android/media/AudioTrack", "<init>", NULL, tt_at_init},
    {"android/media/AudioTrack", "getMinBufferSize", NULL, tt_at_minbuf},
    {"android/media/AudioTrack", "getNativeOutputSampleRate", NULL, tt_at_nativerate},
    {"android/media/AudioTrack", "write", "([BII)I", tt_at_write_b},
    {"android/media/AudioTrack", "write", "([SII)I", tt_at_write_s},
    {"android/media/AudioTrack", "write", NULL, tt_at_write_b},
    {"android/media/AudioTrack", "play", NULL, tt_at_play},
    {"android/media/AudioTrack", "pause", NULL, tt_at_stop},
    {"android/media/AudioTrack", "stop", NULL, tt_at_stop},
    {"android/media/AudioTrack", "flush", NULL, tt_at_flush},
    {"android/media/AudioTrack", "release", NULL, jni_h_void},
    {"android/media/AudioTrack", "getPlayState", NULL, tt_at_playstate},
    {"android/media/AudioTrack", "getState", NULL, tt_at_state},
    {"android/media/AudioTrack", "getPlaybackHeadPosition", NULL, tt_at_head},
    {"android/media/AudioTrack", "getSampleRate", NULL, tt_at_rate},
    {"android/media/AudioTrack", NULL, NULL, jni_h_zero},
    {NULL, NULL, NULL, NULL},
};
const JFieldDef jni_field_defs[] = {
    {NULL, NULL, NULL, 0, NULL},
};
const char *const jni_class_supers[][2] = {
    {NULL, NULL},
};
const char *const jni_missing_classes[] = {
    NULL,
};

static const char *const k_libs[] = {TT_LIB_STORM, TT_LIB_TINTIN};
const RtSetupPlan port_setup_plan = {
    .libs = k_libs,
    .nlibs = 2,
    .libs_what = "Unpacking Tintin's engine",
    .apk_requirement = "This port needs The Adventures of Tintin: The Game HD 1.0.5 for 32-bit ARM (armeabi-v7a).",
    .libs_p0 = 200,
    .libs_p1 = 900,
    .classes_p0 = 900,
    .classes_p1 = 950,
};

static int load_one(so_module *m, const char *name) {
  char path[512];
  snprintf(path, sizeof path, "%s/%s", dcr_game_root(), name);
  debugPrintf("[stage1] loading %s\n", path);

  int rc = so_load(m, path, NULL, PORT_SO_REGION_BYTES);
  if (rc < 0)
    fatal_error("Stage 1: so_load failed for %s (rc=%d). See debug.log.", path, rc);

  so_relocate(m);
  int missing = so_resolve(m, dcr_imports, dcr_imports_count, 1);
  int kuser = so_fix_kuser_helpers(m);
  debugPrintf("[stage1] %s: size=%u KB unresolved=%d kuser_literals=%d\n", name,
              (unsigned)(m->load_size >> 10), missing, kuser);
  if (missing)
    fatal_error("Stage 1: %s has %d unresolved imports. Send debug.log.", name, missing);

  so_finalize(m);
  so_flush_caches(m);
  return 0;
}

static void log_known_exports(void) {
  static const char *const syms[] = {
      TT_JNI("Game", "nativeInit"),
      TT_JNI("GameRenderer", "nativeInit"),
      TT_JNI("GameRenderer", "nativeResize"),
      TT_JNI("GameRenderer", "nativeRender"),
      TT_JNI("GameGLSurfaceView", "nativeOnTouch"),
      NULL,
  };
  for (int i = 0; syms[i]; ++i)
    debugPrintf("[stage1] export %-90s %p\n", syms[i],
                (void *)so_try_find_addr_rx(&g_tintin, syms[i]));
}

int port_load(const char *apk) {
  dcr_setup_from_apk(apk);
  /* order matters: StormGLOFT provides operator new/delete and __cxa_* */
  load_one(&g_storm, TT_LIB_STORM);
  load_one(&g_tintin, TT_LIB_TINTIN);
  log_known_exports();
  return 0;
}

extern volatile uint32_t tt_swp_count, tt_swp_first_type, tt_swp_first_pc;
static EGLDisplay g_dpy = EGL_NO_DISPLAY;
static EGLSurface g_surf = EGL_NO_SURFACE;
static EGLContext g_ctx = EGL_NO_CONTEXT;
EGLBoolean b_eglSwapBuffers(EGLDisplay dpy, EGLSurface surface);

static int tt_gfx_init(void) {
  debugPrintf("[stage4] creating GLES2 context...\n");
  log_flush_ring();
  if (log_console_active()) log_console_close();
  dcr_window_prepare();

  g_dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  EGLint major = 0, minor = 0;
  if (g_dpy == EGL_NO_DISPLAY || !eglInitialize(g_dpy, &major, &minor)) {
    debugPrintf("[stage4] eglInitialize FAILED 0x%x\n", eglGetError());
    return -1;
  }
  if (!eglBindAPI(EGL_OPENGL_ES_API)) {
    debugPrintf("[stage4] eglBindAPI FAILED 0x%x\n", eglGetError());
    return -1;
  }
  EGLConfig cfg = 0;
  EGLint n = 0;
  static const EGLint want[] = {
      EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
      EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
      EGL_DEPTH_SIZE, 24, EGL_STENCIL_SIZE, 8, EGL_NONE};
  static const EGLint fallback[] = {
      EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
      EGL_NONE};
  if (!eglChooseConfig(g_dpy, want, &cfg, 1, &n) || n < 1) {
    debugPrintf("[stage4] exact config unavailable; fallback\n");
    if (!eglChooseConfig(g_dpy, fallback, &cfg, 1, &n) || n < 1) {
      debugPrintf("[stage4] no GLES2 config 0x%x\n", eglGetError());
      return -1;
    }
  }
  static const EGLint ctxa[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
  g_surf = eglCreateWindowSurface(g_dpy, cfg, (EGLNativeWindowType)nwindowGetDefault(), NULL);
  g_ctx = eglCreateContext(g_dpy, cfg, EGL_NO_CONTEXT, ctxa);
  if (g_surf == EGL_NO_SURFACE || g_ctx == EGL_NO_CONTEXT ||
      !eglMakeCurrent(g_dpy, g_surf, g_surf, g_ctx)) {
    debugPrintf("[stage4] surface/context/current FAILED 0x%x\n", eglGetError());
    return -1;
  }
  eglSwapInterval(g_dpy, 1);
  debugPrintf("[stage4] EGL %d.%d\n", major, minor);
  debugPrintf("[stage4] GL_VENDOR   = %s\n", (const char *)glGetString(GL_VENDOR));
  debugPrintf("[stage4] GL_RENDERER = %s\n", (const char *)glGetString(GL_RENDERER));
  debugPrintf("[stage4] GL_VERSION  = %s\n", (const char *)glGetString(GL_VERSION));
  const char *ext = (const char *)glGetString(GL_EXTENSIONS);
  debugPrintf("[stage4] DXT=%d PVRTC=%d ATC=%d\n",
              ext && strstr(ext, "GL_EXT_texture_compression_dxt1") ? 1 : 0,
              ext && strstr(ext, "GL_IMG_texture_compression_pvrtc") ? 1 : 0,
              ext && strstr(ext, "GL_AMD_compressed_ATC_texture") ? 1 : 0);
  log_flush_ring();
  return 0;
}

/* ---- input: layar sentuh + pad -> nativeOnTouch(action,x,y,id); 0 down, 1 up, 2 move ---- */
typedef void (*fn_touch)(void *env, void *obj, int action, int x, int y, int id);
static fn_touch g_touch;
typedef void (*fn_key)(void *env, void *obj, int key);
typedef void (*fn_keycodes)(void *env, void *obj, int a, int b);
static fn_key g_key_down, g_key_up;
static int g_key_log;
static u64 g_dpad_prev;
#define TT_MAXP 8
typedef struct { int used; u32 key; int x, y; } tt_ptr;
typedef struct { u32 key; int x, y; } tt_cur;
static tt_ptr g_ptr[TT_MAXP];
static PadState g_pad;
static float g_cx = 640.0f, g_cy = 360.0f;
static int g_in_log;
static int g_mv_log;

static void tt_send(int action, int x, int y, int id) {
  if ((action != 2 || g_mv_log < 60) && g_in_log < 1500) {
    if (action == 2) g_mv_log++;
    g_in_log++;
    debugPrintf("[input] action=%d x=%d y=%d id=%d\n", action, x, y, id);
  }
  if (g_touch) g_touch(g_jni_env, NULL, action == 0 ? 1 : (action == 1 ? 0 : action), x, y, id);
}

static void tt_apply(const tt_cur *cur, int n) {
  for (int s = 0; s < TT_MAXP; s++) {          /* jari yang diangkat */
    if (!g_ptr[s].used) continue;
    int found = 0;
    for (int i = 0; i < n; i++)
      if (cur[i].key == g_ptr[s].key) { found = 1; break; }
    if (!found) { tt_send(1, g_ptr[s].x, g_ptr[s].y, s); g_ptr[s].used = 0; }
  }
  for (int i = 0; i < n; i++) {                /* jari baru / bergerak */
    int s = -1;
    for (int k = 0; k < TT_MAXP; k++)
      if (g_ptr[k].used && g_ptr[k].key == cur[i].key) { s = k; break; }
    if (s < 0) {
      for (int k = 0; k < TT_MAXP; k++) if (!g_ptr[k].used) { s = k; break; }
      if (s < 0) continue;
      g_ptr[s].used = 1; g_ptr[s].key = cur[i].key;
      g_ptr[s].x = cur[i].x; g_ptr[s].y = cur[i].y;
      tt_send(0, cur[i].x, cur[i].y, s);
    } else if (g_ptr[s].x != cur[i].x || g_ptr[s].y != cur[i].y || cur[i].key == 0xFFFEu) {
      g_ptr[s].x = cur[i].x; g_ptr[s].y = cur[i].y;
      tt_send(2, cur[i].x, cur[i].y, s);
    }
  }
}

static void tt_input_init(void) {
  hidInitializeTouchScreen();
  rt_pad_setup(1, 1);
  padInitializeDefault(&g_pad);
}

#define TT_JOY_R 80
#define TT_HOLD 3
#define TT_NBTN 9
static int g_joy_x = 185, g_joy_y = 524;
static int g_act_x = 1125, g_act_y = 355;
static int g_a_cursor;
static int g_jx, g_jy;
typedef struct { const char *name; u64 mask; int x, y, hold; } tt_btn_t;
static tt_btn_t tt_btns[TT_NBTN] = {
  { "PLUS",  HidNpadButton_Plus,  1197,  58, 0 },
  { "B",     HidNpadButton_B,     1125, 355, 0 },
  { "X",     HidNpadButton_X,      614, 401, 0 },
  { "Y",     HidNpadButton_Y,       65,  75, 0 },
  { "L",     HidNpadButton_L,       -1,  -1, 0 },
  { "R",     HidNpadButton_R,       -1,  -1, 0 },
  { "ZL",    HidNpadButton_ZL,      -1,  -1, 0 },
  { "ZR",    HidNpadButton_ZR,      -1,  -1, 0 },
  { "MINUS", HidNpadButton_Minus,   -1,  -1, 0 },
};
static int g_cur_vis;
static int g_joy_on;
static int g_a_hold;

static void tt_load_buttons(void) {
  char path[512];
  snprintf(path, sizeof path, "%s/buttons.txt", dcr_game_root());
  FILE *f = fopen(path, "r");
  if (!f) {
    debugPrintf("[input] %s not found, default button map\n", path);
  } else {
    char line[128];
    while (fgets(line, sizeof line, f)) {
      char nm[16];
      int x, y;
      if (line[0] == '#' || sscanf(line, "%15s %d %d", nm, &x, &y) != 3) continue;
      for (char *c = nm; *c; c++) *c = (char)toupper((unsigned char)*c);
      if (!strcmp(nm, "JOY")) { g_joy_x = x; g_joy_y = y; continue; }
      if (!strcmp(nm, "A")) { g_act_x = x; g_act_y = y; continue; }
      for (int i = 0; i < TT_NBTN; i++)
        if (!strcmp(nm, tt_btns[i].name)) { tt_btns[i].x = x; tt_btns[i].y = y; }
    }
    fclose(f);
    debugPrintf("[input] loaded %s\n", path);
  }
  debugPrintf("[input] JOY -> %d,%d\n", g_joy_x, g_joy_y);
  for (int i = 0; i < TT_NBTN; i++)
    debugPrintf("[input] btn %s -> %d,%d\n", tt_btns[i].name, tt_btns[i].x, tt_btns[i].y);
}

static void tt_add(tt_cur *cur, int *n, u32 key, int x, int y) {
  if (*n >= TT_MAXP) return;
  cur[*n].key = key; cur[*n].x = x; cur[*n].y = y; (*n)++;
}

static void tt_dpad_keys(u64 b) {
  static const struct { u64 m; int code; } map[4] = {
    { HidNpadButton_Up, 19 }, { HidNpadButton_Down, 20 },
    { HidNpadButton_Left, 21 }, { HidNpadButton_Right, 22 } };
  for (int i = 0; i < 4; i++) {
    int now = (b & map[i].m) != 0, was = (g_dpad_prev & map[i].m) != 0;
    if (now == was) continue;
    fn_key f = now ? g_key_down : g_key_up;
    if (g_key_log < 80) { g_key_log++; debugPrintf("[input] key%s %d (fn=%p)\n", now ? "Down" : "Up", map[i].code, (void *)f); }
    if (f) f(g_jni_env, NULL, map[i].code);
  }
  g_dpad_prev = b;
}

static void tt_input_poll(void) {
  tt_cur cur[TT_MAXP];
  int n = 0;
  HidTouchScreenState ts = {0};
  if (hidGetTouchScreenStates(&ts, 1) > 0) {
    for (int i = 0; i < (int)ts.count && i < 3; i++)
      tt_add(cur, &n, ts.touches[i].finger_id, (int)ts.touches[i].x, (int)ts.touches[i].y);
  }
  padUpdate(&g_pad);
  float st[4] = {0};
  u64 b = rt_pad_read(&g_pad, st);

  float lx = st[0], ly = -st[1];
  if (lx * lx + ly * ly > (g_joy_on ? 0.04f : 0.09f)) {
    if (!g_joy_on) { g_jx = g_joy_x; g_jy = g_joy_y; tt_add(cur, &n, 0xFFFEu, g_jx, g_jy); g_joy_on = 1; }
    else {
      int tx = g_joy_x + (int)(lx * TT_JOY_R), ty = g_joy_y + (int)(ly * TT_JOY_R);
      int ddx = tx - g_jx, ddy = ty - g_jy;
      if (ddx > 18) ddx = 18;
      if (ddx < -18) ddx = -18;
      if (ddy > 18) ddy = 18;
      if (ddy < -18) ddy = -18;
      g_jx += ddx;
      g_jy += ddy;
      tt_add(cur, &n, 0xFFFEu, g_jx, g_jy);
    }
  } else g_joy_on = 0;

  float dx = st[2], dy = -st[3];
  if (fabsf(dx) < 0.15f) dx = 0;
  if (fabsf(dy) < 0.15f) dy = 0;
  dx *= 14.0f; dy *= 14.0f;




  tt_dpad_keys(b);
  if (dx != 0 || dy != 0) g_cur_vis = 240;
  g_cx += dx; g_cy += dy;
  if (g_cx < 0) g_cx = 0;
  if (g_cx > 1279) g_cx = 1279;
  if (g_cy < 0) g_cy = 0;
  if (g_cy > 719) g_cy = 719;
  if (b & HidNpadButton_A) {
    if (g_a_hold <= 0) g_a_cursor = (g_cur_vis > 0);
    g_a_hold = TT_HOLD;
    if (g_a_cursor) g_cur_vis = 240;
  }
  if (g_a_hold > 0) {
    g_a_hold--;
    if (g_a_cursor) tt_add(cur, &n, 0xFFFFu, (int)g_cx, (int)g_cy);
    else tt_add(cur, &n, 0xFFFFu, g_act_x, g_act_y);
  }

  for (int i = 0; i < TT_NBTN; i++) {
    if (tt_btns[i].x < 0) continue;
    if (b & tt_btns[i].mask) tt_btns[i].hold = TT_HOLD;
    if (tt_btns[i].hold > 0) {
      tt_btns[i].hold--;
      tt_add(cur, &n, 0xFD00u + i, tt_btns[i].x, tt_btns[i].y);
    }
  }
  tt_apply(cur, n);
}

static void tt_draw_cursor(void) {
  if (g_cur_vis <= 0) return;
  g_cur_vis--;
  GLint sb[4];
  GLfloat cc[4];
  GLboolean was = glIsEnabled(GL_SCISSOR_TEST);
  glGetIntegerv(GL_SCISSOR_BOX, sb);
  glGetFloatv(GL_COLOR_CLEAR_VALUE, cc);
  int x = (int)g_cx, y = 720 - (int)g_cy;
  glEnable(GL_SCISSOR_TEST);
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glScissor(x - 9, y - 9, 18, 18);
  glClear(GL_COLOR_BUFFER_BIT);
  glClearColor(1.0f, 1.0f, 0.0f, 1.0f);
  glScissor(x - 6, y - 6, 12, 12);
  glClear(GL_COLOR_BUFFER_BIT);
  glClearColor(cc[0], cc[1], cc[2], cc[3]);
  glScissor(sb[0], sb[1], sb[2], sb[3]);
  if (!was) glDisable(GL_SCISSOR_TEST);
}
void port_run(void) {







  typedef void (*fn_void)(void *env, void *obj);
  typedef void (*fn_rinit)(void *env, void *obj, int man, int w, int h, void *ver);
  typedef void (*fn_resize)(void *env, void *obj, int w, int h);

  debugPrintf("[stage2] constructors...\n");
  log_flush_ring();
  so_execute_init_array(&g_storm);
  so_execute_init_array(&g_tintin);
  debugPrintf("[stage2] constructors PASS\n");
  jni_init();
  g_jni_log = 1;
  {
    typedef int (*fn_onload)(void *vm, void *res);
    fn_onload ol = (fn_onload)so_try_find_addr_rx(&g_tintin, "JNI_OnLoad");
    debugPrintf("[stage4] JNI_OnLoad = %p\n", (void *)ol);
    log_flush_ring();
    if (ol) {
      int v = ol(g_jni_vm, NULL);
      debugPrintf("[stage4] JNI_OnLoad returned 0x%x\n", v);
      log_flush_ring();
    }
  }
  log_flush_ring();

  fn_void game_init = (fn_void)so_try_find_addr_rx(&g_tintin, TT_JNI("Game", "nativeInit"));
  fn_rinit r_init = (fn_rinit)so_try_find_addr_rx(&g_tintin, TT_JNI("GameRenderer", "nativeInit"));
  fn_resize r_resize = (fn_resize)so_try_find_addr_rx(&g_tintin, TT_JNI("GameRenderer", "nativeResize"));
  fn_void r_render = (fn_void)so_try_find_addr_rx(&g_tintin, TT_JNI("GameRenderer", "nativeRender"));
  if (!game_init || !r_init || !r_resize || !r_render) {
    debugPrintf("[stage4] missing export\n");
    log_flush_ring();
    return;
  }

  if (tt_gfx_init() != 0) return;
  int w = 0, h = 0;
  dcr_window_size(&w, &h);

  debugPrintf("[stage4] Game.nativeInit...\n");
  log_flush_ring();
  game_init(g_jni_env, jni_class("com/gameloft/android/GAND/GloftTTSS/Game")->obj);
  debugPrintf("[stage4] GameRenderer.nativeInit(0,%d,%d,\"1.0.5\")...\n", w, h);
  log_flush_ring();
  r_init(g_jni_env, NULL, 0, w, h, jni_str("1.0.5"));
  debugPrintf("[stage4] GameRenderer.nativeInit RETURNED\n");
  log_flush_ring();
  r_resize(g_jni_env, NULL, w, h);
  debugPrintf("[stage4] nativeResize RETURNED\n");
  log_flush_ring();

  g_touch = (fn_touch)so_try_find_addr_rx(&g_tintin, TT_JNI("GameGLSurfaceView", "nativeOnTouch"));
  debugPrintf("[input] nativeOnTouch = %p\n", (void *)g_touch);
  g_key_down = (fn_key)so_try_find_addr_rx(&g_tintin, TT_JNI("Game", "nativeOnKeyDown"));
  g_key_up = (fn_key)so_try_find_addr_rx(&g_tintin, TT_JNI("Game", "nativeOnKeyUp"));
  fn_keycodes set_codes = (fn_keycodes)so_try_find_addr_rx(&g_tintin, TT_JNI("Game", "nativeSetAndroidKeyCodes"));
  debugPrintf("[input] keyDown=%p keyUp=%p setKeyCodes=%p\n", (void *)g_key_down, (void *)g_key_up, (void *)set_codes);
  if (set_codes) set_codes(g_jni_env, NULL, 4, 82);
  {
    typedef int (*fn_sup)(void *env, void *obj, int v);
    typedef void (*fn_open)(void *env, void *obj, int v);
    typedef void (*fn_ori)(void *env, void *obj, int v);
    fn_sup sup = (fn_sup)so_try_find_addr_rx(&g_tintin, TT_JNI("Game", "nativeSetPhoneSupportGamepad"));
    fn_open gpo = (fn_open)so_try_find_addr_rx(&g_tintin, TT_JNI("Game", "nativeOnGamePadOpen"));
    fn_ori ori = (fn_ori)so_try_find_addr_rx(&g_tintin, TT_JNI("Game", "nativeSetDeviceHandlesScrOrientation"));
    debugPrintf("[probe] sup=%p gpo=%p ori=%p\n", (void *)sup, (void *)gpo, (void *)ori);
    if (sup) { int r = sup(g_jni_env, NULL, 0); debugPrintf("[probe] SetPhoneSupportGamepad(0) -> %d\n", r); }
  }
  static int g_probe_done; (void)g_probe_done;
  tt_input_init();
  tt_load_buttons();

  g_jni_log = 0;
  log_set_quiet(1);
  for (int frame = 0; !rt_exit_requested() && appletMainLoop(); frame++) {
    rt_applet_poll();
    dcr_boost_launch_tick();
    tt_input_poll();
    r_render(g_jni_env, NULL);
    tt_draw_cursor();
    if (!b_eglSwapBuffers(g_dpy, g_surf)) {
      debugPrintf("[stage4] swap FAILED frame %d 0x%x\n", frame, eglGetError());
      break;
    }
    if (frame == 0) {
      dcr_boost_first_picture();
      /* the launch boost ends in dcr_boost_launch_tick */
    }
    if (frame < 3 || frame % 60 == 0) {
      debugPrintf("[stage4] FRAME %d presented (swp traps=%u first type=0x%x pc=0x%x)\n", frame, (unsigned)tt_swp_count, (unsigned)tt_swp_first_type, (unsigned)tt_swp_first_pc);
      if (frame < 3 || frame % 300 == 0) log_flush_ring();
    }
  }
  log_set_quiet(0);
  debugPrintf("[stage4] loop done\n");
  log_flush_ring();
}
uint64_t dcr_boot_frames(void) { return dcr_gl_frames(); }









