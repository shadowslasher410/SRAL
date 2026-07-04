-keep class org.sral.AndroidAccessibilityManagerHelper {
    public <init>(android.content.Context);
    public boolean isActive();
    public void announce(java.lang.String, boolean);
    public void stop();
    public void shutdown();
}

-keep class org.sral.AndroidTTSHelper {
    public <init>(android.content.Context);
    public boolean isActive();
    public boolean isSpeaking();
    public void speak(java.lang.String, boolean);
    public void stop();
    public void setSpeechRate(float);
    public void setVolume(float);
    public float getRate();
    public float getVolume();
    public void shutdown();
}

-keep class org.sral.** {
    <init>(...);
    public *;
}

-dontwarn org.sral.**
