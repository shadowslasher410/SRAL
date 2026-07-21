package org.sral;

import android.content.Context;
import android.os.Bundle;
import android.speech.tts.TextToSpeech;
import android.speech.tts.UtteranceProgressListener;
import android.util.Log;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import java.lang.ref.WeakReference;
import java.util.Locale;
import java.util.Objects;
import java.util.concurrent.locks.Condition;
import java.util.concurrent.locks.ReentrantLock;

public class AndroidTTSHelper {
  private static final String TAG = "SRAL_TTSHelper";
  private static final String UTTERANCE_ID = "sral_utterance";
  private static final int INITIAL_CAPACITY = 8;

  private final ReentrantLock lock;
  private final Condition loopCondition;
  private final Condition speechCompletionCondition;

  private volatile TextToSpeech tts;
  private volatile boolean ready = false;
  private volatile boolean speaking = false;
  private volatile boolean isRunning = false;
  private volatile float currentRate = 1.0f;
  private volatile float currentVolume = 1.0f;

  private String[] ringBuffer;
  private int capacity;
  private int head = 0;
  private int tail = 0;
  private int size = 0;
  private Thread workerThread;

  public AndroidTTSHelper(@NonNull Context context) {
    if (context == null) {
      throw new NullPointerException("Context cannot be null");
    }
    Context appContext = context.getApplicationContext();

    this.capacity = INITIAL_CAPACITY;
    this.ringBuffer = new String[capacity];
    this.lock = new ReentrantLock();
    this.loopCondition = lock.newCondition();
    this.speechCompletionCondition = lock.newCondition();

    this.tts = new TextToSpeech(appContext, new SafeOnInitListener(this));
  }

  public boolean isActive() {
    lock.lock();
    try {
      return ready && tts != null;
    } finally {
      lock.unlock();
    }
  }

  public boolean isSpeaking() {
    lock.lock();
    try {
      return speaking;
    } finally {
      lock.unlock();
    }
  }

  public float getRate() {
    return currentRate;
  }

  public float getVolume() {
    return currentVolume;
  }

  public void setSpeechRate(float rate) {
    lock.lock();
    try {
      currentRate = rate;
      TextToSpeech localTts = tts;
      if (localTts != null && ready) {
        try {
          localTts.setSpeechRate(rate);
        } catch (Exception e) {
          Log.e(TAG, "Failed to apply runtime speech rate updates", e);
        }
      }
    } finally {
      lock.unlock();
    }
  }

  public void setVolume(float volume) {
    currentVolume = Math.max(0.0f, Math.min(volume, 1.0f));
  }

  public void speak(@Nullable String text, boolean interrupt) {
    if (text == null || text.trim().isEmpty()) {
      return;
    }

    lock.lock();
    try {
      if (interrupt) {
        java.util.Arrays.fill(ringBuffer, null);
        head = 0;
        tail = 0;
        size = 0;

        TextToSpeech localTts = tts;
        if (localTts != null && ready) {
          try {
            localTts.stop();
          } catch (Exception e) {
            Log.e(TAG, "Failed to interrupt active playback safely", e);
          }
        }

        speaking = false;
        speechCompletionCondition.signalAll();
      }

      if (size == capacity) {
        resizeBuffer();
      }

      ringBuffer[tail] = text.trim();
      tail = (tail + 1) % capacity;
      size++;
      loopCondition.signal();
    } finally {
      lock.unlock();
    }
  }

  private void resizeBuffer() {
    assert lock.isHeldByCurrentThread() : "Lock must be held when resizing the buffer";

    int newCapacity = capacity * 2;
    String[] newBuffer = new String[newCapacity];

    if (head == 0) {
      System.arraycopy(ringBuffer, 0, newBuffer, 0, capacity);
    } else {
      int itemsToEnd = capacity - head;
      System.arraycopy(ringBuffer, head, newBuffer, 0, itemsToEnd);
      System.arraycopy(ringBuffer, 0, newBuffer, itemsToEnd, head);
    }

    this.ringBuffer = newBuffer;
    this.head = 0;
    this.tail = capacity;
    this.capacity = newCapacity;

    Log.d(TAG,
        String.format(Locale.US, "Dynamic ring buffer expanded. New capacity: %d", newCapacity));
  }

  public void startWorker() {
    lock.lock();
    try {
      if (workerThread != null && workerThread.isAlive()) {
        return;
      }
      isRunning = true;
      workerThread = new Thread(new TTSWorkerRunnable(), "TTSBackgroundWorker");
      workerThread.setDaemon(true);
      workerThread.start();
    } finally {
      lock.unlock();
    }
  }

