/* The Adventures of Tintin: The Game HD settings for android32. */
#ifndef PORT_CONFIG_H
#define PORT_CONFIG_H

#define PORT_TITLE    "The Adventures of Tintin: The Game HD"
#define PORT_NAME     "tintin_nx"
#define PORT_PACKAGE  "com.gameloft.android.GAND.GloftTTSS"
#define PORT_BANNER   "tintin_nx: The Adventures of Tintin: The Game HD (Gameloft StormGLOFT engine, armeabi-v7a)"
#define PORT_ABI_DIR  "lib/armeabi-v7a/"

/* libtintin.so maps to ~5.5 MiB, libStormGLOFT.so ~0.85 MiB; leave room. */
#define PORT_SO_REGION_BYTES (16u * 1024 * 1024)

#define PORT_APK_DESC "The Adventures of Tintin: The Game HD 1.0.5 (armeabi-v7a)"
#define PORT_APK_ROLES                                                                  \
  {.what = "the game",                                                                 \
   .need = (const char *const[]){"lib/armeabi-v7a/libtintin.so",                       \
                                 "lib/armeabi-v7a/libStormGLOFT.so", NULL},             \
   .flags = RT_APK_HIGHEST_VERSION}
#define RT_PACKAGE_MISMATCH_FATAL 1

#define PORT_LAUNCHER_START_NOTE "(Unpacks the game libraries from your APK on first launch)"
#define PORT_LAUNCHER_BYLINE     "Unofficial homebrew wrapper; bring your own APK"

/* The game expects /sdcard/gameloft/games/GloftTTSS/ for its data. */
#define RT_PATH_SD_SUBDIRS "gameloft"
#define RT_PATH_EXTRA_DIRS "gameloft", "gameloft/games", "gameloft/games/GloftTTSS"

#define RT_PAD_MAX_PLAYERS 1
#define RT_SETUP_UPDATE_PERMILLE 500

#endif

#define RT_BOOST_LAUNCH_AFTER_PICTURE_MS 20000
