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
    private volatile TextToSpeech tts;
    private volatile boolean ready = false;
    private volatile boolean speaking = false;
    private float currentRate = 1.0f;
    private float currentVolume = 1.0f;
    private String[] ringBuffer;
    private int capacity;
    private int head = 0;
    private int tail = 0;
    private int size = 0;
    private final ReentrantLock lock;
    private final Condition loopCondition;
    private final Condition speechCompletionCondition;
    private Thread workerThread;
    private volatile boolean isRunning;

    private static final class SafeUtteranceListener extends UtteranceProgressListener {
        private final WeakReference<AndroidTTSHelper> helperRef;

        SafeUtteranceListener(AndroidTTSHelper helper) {
            this.helperRef = new WeakReference<>(helper);
        }

        @Override
        public void onStart(String utteranceId) {
            if (helperRef.get() instanceof AndroidTTSHelper helper) {
                Log.d(TAG, "Native hardware engine started rendering utterance: " + utteranceId);
            }
        }

        @Override
        public void onDone(String utteranceId) {
            if (helperRef.get() instanceof AndroidTTSHelper helper) {
                helper.lock.lock();
                try {
                    helper.speaking = false;
                    helper.speechCompletionCondition.signalAll();
                } finally {
                    helper.lock.unlock();
                }
            }
        }

        @Override
        @Deprecated
        public void onError(String utteranceId) {
            onError(utteranceId, -1);
        }

        @Override
        public void onError(String utteranceId, int errorCode) {
            if (helperRef.get() instanceof AndroidTTSHelper helper) {
                helper.lock.lock();
                try {
                    helper.speaking = false;
                    helper.speechCompletionCondition.signalAll();
                    Log.w(TAG, "Utterance playback failed for ID: %s with error code: %d".formatted(utteranceId, errorCode));
                } finally {
                    helper.lock.unlock();
                }
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
            if (!(helperRef.get() instanceof AndroidTTSHelper helper)) {
                return;
            }

            if (status == TextToSpeech.SUCCESS) {
                try {
                    helper.lock.lock();
                    try {
                        TextToSpeech actualTts = helper.tts;
                        if (actualTts == null) {
                            Log.w(TAG, "TTS instance reference not fully committed yet during initialization callback loop.");
                            return;
                        }

                        actualTts.setLanguage(Locale.getDefault());
                        actualTts.setOnUtteranceProgressListener(new SafeUtteranceListener(helper));
                        actualTts.setSpeechRate(helper.currentRate);
                        
                        helper.ready = true;
                        helper.startWorker();
                    } finally {
                        helper.lock.unlock();
                    }
                } catch (Throwable t) {
                    switch (t) {
                        case Exception e -> Log.e(TAG, "Failed to completely initialize TTS listener tracks", e);
                        default -> Log.wtf(TAG, "Fatal engine breakdown during TTS tracking setup pass", t);
                    }
                    
                    helper.lock.lock();
                    try {
                        helper.ready = false;
                    } finally {
                        helper.lock.unlock();
                    }
                }
            } else {
                Log.e(TAG, "Android TextToSpeech initialization engine failed with status: %d".formatted(status));
                
                helper.lock.lock();
                try {
                    helper.ready = false;
                } finally {
                    helper.lock.unlock();
                }
            }
        }
    }

    public AndroidTTSHelper(@NonNull Context context) {
        Objects.requireNonNull(context, "Context cannot be null");
        final Context appContext = context.getApplicationContext();
        
        this.capacity = INITIAL_CAPACITY;
        this.ringBuffer = new String[capacity];
        this.lock = new ReentrantLock();
        this.loopCondition = lock.newCondition();
        this.speechCompletionCondition = lock.newCondition();
        
        this.tts = new TextToSpeech(appContext, new SafeOnInitListener(this));
    }

    public boolean isActive() { 
        return ready && tts != null; 
    }

    public boolean isSpeaking() { 
        lock.lock();
        try {
            return speaking; 
        } finally {
            lock.unlock();
        }
    }

    public void speak(String text, boolean interrupt) {
        if (text == null || text.trim().isEmpty()) {
            return;
        }

        lock.lock();
        try {
            if (interrupt) {
                clearQueueAndNativeFrameworkStop();
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

    private void startWorker() {
        if (workerThread != null && workerThread.isAlive()) {
            return;
        }
        isRunning = true;
        workerThread = Thread.ofVirtual()
                .name("TTSBackgroundWorker")
                .start(new TTSWorkerRunnable());
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

                    if (!isRunning) break;
                    while (speaking && isRunning) {
                        speechCompletionCondition.await();
                    }

                    if (!isRunning) break;
                    targetText = ringBuffer[head];
                    ringBuffer[head] = null;
                    head = (head + 1) % capacity;
                    size--;

                } catch (InterruptedException e) {
                    Thread.currentThread().interrupt();
                    break;
                } finally {
                    lock.unlock();
                }

                if (targetText != null && isActive()) {
                    executeNativeSpeak(targetText);
                }
            }
        }
    }

    private void executeNativeSpeak(String text) {
        if (tts == null) return;

        Bundle params = new Bundle();
        if (currentVolume != 1.0f) {
            params.putFloat(TextToSpeech.Engine.KEY_PARAM_VOLUME, currentVolume);
        }

        lock.lock();
        try {
            speaking = true;
        } finally {
            lock.unlock();
        }

        boolean submissionSuccess = false;
        try {
            int result = tts.speak(text, TextToSpeech.QUEUE_ADD, params, UTTERANCE_ID);
            submissionSuccess = (result == TextToSpeech.SUCCESS);
            
            if (!submissionSuccess) {
                Log.e(TAG, "Native speech submission rejected by engine with status code: " + result);
            }
        } catch (Throwable t) {
            switch (t) {
                case Exception e -> Log.e(TAG, "Error executing bundle-based speech invocation", e);
                default -> Log.wtf(TAG, "Fatal error executing bundle-based speech invocation", t);
            }
        }

        if (!submissionSuccess) {
            lock.lock();
            try {
                speaking = false;
                speechCompletionCondition.signalAll();
            } finally {
                lock.unlock();
            }
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
        
        Log.d(TAG, "Dynamic ring buffer expanded. New capacity: %d".formatted(newCapacity));
    }

    private void clearQueueAndNativeFrameworkStop() {
        assert lock.isHeldByCurrentThread() : "Lock must be held when clearing queues";
        
        java.util.Arrays.fill(ringBuffer, null);
        head = 0;
        tail = 0;
        size = 0;
        speaking = false;

        if (tts != null) {
            try {
                tts.stop();
            } catch (Exception e) {
                Log.e("SRAL_TTSHelper", "Failed to interrupt active playback queue safely", e);
            }
        }
        
        speechCompletionCondition.signalAll();
        loopCondition.signalAll(); 
    }

    public void stop() {
        lock.lock();
        try {
            clearQueueAndNativeFrameworkStop();
        } finally {
            lock.unlock();
        }
    }

    public void setSpeechRate(float rate) {
        currentRate = rate;
        if (tts != null && ready) {
            try {
                tts.setSpeechRate(rate);
            } catch (Exception e) {
                Log.e(TAG, "Failed to apply runtime speech rate updates", e);
            }
        }
    }

    public void setVolume(float volume) {
        currentVolume = Math.clamp(volume, 0.0f, 1.0f);
    }

    public float getRate() { 
        return currentRate; 
    }
    
    public float getVolume() { 
        return currentVolume; 
    }

    public void shutdown() {
        lock.lock();
        try {
            isRunning = false;
            ready = false;
            clearQueueAndNativeFrameworkStop();
            loopCondition.signalAll();
            speechCompletionCondition.signalAll();
            
            if (workerThread != null) {
                workerThread.interrupt();
                workerThread = null;
            }
        } finally {
            lock.unlock();
        }

        if (tts != null) {
            try {
                tts.shutdown();
            } catch (Exception e) {
                Log.e(TAG, "Error encountered during engine lifecycle shutdown pass", e);
            } finally {
                tts = null;
            }
        }
    }
}
