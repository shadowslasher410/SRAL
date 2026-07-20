package org.sral;

import android.content.Context;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.util.Log;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityManager;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.lifecycle.DefaultLifecycleObserver;
import androidx.lifecycle.LifecycleOwner;
import java.lang.ref.WeakReference;
import java.util.Arrays;
import java.util.Objects;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicReference;

public final class AndroidAccessibilityManagerHelper implements DefaultLifecycleObserver {
  private static final String TAG = "AccessibilityHelper";
  private static final int MILLISECONDS_PER_WORD = 400;
  private static final int MIN_PADDING_MS = 1200;
  private static final int MAX_PADDING_MS = 8000;
  private static final int MAX_TEXT_LENGTH = 200;
  private static final int BUFFER_CAPACITY = 50;

  private static final long FRAMEWORK_FLUSH_DELAY_MS = 100;
  private static final int MSG_EXECUTE_ANNOUNCEMENT = 0xAF01;
  private static final String TRUNCATION_SUFFIX = "...";

  private final Context appContext;
  private final AccessibilityManager am;
  private final Handler mainHandler;

  private final String[] ringBuffer = new String[BUFFER_CAPACITY];
  private int head = 0;
  private int tail = 0;
  private int size = 0;

  private final Object bufferLock = new Object();

  private final AtomicBoolean isRunning = new AtomicBoolean(false);
  private final AtomicBoolean frameworkJustInterrupted = new AtomicBoolean(false);
  private final AtomicReference<Thread> workerThread = new AtomicReference<>(null);

  public AndroidAccessibilityManagerHelper(
      @NonNull Context context, @Nullable final LifecycleOwner lifecycleOwner) {
    Objects.requireNonNull(context, "Context cannot be null");
    this.appContext = context.getApplicationContext();
    this.am = (AccessibilityManager) appContext.getSystemService(Context.ACCESSIBILITY_SERVICE);

    this.mainHandler = new Handler(Looper.getMainLooper(), msg -> {
      if (msg.what == MSG_EXECUTE_ANNOUNCEMENT) {
        if (msg.obj instanceof String) {
          executeAnnounceEvent((String) msg.obj);
        }
        return true;
      }
      return false;
    });

    startWorker();

    if (lifecycleOwner != null) {
      if (Looper.myLooper() == Looper.getMainLooper()) {
        attachLifecycle(lifecycleOwner);
      } else {
        final WeakReference<LifecycleOwner> weakOwner = new WeakReference<>(lifecycleOwner);
        mainHandler.post(() -> {
          LifecycleOwner owner = weakOwner.get();
          if (owner != null) {
            attachLifecycle(owner);
          }
        });
      }
    }
  }

  public AndroidAccessibilityManagerHelper(@NonNull Context context) {
    this(context, null);
  }

  private void attachLifecycle(@NonNull LifecycleOwner lifecycleOwner) {
    try {
      lifecycleOwner.getLifecycle().addObserver(this);
    } catch (Exception e) {
      Log.e(TAG, "Failed to add lifecycle observer context", e);
    }
  }

  @Override
  public void onResume(@NonNull LifecycleOwner owner) {
    startWorker();
  }

  @Override
  public void onPause(@NonNull LifecycleOwner owner) {
    stopWorker();
  }

  @Override
  public void onDestroy(@NonNull LifecycleOwner owner) {
    shutdownHelper();
    try {
      owner.getLifecycle().removeObserver(this);
    } catch (Exception e) {
      Log.e(TAG, "Failed to remove lifecycle observer during destruction", e);
    }
  }

  public boolean isActive() {
    return am != null && am.isEnabled();
  }

  public void announce(final String text, final boolean interrupt) {
    if (text == null || text.isBlank() || !isActive() || !isRunning.get()) {
      return;
    }

    String processedText = text.trim();
    if (processedText.length() > MAX_TEXT_LENGTH) {
      processedText = processedText.substring(0, MAX_TEXT_LENGTH).concat(TRUNCATION_SUFFIX);
    }

    Thread threadToInterrupt = null;

    synchronized (bufferLock) {
      if (!isRunning.get()) {
        return;
      }

      if (interrupt) {
        clearQueueAndFrameworkInterrupt();

        head = tail;
        size = 0;
        Arrays.fill(ringBuffer, null);

        frameworkJustInterrupted.set(true);
        threadToInterrupt = workerThread.get();
      }

      if (size < BUFFER_CAPACITY) {
        ringBuffer[tail] = processedText;
        tail = (tail + 1) % BUFFER_CAPACITY;
        size++;
        bufferLock.notifyAll();
      } else {
        Log.w(TAG, "Ring buffer is full. Dropping announcement.");
      }
    }

    if (threadToInterrupt != null) {
      threadToInterrupt.interrupt();
    }
  }

