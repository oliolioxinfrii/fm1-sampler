package com.ferluht.fm1sampler;

import android.graphics.Rect;
import android.os.Build;
import android.view.DisplayCutout;
import android.view.View;
import android.view.WindowInsets;

/** Actual left-edge display obstruction, in local view pixels. No device-model assumptions. */
final class CutoutGeometry {
    private CutoutGeometry() {}
    static Rect left(View view) {
        Rect result=new Rect();
        if(Build.VERSION.SDK_INT<28 || view.getWidth()<=0 || view.getHeight()<=0)return result;
        WindowInsets insets=view.getRootWindowInsets();
        DisplayCutout cutout=insets==null?null:insets.getDisplayCutout();
        if(cutout==null || cutout.getSafeInsetLeft()<=0)return result;
        int[] origin=new int[2];
        view.getLocationInWindow(origin);
        for(Rect bounds:cutout.getBoundingRects()){
            if(bounds.isEmpty()||bounds.left>=cutout.getSafeInsetLeft())continue;
            Rect local=new Rect(bounds);
            local.offset(-origin[0],-origin[1]);
            if(local.intersect(0,0,view.getWidth(),view.getHeight()))result.union(local);
        }
        return result;
    }
}
