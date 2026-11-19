#ifndef CONFIG_H
#define CONFIG_H
/* The game list the app reads. Can be overridden on the console without rebuilding by putting a
   URL on the first line of /data/ps4store/feed_url.txt */
#ifndef FEED_URL
#define FEED_URL "https://raw.githubusercontent.com/M3hmetSa1t/pegasus-ps4-collection-catalog/main/pegasus-ps4-catalog.json"
#endif
#ifndef FEED_OVERRIDE_FILE
#define FEED_OVERRIDE_FILE "/data/ps4store/feed_url.txt"
#endif
#ifndef SAVE_DIR
#define SAVE_DIR "/data/pkg"
#endif           /* where .pkg files are saved (GoldHEN Package Installer scans this) */
#define APP_NAME  "PS4 Downloader"
#define APP_VERSION_STR "1.10"

#define FEED_MAX_BYTES (6 * 1024 * 1024) /* biggest feed file accepted */
#define FEED_MAX_NODES 200000            /* JSON nodes */
#define MAX_GAMES 2048
#endif
