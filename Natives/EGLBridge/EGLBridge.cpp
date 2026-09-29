#include <EGL/egl.h>
#include <pthread.h>
#include <cstdio>

static pthread_key_t s_CurrentEGLContextKey;
static pthread_once_t s_ContextKeyInit = PTHREAD_ONCE_INIT;

static void InitContextKey() {
    pthread_key_create(&s_CurrentEGLContextKey, nullptr);
}

extern "C" {

bool EGLBridge_MakeCurrent(EGLDisplay dpy, EGLSurface draw, EGLSurface read, EGLContext ctx) {
    pthread_once(&s_ContextKeyInit, InitContextKey);

    EGLContext current = eglGetCurrentContext();
    if (current == ctx && ctx != EGL_NO_CONTEXT) {
        return true;
    }

    if (ctx == EGL_NO_CONTEXT) {
        // Safe context detachment during reload thread shifts
        eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        pthread_setspecific(s_CurrentEGLContextKey, nullptr);
        return true;
    }

    if (!eglMakeCurrent(dpy, draw, read, ctx)) {
        EGLint err = eglGetError();
        fprintf(stderr, "[EGLBridge] Context switch failed: 0x%X\n", err);

        // Recovery path for reload transient surface drops
        if (err == EGL_BAD_SURFACE || err == EGL_BAD_NATIVE_WINDOW) {
            fprintf(stderr, "[EGLBridge] Attempting fallback to main draw surface...\n");
            // Retry binding
            if (eglMakeCurrent(dpy, draw, draw, ctx)) {
                pthread_setspecific(s_CurrentEGLContextKey, ctx);
                return true;
            }
        }
        return false;
    }

    pthread_setspecific(s_CurrentEGLContextKey, ctx);
    return true;
}

EGLContext EGLBridge_GetThreadContext() {
    pthread_once(&s_ContextKeyInit, InitContextKey);
    return (EGLContext)pthread_getspecific(s_CurrentEGLContextKey);
}

} // extern "C"
