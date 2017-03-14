package com.balthazargronon.RCTZeroconf;

import android.content.Context;
import android.net.nsd.NsdManager;
import android.net.nsd.NsdServiceInfo;
import android.os.AsyncTask;

import java.io.IOException;
import java.util.HashMap;
import java.util.Iterator;
import java.util.LinkedHashMap;
import java.util.Map;

import com.youview.tinydnssd.MDNSDiscover;

/**
 * Created by gabriel on 3/14/17.
 */

public abstract class DiscoverResolver {
    private static final int RESOLVE_TIMEOUT = 5000;

    private final MapDebouncer<String, Object> mDebouncer;
    private final Context mContext;
    private final String mServiceType;
    private final HashMap<String, MDNSDiscover.Result> mServices = new HashMap<>();
    private boolean mStarted;
    private boolean mTransitioning;
    private ResolveTask mResolveTask;
    private final Map<String, NsdServiceInfo> mResolveQueue = new LinkedHashMap<>();

    DiscoverResolver(Context context, String serviceType, int debounceMillis) {
        if (context == null) throw new NullPointerException("context was null");
        if (serviceType == null) throw new NullPointerException("serviceType was null");

        mContext = context;
        mServiceType = serviceType;

        mDebouncer = new MapDebouncer<>(debounceMillis, new MapDebouncer.Listener<String, Object>() {
            @Override
            public void put(String name, Object o) {
                if (o != null) {
                    synchronized (mResolveQueue) {
                        mResolveQueue.put(name, null);
                    }
                    startResolveTaskIfNeeded();
                } else {
                    synchronized (DiscoverResolver.this) {
                        synchronized (mResolveQueue) {
                            mResolveQueue.remove(name);
                        }
                        if (mStarted) {
                            MDNSDiscover.Result service = mServices.remove(name);
                            if (service != null) {
                                onServiceLost(service);
                            }
                        }
                    }
                }
            }
        });
    }

    public synchronized void start() {
        if (mStarted) {
            return;
        }
        if (!mTransitioning) {
            discoverServices(mServiceType, NsdManager.PROTOCOL_DNS_SD, mDiscoveryListener);
            mTransitioning = true;
        }
        mStarted = true;
    }

    public synchronized void stop() {
        if (!mStarted) {
            return;
        }
        if (!mTransitioning) {
            stopServiceDiscovery(mDiscoveryListener);
            mTransitioning = true;
        }
        synchronized (mResolveQueue) {
            mResolveQueue.clear();
        }
        mDebouncer.clear();
        mServices.clear();
        mStarted = false;
    }

    public abstract void onStartDiscoveryFailed(int errorCode);
    public abstract void onStopDiscoveryFailed(int errorCode);
    public abstract void onDiscoveryStarted();
    public abstract void onDiscoveryStopped();
    public abstract void onServiceFound(NsdServiceInfo serviceInfo);
    public abstract void onServiceLost(MDNSDiscover.Result serviceResult);
    public abstract void onServiceResolved(MDNSDiscover.Result serviceResult);
    public abstract void onResolveFailed(String errorMessage);

    private NsdManager.DiscoveryListener mDiscoveryListener = new NsdManager.DiscoveryListener() {
        @Override
        public void onStartDiscoveryFailed(String serviceType, int errorCode) {
            DiscoverResolver.this.onStartDiscoveryFailed(errorCode);
        }

        @Override
        public void onStopDiscoveryFailed(String serviceType, int errorCode) {
            DiscoverResolver.this.onStopDiscoveryFailed(errorCode);
        }

        @Override
        public void onDiscoveryStarted(String serviceType) {
            DiscoverResolver.this.onDiscoveryStarted();
            synchronized (DiscoverResolver.this) {
                if (!mStarted) {
                    stopServiceDiscovery(this);
                } else {
                    mTransitioning = false;
                }
            }
        }

        @Override
        public void onDiscoveryStopped(String serviceType) {
            DiscoverResolver.this.onDiscoveryStopped();
            if (mStarted) {
                discoverServices(serviceType, NsdManager.PROTOCOL_DNS_SD, this);
            } else {
                mTransitioning = false;
            }
        }

        @Override
        public void onServiceFound(final NsdServiceInfo serviceInfo) {
            DiscoverResolver.this.onServiceFound(serviceInfo);
            synchronized (DiscoverResolver.this) {
                if (mStarted) {
                    String name = serviceInfo.getServiceName() + "." + serviceInfo.getServiceType() + "local";
                    mDebouncer.put(name, DUMMY);
                }
            }
        }

        @Override
        public void onServiceLost(final NsdServiceInfo serviceInfo) {
            synchronized (DiscoverResolver.this) {
                if (mStarted) {
                    String name = serviceInfo.getServiceName() + "." + serviceInfo.getServiceType() + "local";
                    mDebouncer.put(name, null);
                }
            }
        }
    };

    /**
     * A non-null value that indicates membership in the MapDebouncer, null indicates non-membership
     */
    private Object DUMMY = new Object();

    private class ResolveTask extends AsyncTask<Void, Void, Void> {
        @Override
        protected Void doInBackground(Void... params) {
            while (!isCancelled()) {
                String serviceName;
                synchronized (mResolveQueue) {
                    Iterator<String> it = mResolveQueue.keySet().iterator();
                    if (!it.hasNext()) {
                        break;
                    }
                    serviceName = it.next();
                    it.remove();
                }

                try {
                    MDNSDiscover.Result result = resolve(serviceName, RESOLVE_TIMEOUT);
                    synchronized (DiscoverResolver.this) {
                        if (mStarted) {
                            mServices.put(serviceName, result);
                            onServiceResolved(result);
                        } else {
                            onResolveFailed("Discovery is not running anymore");
                        }
                    }
                } catch(IOException e) {
                    onResolveFailed("" + e);
                }
            }
            return null;
        }

        @Override
        protected void onPostExecute(Void aVoid) {
            mResolveTask = null;
            startResolveTaskIfNeeded();
        }
    }

    private void startResolveTaskIfNeeded() {
        if (mResolveTask == null) {
            synchronized (mResolveQueue) {
                if (!mResolveQueue.isEmpty()) {
                    mResolveTask = new ResolveTask();
                    mResolveTask.execute();
                }
            }
        }
    }

    protected void discoverServices(String serviceType, int protocol, NsdManager.DiscoveryListener listener) {
        ((NsdManager) mContext.getSystemService(Context.NSD_SERVICE)).discoverServices(serviceType, protocol, listener);
    }

    protected void stopServiceDiscovery(NsdManager.DiscoveryListener listener) {
        ((NsdManager) mContext.getSystemService(Context.NSD_SERVICE)).stopServiceDiscovery(listener);
    }

    protected MDNSDiscover.Result resolve(String serviceName, int resolveTimeout) throws IOException {
        return MDNSDiscover.resolve(serviceName, resolveTimeout);
    }
}
