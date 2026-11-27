package com.ferluht.fm1sampler;

import android.Manifest;
import android.app.Activity;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Bundle;
import android.view.View;
import android.view.WindowManager;
import java.io.File;

/** The app: starts the backend (engine, FM-1 link, audio), shows the sampler and opens the system file picker when the UI asks for a file. */
public class SamplerActivity extends Activity {
    static final int PICK = 7;
    Backend backend;
    SamplerView sampler;
    FieldRecordingView field;
    android.widget.FrameLayout root;
    boolean examining;
    FileManager files;

    @Override protected void onCreate(Bundle b) {
        super.onCreate(b);
        installCrashLog();
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON | WindowManager.LayoutParams.FLAG_FULLSCREEN);
        // Immersive mode alone does not extend the window into a display
        // cutout. In landscape Android otherwise reserves an entire strip.
        WindowManager.LayoutParams attrs=getWindow().getAttributes();
        if(android.os.Build.VERSION.SDK_INT>=30){
            attrs.layoutInDisplayCutoutMode=WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_ALWAYS;
            getWindow().setAttributes(attrs);
            getWindow().setDecorFitsSystemWindows(false);
        }else if(android.os.Build.VERSION.SDK_INT>=28){
            attrs.layoutInDisplayCutoutMode=WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
            getWindow().setAttributes(attrs);
        }
        getWindow().setStatusBarColor(0xff1a1a1f);
        getWindow().setNavigationBarColor(0xff1a1a1f);
        backend = Backend.get(this);
        backend.onFileRequest = this::pickFile;
        backend.start();
        sampler = new SamplerView(this, backend.engine, 0, 0);
        root=new android.widget.FrameLayout(this);
        root.addView(sampler,new android.widget.FrameLayout.LayoutParams(-1,-1));
        root.setFitsSystemWindows(false);
        setContentView(root);
        // Log actual window dimensions and the camera bounds for diagnosing
        // OEM letterboxing. Do not pad the entire side of the sampler.
        root.addOnLayoutChangeListener((view,l,t,r,bottom,oldL,oldT,oldR,oldB)->{
            if(r-l==oldR-oldL&&bottom-t==oldB-oldT)return;
            android.view.WindowInsets insets=view.getRootWindowInsets();
            String cutout="none";
            if(android.os.Build.VERSION.SDK_INT>=28&&insets!=null&&insets.getDisplayCutout()!=null)
                cutout=insets.getDisplayCutout().getBoundingRects().toString();
            int[] xy=new int[2];view.getLocationOnScreen(xy);
            android.util.Log.i("fm1sampler","Fullscreen root="+(r-l)+"x"+(bottom-t)+" origin="+xy[0]+","+xy[1]+" cutout="+cutout);
        });
        backend.onFieldFrame=this::fieldFrame;
        files=new FileManager(this,backend);
        backend.onFileAction=action->{if(!examining){if(action==1)files.show();else if(action==2)files.saveProject();}};
        hideSystemBars();
        if (checkSelfPermission(Manifest.permission.RECORD_AUDIO) == PackageManager.PERMISSION_GRANTED) backend.startMic();
        else requestPermissions(new String[]{Manifest.permission.RECORD_AUDIO}, 1);
    }

    void fieldFrame(boolean active,int[] events) {
        if(active!=examining){
            examining=active;
            if(active){
                backend.setFieldAudio(true);
                if(field==null){
                    field=new FieldRecordingView(this,backend);
                    root.addView(field,new android.widget.FrameLayout.LayoutParams(-1,-1));
                }
                sampler.setVisibility(View.GONE);
                field.enter();
            }else{
                sampler.setVisibility(View.VISIBLE);
                if(field!=null)field.leave(()->{if(!examining)backend.setFieldAudio(false);});
                else backend.setFieldAudio(false);
            }
        }
        if(active&&field!=null)field.events(events);
    }
    @Override public void onBackPressed(){
        if(examining){Native.setFieldMode(backend.engine,false);fieldFrame(false,new int[0]);}
        else super.onBackPressed();
    }
    @Override protected void onPause(){
        if(examining&&field!=null)field.suspendCapture();
        super.onPause();
    }
    @Override protected void onDestroy(){
        backend.onFieldFrame=null;
        backend.onFileRequest=null;
        backend.onFileAction=null;
        if(files!=null)files.close();
        if(field!=null){field.dispose();root.removeView(field);field.destroy();}
        Native.setFieldMode(backend.engine,false);backend.setFieldAudio(false);
        super.onDestroy();
    }

    /** Full screen: no status or navigation bar (a swipe from the edge shows them for a moment). */
    void hideSystemBars() {
        if(android.os.Build.VERSION.SDK_INT>=30){
            android.view.WindowInsetsController controller=getWindow().getInsetsController();
            if(controller!=null){
                controller.setSystemBarsBehavior(android.view.WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
                controller.hide(android.view.WindowInsets.Type.systemBars());
                return;
            }
        }
        getWindow().getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_LAYOUT_STABLE | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
            | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_FULLSCREEN | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);
    }
    @Override public void onWindowFocusChanged(boolean focus) { super.onWindowFocusChanged(focus); if (focus) hideSystemBars(); }
    @Override protected void onResume() { super.onResume(); hideSystemBars(); }

    /** Java crashes end up in files/crash.log (the last one), so a crash can be read without adb. */
    void installCrashLog() {
        final Thread.UncaughtExceptionHandler prev = Thread.getDefaultUncaughtExceptionHandler();
        final File f = new File(getFilesDir(), "crash.log");
        Thread.setDefaultUncaughtExceptionHandler((t, e) -> {
            try (java.io.PrintWriter w = new java.io.PrintWriter(new java.io.FileWriter(f))) { w.println(new java.util.Date() + " thread " + t.getName()); e.printStackTrace(w); } catch (Exception ignored) { }
            if (prev != null) prev.uncaughtException(t, e);
        });
    }

    void pickFile() {
        Intent i = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        i.addCategory(Intent.CATEGORY_OPENABLE);
        i.setType("*/*");
        i.putExtra(Intent.EXTRA_MIME_TYPES, new String[]{"audio/*", "application/ogg"});
        try { startActivityForResult(i, PICK); } catch (Exception e) { Native.say(backend.engine, "NO FILE PICKER ON THIS PHONE"); }
    }

    @Override protected void onActivityResult(int code, int result, Intent data) {
        super.onActivityResult(code, result, data);
        if(code==FieldRecordingView.PICK_FIELD&&field!=null){field.fileResult(result,data);return;}
        if (code != PICK || result != RESULT_OK || data == null || data.getData() == null) return;
        final Uri uri = data.getData();
        Native.say(backend.engine, "READING THE FILE...");
        new Thread(() -> {   // decoding can take a moment: off the UI thread
            try {
                File out = AudioImport.importTo(this, uri, getFilesDir());
                Native.importFile(backend.engine, out.getAbsolutePath());
            } catch (Exception e) {
                android.util.Log.w("fm1sampler", "import: " + e);
                Native.say(backend.engine, "CANNOT READ THIS FILE");
            }
        }, "import").start();
    }

    @Override public void onRequestPermissionsResult(int code, String[] perms, int[] res) {
        super.onRequestPermissionsResult(code,perms,res);
        if(code==74&&field!=null){field.permissionResult(res.length>0&&res[0]==PackageManager.PERMISSION_GRANTED);return;}
        if (res.length > 0 && res[0] == PackageManager.PERMISSION_GRANTED) backend.startMic();
    }
}
