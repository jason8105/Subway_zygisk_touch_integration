#include <jni.h>
#include <pthread.h>
#include <dirent.h>
#include <cstring>
#include <cstdio>
#include "hook.h"
#include "zygisk.hpp"
#include "il2cpp.h"
#include "xdl.h"

using zygisk::Api;
using zygisk::AppSpecializeArgs;

// Helper to log all running processes (for debugging)
static void logRunningProcesses() {
    DIR* dir = opendir("/proc");
    if (!dir) return;
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_type == DT_DIR) {
            int pid = atoi(entry->d_name);
            if (pid > 0) {
                char cmdline[256] = {0};
                char path[64];
                snprintf(path, sizeof(path), "/proc/%d/cmdline", pid);
                FILE* f = fopen(path, "r");
                if (f) {
                    if (fgets(cmdline, sizeof(cmdline), f)) {
                        // Only log if not empty
                        if (strlen(cmdline) > 0)
                            LOGI("Running process: pid=%d cmd=%s", pid, cmdline);
                    }
                    fclose(f);
                }
            }
        }
    }
    closedir(dir);
}

// ---------------------------------------------------------------------------
class MyModule : public zygisk::ModuleBase {
public:
    void onLoad(Api* api, JNIEnv* env) override {
        this->api_ = api;
        this->env_ = env;
        LOGI("onLoad: api_=%p", (void*)api_);
    }

    void preAppSpecialize(AppSpecializeArgs* args) override {
        if (!args) {
            LOGW("preAppSpecialize: args is null");
            return;
        }
        if (!args->nice_name) {
            LOGW("preAppSpecialize: nice_name is null");
            return;
        }
        // Log nice_name for every process
        const char* nice = env_->GetStringUTFChars(args->nice_name, nullptr);
        LOGI("preAppSpecialize: nice_name=%s", nice);
        env_->ReleaseStringUTFChars(args->nice_name, nice);

        // Log app_data_dir if available
        if (args->app_data_dir) {
            const char* dir = env_->GetStringUTFChars(args->app_data_dir, nullptr);
            LOGI("preAppSpecialize: app_data_dir=%s", dir);
            env_->ReleaseStringUTFChars(args->app_data_dir, dir);
        } else {
            LOGW("preAppSpecialize: app_data_dir is null");
        }

        enable_hack = isGame(env_, args->app_data_dir);
        LOGI("preAppSpecialize: enable_hack=%d, api_=%p", enable_hack, (void*)api_);

        // Log denylist status
        if (api_) {
            uint32_t flags = api_->getFlags();
            LOGI("preAppSpecialize: flags=0x%x (DENYLIST=%d, GRANTED_ROOT=%d)",
                 flags,
                 (flags & zygisk::PROCESS_ON_DENYLIST) ? 1 : 0,
                 (flags & zygisk::PROCESS_GRANTED_ROOT) ? 1 : 0);
        }

        // Register PLT hook while Zygisk API is still alive
        if (enable_hack && api_) {
            registerPltHook(api_);
        } else {
            LOGE("preAppSpecialize: skipping PLT hook (enable_hack=%d, api_=%p)", enable_hack, (void*)api_);
        }
    }

    void postAppSpecialize(const AppSpecializeArgs*) override {
        LOGI("postAppSpecialize: enable_hack=%d, api_=%p", enable_hack, (void*)api_);
        if (enable_hack && api_) {
            // Log running processes for debugging
            logRunningProcesses();
            pthread_t th;
            pthread_create(&th, nullptr, hack_thread, nullptr);
            pthread_detach(th);
        }
    }

private:
    Api*        api_ = nullptr;
    JNIEnv*     env_ = nullptr;
};

REGISTER_ZYGISK_MODULE(MyModule)