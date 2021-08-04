package com.balthazargronon.RCTZeroconf.nsd;

import android.annotation.SuppressLint;
import android.content.Context;
import android.net.nsd.NsdManager;
import android.net.nsd.NsdServiceInfo;
import android.net.wifi.WifiManager;
import android.os.Build;

import com.balthazargronon.RCTZeroconf.Zeroconf;
import com.balthazargronon.RCTZeroconf.ZeroconfModule;
import com.facebook.react.bridge.ReactApplicationContext;

import com.facebook.react.bridge.ReadableMap;
import com.facebook.react.bridge.ReadableMapKeySetIterator;
import com.facebook.react.bridge.WritableArray;
import com.facebook.react.bridge.WritableMap;
import com.facebook.react.bridge.WritableNativeArray;
import com.facebook.react.bridge.WritableNativeMap;

import java.io.UnsupportedEncodingException;
import java.net.InetAddress;
import java.util.HashMap;
import java.util.Locale;
import java.util.Map;

public class NsdServiceImpl implements Zeroconf {
    private NsdManager mNsdManager;
    private NsdManager.DiscoveryListener mDiscoveryListener;
    private WifiManager.MulticastLock multicastLock;
    private Map<String, NsdManager.RegistrationListener> mPublishedServices;
    private ZeroconfModule zeroconfModule;
    private ReactApplicationContext reactApplicationContext;

    public NsdServiceImpl(ZeroconfModule zeroconfModule, ReactApplicationContext reactApplicationContext) {
        this.zeroconfModule = zeroconfModule;
        this.reactApplicationContext = reactApplicationContext;
        mPublishedServices = new HashMap<String, NsdManager.RegistrationListener>();
    }

    @Override
    public void scan(String type, String protocol, String domain) {
        if (mNsdManager == null) {
            mNsdManager = (NsdManager) getReactApplicationContext().getSystemService(Context.NSD_SERVICE);
        }

        this.stop();

        if (multicastLock == null) {
            @SuppressLint("WifiManagerLeak") WifiManager wifi = (WifiManager) getReactApplicationContext().getSystemService(Context.WIFI_SERVICE);
            multicastLock = wifi.createMulticastLock("multicastLock");
            multicastLock.setReferenceCounted(true);
            multicastLock.acquire();
        }

        mDiscoveryListener = new NsdManager.DiscoveryListener() {
            @Override
            public void onStartDiscoveryFailed(String serviceType, int errorCode) {
                String error = "Starting service discovery failed with code: " + errorCode;
                zeroconfModule.sendEvent(getReactApplicationContext(), ZeroconfModule.EVENT_ERROR, error);
            }

            @Override
            public void onStopDiscoveryFailed(String serviceType, int errorCode) {
                String error = "Stopping service discovery failed with code: " + errorCode;
                zeroconfModule.sendEvent(getReactApplicationContext(), ZeroconfModule.EVENT_ERROR, error);
            }

            @Override
            public void onDiscoveryStarted(String serviceType) {
                System.out.println("On Discovery Started");
                zeroconfModule.sendEvent(getReactApplicationContext(), ZeroconfModule.EVENT_START, null);
            }

            @Override
            public void onDiscoveryStopped(String serviceType) {
                System.out.println("On Discovery Stopped");
                zeroconfModule.sendEvent(getReactApplicationContext(), ZeroconfModule.EVENT_STOP, null);
            }

            @Override
            public void onServiceFound(NsdServiceInfo serviceInfo) {
                System.out.println("On Service Found");
                WritableMap service = new WritableNativeMap();
                service.putString(ZeroconfModule.KEY_SERVICE_NAME, serviceInfo.getServiceName());

                zeroconfModule.sendEvent(getReactApplicationContext(), ZeroconfModule.EVENT_FOUND, service);
                mNsdManager.resolveService(serviceInfo, new NsdServiceImpl.ZeroResolveListener());
            }

            @Override
            public void onServiceLost(NsdServiceInfo serviceInfo) {
                System.out.println("On Service Lost");
                WritableMap service = new WritableNativeMap();
                service.putString(ZeroconfModule.KEY_SERVICE_NAME, serviceInfo.getServiceName());
                zeroconfModule.sendEvent(getReactApplicationContext(), ZeroconfModule.EVENT_REMOVE, service);
            }
        };

        String serviceType = String.format("_%s._%s.", type, protocol);
        mNsdManager.discoverServices(serviceType, NsdManager.PROTOCOL_DNS_SD, mDiscoveryListener);
    }

    @Override
    public void stop() {
        if (mDiscoveryListener != null) {
            mNsdManager.stopServiceDiscovery(mDiscoveryListener);
        }

        if (multicastLock != null) {
            multicastLock.release();
        }

        mDiscoveryListener = null;
        multicastLock = null;
    }

