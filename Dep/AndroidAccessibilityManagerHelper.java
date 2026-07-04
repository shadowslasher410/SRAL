package org.sral;

import android.content.Context;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.util.Log;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityManager;
import androidx.annotation.NonNull;
import androidx.lifecycle.DefaultLifecycleObserver;
import androidx.lifecycle.LifecycleOwner;
import java.util.LinkedList;
import java.util.Objects;
import java.util.Queue;

public final class AndroidAccessibilityManagerHelper implements DefaultLifecycleObserver {
    private static final String TAG = "AccessibilityHelper";
    private static final int MILLISECONDS_PER_WORD = 400;
    private static final int MIN_PADDING_MS = 1200;
    private static final Object ANNOUNCEMENT_TOKEN = new Object();
    private final Context appContext;
    private final AccessibilityManager am;
    private final Handler mainHandler;
    private final Queue<String> announcementQueue;
    private final Runnable processNextAnnouncementRunnable;
    private boolean isProcessingQueue;

    public AndroidAccessibilityManagerHelper(@NonNull Context context, LifecycleOwner lifecycleOwner) {
        Objects.requireNonNull(context, "Context cannot be null");
        this.appContext = context.getApplicationContext();
        this.am = (AccessibilityManager) appContext.getSystemService(Context.ACCESSIBILITY_SERVICE);
        this.mainHandler = new Handler(Looper.getMainLooper());
        this.announcementQueue = new LinkedList<>();
        this.processNextAnnouncementRunnable = this::processNextAnnouncement;
        this.isProcessingQueue = false;

        if (lifecycleOwner != null) {
            if (Looper.myLooper() == Looper.getMainLooper()) {
                try {
                    lifecycleOwner.getLifecycle().addObserver(this);
                } catch (Exception e) {
                    Log.e(TAG, "Failed to add lifecycle observer on main thread", e);
                }
            } else {
                mainHandler.post(() -> {
                    try {
                        lifecycleOwner.getLifecycle().addObserver(this);
                    } catch (Exception e) {
                        Log.e(TAG, "Failed to add lifecycle observer via handler post", e);
                    }
                });
            }
        }
    }

    public AndroidAccessibilityManagerHelper(@NonNull Context context) {
        this(context, null);
    }

    @Override
    public void onPause(@NonNull LifecycleOwner owner) {
        stop();
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

    public boolean isActive() {
        return am != null && am.isEnabled();
    }

    public void announce(final String text, final boolean interrupt) {
        if (text == null || text.isBlank() || !isActive()) {
            return;
        }

        if (Looper.myLooper() == Looper.getMainLooper()) {
            enqueueAnnouncement(text, interrupt);
        } else {
            mainHandler.post(() -> enqueueAnnouncement(text, interrupt));
        }
    }

    private void enqueueAnnouncement(String text, boolean interrupt) {
        if (interrupt) {
            clearQueueAndStop();
        }

        announcementQueue.add(text.trim());
        if (!isProcessingQueue) {
            isProcessingQueue = true;
            processNextAnnouncement();
        }
    }

    private void processNextAnnouncement() {
        if (!isActive()) {
            isProcessingQueue = false;
            return;
        }

        String nextText = null;
        while (!announcementQueue.isEmpty()) {
            String polled = announcementQueue.poll();
            if (polled != null && !polled.isBlank()) {
                nextText = polled;
                break;
            }
        }

        if (nextText == null) {
            isProcessingQueue = false;
            return;
        }

        executeAnnounceEvent(nextText);
        long delay = calculateReadingDelay(nextText);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            mainHandler.postDelayed(processNextAnnouncementRunnable, ANNOUNCEMENT_TOKEN, delay);
        } else {
            mainHandler.postAtTime(processNextAnnouncementRunnable, ANNOUNCEMENT_TOKEN, SystemClock.uptimeMillis() + delay);
        }
    }

    private void executeAnnounceEvent(String text) {
        AccessibilityEvent event = createAnnouncementEvent();
        if (event == null) {
            return;
        }

        event.setPackageName(appContext.getPackageName());
        event.setClassName(AndroidAccessibilityManagerHelper.class.getName());
        event.getText().add(text);
        
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT) {
            event.setContentChangeTypes(AccessibilityEvent.CONTENT_CHANGE_TYPE_ANNOUNCEMENT);
        }

        try {
            am.sendAccessibilityEvent(event);
        } catch (Exception e) {
            Log.e(TAG, "Error sending accessibility event to framework", e);
        }
    }

    @SuppressWarnings("deprecation")
    private AccessibilityEvent createAnnouncementEvent() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            return new AccessibilityEvent(AccessibilityEvent.TYPE_ANNOUNCEMENT);
        } else {
            try {
                AccessibilityEvent event = AccessibilityEvent.obtain();
                if (event != null) {
                    event.setEventType(AccessibilityEvent.TYPE_ANNOUNCEMENT);
                }
                return event;
            } catch (Exception e) {
                Log.e(TAG, "Failed to instantiate deprecated legacy AccessibilityEvent", e);
                return null;
            }
        }
    }

    private long calculateReadingDelay(String text) {
        String[] words = text.split("\\s+");
        long estimatedTime = (long) words.length * MILLISECONDS_PER_WORD;
        return Math.max(estimatedTime, MIN_PADDING_MS);
    }

    private void clearQueueAndStop() {
        announcementQueue.clear();
        mainHandler.removeCallbacksAndMessages(ANNOUNCEMENT_TOKEN);
        
        if (am != null && am.isEnabled()) {
            try {
                am.interrupt();
            } catch (Exception e) {
                Log.e(TAG, "Error trying to interrupt active accessibility speech loop", e);
            }
        }
        isProcessingQueue = false;
    }

    public void stop() {
        if (Looper.myLooper() == Looper.getMainLooper()) {
            clearQueueAndStop();
        } else {
            mainHandler.post(this::clearQueueAndStop);
        }
    }

    public void shutdown() {
        stop();
    }
}
