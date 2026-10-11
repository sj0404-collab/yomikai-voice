#!/usr/bin/env bash
# Скачивает движок qwen3-tts в engine/ по закреплённому SHA (детерминированно).
set -euo pipefail
SHA="ef339be58a778b062e1c14382347964552eae007"
cd "$(dirname "$0")/.."
if [ -f engine/.pinned_sha ] && [ "$(cat engine/.pinned_sha)" = "$SHA" ]; then
    echo "engine уже на $SHA"
    exit 0
fi
rm -rf engine
curl -sL "https://codeload.github.com/gabriele-mastrapasqua/qwen3-tts/tar.gz/$SHA" | tar xz
mv "qwen3-tts-$SHA" engine
echo "$SHA" > engine/.pinned_sha
# наши патчи поверх движка
if ls patches/*.patch >/dev/null 2>&1; then
    for p in patches/*.patch; do
        (cd engine && patch -p1 < "../$p")
        echo "applied $p"
    done
fi
echo "engine готов: $SHA"
