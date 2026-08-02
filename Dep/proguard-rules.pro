-keepattributes InnerClasses, EnclosingMethod, Signature, *Annotation*

-dontwarn org.sral.**

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

-assumenosideeffects class android.util.Log {
    public static boolean isLoggable(java.lang.String, int);
    public static int v(...);
    public static int d(...);
}