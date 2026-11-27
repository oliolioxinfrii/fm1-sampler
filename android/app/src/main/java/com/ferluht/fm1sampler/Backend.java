package com.ferluht.fm1sampler;

import android.content.Context;
import android.media.AudioDeviceCallback;
import android.media.AudioDeviceInfo;
import android.media.AudioManager;
import android.media.midi.MidiDevice;
import android.media.midi.MidiDeviceInfo;
import android.media.midi.MidiInputPort;
import android.media.midi.MidiManager;
import android.media.midi.MidiOutputPort;
import android.media.midi.MidiReceiver;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.util.Log;

/** Owns the native engine and the link to the FM-1: finds it on USB (MIDI + audio), routes the audio to it, runs the 30 Hz surface tick. */
public class Backend {
    static final String TAG = "fm1sampler";
    public static Backend instance;
    public final long engine;
    final Context ctx;
    final Handler main = new Handler(Looper.getMainLooper());
    final MidiManager midi;
    final AudioManager am;
    MidiDevice device;
    volatile MidiInputPort toFm1;
    MidiOutputPort fromFm1;
    volatile boolean running;
    Thread ticker;
    boolean audioStarted, micStarted, micWanted;
    volatile int usbAudioId = 0;
    boolean forceAudioRestart;
    final Runnable audioRestart = () -> {
        boolean force = forceAudioRestart;
        forceAudioRestart = false;
        if (running && (force || !audioStarted || findUsbAudio() != usbAudioId)) startAudio();
    };
    /** Set by the activity: opens the system file picker (the phone UI asked for LOAD FILE FROM DISK). */
    public volatile Runnable onFileRequest;
    interface FileActionListener {void accept(int action);}
    public volatile FileActionListener onFileAction;
    interface FieldListener { void frame(boolean active,int[] events); }
    public volatile FieldListener onFieldFrame;
    volatile boolean fieldAudio=false;
    // Web Audio must have exclusive microphone/output access during examination.
    synchronized void setFieldAudio(boolean active) {
        if(fieldAudio==active)return;
        fieldAudio=active;
        main.removeCallbacks(audioRestart);forceAudioRestart=false;
        if(active){Native.stopAudio(engine);audioStarted=micStarted=false;}
        else if(running){startAudio();if(micWanted)startMic();}
    }

    public static synchronized Backend get(Context c) {
        if (instance == null) instance = new Backend(c.getApplicationContext());
        return instance;
    }

    Backend(Context c) {
        ctx = c;
        java.io.File samples = new java.io.File(c.getFilesDir(), "samples");   // the bundled sample library, copied out once: the engine reads plain files
        if (!new java.io.File(samples, ".v1").exists()) {
            try { copyAssets(c, "samples", samples); new java.io.File(samples, ".v1").createNewFile(); } catch (java.io.IOException e) { android.util.Log.w("fm1sampler", "samples: " + e); }
        }
        engine = Native.create(c.getFilesDir().getAbsolutePath(), samples.getAbsolutePath());
        midi = (MidiManager) c.getSystemService(Context.MIDI_SERVICE);
        am = (AudioManager) c.getSystemService(Context.AUDIO_SERVICE);
    }

    public synchronized void start() {
        if (running) return;
        running = true;
        startAudio();
        if (midi != null) {
            midi.registerDeviceCallback(new MidiManager.DeviceCallback() {
                @Override public void onDeviceAdded(MidiDeviceInfo i) { try { connectMidi(); } catch (Throwable t) { Log.w(TAG, "midi add: " + t); } }
                @Override public void onDeviceRemoved(MidiDeviceInfo i) { try { disconnectMidi(); connectMidi(); } catch (Throwable t) { Log.w(TAG, "midi remove: " + t); } }
            }, main);
            connectMidi();
        }
        am.registerAudioDeviceCallback(new AudioDeviceCallback() {
            @Override public void onAudioDevicesAdded(AudioDeviceInfo[] a) { try { restartAudioIfUsbChanged(); if (micWanted && !micStarted) startMic(); } catch (Throwable t) { Log.w(TAG, "audio add: " + t); } }
            @Override public void onAudioDevicesRemoved(AudioDeviceInfo[] a) { try { restartAudioIfUsbChanged(); } catch (Throwable t) { Log.w(TAG, "audio remove: " + t); } }
        }, main);
        ticker = new Thread(() -> {
            long t0 = SystemClock.uptimeMillis(), lastStats = 0;
            int lastXruns = -2, lastOverruns = -1;
            boolean lastField=false; FieldListener lastListener=null;
            while (running) {
                try {
                    MidiInputPort port = toFm1;
                    byte[] out = Native.tick(engine, SystemClock.uptimeMillis() - t0);
                    send(port, out);
                    Native.uiLink(engine, toFm1 != null, usbAudioId != 0);
                    if (Native.wantFile(engine)) { Runnable r = onFileRequest; if (r != null) main.post(r); }
                    int action=Native.fileAction(engine);
                    FileActionListener actionListener=onFileAction;
                    if(action!=0&&actionListener!=null)main.post(()->{if(onFileAction==actionListener)actionListener.accept(action);});
                    boolean field=Native.fieldMode(engine);
                    int[] events=Native.fieldEvents(engine);
                    FieldListener listener=onFieldFrame;
                    if(listener!=null&&(field!=lastField||listener!=lastListener||events.length>0)){
                        main.post(()->{if(onFieldFrame==listener)listener.frame(field,events);});
                    }
                    lastField=field;lastListener=listener;
                    if (Native.audioDead(engine)&&!fieldAudio) main.post(() -> scheduleAudioRestart(true));
                    long now = SystemClock.uptimeMillis();
                    if (now - lastStats >= 2000) {
                        lastStats = now;
                        int xruns = Native.xruns(engine), overruns = Native.audioOverruns(engine);
                        if (xruns != lastXruns || overruns != lastOverruns) {
                            Log.i(TAG, "audio diagnostics: xruns=" + xruns + " processing overruns=" + overruns);
                            lastXruns = xruns; lastOverruns = overruns;
                        }
                    }
                } catch (Throwable t) { Log.w(TAG, "tick: " + t); }
                try { Thread.sleep(16); } catch (InterruptedException ignored) { }
            }
        }, "fm1-surface");
        ticker.start();
    }