    @Override
    public void registerService(String type, String protocol, String domain, String name, int port, ReadableMap txt) {
        String serviceType = String.format("_%s._%s.", type, protocol);

        final NsdManager nsdManager = this.getNsdManager();
        NsdServiceInfo serviceInfo  = new NsdServiceInfo();
        serviceInfo.setServiceName(name);
        serviceInfo.setServiceType(serviceType);
        serviceInfo.setPort(port);

        ReadableMapKeySetIterator iterator = txt.keySetIterator();
        while (iterator.hasNextKey()) {
            String key = iterator.nextKey();
            serviceInfo.setAttribute(key, txt.getString(key));
        }

        nsdManager.registerService(
                serviceInfo, NsdManager.PROTOCOL_DNS_SD, new ServiceRegistrationListener());
    }

    @Override
    public void unregisterService(String serviceName) {

        final NsdManager nsdManager = this.getNsdManager();

        NsdManager.RegistrationListener serviceListener = mPublishedServices.get(serviceName);

        if (serviceListener != null) {
            mPublishedServices.remove(serviceName);
            nsdManager.unregisterService(serviceListener);
        }
    }

    private NsdManager getNsdManager() {
        if (mNsdManager == null) {
            mNsdManager = (NsdManager) getReactApplicationContext().getSystemService(Context.NSD_SERVICE);
        }
        return mNsdManager;
    }

    private ReactApplicationContext getReactApplicationContext() {
        return reactApplicationContext;
    }

    private class ZeroResolveListener implements NsdManager.ResolveListener {
        @Override
        public void onResolveFailed(NsdServiceInfo serviceInfo, int errorCode) {
            if (errorCode == NsdManager.FAILURE_ALREADY_ACTIVE) {
                mNsdManager.resolveService(serviceInfo, this);
            } else {
                String error = "Resolving service failed with code: " + errorCode;
                zeroconfModule.sendEvent(getReactApplicationContext(), ZeroconfModule.EVENT_ERROR, error);
            }
        }

        @Override
        public void onServiceResolved(NsdServiceInfo serviceInfo) {
            WritableMap service = serviceInfoToMap(serviceInfo);
            zeroconfModule.sendEvent(getReactApplicationContext(), ZeroconfModule.EVENT_RESOLVE, service);
        }
    }

    private class ServiceRegistrationListener implements NsdManager.RegistrationListener {

        @Override
        public void onServiceRegistered(NsdServiceInfo NsdServiceInfo) {
            // Save the service name.  Android may have changed it in order to
            // resolve a conflict, so update the name you initially requested
            // with the name Android actually used.

            final String serviceName = NsdServiceInfo.getServiceName();
            mPublishedServices.put(serviceName, this);

            WritableMap service = serviceInfoToMap(NsdServiceInfo);
            zeroconfModule.sendEvent(getReactApplicationContext(), ZeroconfModule.EVENT_PUBLISHED, service);
        }

        @Override
        public void onRegistrationFailed(NsdServiceInfo serviceInfo, int errorCode) {
            // Registration failed!  Put debugging code here to determine why.
        }

        @Override
        public void onServiceUnregistered(NsdServiceInfo nsdServiceInfo) {
            // Service has been unregistered.  This only happens when you call
            // NsdManager.unregisterService() and pass in this listener.
            final WritableMap service = serviceInfoToMap(nsdServiceInfo);
            zeroconfModule.sendEvent(getReactApplicationContext(), ZeroconfModule.EVENT_UNREGISTERED, service);
        }

        @Override
        public void onUnregistrationFailed(NsdServiceInfo serviceInfo, int errorCode) {
            // Unregistration failed.  Put debugging code here to determine why.
        }
    }

    private WritableMap serviceInfoToMap(NsdServiceInfo serviceInfo) {
        WritableMap service = new WritableNativeMap();
        service.putString(ZeroconfModule.KEY_SERVICE_NAME, serviceInfo.getServiceName());
        final InetAddress host = serviceInfo.getHost();
        final String fullServiceName;
        if (host == null) {
            fullServiceName = serviceInfo.getServiceName();
        } else {
            fullServiceName = host.getHostName() + serviceInfo.getServiceType();
            service.putString(ZeroconfModule.KEY_SERVICE_HOST, host.getHostName());

            WritableArray addresses = new WritableNativeArray();
            addresses.pushString(host.getHostAddress());

            service.putArray(ZeroconfModule.KEY_SERVICE_ADDRESSES, addresses);
        }
        service.putString(ZeroconfModule.KEY_SERVICE_FULL_NAME, fullServiceName);
        service.putInt(ZeroconfModule.KEY_SERVICE_PORT, serviceInfo.getPort());

        WritableMap txtRecords = new WritableNativeMap();

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            Map<String, byte[]> attributes = serviceInfo.getAttributes();
            for (String key : attributes.keySet()) {
                try {
                    byte[] recordValue = attributes.get(key);
                    txtRecords.putString(String.format(Locale.getDefault(), "%s", key), String.format(Locale.getDefault(), "%s", recordValue != null ? new String(recordValue, "UTF_8") : ""));
                } catch (UnsupportedEncodingException e) {
                    String error = "Failed to encode txtRecord: " + e;
                    zeroconfModule.sendEvent(getReactApplicationContext(), ZeroconfModule.EVENT_ERROR, error);
                }
            }
        }

        service.putMap(ZeroconfModule.KEY_SERVICE_TXT, txtRecords);

        return service;
    }
}
