package com.ferluht.fm1sampler;

import android.app.AlertDialog;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.view.View;
import android.view.Window;
import android.widget.*;
import android.text.InputType;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Comparator;

/** Private USER library and independent project snapshots. No system file permissions needed. */
final class FileManager {
    private final SamplerActivity activity;
    private final Backend backend;
    private AlertDialog dialog, prompt;
    private boolean projects=false,busy=false,closed=false;
    private String selected="";
    private int bg,surface,raised,ink,dim,blue,green,red,outline;
    private final ArrayList<File> rows=new ArrayList<>();
    FileManager(SamplerActivity a,Backend b){activity=a;backend=b;}
    void close(){closed=true;if(prompt!=null)prompt.dismiss();if(dialog!=null)dialog.dismiss();}
    private void colors(){
        boolean light=Native.lightTheme(backend.engine);
        bg=light?0xffc6c9cb:0xff181526;surface=light?0xffd9dce0:0xff201c32;
        raised=light?0xffe9e9e5:0xff302845;ink=light?0xff161b29:0xfff2f2ff;
        dim=light?0xff4e5868:0xffb3a9cb;blue=light?0xff164bc9:0xff49d9ff;
        green=light?0xff007d58:0xff53ffab;red=light?0xffc42248:0xffff5279;outline=light?0xff828b99:0xff685588;
    }
    private int dp(int n){return (int)(activity.getResources().getDisplayMetrics().density*n+.5f);}
    private GradientDrawable panel(int fill){GradientDrawable d=new GradientDrawable();d.setColor(fill);d.setStroke(dp(1),outline);return d;}
    private TextView label(String text,int size,int color){
        TextView v=new TextView(activity);v.setText(text);v.setTextColor(color);v.setTextSize(size);v.setTypeface(Typeface.MONOSPACE);v.setGravity(android.view.Gravity.CENTER_VERTICAL);v.setPadding(dp(8),dp(4),dp(8),dp(4));return v;
    }
    private Button key(String text,int color,Runnable fn){
        Button v=new Button(activity);v.setText(text);v.setTextColor(color);v.setTextSize(12);v.setTypeface(Typeface.MONOSPACE,Typeface.BOLD);v.setBackground(panel(surface));v.setMinHeight(0);v.setMinimumHeight(0);v.setPadding(dp(5),0,dp(5),0);v.setOnClickListener(x->fn.run());return v;
    }
    private LinearLayout vertical(){LinearLayout v=new LinearLayout(activity);v.setOrientation(LinearLayout.VERTICAL);return v;}
    private void addKey(LinearLayout row,Button b){LinearLayout.LayoutParams p=new LinearLayout.LayoutParams(0,dp(35),1);p.setMargins(dp(2),0,dp(2),0);row.addView(b,p);}
    private String title(File f){
        String n=f.getName();
        if(f.isDirectory()){
            try(BufferedReader r=new BufferedReader(new InputStreamReader(new FileInputStream(new File(f,"name.txt")),StandardCharsets.UTF_8))){
                String s=r.readLine();if(s!=null&&!s.trim().isEmpty())return s;
            }catch(IOException ignored){}
            return n;
        }
        return n.startsWith("sample_")&&n.endsWith(".wav")?n.substring(7,n.length()-4):n;
    }
    private File pick(){for(File f:rows)if(f.getAbsolutePath().equals(selected))return f;return null;}
    private void list(){
        rows.clear();File[] all=activity.getFilesDir().listFiles();
        if(all!=null){
            Arrays.sort(all,Comparator.comparingLong(File::lastModified).reversed());
            for(File f:all){
                if(projects&&f.isDirectory()&&f.getName().matches("project_[0-9]+")&&new File(f,"state.txt").isFile()&&new File(f,"loops.bin").isFile())rows.add(f);
                if(!projects&&f.isFile()&&f.getName().startsWith("sample_")&&f.getName().endsWith(".wav"))rows.add(f);
            }
        }
        if(pick()==null)selected="";
    }
    void show(){
        if(closed||busy||activity.isFinishing()||activity.examining)return;
        if(dialog!=null&&dialog.isShowing())return;
        colors();list();
        dialog=new AlertDialog.Builder(activity).create();
        dialog.setOnDismissListener(x->{dialog=null;activity.hideSystemBars();});
        dialog.show();render();
        Window w=dialog.getWindow();if(w!=null){w.setBackgroundDrawableResource(android.R.color.transparent);w.setLayout(Math.min(dp(620),activity.getResources().getDisplayMetrics().widthPixels-dp(32)),Math.min(dp(355),activity.getResources().getDisplayMetrics().heightPixels-dp(24)));}
    }
    private void render(){
        if(dialog==null)return;
        list();LinearLayout box=vertical();box.setPadding(dp(8),dp(8),dp(8),dp(8));box.setBackground(panel(bg));
        TextView head=label("FILES / PROJECTS",16,Native.lightTheme(backend.engine)?Color.WHITE:0xff10101a);head.setTypeface(Typeface.MONOSPACE,Typeface.BOLD);head.setBackgroundColor(blue);box.addView(head,new LinearLayout.LayoutParams(-1,dp(29)));
        LinearLayout tabs=new LinearLayout(activity);addKey(tabs,key(projects?"USER SAMPLES":"● USER SAMPLES",projects?dim:blue,()->{projects=false;selected="";render();}));
        addKey(tabs,key(projects?"● PROJECTS":"PROJECTS",projects?blue:dim,()->{projects=true;selected="";render();}));
        addKey(tabs,key("CLOSE",ink,()->dialog.dismiss()));box.addView(tabs);
        ScrollView scroll=new ScrollView(activity);scroll.setBackground(panel(raised));LinearLayout list=vertical();
        if(rows.isEmpty())list.addView(label(projects?"NO SAVED PROJECTS":"NO USER SAMPLES",12,dim));
        for(File file:rows){
            boolean on=file.getAbsolutePath().equals(selected);
            TextView row=label(title(file),13,on?(Native.lightTheme(backend.engine)?Color.WHITE:0xff10101a):ink);
            row.setSingleLine(true);row.setEllipsize(android.text.TextUtils.TruncateAt.END);row.setBackgroundColor(on?blue:raised);
            row.setOnClickListener(x->{selected=file.getAbsolutePath();render();});list.addView(row,new LinearLayout.LayoutParams(-1,dp(35)));
            View sep=new View(activity);sep.setBackgroundColor(outline);list.addView(sep,new LinearLayout.LayoutParams(-1,dp(1)));
        }
        scroll.addView(list);box.addView(scroll,new LinearLayout.LayoutParams(-1,0,1));
        LinearLayout actions=new LinearLayout(activity);
        Button open=key("OPEN",green,()->{File f=pick();if(f==null)return;if(projects)confirmOpen(f);else operation(0,f,"",true);});
        Button rename=key("RENAME",blue,()->{File f=pick();if(f!=null)namePrompt("RENAME",title(f),s->operation(1,f,s,false));});
        Button delete=key("DELETE",red,()->{File f=pick();if(f!=null)confirmDelete(f);});
        open.setEnabled(pick()!=null);rename.setEnabled(pick()!=null);delete.setEnabled(pick()!=null);
        addKey(actions,open);addKey(actions,rename);addKey(actions,delete);
        addKey(actions,key(projects?"SAVE PROJECT":"IMPORT",blue,()->{if(projects)saveProject();else{dialog.dismiss();activity.pickFile();}}));
        box.addView(actions);dialog.setContentView(box);
    }
    private void confirmOpen(File f){
        new AlertDialog.Builder(activity).setTitle("Open project").setMessage("Replace the current session with \""+title(f)+"\"? Save any unsaved work first.")
            .setNegativeButton("Cancel",null).setPositiveButton("Open",(x,y)->operation(4,f,"",true)).show();
    }
    private void confirmDelete(File f){
        new AlertDialog.Builder(activity).setTitle("Delete saved item").setMessage("Permanently delete \""+title(f)+"\"?")
            .setNegativeButton("Cancel",null).setPositiveButton("Delete",(x,y)->operation(2,f,"",false)).show();
    }
    interface Named {void run(String s);}
    private void namePrompt(String title,String initial,Named fn){
        if(prompt!=null&&prompt.isShowing())return;
        EditText e=new EditText(activity);e.setSingleLine(true);e.setInputType(InputType.TYPE_CLASS_TEXT|InputType.TYPE_TEXT_FLAG_CAP_SENTENCES);e.setText(initial);e.selectAll();
        AlertDialog p=new AlertDialog.Builder(activity).setTitle(title).setView(e).setNegativeButton("Cancel",null).setPositiveButton("OK",null).create();
        prompt=p;p.setOnDismissListener(x->{prompt=null;activity.hideSystemBars();});p.show();
        p.getButton(AlertDialog.BUTTON_POSITIVE).setOnClickListener(x->{String s=e.getText().toString().trim();if(s.isEmpty()){e.setError("Name required");return;}p.dismiss();fn.run(s);});
        if(p.getWindow()!=null)p.getWindow().setSoftInputMode(android.view.WindowManager.LayoutParams.SOFT_INPUT_STATE_ALWAYS_VISIBLE);
        e.requestFocus();
    }
    void saveProject(){if(!closed&&!busy&&!activity.examining)namePrompt("SAVE PROJECT","Untitled project",s->operation(3,null,s,true));}
    private void operation(int op,File f,String name,boolean dismiss){
        if(busy||closed||activity.examining)return;busy=true;
        if(dismiss&&dialog!=null)dialog.dismiss();
        // Project manifests/loop decoding and filesystem changes must not run on the Android UI thread.
        new Thread(()->{
            String error;
            try{error=Native.manageFile(backend.engine,op,f==null?"":f.getAbsolutePath(),name);}
            catch(Throwable ex){error="FILE OPERATION FAILED";}
            final String result=error;
            activity.runOnUiThread(()->{
                busy=false;if(closed||activity.isFinishing())return;
                if(result!=null&&!result.isEmpty())new AlertDialog.Builder(activity).setTitle("FILES").setMessage(result).setPositiveButton("OK",null).show();
                if(dialog!=null&&dialog.isShowing()){selected="";render();}
            });
        },"fm1-files").start();
    }
}