    public synchronized void stop() {
        running = false;
        main.removeCallbacks(audioRestart);
        forceAudioRestart = false;
        try { if (ticker != null) ticker.join(300); } catch (InterruptedException ignored) { }
        Native.stopAudio(engine);
        audioStarted = micStarted = false;
        disconnectMidi();
    }

    int findUsbAudio() {
        int id = 0;
        for (AudioDeviceInfo d : am.getDevices(AudioManager.GET_DEVICES_OUTPUTS))
            if (d.getType() == AudioDeviceInfo.TYPE_USB_DEVICE || d.getType() == AudioDeviceInfo.TYPE_USB_HEADSET) id = d.getId();
        return id;
    }
    void startAudio() {
        if(fieldAudio)return;
        usbAudioId = findUsbAudio();
        int r = Native.startAudio(engine, usbAudioId);
        audioStarted = r == 0;
        Log.i(TAG, "audio start " + r + " usb device " + usbAudioId);
    }
    void restartAudioIfUsbChanged() {
        if (!running || fieldAudio) return;
        int id = findUsbAudio();
        if (id != usbAudioId) {
            Log.i(TAG, "usb audio device changed " + usbAudioId + " -> " + id);
            scheduleAudioRestart(false);
        }
    }
    // Device notifications and stream errors can describe the same routing change.
    void scheduleAudioRestart(boolean force) {
        if (!running || fieldAudio) return;
        forceAudioRestart |= force;
        main.removeCallbacks(audioRestart);
        main.postDelayed(audioRestart, 300);
    }
    /** Call after RECORD_AUDIO was granted. The phone's own microphone, never the USB device. */
    public synchronized void startMic() {
        micWanted=true;
        if (micStarted||fieldAudio) return;
        int mic = 0;
        for (AudioDeviceInfo d : am.getDevices(AudioManager.GET_DEVICES_INPUTS)) if (d.getType() == AudioDeviceInfo.TYPE_BUILTIN_MIC) mic = d.getId();
        micWanted = true;
        micStarted = Native.startInput(engine, mic) == 0;
        Log.i(TAG, "mic start " + micStarted + " device " + mic);
    }

    // ---- MIDI ----
    boolean connecting;
    synchronized void connectMidi() {
        if (toFm1 != null || midi == null || connecting) return;
        MidiDeviceInfo pick = null;
        for (MidiDeviceInfo i : midi.getDevices()) {
            String n = String.valueOf(i.getProperties().getString(MidiDeviceInfo.PROPERTY_NAME)).toLowerCase();
            if (n.contains("fm") || n.contains("m-vave") || n.contains("mvave")) pick = i;
        }
        if (pick == null) return;
        connecting = true;   // openDevice is asynchronous and the device callback fires once per port: open it once
        midi.openDevice(pick, d -> {
            synchronized (Backend.this) {
                connecting = false;
                if (d == null || toFm1 != null) { if (d != null) try { d.close(); } catch (Exception ignored) { } return; }
                try {
                    MidiInputPort in = d.openInputPort(0);
                    MidiOutputPort outp = d.openOutputPort(0);
                    if (in == null) { d.close(); return; }
                    device = d; fromFm1 = outp;
                    if (outp != null) outp.connect(new MidiReceiver() {
                        @Override public void onSend(byte[] data, int off, int count, long ts) { try { Native.midiIn(engine, data, off, count, SystemClock.uptimeMillis()); } catch (Throwable t) { Log.w(TAG, "midiIn: " + t); } }
                    });
                    Native.surfaceConnected(engine);
                    toFm1 = in;
                    Log.i(TAG, "FM-1 MIDI connected");
                } catch (Throwable t) { Log.w(TAG, "open ports: " + t); }
            }
        }, main);
    }
    synchronized void disconnectMidi() {
        try { if (fromFm1 != null) fromFm1.close(); } catch (Exception ignored) { }
        try { if (toFm1 != null) toFm1.close(); } catch (Exception ignored) { }
        try { if (device != null) device.close(); } catch (Exception ignored) { }
        fromFm1 = null; toFm1 = null; device = null;
    }
    void send(MidiInputPort port, byte[] b) {
        if (port == null || port != toFm1 || b.length == 0) return;
        int s = 0;
        for (int i = 0; i < b.length; i++) if ((b[i] & 0xff) == 0xf7) {
            try { port.send(b, s, i - s + 1); } catch (Exception e) { Log.w(TAG, "send: " + e); return; }
            s = i + 1;
        }
    }

    static void copyAssets(Context c, String from, java.io.File to) throws java.io.IOException {
        String[] list = c.getAssets().list(from);
        if (list == null || list.length == 0) {   // a file
            to.getParentFile().mkdirs();
            try (java.io.InputStream in = c.getAssets().open(from); java.io.OutputStream out = new java.io.FileOutputStream(to)) {
                byte[] b = new byte[65536]; int n; while ((n = in.read(b)) > 0) out.write(b, 0, n);
            }
            return;
        }
        to.mkdirs();
        for (String f : list) copyAssets(c, from + "/" + f, new java.io.File(to, f));
    }
}