  private final class TTSWorkerRunnable implements Runnable {
    @Override
    public void run() {
      while (isRunning) {
        String targetText = null;

        lock.lock();
        try {
          while (size == 0 && isRunning) {
            loopCondition.await();
          }

          while (speaking && isRunning) {
            speechCompletionCondition.await();
          }

          if (!isRunning) {
            break;
          }

          targetText = ringBuffer[head];
          ringBuffer[head] = null;
          head = (head + 1) % capacity;
          size--;

          if (targetText != null && ready && tts != null) {
            speaking = true;
          } else {
            targetText = null;
          }

        } catch (InterruptedException e) {
          Thread.currentThread().interrupt();
          break;
        } finally {
          lock.unlock();
        }

        if (targetText != null) {
          executeNativeSpeak(targetText);
        }
      }
    }
  }

  private void executeNativeSpeak(@NonNull String text) {
    TextToSpeech localTts = tts;
    lock.lock();
    boolean isReadySnapshot = ready;
    lock.unlock();

    if (localTts == null || !isReadySnapshot) {
      resetSpeakingState();
      return;
    }

    Bundle params = new Bundle();
    if (currentVolume != 1.0f) {
      params.putFloat(TextToSpeech.Engine.KEY_PARAM_VOLUME, currentVolume);
    }

    boolean submissionSuccess = false;
    try {
      int result = localTts.speak(text, TextToSpeech.QUEUE_ADD, params, UTTERANCE_ID);
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

  private void resetSpeakingState() {
    lock.lock();
    try {
      if (isRunning) {
        speaking = false;
        speechCompletionCondition.signalAll();
      }
    } finally {
      lock.unlock();
    }
  }

  public void stop() {
    TextToSpeech localTts;
    boolean isReadySnapshot;

    lock.lock();
    try {
      java.util.Arrays.fill(ringBuffer, null);
      head = 0;
      tail = 0;
      size = 0;
      speaking = false;

      speechCompletionCondition.signalAll();
      loopCondition.signalAll();

      localTts = tts;
      isReadySnapshot = ready;
    } finally {
      lock.unlock();
    }

    if (localTts != null && isReadySnapshot) {
      try {
        localTts.stop();
      } catch (Exception e) {
        Log.e(TAG, "Failed to stop engine hardware tracks safely", e);
      }
    }
  }

  public void shutdown() {
    Thread threadToInterrupt;
    TextToSpeech localTts;

    lock.lock();
    try {
      isRunning = false;
      ready = false;
      java.util.Arrays.fill(ringBuffer, null);
      head = 0;
      tail = 0;
      size = 0;
      speaking = false;

      loopCondition.signalAll();
      speechCompletionCondition.signalAll();

      localTts = tts;
      tts = null;

      threadToInterrupt = workerThread;
      workerThread = null;
    } finally {
      lock.unlock();
    }

    if (threadToInterrupt != null) {
      try {
        threadToInterrupt.interrupt();
      } catch (Exception e) {
        Log.e(TAG, "Failed to terminate worker thread cleanly", e);
      }
    }

    if (localTts != null) {
      try {
        localTts.setOnUtteranceProgressListener(null);
        localTts.stop();
        localTts.shutdown();
      } catch (Exception e) {
        Log.e(TAG, "Error encountered during engine lifecycle shutdown pass", e);
      }
    }
  }

  private static final class SafeUtteranceListener extends UtteranceProgressListener {
    private final WeakReference<AndroidTTSHelper> helperRef;

    SafeUtteranceListener(AndroidTTSHelper helper) {
      this.helperRef = new WeakReference<>(helper);
    }

    @Override
    public void onStart(String utteranceId) {}

    @Override
    public void onDone(String utteranceId) {
      clearSpeakingFlag();
    }

    @Override
    @Deprecated
    public void onError(String utteranceId) {
      clearSpeakingFlag();
    }

    @Override
    public void onError(String utteranceId, int errorCode) {
      clearSpeakingFlag();
    }

    private void clearSpeakingFlag() {
      AndroidTTSHelper helper = helperRef.get();
      if (helper == null)
        return;

      if (!helper.isRunning)
        return;

      helper.lock.lock();
      try {
        if (helper.isRunning) {
          helper.speaking = false;
          helper.speechCompletionCondition.signalAll();
        }
      } finally {
        helper.lock.unlock();
      }
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

      if (status == TextToSpeech.SUCCESS) {
        helper.lock.lock();
        try {
          TextToSpeech actualTts = helper.tts;
          if (actualTts == null)
            return;

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
          helper.ready = false;
        } finally {
          helper.lock.unlock();
        }
      } else {
        helper.ready = false;
      }
    }
  }
}