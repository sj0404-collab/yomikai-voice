cmake_minimum_required(VERSION 3.22.1)
project(qwents C)

# Движок скачивается CI в <repo>/engine (scripts/fetch_engine.sh, штрих-пин по SHA)
get_filename_component(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
set(ENGINE_DIR "${REPO_ROOT}/engine" CACHE PATH "qwen3-tts engine checkout")
set(ENG "${ENGINE_DIR}")

if(NOT EXISTS "${ENG}/qwen_tts.c")
    message(FATAL_ERROR "engine/ не найден. Запусти scripts/fetch_engine.sh")
endif()

# Аналог qwen_build_id.h из Makefile движка
file(WRITE "${CMAKE_BINARY_DIR}/qwen_build_id.h"
     "#define QWEN_BUILD_GIT_REV \"android\"\n"
     "#define QWEN_BUILD_SOURCE_FP \"android\"\n"
     "#define QWEN_BUILD_SIMD \"neon-dotprod\"\n")

add_library(qwents SHARED
    native-lib.c
    ${ENG}/qwen_tts_kleidi.c
    ${ENG}/qwen_tts_q8repack.c
    ${ENG}/qwen_tts_q4export.c
    ${ENG}/qwen_tts.c
    ${ENG}/qwen_tts_gguf.c
    ${ENG}/qwen_tts_talker.c
    ${ENG}/qwen_tts_code_predictor.c
    ${ENG}/qwen_tts_speech_decoder.c
    ${ENG}/qwen_tts_kernels.c
    ${ENG}/qwen_tts_dispatch.c
    ${ENG}/qwen_tts_costmap.c
    ${ENG}/qwen_tts_sd_gemm.c
    ${ENG}/qwen_tts_thread.c
    ${ENG}/qwen_tts_kernels_generic.c
    ${ENG}/qwen_tts_kernels_neon.c
    ${ENG}/qwen_tts_kernels_avx.c
    ${ENG}/qwen_tts_audio.c
    ${ENG}/qwen_tts_emotion.c
    ${ENG}/qwen_tts_compose.c
    ${ENG}/qwen_tts_sampling.c
    ${ENG}/qwen_tts_tokenizer.c
    ${ENG}/qwen_json.c
    ${ENG}/qwen_tts_server.c
    ${ENG}/qwen_tts_voice_clone.c
    ${ENG}/qwen_tts_speech_encoder.c
    ${ENG}/vendor/lz4.c
    ${ENG}/third_party/ingot/amalgam/ingot.c)

target_include_directories(qwents PRIVATE
    "${CMAKE_BINARY_DIR}"
    "${ENG}"
    "${ENG}/vendor"
    "${ENG}/third_party/ingot/include"
    "${ENG}/third_party/ingot/amalgam"
    "${ENG}/third_party/ingot/src")

# Без BLAS: у всех cblas-мест в движке есть naive-fallback (#ifndef USE_BLAS) — проверено.
target_compile_options(qwents PRIVATE -O3 -ffast-math -DQWEN_SIMD_PROFILE=\"android-neon\")
if("${ANDROID_ABI}" STREQUAL "arm64-v8a")
    # SDOT — основной int8/int4 движок на ARM; ~99% устройств 2019+
    target_compile_options(qwents PRIVATE -march=armv8.2-a+dotprod)
endif()

find_library(LOG_LIB log)
target_link_libraries(qwents ${LOG_LIB} m)
