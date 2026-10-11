/* JNI-мост к C-движку qwen3-tts.
 * ВАЖНО: GGUF-переопределение в upstream работает ПОВЕРХ загруженных bf16 safetensors.
 * Для телефона (без bf16) нужен патч qwen_tts_load_gguf_primary — см. patches/README.md.
 * Пока патча нет, qLoad ожидает modelDir с полными весами (отладка на большом железе/эмуляторе). */
#include <jni.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <android/log.h>

#include "qwen_tts.h"

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  "qwents", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "qwents", __VA_ARGS__)

extern int qwen_gguf_override_talker(qwen_tts_ctx_t *, const char *, int);
extern int qwen_gguf_override_rest(qwen_tts_ctx_t *, const char *, int);

#define SAMPLE_RATE 24000

static volatile int g_cancel = 0;
static int cancel_trampoline(void *ud) { (void)ud; return g_cancel; }

static int write_wav16(const char *path, const float *s, int n) {
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    int data = n * 2;
    fwrite("RIFF", 1, 4, f);
    uint32_t u32 = 36 + data; fwrite(&u32, 4, 1, f);
    fwrite("WAVEfmt ", 1, 8, f);
    u32 = 16; fwrite(&u32, 4, 1, f);
    uint16_t u16 = 1;  fwrite(&u16, 2, 1, f);          /* PCM */
    u16 = 1;           fwrite(&u16, 2, 1, f);          /* mono */
    u32 = SAMPLE_RATE; fwrite(&u32, 4, 1, f);
    u32 = SAMPLE_RATE * 2; fwrite(&u32, 4, 1, f);      /* byte rate */
    u16 = 2;           fwrite(&u16, 2, 1, f);          /* block align */
    u16 = 16;          fwrite(&u16, 2, 1, f);          /* bits */
    fwrite("data", 1, 4, f);
    fwrite(&data, 4, 1, f);
    for (int i = 0; i < n; i++) {
        float v = s[i];
        if (v > 1.f) v = 1.f; else if (v < -1.f) v = -1.f;
        int16_t q = (int16_t)(v * 32767.f);
        fwrite(&q, 2, 1, f);
    }
    fclose(f);
    return 0;
}

JNIEXPORT jlong JNICALL
Java_com_yomikai_voice_MainActivity_qLoad(JNIEnv *env, jobject thiz,
                                          jstring modelDir, jstring ggufTalker, jstring ggufRest) {
    (void)thiz;
    const char *dir = (*env)->GetStringUTFChars(env, modelDir, 0);
    const char *gt  = (*env)->GetStringUTFChars(env, ggufTalker, 0);
    const char *gr  = (*env)->GetStringUTFChars(env, ggufRest, 0);
    LOGI("qLoad dir=%s gguf=%s", dir, (gt && gt[0]) ? gt : "(none)");

    qwen_tts_ctx_t *ctx = qwen_tts_load_ex(dir, 1 /*silent*/, 0 /*int8*/, 0 /*int4*/);
    if (!ctx) { LOGE("load_ex failed"); goto fail; }
    if (gt && gt[0]) {
        int n = qwen_gguf_override_talker(ctx, gt, 1);
        LOGI("gguf talker tensors: %d", n);
        if (n <= 0) { LOGE("gguf talker override empty"); qwen_tts_unload(ctx); goto fail; }
    }
    if (gr && gr[0] && (!gt || strcmp(gr, gt) != 0)) {
        int n = qwen_gguf_override_rest(ctx, gr, 1);
        LOGI("gguf rest tensors: %d", n);
        if (n <= 0) { LOGE("gguf rest override empty"); qwen_tts_unload(ctx); goto fail; }
    }
    ctx->cancel_cb = cancel_trampoline;
    ctx->cancel_cb_userdata = NULL;
    g_cancel = 0;
    (*env)->ReleaseStringUTFChars(env, modelDir, dir);
    (*env)->ReleaseStringUTFChars(env, ggufTalker, gt);
    (*env)->ReleaseStringUTFChars(env, ggufRest, gr);
    return (jlong)(intptr_t)ctx;

fail:
    (*env)->ReleaseStringUTFChars(env, modelDir, dir);
    (*env)->ReleaseStringUTFChars(env, ggufTalker, gt);
    (*env)->ReleaseStringUTFChars(env, ggufRest, gr);
    return 0;
}

JNIEXPORT jint JNICALL
Java_com_yomikai_voice_MainActivity_qSynth(JNIEnv *env, jobject thiz,
                                           jlong handle, jstring text,
                                           jstring speaker, jstring language, jstring outWav) {
    (void)thiz;
    qwen_tts_ctx_t *ctx = (qwen_tts_ctx_t *)(intptr_t)handle;
    if (!ctx) return -1;
    const char *t  = (*env)->GetStringUTFChars(env, text, 0);
    const char *sp = (*env)->GetStringUTFChars(env, speaker, 0);
    const char *ln = (*env)->GetStringUTFChars(env, language, 0);
    const char *ow = (*env)->GetStringUTFChars(env, outWav, 0);

    int sid = qwen_tts_speaker_id(sp);
    if (sid >= 0) qwen_tts_set_speaker(ctx, sid);
    if (ln && ln[0]) qwen_tts_set_language(ctx, ln);

    g_cancel = 0;
    float *pcm = NULL; int n = 0;
    LOGI("synth: %zu chars, spk=%s", strlen(t), sp);
    int rc = qwen_tts_generate(ctx, t, &pcm, &n);
    int wrc = -99;
    if (rc == 0 && pcm && n > 0) wrc = write_wav16(ow, pcm, n);
    LOGI("synth rc=%d n=%d wav=%d", rc, n, wrc);
    if (pcm) free(pcm);

    (*env)->ReleaseStringUTFChars(env, text, t);
    (*env)->ReleaseStringUTFChars(env, speaker, sp);
    (*env)->ReleaseStringUTFChars(env, language, ln);
    (*env)->ReleaseStringUTFChars(env, outWav, ow);
    return (rc == 0 && wrc == 0) ? 0 : (rc != 0 ? rc : wrc);
}

JNIEXPORT jint JNICALL
Java_com_yomikai_voice_MainActivity_qCancel(JNIEnv *env, jobject thiz, jlong handle) {
    (void)env; (void)thiz; (void)handle;
    g_cancel = 1;
    return 0;
}

JNIEXPORT void JNICALL
Java_com_yomikai_voice_MainActivity_qUnload(JNIEnv *env, jobject thiz, jlong handle) {
    (void)env; (void)thiz;
    qwen_tts_ctx_t *ctx = (qwen_tts_ctx_t *)(intptr_t)handle;
    if (ctx) qwen_tts_unload(ctx);
}