  public void startWorker() {
    synchronized (bufferLock) {
      if (isRunning.get()) {
        Thread currentWorker = workerThread.get();
        if (currentWorker != null && currentWorker.isAlive()) {
          return;
        }
      }

      isRunning.set(true);

      Thread newWorker = Thread.ofVirtual()
                             .name("AccessibilityAnnouncementWorker")
                             .unstarted(new AnnouncementWorkerRunnable());

      workerThread.set(newWorker);
      newWorker.start();
    }
  }

  public void stopWorker() {
    synchronized (bufferLock) {
      isRunning.set(false);
      clearQueueAndFrameworkInterrupt();
      head = 0;
      tail = 0;
      size = 0;
      Arrays.fill(ringBuffer, null);
      bufferLock.notifyAll();
    }

    Thread threadToInterrupt = workerThread.getAndSet(null);
    if (threadToInterrupt != null) {
      threadToInterrupt.interrupt();
    }
  }

  private void shutdownHelper() {
    stopWorker();
    mainHandler.removeMessages(MSG_EXECUTE_ANNOUNCEMENT);
  }

  private void clearQueueAndFrameworkInterrupt() {
    mainHandler.removeMessages(MSG_EXECUTE_ANNOUNCEMENT);
    if (am != null && am.isEnabled()) {
      try {
        am.interrupt();
      } catch (Exception e) {
        Log.e(TAG, "Error trying to interrupt active accessibility speech loop", e);
      }
    }
  }

  private long calculateReadingDelay(String text) {
    String[] words = text.split("\\s+");
    long estimatedTime = (long) words.length * MILLISECONDS_PER_WORD;
    return Math.min(Math.max(estimatedTime, MIN_PADDING_MS), MAX_PADDING_MS);
  }

  private final class AnnouncementWorkerRunnable implements Runnable {
    @Override
    public void run() {
      Thread.interrupted();

      while (isRunning.get()) {
        String targetText = null;
        boolean shouldDelayFlush = false;
        boolean interruptedInsideWaitLoop = false;

        synchronized (bufferLock) {
          while (size == 0 && isRunning.get()) {
            try {
              bufferLock.wait();
            } catch (InterruptedException e) {
              Thread.interrupted();
              interruptedInsideWaitLoop = true;
              break;
            }
          }

          if (!isRunning.get()) {
            break;
          }

          if (!interruptedInsideWaitLoop) {
            if (frameworkJustInterrupted.compareAndSet(true, false)) {
              shouldDelayFlush = true;
            }

            if (size > 0) {
              targetText = ringBuffer[head];
              ringBuffer[head] = null;
              head = (head + 1) % BUFFER_CAPACITY;
              size--;
            }
          }
        }

        if (interruptedInsideWaitLoop) {
          continue;
        }

        if (shouldDelayFlush) {
          try {
            Thread.sleep(FRAMEWORK_FLUSH_DELAY_MS);
          } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
            continue;
          }
        }

        if (targetText != null && isActive()) {
          Message msg = mainHandler.obtainMessage(MSG_EXECUTE_ANNOUNCEMENT, targetText);
          mainHandler.sendMessage(msg);

          long delay = calculateReadingDelay(targetText);
          try {
            Thread.sleep(delay);
          } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
          }
        }
      }

      workerThread.compareAndSet(Thread.currentThread(), null);
    }
  }

  private void executeAnnounceEvent(String text) {
    if (!isActive())
      return;

    AccessibilityEvent event = createAnnouncementEvent();
    if (event == null)
      return;

    event.setPackageName(appContext.getPackageName());
    event.setClassName(AndroidAccessibilityManagerHelper.class.getName());
    event.getText().add(text);

    if (Build.VERSION.SDK_INT >= 30) {
      event.setContentChangeTypes(AccessibilityEvent.CONTENT_CHANGE_TYPE_ANNOUNCEMENT);
    }

    try {
      am.sendAccessibilityEvent(event);
    } catch (Exception e) {
      Log.e(TAG, "Error sending accessibility event to framework", e);
      if (Build.VERSION.SDK_INT < 30) {
        try {
          event.recycle();
        } catch (Exception ignored) {
        }
      }
    }
  }

  @SuppressWarnings("deprecation")
  private AccessibilityEvent createAnnouncementEvent() {
    if (Build.VERSION.SDK_INT >= 30) {
      return new AccessibilityEvent(AccessibilityEvent.TYPE_ANNOUNCEMENT);
    } else {
      try {
        AccessibilityEvent event = AccessibilityEvent.obtain();
        if (event != null) {
          event.setEventType(AccessibilityEvent.TYPE_ANNOUNCEMENT);
        }
        return event;
      } catch (Exception e) {
        Log.e(TAG, "Failed to instantiate legacy AccessibilityEvent via pool", e);
        return null;
      }
    }
  }
}
