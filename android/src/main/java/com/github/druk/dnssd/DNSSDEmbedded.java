/*
 * Copyright (C) 2016 Andriy Druk
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.github.druk.dnssd;

import android.content.Context;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;

/**
 * DNSSD implementation running the embedded mDNSResponder  {@link InternalDNSSD}
 *
 * The responder's event loop runs on its own thread, started by the first operation and stopped
 * a while after the last one ended. Operations start and stop on arbitrary threads, so the
 * lifecycle state below is guarded by the class monitor.
 */
public class DNSSDEmbedded extends DNSSD {

    public static final int DEFAULT_STOP_TIMER_DELAY = 5000; //5 sec

    private static final String TAG = "DNSSDEmbedded";
    private final long mStopTimerDelay;
    private final Handler handler = new Handler(Looper.getMainLooper());
    private final ExitTimer exitTimer;
    private Thread mThread;                       // guarded by DNSSDEmbedded.class
    private volatile boolean isStarted = false;   // the loop is running (set by the loop thread)
    private volatile boolean isExiting = false;   // nativeExit() was called, the thread is on its way out
    private int serviceCount = 0;                 // guarded by DNSSDEmbedded.class

    public DNSSDEmbedded(Context context) {
        this(context, DEFAULT_STOP_TIMER_DELAY);
    }

    public DNSSDEmbedded(Context context, long stopTimerDelay) {
        super(context, "jdns_sd_embedded");
        mStopTimerDelay = stopTimerDelay;
        exitTimer = new ExitTimer(handler, stopTimerDelay, this::exitIfIdle);
    }

    /**
     * Stops the responder a while after the last operation ended, unless another one starts first.
     * The same Runnable is posted and removed: evaluating a method reference at two call sites
     * gives two objects, so posting one and removing the other would never cancel anything.
     */
    static final class ExitTimer {
        private final Handler handler;
        private final long delayMillis;
        private final Runnable exit;

        ExitTimer(Handler handler, long delayMillis, Runnable exit) {
            this.handler = handler;
            this.delayMillis = delayMillis;
            this.exit = exit;
        }

        /** The last operation stopped: exit after the delay. */
        void schedule() {
            handler.removeCallbacks(exit);
            handler.postDelayed(exit, delayMillis);
        }

        /** An operation is starting: the responder stays up. */
        void cancel() {
            handler.removeCallbacks(exit);
        }
    }

    static native int nativeInit();

    static native int nativeLoop();

    static native void nativeExit();

    /**
     * Init DNS-SD thread and start event loop. Should be called before using any of DNSSD operations.
     * If DNS-SD thread has already initialised will try to reuse it.
     *
     * Note: This method will block thread until DNS-SD initialization finish.
     */
    public void init() {
        synchronized (DNSSDEmbedded.class) {
            exitTimer.cancel();

            if (mThread != null && mThread.isAlive()) {
                if (!isExiting) {
                    Log.i(TAG, "already started");
                    waitUntilStarted();
                    return;
                }
                // The exit timer fired just before this operation: let the thread finish, then start afresh
                Log.i(TAG, "restarting");
                try {
                    mThread.join();
                } catch (InterruptedException e) {
                    Log.e(TAG, "init interrupted while waiting for the previous loop: ", e);
                }
            }

            isStarted = false;
            isExiting = false;

            InternalDNSSD.getInstance();
            mThread = new Thread() {
                public void run() {
                    Log.i(TAG, "init");
                    int err = nativeInit();
                    synchronized (DNSSDEmbedded.class) {
                        isStarted = true;
                        DNSSDEmbedded.class.notifyAll();
                    }
                    if (err != 0) {
                        Log.e(TAG, "error: " + err);
                        return;
                    }
                    Log.i(TAG, "start");
                    int ret = nativeLoop();
                    isStarted = false;
                    Log.i(TAG, "finish with code: " + ret);
                }
            };
            mThread.setPriority(Thread.MAX_PRIORITY);
            mThread.setName("DNS-SDEmbedded");
            mThread.start();

            waitUntilStarted();
        }
    }

    /**
     * Exit from embedded DNS-SD loop. This method will stop DNS-SD after the delay (it makes possible to reuse already initialised DNS-SD thread).
     *
     * Note: method isn't blocking, can be used from any thread.
     */
    public void exit() {
        synchronized (DNSSDEmbedded.class) {
            Log.i(TAG, "post exit");
            exitTimer.schedule();
        }
    }

    // Runs on the main thread when the timer fires; an operation may have started since it was scheduled
    private void exitIfIdle() {
        synchronized (DNSSDEmbedded.class) {
            if (serviceCount != 0 || isExiting || mThread == null || !mThread.isAlive()) {
                return;
            }
            Log.i(TAG, "exit");
            isExiting = true;
            nativeExit();
        }
    }

    private void waitUntilStarted() {
        synchronized (DNSSDEmbedded.class) {
            while (!isStarted) {
                try {
                    DNSSDEmbedded.class.wait();
                } catch (InterruptedException e) {
                    Log.e(TAG, "waitUntilStarted exception: ", e);
                }
            }
        }
    }

    @Override
    public void onServiceStarting() {
        synchronized (DNSSDEmbedded.class) {
            super.onServiceStarting();
            this.init();
            serviceCount++;
        }
    }

    @Override
    public void onServiceStopped() {
        synchronized (DNSSDEmbedded.class) {
            super.onServiceStopped();
            serviceCount--;
            if (serviceCount == 0) {
                this.exit();
            }
        }
    }
}
