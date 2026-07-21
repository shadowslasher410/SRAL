-keepdirectories org.sral
-keepclassmembers class org.sral.** { *; }
-keep class org.sral.** {
    <init>(...);
    *;
}
-keepclassmembers class * extends android.speech.tts.UtteranceProgressListener {
    public void onStart(java.lang.String);
    public void onDone(java.lang.String);
    public void onError(java.lang.String);
    public void onError(java.lang.String, int);
    public void onStop(java.lang.String, boolean);
}

-keepclassmembers class * implements android.speech.tts.TextToSpeech$OnInitListener {
    public void onInit(int);
}
-keep class * implements androidx.lifecycle.DefaultLifecycleObserver {
    public void onCreate(androidx.lifecycle.LifecycleOwner);
    public void onStart(androidx.lifecycle.LifecycleOwner);
    public void onResume(androidx.lifecycle.LifecycleOwner);
    public void onPause(androidx.lifecycle.LifecycleOwner);
    public void onStop(androidx.lifecycle.LifecycleOwner);
    public void onDestroy(androidx.lifecycle.LifecycleOwner);
}
-keepattributes InnerClasses, Signature, EnclosingMethod, *Annotation*, MethodParameters
-dontwarn org.sral.**
