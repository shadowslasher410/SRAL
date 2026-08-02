package org.sral;

import android.content.Context;
import android.os.Bundle;
import android.speech.tts.TextToSpeech;
import android.speech.tts.UtteranceProgressListener;
import android.util.Log;
import androidx.annotation.Keep;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import java.lang.ref.WeakReference;
import java.util.Locale;
import java.util.Objects;
import java.util.concurrent.ConcurrentLinkedQueue;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.locks.ReentrantLock;

public final class AndroidTTSHelper {
  private static final String TAG = "SRAL_TTSHelper";
  private static final String UTTERANCE_ID = "sral_utterance";

  private final ReentrantLock lock = new ReentrantLock();
  private final ConcurrentLinkedQueue<String> speechQueue = new ConcurrentLinkedQueue<>();
  private final Bundle paramsCache = new Bundle();
  private final AtomicBoolean isSpeaking = new AtomicBoolean(false);

  private volatile TextToSpeech tts;
  private volatile boolean ready = false;
  private volatile boolean isRunning = false;
  private volatile float currentRate = 1.0f;
  private volatile float currentVolume = 1.0f;

  public AndroidTTSHelper(@NonNull Context context) {
    Context appContext =
        Objects.requireNonNull(context, "Context cannot be null").getApplicationContext();
    this.tts = new TextToSpeech(appContext, new SafeOnInitListener(this));
  }

  public boolean isActive() {
    return ready && tts != null;
  }

  public boolean isSpeaking() {
    return isSpeaking.get();
  }

  public float getRate() {
    return currentRate;
  }

  public float getVolume() {
    return currentVolume;
  }

  public void setSpeechRate(float rate) {
    this.currentRate = rate;
    if (ready) {
      lock.lock();
      try {
        TextToSpeech localTts = tts;
        if (localTts != null) {
          localTts.setSpeechRate(rate);
        }
      } catch (Exception e) {
        Log.e(TAG, "Failed to apply runtime speech rate updates", e);
      } finally {
        lock.unlock();
      }
    }
  }

  public void setVolume(float volume) {
    this.currentVolume = volume < 0.0f ? 0.0f : (volume > 1.0f ? 1.0f : volume);
  }

  public void startWorker() {
    lock.lock();
    try {
      isRunning = true;
      tryTriggerNextUtterance();
    } finally {
      lock.unlock();
    }
  }

  public void speak(@Nullable String text, boolean interrupt) {
    if (text == null)
      return;

    String cleanText = text.strip();
    if (cleanText.isEmpty())
      return;

    lock.lock();
    try {
      if (interrupt) {
        speechQueue.clear();
        TextToSpeech localTts = tts;
        if (localTts != null && ready) {
          try {
            localTts.stop();
          } catch (Exception e) {
            Log.e(TAG, "Failed to interrupt active playback safely", e);
          }
        }
        isSpeaking.set(false);
      }

      speechQueue.add(cleanText);
      tryTriggerNextUtterance();
    } finally {
      lock.unlock();
    }
  }

  private void tryTriggerNextUtterance() {
    if (!isRunning || !ready || isSpeaking.get() || speechQueue.isEmpty()) {
      return;
    }

    Thread.ofVirtual().name("TTS-Dispatcher").start(this::dispatchNextUtterance);
  }

  private void dispatchNextUtterance() {
    if (!isSpeaking.compareAndSet(false, true)) {
      return;
    }

    String targetText = speechQueue.poll();
    if (targetText == null) {
      isSpeaking.set(false);
      return;
    }

    executeNativeSpeak(targetText);
  }

  private void executeNativeSpeak(@NonNull String text) {
    TextToSpeech localTts = tts;
    if (localTts == null || !ready || !isRunning) {
      resetSpeakingState();
      return;
    }

    paramsCache.putFloat(TextToSpeech.Engine.KEY_PARAM_VOLUME, currentVolume);

    boolean submissionSuccess = false;
    try {
      int result = localTts.speak(text, TextToSpeech.QUEUE_ADD, paramsCache, UTTERANCE_ID);
      submissionSuccess = (result == TextToSpeech.SUCCESS);

      if (!submissionSuccess) {
        Log.e(TAG, "Native speech submission rejected by engine. Code: " + result);
      }
    } catch (Throwable t) {
      Log.e(TAG, "Fatal execution failure during speech invocation", t);
      try {
        localTts.stop();
      } catch (Exception ignored) {
      }
    }

    if (!submissionSuccess) {
      resetSpeakingState();
    }
  }

  public void resetSpeakingState() {
    isSpeaking.set(false);
    tryTriggerNextUtterance();
  }

  public void shutdown() {
    TextToSpeech localTts;

    lock.lock();
    try {
      isRunning = false;
      ready = false;
      speechQueue.clear();
      isSpeaking.set(false);

      localTts = tts;
      tts = null;
    } finally {
      lock.unlock();
    }

    if (localTts != null) {
      Thread.ofVirtual().name("TTS-Shutdown").start(() -> {
        try {
          localTts.setOnUtteranceProgressListener(null);
          localTts.stop();
          localTts.shutdown();
        } catch (Exception e) {
          Log.e(TAG, "Error encountered during engine lifecycle shutdown pass", e);
        }
      });
    }
  }

  private static final class SafeOnInitListener implements TextToSpeech.OnInitListener {
    private final WeakReference<AndroidTTSHelper> helperRef;

    SafeOnInitListener(AndroidTTSHelper helper) {
      this.helperRef = new WeakReference<>(helper);
    }

    @Override
    public void onInit(int status) {
      AndroidTTSHelper helper = helperRef.get();
      if (helper == null)
        return;

      helper.lock.lock();
      try {
        TextToSpeech actualTts = helper.tts;
        if (status != TextToSpeech.SUCCESS || actualTts == null) {
          helper.ready = false;
          return;
        }

        Locale targetLocale = Locale.getDefault();
        int langResult = actualTts.setLanguage(targetLocale);
        if (langResult == TextToSpeech.LANG_NOT_SUPPORTED
            || langResult == TextToSpeech.LANG_MISSING_DATA) {
          actualTts.setLanguage(Locale.US);
        }

        actualTts.setOnUtteranceProgressListener(new SafeUtteranceListener(helper));
        actualTts.setSpeechRate(helper.currentRate);

        helper.ready = true;
        helper.startWorker();
      } catch (Throwable t) {
        Log.e(TAG, "Initialization failure within engine configuration gate", t);
        helper.ready = false;
      } finally {
        helper.lock.unlock();
      }
    }
  }

  @Keep
  private static final class SafeUtteranceListener extends UtteranceProgressListener {
    private final WeakReference<AndroidTTSHelper> helperRef;

    SafeUtteranceListener(AndroidTTSHelper helper) {
      this.helperRef = new WeakReference<>(helper);
    }

    @Override
    public void onStart(String utteranceId) {}

    @Override
    public void onDone(String utteranceId) {
      AndroidTTSHelper helper = helperRef.get();
      if (helper != null && UTTERANCE_ID.equals(utteranceId)) {
        helper.resetSpeakingState();
      }
    }

    @Override
    @Deprecated
    public void onError(String utteranceId) {
      AndroidTTSHelper helper = helperRef.get();
      if (helper != null && UTTERANCE_ID.equals(utteranceId)) {
        helper.resetSpeakingState();
      }
    }

    @Override
    public void onError(String utteranceId, int errorCode) {
      AndroidTTSHelper helper = helperRef.get();
      if (helper != null && UTTERANCE_ID.equals(utteranceId)) {
        helper.resetSpeakingState();
      }
    }
  }
}