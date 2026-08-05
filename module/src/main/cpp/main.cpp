#include <cstring>
#include <jni.h>
#include <pthread.h>
#include "hook.h"
#include "zygisk.hpp"
#include "il2cpp.h"
#include "xdl.h"

using zygisk::Api;
using zygisk::AppSpecializeArgs;

class MyModule : public zygisk::ModuleBase {
public:
    void onLoad(Api *api, JNIEnv *env) override {
        this->api_ = api;
        this->env_ = env;
    }

    void preAppSpecialize(AppSpecializeArgs *args) override {
        if (!args || !args->nice_name) return;
        enable_hack = isGame(env_, args->app_data_dir);
    }

    void postAppSpecialize(const AppSpecializeArgs *) override {
        if (enable_hack && api_) {
            pthread_t ntid;
            pthread_create(&ntid, nullptr, hack_thread, (void *)api_);
        }
    }

private:
    Api *api_ = nullptr;
    JNIEnv *env_ = nullptr;
};

REGISTER_ZYGISK_MODULE(MyModule)