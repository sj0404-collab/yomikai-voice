#!/usr/bin/env python3
"""TTS-киоск: драйвер над CLI движка qwen_tts.
Делит текст на предложения, синтезирует кусками, сшивает, нормализует пик до 92%.
Вызывается из workflow tts.yml, но работает и локально (нужен собранный ./engine/qwen_tts)."""
import argparse, os, re, subprocess, struct, sys, time, wave

def log(*a): print('[synth]', *a, flush=True)

def split_chunks(text, cap=350):
    text = text.strip()
    sents = re.split(r'(?<=[.!?…])\s+', text)
    chunks, cur = [], ''
    for s in sents:
        if not s: continue
        if cur and len(cur) + 1 + len(s) > cap:
            chunks.append(cur); cur = s
        else:
            cur = (cur + ' ' + s).strip()
    if cur: chunks.append(cur)
    # если одна строка длиннее cap — режем по запятым/пробелам жёстко
    out = []
    for c in chunks:
        while len(c) > cap:
            cut = c.rfind(' ', 0, cap)
            cut = cut if cut > 100 else cap
            out.append(c[:cut]); c = c[cut:].strip()
        if c: out.append(c)
    return out

def wav_read(p):
    w = wave.open(p, 'rb')
    fr, ch, sw = w.getframerate(), w.getnchannels(), w.getsampwidth()
    raw = w.readframes(w.getnframes()); w.close()
    if ch != 1 or sw != 2: raise RuntimeError(f'неожиданный формат {p}: {ch}ch {sw*8}bit')
    return fr, list(struct.unpack(f'<{len(raw)//2}h', raw))

def wav_write(p, fr, samples):
    w = wave.open(p, 'wb')
    w.setnchannels(1); w.setsampwidth(2); w.setframerate(fr)
    w.writeframes(struct.pack(f'<{len(samples)}h', *samples)); w.close()

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--text-file', required=True)
    ap.add_argument('--engine', default='engine/qwen_tts')
    ap.add_argument('--model', required=True)
    ap.add_argument('--voice', default='serena')          # пресет
    ap.add_argument('--custom-voice', default='')          # путь к .bin (xvector) — вместо пресета
    ap.add_argument('--lang', default='Russian')
    ap.add_argument('--emotion', default='none')
    ap.add_argument('--rate', default='1.0')
    ap.add_argument('--volume', default='1.0')
    ap.add_argument('--quant', default='int8', choices=['int8', 'int4', 'bf16'])
    ap.add_argument('--out', required=True)
    a = ap.parse_args()

    text = open(a.text_file, encoding='utf-8').read()
    chunks = split_chunks(text)
    log(f'символов: {len(text)}, кусков: {len(chunks)}')
    os.makedirs('parts', exist_ok=True)

    qflags = [] if a.quant == 'bf16' else [f'--{a.quant}']
    tag = '' if a.emotion in ('none', '') else f'[{a.emotion}] '
    vflags = ['-s', a.voice]
    if a.custom_voice:
        vflags = ['--load-voice', a.custom_voice, '--xvector-only']

    total_audio, t0 = 0.0, time.time()
    merged, fr0 = [], None
    for i, c in enumerate(chunks):
        part = f'parts/chunk_{i:03d}.wav'
        cmd = [a.engine, '-d', a.model, *qflags, *vflags, '-l', a.lang,
               '--text', tag + c, '-o', part, '-j', '4',
               '--rate', a.rate, '--volume', a.volume]
        log(f'кусок {i+1}/{len(chunks)}: {len(c)} симв.')
        t1 = time.time()
        r = subprocess.run(cmd, capture_output=True, text=True)
        if r.returncode != 0:
            print(r.stdout[-2000:]); print(r.stderr[-2000:], file=sys.stderr)
            sys.exit(f'движок вернул {r.returncode} на куске {i+1}')
        fr, samp = wav_read(part)
        fr0 = fr0 or fr
        dur = len(samp) / fr
        total_audio += dur
        log(f'  -> {dur:.1f} с аудио за {time.time()-t1:.0f} с')
        if merged:
            merged += [0] * int(fr0 * 0.12)     # 120 мс пауза между кусками
        merged += samp

    # нормализация пика до 92%
    peak = max(1, max(abs(min(merged)), max(merged)))
    target = int(32767 * 0.92)
    if peak < target * 0.9:
        g = target / peak
        merged = [max(-32768, min(32767, int(s * g))) for s in merged]
        log(f'нормализация x{g:.2f}')
    wav_write(a.out, fr0, merged)
    wall = time.time() - t0
    log(f'ГОТОВО: {a.out} — {len(merged)/fr0:.1f} с аудио за {wall:.0f} с (RTF {wall/max(total_audio,0.1):.2f})')

if __name__ == '__main__':
    main()
