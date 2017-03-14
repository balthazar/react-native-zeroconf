/* The MIT License (MIT)
 * Copyright (c) 2016 YouView Ltd
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

package com.balthazargronon.RCTZeroconf;

import android.os.Handler;
import android.os.SystemClock;

import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;

/**
 * <p>Map-like interface supporting service visibility debouncing:</p>
 * <ul>
 * <li>Inserts (null -> A) are immediately notified to the {@code Listener}
 * <li>Updates (A -> B) are immediate
 * <li>Deletes (A -> null) are debounced such that (A -> null -> A) within a certain time interval
 * will never be deleted
 * </ul>
 */
class MapDebouncer<Key, Value> {

    interface Listener<Key, Value> {
        void put(Key key, Value value);
    }

    private final int mDebouncePeriodMillis;
    private final Listener<Key, Value> mListener;

    private final Map<Key, Value> mBackingMap = new HashMap<>();
    private final Map<Key, Long> mRemovalSchedule = new HashMap<>();

    private long mNextScheduledRemoval;
    private Handler mHandler;

    MapDebouncer(int debouncePeriodMillis, Listener<Key, Value> listener) {
        if (debouncePeriodMillis < 0) {
            throw new IllegalArgumentException();
        }
        if (listener == null) {
            throw new NullPointerException();
        }
        mDebouncePeriodMillis = debouncePeriodMillis;
        mListener = listener;
    }

    void put(Key key, Value newValue) {
        if (mDebouncePeriodMillis == 0) {
            mListener.put(key, newValue);
            return;
        }

        // lazy init the handler to match the thread that calls put()
        // also avoids creating a handler if there is no debounce period
        if (mHandler == null) {
            mHandler = new Handler();
        }

        Value oldValue = mBackingMap.get(key);
        if (oldValue == null) {
            if (newValue != null) {
                immediateUpdate(key, newValue);
            }
        } else{
            if (newValue == null) {
                timedRemoval(key);
            } else if (oldValue.equals(newValue)) {
                cancelTimedRemoval(key);
            } else {
                immediateUpdate(key, newValue);
            }
        }
    }

    private final Runnable mRemoveRunnable = new Runnable() {
        @Override
        public void run() {
            long currentTime = SystemClock.uptimeMillis();
            Iterator<Map.Entry<Key, Long>> it = mRemovalSchedule.entrySet().iterator();
            long nextScheduleTime = Long.MAX_VALUE;
            while (it.hasNext()) {
                Map.Entry<Key, Long> entry = it.next();
                long itemTime = entry.getValue();
                if (itemTime <= currentTime) {
                    Key key = entry.getKey();
                    performUpdate(key, null);
                    it.remove();
                } else if (itemTime < nextScheduleTime) {
                    nextScheduleTime = itemTime;
                }
            }
            mNextScheduledRemoval = 0;
            if (nextScheduleTime < Long.MAX_VALUE) {
                mHandler.postAtTime(mRemoveRunnable, nextScheduleTime);
                mNextScheduledRemoval = nextScheduleTime;
            }
        }
    };

    private void performUpdate(Key key, Value value) {
        mBackingMap.put(key, value);
        mListener.put(key, value);
    }

    private void cancelTimedRemoval(Key key) {
        Long scheduled = mRemovalSchedule.remove(key);
        // if this item was the next to be scheduled, calculate the next most imminent item, if any
        if (scheduled != null && scheduled == mNextScheduledRemoval) {
            long newSchedule = Long.MAX_VALUE;
            for (long time : mRemovalSchedule.values()) {
                if (time < newSchedule) {
                    newSchedule = time;
                }
            }
            if (scheduled != newSchedule) {
                mHandler.removeCallbacks(mRemoveRunnable);
                if (newSchedule < Long.MAX_VALUE) {
                    mHandler.postAtTime(mRemoveRunnable, newSchedule);
                }
            }
        }
    }

    private void timedRemoval(Key key) {
        long removalTime = SystemClock.uptimeMillis() + mDebouncePeriodMillis;

        if (mNextScheduledRemoval == 0) {
            mNextScheduledRemoval = removalTime;
            mHandler.postAtTime(mRemoveRunnable, removalTime);
        } else if (removalTime < mNextScheduledRemoval) {
            mHandler.removeCallbacks(mRemoveRunnable);
            mNextScheduledRemoval = removalTime;
            mHandler.postAtTime(mRemoveRunnable, removalTime);
        }
        mRemovalSchedule.put(key, removalTime);
    }

    private void immediateUpdate(Key key, Value value) {
        cancelTimedRemoval(key);
        performUpdate(key, value);
    }

    void clear() {
        mBackingMap.clear();
        mRemovalSchedule.clear();
        if (mHandler != null) {
            mHandler.removeCallbacks(mRemoveRunnable);
        }
        mNextScheduledRemoval = 0;
    }
}