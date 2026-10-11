#!/usr/bin/env python3
"""Собирает rest/ — облегчённую папку-модель для телефона/первичной gguf-загрузки.
Выкидывает из model.safetensors только те слои, что GGUF-экспорт покрывает полностью
(7 больших матриц на слой Talker'а и CP). Embeddings/нормы/lm_head/mtp_proj остаются.
Заполненная копия живёт рядом: rest/model.safetensors + конфиги + speech_tokenizer/."""
import os, re, shutil, sys
from safetensors import safe_open
from safetensors.torch import save_file

DROP = re.compile(r'^talker\.(model|code_predictor\.model)\.layers\.\d+\.'
                  r'(self_attn\.(q|k|v|o)_proj|mlp\.(gate|up|down)_proj)\.weight$')

def main(src_dir, dst_dir):
    src = os.path.join(src_dir, 'model.safetensors')
    kept, dropped, kept_bytes, drop_bytes = {}, [], 0, 0
    with safe_open(src, framework='pt') as f:
        for name in f.keys():
            t = f.get_tensor(name)
            if DROP.match(name):
                drop_bytes += t.numel() * t.element_size()
                if len(dropped) < 6: dropped.append(name)
            else:
                kept[name] = t
                kept_bytes += t.numel() * t.element_size()
    os.makedirs(dst_dir, exist_ok=True)
    save_file(kept, os.path.join(dst_dir, 'model.safetensors'))
    for fn in os.listdir(src_dir):
        if fn == 'model.safetensors' or os.path.isdir(os.path.join(src_dir, fn)):
            continue
        shutil.copy2(os.path.join(src_dir, fn), os.path.join(dst_dir, fn))
    st = os.path.join(src_dir, 'speech_tokenizer')
    if os.path.isdir(st):
        shutil.copytree(st, os.path.join(dst_dir, 'speech_tokenizer'), dirs_exist_ok=True)
    print(f'[strip] kept {kept_bytes/1e9:.2f} GB, dropped {drop_bytes/1e9:.2f} GB, е.g. {dropped[:3]}')
    print(f'[strip] tensors: {len(kept)} kept / {len(kept) + len(dropped)} total (drop shown partially)')

if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
