#include <string>
#include <unistd.h>
#include "yabause.h"
#include "threads.h"

static int use_bios = 0;
static std::string backup_path;

int YabauseThread_IsUseBios() {
    return use_bios;
}

const char *YabauseThread_getBackupPath() {
    return backup_path.c_str();
}

void YabauseThread_setUseBios(int use) {
    use_bios = use;
}

void YabauseThread_setBackupPath(const char *buf) {
    backup_path = buf;
}

void YabauseThread_coldBoot() {
}

void YabauseThread_resetPlaymode() {
}

int YabMakeCleanDir(const char *dirname) {
    return 0;
}

int YabNanosleep(u64 ns) {
    usleep(ns / 1000);
    return 0;
}
