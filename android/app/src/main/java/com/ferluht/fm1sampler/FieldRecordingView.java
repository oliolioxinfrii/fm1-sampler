package com.ferluht.fm1sampler;

import android.Manifest;
import android.app.Activity;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.util.Base64;
import android.view.View;
import android.webkit.*;
import java.io.*;
import java.util.ArrayList;
import java.util.UUID;

/** The approved field study runs locally, with native hardware events and atomic library exports. */
final class FieldRecordingView extends WebView {
    static final int PICK_FIELD=73;
    private static final String PAGE="https://fm1.local/field.html";
    private final SamplerActivity activity;
    private final Backend backend;
    private boolean ready=false;
    private volatile boolean active=false;
    private final ArrayList<Integer> pending=new ArrayList<>();
    private ValueCallback<Uri[]> picker;
    private PermissionRequest micRequest;
    private File temp,target;
    private FileOutputStream output;
    private long written;
    FieldRecordingView(SamplerActivity a,Backend b){
        super(a);activity=a;backend=b;
        setBackgroundColor(0xffc6c9cb);setOverScrollMode(OVER_SCROLL_NEVER);
        WebSettings s=getSettings();
        s.setJavaScriptEnabled(true);s.setMediaPlaybackRequiresUserGesture(false);
        s.setAllowFileAccess(false);s.setAllowContentAccess(true);s.setDomStorageEnabled(false);
        s.setBlockNetworkLoads(true);
        addJavascriptInterface(new Bridge(),"FM1Native");
        setWebViewClient(new WebViewClient(){
            @Override public WebResourceResponse shouldInterceptRequest(WebView v,WebResourceRequest req){
                if(PAGE.equals(req.getUrl().toString())){
                    try{return new WebResourceResponse("text/html","UTF-8",activity.getAssets().open("field-recording.html"));}
                    catch(IOException e){android.util.Log.e("fm1sampler","field asset",e);}
                }
                return new WebResourceResponse("text/plain","UTF-8",new ByteArrayInputStream(new byte[0]));
            }
            @Override public boolean shouldOverrideUrlLoading(WebView v,WebResourceRequest r){return !PAGE.equals(r.getUrl().toString());}
            @Override public void onPageFinished(WebView v,String url){if(PAGE.equals(url)){ready=true;syncActive();flush();}}
        });
        setWebChromeClient(new WebChromeClient(){
            @Override public boolean onShowFileChooser(WebView v,ValueCallback<Uri[]> cb,FileChooserParams p){
                if(picker!=null)picker.onReceiveValue(null);
                picker=cb;
                Intent i=new Intent(Intent.ACTION_OPEN_DOCUMENT);i.addCategory(Intent.CATEGORY_OPENABLE);i.setType("audio/*");
                try{activity.startActivityForResult(i,PICK_FIELD);}catch(Exception ex){picker.onReceiveValue(null);picker=null;}
                return true;
            }
            @Override public void onPermissionRequest(PermissionRequest r){
                activity.runOnUiThread(()->{
                    if(!active||!r.getOrigin().toString().equals("https://fm1.local/")&&!r.getOrigin().toString().equals("https://fm1.local")){
                        r.deny();return;
                    }
                    boolean audio=false;for(String q:r.getResources())if(PermissionRequest.RESOURCE_AUDIO_CAPTURE.equals(q))audio=true;
                    if(!audio){r.deny();return;}
                    if(activity.checkSelfPermission(Manifest.permission.RECORD_AUDIO)!=PackageManager.PERMISSION_GRANTED){
                        if(micRequest!=null)micRequest.deny();micRequest=r;
                        activity.requestPermissions(new String[]{Manifest.permission.RECORD_AUDIO},74);return;
                    }
                    r.grant(new String[]{PermissionRequest.RESOURCE_AUDIO_CAPTURE});
                });
            }
            @Override public void onPermissionRequestCanceled(PermissionRequest r){if(micRequest==r)micRequest=null;}
            @Override public boolean onConsoleMessage(ConsoleMessage msg){
                android.util.Log.d("FM1Field",msg.message()+" : "+msg.lineNumber());return true;
            }
        });
        try(InputStream in=activity.getAssets().open("field-recording.html");ByteArrayOutputStream bytes=new ByteArrayOutputStream()){
            byte[] buf=new byte[8192];int n;while((n=in.read(buf))!=-1)bytes.write(buf,0,n);
            loadDataWithBaseURL(PAGE,bytes.toString("UTF-8"),"text/html","UTF-8",PAGE);
        }catch(IOException e){
            android.util.Log.e("FM1Field","Cannot open field recording asset",e);
            activity.runOnUiThread(()->{Native.setFieldMode(backend.engine,false);activity.fieldFrame(false,new int[0]);});
        }
    }
    void permissionResult(boolean granted){
        if(micRequest==null)return;
        PermissionRequest r=micRequest;micRequest=null;
        if(granted&&active)r.grant(new String[]{PermissionRequest.RESOURCE_AUDIO_CAPTURE});else r.deny();
    }
    void fileResult(int result,Intent data){
        ValueCallback<Uri[]> cb=picker;picker=null;
        if(cb!=null)cb.onReceiveValue(active&&result==Activity.RESULT_OK&&data!=null&&data.getData()!=null?new Uri[]{data.getData()}:null);
    }
    void enter(){active=true;setVisibility(View.VISIBLE);onResume();syncActive();}
    void leave(Runnable after){
        active=false;pending.clear();setVisibility(View.GONE);abortSave();
        if(micRequest!=null){micRequest.deny();micRequest=null;}
        final boolean[] completed={false};
        Runnable done=()->{if(completed[0])return;completed[0]=true;if(!active)onPause();after.run();};
        if(ready){evaluateJavascript("window.fm1Host&&fm1Host.active(false,"+Native.lightTheme(backend.engine)+")",v->done.run());postDelayed(done,750);}
        else done.run();
    }
    void suspendCapture(){if(ready)evaluateJavascript("window.fm1Host&&fm1Host.suspend()",null);}
    void dispose(){leave(()->{});if(picker!=null){picker.onReceiveValue(null);picker=null;}if(micRequest!=null){micRequest.deny();micRequest=null;}removeJavascriptInterface("FM1Native");}
    private void syncActive(){
        if(ready)evaluateJavascript("window.fm1Host&&fm1Host.active("+active+","+Native.lightTheme(backend.engine)+")",null);
    }
    void events(int[] data){
        if(!active)return;
        for(int x:data){if(pending.size()>=512){pending.remove(0);pending.remove(0);}pending.add(x);}
        flush();
    }
    private void flush(){
        if(!ready||!active||pending.isEmpty())return;
        String args=pending.toString();pending.clear();
        evaluateJavascript("window.fm1Host&&fm1Host.events("+args+")",null);
    }
    private synchronized void abortSave(){
        if(output!=null)try{output.close();}catch(IOException ignored){}
        output=null;if(temp!=null)temp.delete();temp=target=null;written=0;
    }
    public final class Bridge{
        @JavascriptInterface public void exit(){activity.runOnUiThread(()->{Native.setFieldMode(backend.engine,false);activity.fieldFrame(false,new int[0]);});}
        @JavascriptInterface public void feedback(String summary,int leds,String pixels){if(active&&summary!=null&&summary.length()<4096&&pixels!=null&&(pixels.length()==756||pixels.length()==1512))Native.fieldFeedback(backend.engine,summary,leds,pixels);}
        @JavascriptInterface public boolean beginSave(String name){
            synchronized(FieldRecordingView.this){
                abortSave();if(!active||name==null)return false;
                String safe=name.replaceAll("[^a-zA-Z0-9 _.-]","-").replaceAll("^\\.+","");
                if(safe.length()>80)safe=safe.substring(0,80);
                String id=UUID.randomUUID().toString().substring(0,8);
                File dir=activity.getFilesDir();
                target=new File(dir,"sample_"+id+"_"+safe.replaceFirst("(?i)\\.wav$","")+".wav");
                try{temp=File.createTempFile("field_",".tmp",dir);output=new FileOutputStream(temp);return true;}
                catch(IOException e){abortSave();return false;}
            }
        }
        @JavascriptInterface public boolean appendSave(String chunk){
            synchronized(FieldRecordingView.this){
                if(output==null||chunk==null||chunk.length()>100000)return false;
                try{byte[] b=Base64.decode(chunk,Base64.NO_WRAP);written+=b.length;if(written>268435456L)throw new IOException("too large");output.write(b);return true;}
                catch(Exception e){abortSave();return false;}
            }
        }
        @JavascriptInterface public boolean finishSave(){
            synchronized(FieldRecordingView.this){
                if(output==null||written<44){abortSave();return false;}
                try{output.getFD().sync();output.close();output=null;
                    if(!temp.renameTo(target)){abortSave();return false;}
                    temp=target=null;written=0;Native.refreshLibrary(backend.engine);return true;
                }catch(Exception e){abortSave();return false;}
            }
        }
        @JavascriptInterface public void cancelSave(){abortSave();}
    }
}
