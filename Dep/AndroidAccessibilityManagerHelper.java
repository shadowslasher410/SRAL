package org.sral;

import android.content.Context;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.text.SpannableString;
import android.util.Log;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityManager;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.lifecycle.DefaultLifecycleObserver;
import androidx.lifecycle.LifecycleOwner;
import java.lang.ref.WeakReference;
import java.util.Objects;
import java.util.concurrent.ConcurrentLinkedQueue;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.RejectedExecutionException;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;

public final class AndroidAccessibilityManagerHelper implements DefaultLifecycleObserver {
  private static final String TAG = "AccessibilityHelper";
  private static final String CLASS_NAME = AndroidAccessibilityManagerHelper.class.getName();

  private static final int BUFFER_CAPACITY = 50;
  private static final long FRAMEWORK_FLUSH_DELAY_MS = 100;

  private static final int MSG_EXECUTE_ANNOUNCEMENT = 0xAF01;
  private static final int MSG_COMPLETE_ANNOUNCEMENT = 0xAF02;

  private final Context appContext;
  private final AccessibilityManager am;
  private final Handler mainHandler;

  private final ConcurrentLinkedQueue<String> announcementQueue = new ConcurrentLinkedQueue<>();
  private final AtomicInteger queueSize = new AtomicInteger(0);

  private final ExecutorService virtualExecutor = Executors.newVirtualThreadPerTaskExecutor();

  private final AtomicBoolean isRunning = new AtomicBoolean(false);
  private final AtomicBoolean isProcessing = new AtomicBoolean(false);

