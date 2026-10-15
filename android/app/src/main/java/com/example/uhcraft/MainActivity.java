package com.example.uhcraft;

import android.app.Activity;
import android.os.Bundle;

public class MainActivity extends Activity {
    static {
        System.loadLibrary("uhcraft");
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
    }

    public native String stringFromJNI();
}
