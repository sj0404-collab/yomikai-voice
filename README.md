# Yomikai Voice (Голос Yomikai)

Офлайн-TTS для Android на C-движке **Qwen3-TTS** ([gabriele-mastrapasqua/qwen3-tts](https://github.com/gabriele-mastrapasqua/qwen3-tts)) — «живые голоса» с эмоциями на слабом телефоне.

- инференс чистым C, без Python/torch/ONNX, весы mmap
- максимальное квантование: **INT4 (Q4_0-LSQ) / INT8**, ядра **ARM NEON + SDOT** (`armv8.2-a+dotprod`)
- RTF на Apple M1 ~0.51 (int4); прогноз на средних Android-SoC — 1.5–3 (поток по предложениям → первый звук через секунды)
- 9 пресет-голосов, 10 языков включая русский, inline-эмоции `[joy]`/`[sad]`/`[pause:400ms]`
- весы качаются один раз при первом запуске (~1.15 ГБ: GGUF-Q4 Talker+CP + BF16-декодер)

## Устройство репо
- `app/` — Android-приложение (Java, одна Activity, JNI)
- `app/jni/` — CMake + native-lib.c (мост к libengine)
- `scripts/fetch_engine.sh` — скачивает движок по закреплённому SHA в `engine/`
- `patches/` — наши патчи поверх движка (см. README там)
- `.github/workflows/` — `apk.yml` (сборка APK), `weights.yml` (экспорт Q4-весов → Release)

## Этапы
- [x] С1: скелет, CI native/weights
- [ ] С2: weights-релиз + APK v1
- [ ] С3: потоковое воспроизведение, кэш глав, 3 режима автоэмоций, интеграция с yomikai
- [ ] С4: полировка под слабое железо, x86_64 для эмулятора