  public AndroidAccessibilityManagerHelper(
      @NonNull Context context, @Nullable final LifecycleOwner lifecycleOwner) {
    Objects.requireNonNull(context, "Context cannot be null");
    this.appContext = context.getApplicationContext();
    this.am = (AccessibilityManager) appContext.getSystemService(Context.ACCESSIBILITY_SERVICE);
    this.mainHandler = new Handler(Looper.getMainLooper(), new SafeHandlerCallback(this));

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

  public boolean isActive() {
    return am != null && am.isEnabled();
  }

  public void announce(@Nullable String text, final boolean interrupt) {
    if (text == null || !isActive() || !isRunning.get() || virtualExecutor.isShutdown()) {
      return;
    }

    String processedText = text.strip();
    if (processedText.isEmpty())
      return;

    final AccessibilityManager targetAm = this.am;

    try {
      if (interrupt) {
        announcementQueue.clear();
        queueSize.set(0);
        mainHandler.removeMessages(MSG_EXECUTE_ANNOUNCEMENT);
        mainHandler.removeMessages(MSG_COMPLETE_ANNOUNCEMENT);
        isProcessing.set(false);
        virtualExecutor.submit(() -> {
          try {
            if (targetAm != null && targetAm.isEnabled()) {
              targetAm.interrupt();
            }
          } catch (Exception e) {
            Log.e(TAG, "Failed to broadcast interrupt command to accessibility manager", e);
          }
        });

        Message msg = mainHandler.obtainMessage(MSG_EXECUTE_ANNOUNCEMENT, processedText);
        mainHandler.sendMessageDelayed(msg, FRAMEWORK_FLUSH_DELAY_MS);
        isProcessing.set(true);
        queueSize.incrementAndGet();
        return;
      }

      int previousSize =
          queueSize.getAndUpdate(current -> current < BUFFER_CAPACITY ? current + 1 : current);
      if (previousSize < BUFFER_CAPACITY) {
        announcementQueue.add(processedText);
        tryScheduleNextAnnouncement();
      } else {
        Log.w(TAG, "Ring buffer is full. Dropping announcement.");
      }
    } catch (RejectedExecutionException e) {
      Log.w(TAG, "Submission rejected due to concurrent instance shutdown handling phases");
      isProcessing.set(false);
    }
  }

  private void tryScheduleNextAnnouncement() {
    if (!isRunning.get() || announcementQueue.isEmpty() || isProcessing.get()
        || virtualExecutor.isShutdown()) {
      return;
    }

    try {
      virtualExecutor.submit(this::dispatchNextAnnouncement);
    } catch (RejectedExecutionException e) {
      isProcessing.set(false);
    }
  }

  private void dispatchNextAnnouncement() {
    if (!isProcessing.compareAndSet(false, true))
      return;

    String targetText = announcementQueue.poll();
    if (targetText == null) {
      isProcessing.set(false);
      return;
    }
    queueSize.decrementAndGet();

    Message msg = mainHandler.obtainMessage(MSG_EXECUTE_ANNOUNCEMENT, targetText);
    mainHandler.sendMessage(msg);
  }

  private void executeAnnounceEvent(@NonNull String text) {
    if (!isActive() || !isRunning.get()) {
      handleAnnouncementCompletion();
      return;
    }

    AccessibilityEvent event = null;
    try {
      if (Build.VERSION.SDK_INT >= 30) {
        event = new AccessibilityEvent(AccessibilityEvent.TYPE_ANNOUNCEMENT);
        event.setContentChangeTypes(AccessibilityEvent.CONTENT_CHANGE_TYPE_ANNOUNCEMENT);
      } else {
        event = AccessibilityEvent.obtain();
        event.setEventType(AccessibilityEvent.TYPE_ANNOUNCEMENT);
      }

      event.setPackageName(appContext.getPackageName());
      event.setClassName(CLASS_NAME);
      event.getText().add(new SpannableString(text));

      am.sendAccessibilityEvent(event);
    } catch (Exception e) {
      Log.e(TAG, "Error sending accessibility event to framework", e);
      if (event != null && Build.VERSION.SDK_INT < 30) {
        try {
          event.recycle();
        } catch (Exception ignored) {
        }
      }
    }

    long delay = calculateReadingDelayAllocationFree(text);
    Message completionMsg = mainHandler.obtainMessage(MSG_COMPLETE_ANNOUNCEMENT);
    mainHandler.sendMessageDelayed(completionMsg, delay);
  }

  private void handleAnnouncementCompletion() {
    isProcessing.set(false);
    tryScheduleNextAnnouncement();
  }

  public void startWorker() {
    if (isRunning.compareAndSet(false, true)) {
      tryScheduleNextAnnouncement();
    }
  }

  public void stopWorker() {
    isRunning.set(false);
    announcementQueue.clear();
    queueSize.set(0);
    mainHandler.removeMessages(MSG_EXECUTE_ANNOUNCEMENT);
    mainHandler.removeMessages(MSG_COMPLETE_ANNOUNCEMENT);
    isProcessing.set(false);
  }

  public void interrupt() {
    announce("", true);
  }

  public void shutdown() {
    stopWorker();
    virtualExecutor.shutdownNow();
  }

  private static long calculateReadingDelayAllocationFree(@NonNull String text) {
    int len = text.length();
    if (len == 0)
      return 0L;

    int wordCount = 0;
    boolean inWord = false;

    for (int i = 0; i < len; i++) {
      if (Character.isWhitespace(text.charAt(i))) {
        if (inWord) {
          wordCount++;
          inWord = false;
        }
      } else {
        inWord = true;
      }
    }
    if (inWord)
      wordCount++;

    long estimatedTime = wordCount * 460L + 350L;
    return Math.clamp(estimatedTime, 800L, 8500L);
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
    shutdown();
    try {
      owner.getLifecycle().removeObserver(this);
    } catch (Exception e) {
      Log.e(TAG, "Failed to remove lifecycle observer during destruction", e);
    }
  }

  private static final class SafeHandlerCallback implements Handler.Callback {
    private final WeakReference<AndroidAccessibilityManagerHelper> helperRef;

    SafeHandlerCallback(AndroidAccessibilityManagerHelper helper) {
      this.helperRef = new WeakReference<>(helper);
    }

    @Override
    public boolean handleMessage(@NonNull Message msg) {
      AndroidAccessibilityManagerHelper helper = helperRef.get();
      if (helper == null)
        return false;

      switch (msg.what) {
        case MSG_EXECUTE_ANNOUNCEMENT:
          if (msg.obj instanceof String) {
            helper.executeAnnounceEvent((String) msg.obj);
            return true;
          }
          break;
        case MSG_COMPLETE_ANNOUNCEMENT:
          helper.handleAnnouncementCompletion();
          return true;
      }
      return false;
    }
  }
}