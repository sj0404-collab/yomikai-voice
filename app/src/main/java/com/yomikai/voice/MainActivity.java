package com.yomikai.voice;

import android.app.Activity;
import android.media.MediaPlayer;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.View;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.Spinner;
import android.widget.TextView;

import java.io.File;

public class MainActivity extends Activity {
    static { System.loadLibrary("qwents"); }

    private native long qLoad(String modelDir, String ggufTalker, String ggufRest);
    private native int  qSynth(long handle, String text, String speaker, String language, String outWav);
    private native int  qCancel(long handle);
    private native void qUnload(long handle);

    private static final String[] SPEAKERS = {
        "serena", "vivian", "sohee", "ono_anna", "ryan", "aiden", "eric", "dylan", "uncle_fu"
    };

    private final Handler ui = new Handler(Looper.getMainLooper());
    private volatile long engine = 0;
    private volatile Thread worker;
    private volatile MediaPlayer player;
    private TextView status;
    private Button btnLoad, btnSpeak, btnStop;

    @Override protected void onCreate(Bundle b) {
        super.onCreate(b);
        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        int pad = (int) (8 * getResources().getDisplayMetrics().density);
        root.setPadding(pad, pad, pad, pad);

        status = new TextView(this);
        status.setText("Движок не загружен. Положите веса в filesDir/model и нажмите «Загрузить».");
        root.addView(status);

        btnLoad = new Button(this); btnLoad.setText("Загрузить модель");
        root.addView(btnLoad);

        final Spinner spk = new Spinner(this);
        spk.setAdapter(new ArrayAdapter<>(this, android.R.layout.simple_spinner_dropdown_item, SPEAKERS));
        spk.setSelection(0);
        root.addView(spk);

        final EditText text = new EditText(this);
        text.setMinLines(5);
        text.setText("Привет! Это проверка голоса. [joy] Как же здорово звучит! [sad] Жаль, что всё кончается.");
        ScrollView sv = new ScrollView(this);
        sv.addView(text);
        root.addView(sv, new LinearLayout.LayoutParams(-1, 0, 1f));

        LinearLayout row = new LinearLayout(this);
        btnSpeak = new Button(this); btnSpeak.setText("Слушать"); btnSpeak.setEnabled(false);
        btnStop = new Button(this); btnStop.setText("Стоп"); btnStop.setEnabled(false);
        row.addView(btnSpeak, new LinearLayout.LayoutParams(0, -2, 1f));
        row.addView(btnStop, new LinearLayout.LayoutParams(0, -2, 1f));
        root.addView(row);
        setContentView(root);

        btnLoad.setOnClickListener(v -> loadModel());
        btnStop.setOnClickListener(v -> stopAll());
        btnSpeak.setOnClickListener(v -> {
            String t = text.getText().toString().trim();
            if (t.isEmpty() || engine == 0) return;
            String speaker = SPEAKERS[spk.getSelectedItemPosition()];
            speak(t, speaker);
        });
    }

    private File modelDir() { return new File(getFilesDir(), "model"); }

    private void loadModel() {
        btnLoad.setEnabled(false);
        setStatus("Загрузка модели (mmap)…");
        new Thread(() -> {
            stopAll();
            File dir = modelDir();
            File ggT = new File(dir, "talker_cp.q4.gguf");
            long h = qLoad(dir.getAbsolutePath(),
                           ggT.exists() ? ggT.getAbsolutePath() : "",
                           ggT.exists() ? ggT.getAbsolutePath() : "");
            ui.post(() -> {
                engine = h;
                boolean ok = h != 0;
                setStatus(ok ? "Модель загружена. Введите текст и слушайте."
                             : "Не удалось загрузить модель (нет файлов?) " + dir.getAbsolutePath());
                btnLoad.setEnabled(!ok);
                btnSpeak.setEnabled(ok);
            });
        }, "load").start();
    }

    private void speak(String t, String speaker) {
        btnSpeak.setEnabled(false); btnStop.setEnabled(true);
        setStatus("Синтез…");
        File out = new File(getCacheDir(), "tts_out.wav");
        worker = new Thread(() -> {
            long t0 = System.currentTimeMillis();
            int rc = qSynth(engine, t, speaker, "Russian", out.getAbsolutePath());
            long ms = System.currentTimeMillis() - t0;
            ui.post(() -> {
                btnSpeak.setEnabled(engine != 0); btnStop.setEnabled(false);
                if (rc == 0) {
                    setStatus("Готово за " + ms + " мс. Играю.");
                    play(out);
                } else {
                    setStatus("Ошибка/отмена синтеза (rc=" + rc + ").");
                }
            });
        }, "synth");
        worker.start();
    }

    private void play(File wav) {
        try {
            player = new MediaPlayer();
            player.setDataSource(wav.getAbsolutePath());
            player.setOnCompletionListener(mp -> ui.post(() -> setStatus("Прослушано. Можно ещё.")));
            player.prepare();
            player.start();
        } catch (Exception e) {
            setStatus("Ошибка воспроизведения: " + e.getMessage());
        }
    }

    private void stopAll() {
        if (engine != 0) qCancel(engine);
        if (player != null) { try { player.stop(); } catch (Exception ignore) {} player.release(); player = null; }
        Thread w = worker;
        if (w != null) { try { w.join(200); } catch (InterruptedException ignore) {} worker = null; }
    }

    private void setStatus(String s) { status.setText(s); }

    @Override protected void onDestroy() {
        stopAll();
        if (engine != 0) { qUnload(engine); engine = 0; }
        super.onDestroy();
    }
}
