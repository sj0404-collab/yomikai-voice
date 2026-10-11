# patches/

Наши патчи поверх чистого движка (`engine/`, применяются в `scripts/fetch_engine.sh`).
Именование: `NNN-имя.patch`, `patch -p1` из корня движка.

## Запланировано (задача №1 следующего этапа)

**`001-load-gguf-primary.patch`** — новый путь загрузки весов без bf16 safetensors:
`qwen_tts_load_gguf_primary(model_dir, gguf_talker_cp, silent)` —
грузит конфиг/токенизатор/декодер из `model_dir`, а Talker+CP сразу из GGUF Q4_0
в компактные буферы (как `--gguf-talker/--gguf-rest`, но БЕЗ предварительной загрузки bf16).

Зачем: сегодня `qwen_tts_load_ex` сначала требует `model.safetensors` (1.73 ГБ) и только
потом позволяет `--gguf-*` переопределить тензоры — телефону bf16 не нужен вообще
(экономия 1.7 ГБ загрузки + ~2 ГБ RAM-пика при load-time квантовании).

Когда upstream дорастёт до первичной gguf-загрузки — патч удалить.
