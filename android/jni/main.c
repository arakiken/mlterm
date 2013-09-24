#include  <android/log.h>
#include  <android_native_app_glue.h>

#define LOG_TAG ("mlterm")
#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__))

static int32_t handle_input(struct android_app* app, AInputEvent* event)
{
        LOGI("handle_input");
        switch(AInputEvent_getType(event)) {
        case AINPUT_EVENT_TYPE_MOTION:
                LOGI("(%f, %f)", AMotionEvent_getX(event, 0), AMotionEvent_getY(event, 0));
                break;
        default:
                break;
        }
        return 0;
}

static void handle_cmd(struct android_app* app, int32_t cmd)
{
        LOGI("handle_cmd");
}

void android_main(struct android_app* state) {
    app_dummy();

    state->userData = 0;
    state->onAppCmd = handle_cmd;
    state->onInputEvent = handle_input;

    while (1) {
        int ident;
        int events;
        struct android_poll_source* source;

        while ((ident=ALooper_pollAll(0, NULL, &events, (void**)&source)) >= 0) {
            if (source != NULL) {
                source->process(state, source);
            }

            if (ident == LOOPER_ID_USER) {
            }

            if (state->destroyRequested != 0) {
                break;
            }
        }
    }
}
