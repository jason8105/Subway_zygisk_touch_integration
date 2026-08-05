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
    void onLoad(Api* api, JNIEnv* env) override {
        this->api_ = api;
        this->env_ = env;
        LOGI("onLoad: api_=%p", (void*)api_);
    }

    void preAppSpecialize(AppSpecializeArgs* args) override {
        if (!args || !args->nice_name) return;
        enable_hack = isGame(env_, args->app_data_dir);
        LOGI("preAppSpecialize: enable_hack=%d, api_=%p", enable_hack, (void*)api_);
    }

    void postAppSpecialize(const AppSpecializeArgs*) override {
        LOGI("postAppSpecialize: enable_hack=%d, api_=%p", enable_hack, (void*)api_);
        if (enable_hack) {
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