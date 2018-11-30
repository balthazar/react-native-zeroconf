package com.balthazargronon.RCTZeroconf;

import android.net.wifi.WifiManager;
import android.os.Build;
import android.util.Log;

import com.facebook.react.bridge.Arguments;
import com.facebook.react.bridge.ReactApplicationContext;
import com.facebook.react.bridge.ReactContext;
import com.facebook.react.bridge.ReactContextBaseJavaModule;
import com.facebook.react.bridge.ReactMethod;
import com.facebook.react.bridge.WritableArray;
import com.facebook.react.bridge.WritableMap;
import com.facebook.react.bridge.WritableNativeArray;
import com.facebook.react.bridge.WritableNativeMap;
import com.facebook.react.modules.core.DeviceEventManagerModule;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.IOException;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.net.Inet4Address;
import java.net.Inet6Address;
import java.net.InetAddress;
import java.net.NetworkInterface;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Enumeration;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;

import javax.annotation.Nullable;
import javax.jmdns.JmDNS;
import javax.jmdns.ServiceEvent;
import javax.jmdns.ServiceInfo;
import javax.jmdns.ServiceListener;

import static android.content.Context.WIFI_SERVICE;


/**
 * Created by Jeremy White on 8/1/2016.
 * Modified by James on 30/11/2018.
 * Copyright © 2016 Balthazar Gronon MIT
 */
public class ZeroconfModule extends ReactContextBaseJavaModule {
	private static final String TAG = "ZeroConf";
	public static final String EVENT_START = "RNZeroconfStart";
	public static final String EVENT_STOP = "RNZeroconfStop";
	public static final String EVENT_ERROR = "RNZeroconfError";
	public static final String EVENT_FOUND = "RNZeroconfFound";
	public static final String EVENT_REMOVE = "RNZeroconfRemove";
	public static final String EVENT_RESOLVE = "RNZeroconfResolved";

	public static final String KEY_SERVICE_NAME = "name";
	public static final String KEY_SERVICE_FULL_NAME = "fullName";
	public static final String KEY_SERVICE_HOST = "host";
	public static final String KEY_SERVICE_PORT = "port";
	public static final String KEY_SERVICE_ADDRESSES = "addresses";
	public static final String KEY_SERVICE_TXT = "txt";

	WifiManager.MulticastLock lock;

	private RegistrationManager registrationManager;
	private BrowserManager browserManager;
	private List<InetAddress> addresses;
	private List<InetAddress> ipv6Addresses;
	private List<InetAddress> ipv4Addresses;
	private String hostname;

	private String addressFamily = "any";

	interface DiscoveryListener{
		void onStartDiscoveryFailed(String serviceType, int errorCode);
		void onStopDiscoveryFailed(String serviceType, int errorCode);
		void onDiscoveryStarted(String serviceType);
		void onDiscoveryStopped(String serviceType);
		void onServiceFound(ServiceInfo serviceInfo);
		void onServiceLost(ServiceInfo serviceInfo);
		void onServiceResolved(ServiceInfo serviceInfo);
	}
	public ZeroconfModule(ReactApplicationContext reactContext) {
		super(reactContext);
		WifiManager wifi = (WifiManager) reactContext.getApplicationContext().getSystemService(WIFI_SERVICE);
		lock = wifi.createMulticastLock("ZeroConfPluginLock");
		lock.setReferenceCounted(true);
		lock.acquire();

		try {
			addresses = new ArrayList<InetAddress>();
			ipv6Addresses = new ArrayList<InetAddress>();
			ipv4Addresses = new ArrayList<InetAddress>();
			List<NetworkInterface> intfs = Collections.list(NetworkInterface.getNetworkInterfaces());
			for (NetworkInterface intf : intfs) {
				if (intf.supportsMulticast()) {
					List<InetAddress> addrs = Collections.list(intf.getInetAddresses());
					for (InetAddress addr : addrs) {
						if (!addr.isLoopbackAddress()) {
							if (addr instanceof Inet6Address) {
								addresses.add(addr);
								ipv6Addresses.add(addr);
							} else if (addr instanceof Inet4Address) {
								addresses.add(addr);
								ipv4Addresses.add(addr);
							}
						}
					}
				}
			}
		} catch (Exception e) {
			Log.e(TAG, e.getMessage(), e);
		}

		Log.d(TAG, "Addresses " + addresses);

		try {
			hostname = getHostName();
		} catch (Exception e) {
			Log.e(TAG, e.getMessage(), e);
		}

		Log.d(TAG, "Hostname " + hostname);

		Log.v(TAG, "Initialized");
	}

	@Override
	public String getName() {
		return "RNZeroconf";
	}

	@ReactMethod
	public void scan(String type, String protocol, String domain) {

		this.stop();
		try {
			final String serviceType = String.format("_%s._%s.", type, protocol);
			Log.d(TAG, "Scan " + serviceType);
			if (browserManager == null) {
				List<InetAddress> selectedAddresses = addresses;
				if ("ipv6".equalsIgnoreCase(addressFamily)) {
					selectedAddresses = ipv6Addresses;
				} else if ("ipv4".equalsIgnoreCase(addressFamily)) {
					selectedAddresses = ipv4Addresses;
				}

				browserManager = new BrowserManager(selectedAddresses, hostname);

			}

			browserManager.watch(serviceType, domain, new DiscoveryListener(){

				@Override
	            public void onStartDiscoveryFailed(String serviceType, int errorCode) {
	                String error = "Starting service discovery failed with code: " + errorCode;
	                sendEvent(getReactApplicationContext(), EVENT_ERROR, error);
	            }

	            @Override
	            public void onStopDiscoveryFailed(String serviceType, int errorCode) {
	                String error = "Stopping service discovery failed with code: " + errorCode;
	                sendEvent(getReactApplicationContext(), EVENT_ERROR, error);
	            }

	            @Override
	            public void onDiscoveryStarted(String serviceType) {
	                sendEvent(getReactApplicationContext(), EVENT_START, null);
	            }

	            @Override
	            public void onDiscoveryStopped(String serviceType) {
	                sendEvent(getReactApplicationContext(), EVENT_STOP, null);
	            }

	            @Override
	            public void onServiceFound(ServiceInfo serviceInfo) {
					try {
			            JSONObject serviceJSON = jsonifyService(serviceInfo);
			            WritableMap service = convertJsonToMap(serviceJSON);
			            Log.d(TAG, "service " + service.toString());
			            sendEvent(getReactApplicationContext(), EVENT_FOUND, service);
		            } catch (JSONException e) {
			            e.printStackTrace();
			            sendEvent(getReactApplicationContext(), EVENT_ERROR, e);
		            }
	            }

	            @Override
	            public void onServiceLost(ServiceInfo serviceInfo) {
	                WritableMap service = new WritableNativeMap();
	                service.putString(KEY_SERVICE_NAME, serviceInfo.getType());
	                sendEvent(getReactApplicationContext(), EVENT_REMOVE, service);
	            }

				@Override
				public void onServiceResolved(ServiceInfo serviceInfo) {

					try {
						JSONObject serviceJSON = jsonifyService(serviceInfo);
						WritableMap service = convertJsonToMap(serviceJSON);
						Log.d(TAG, "service " + service.toString());
						sendEvent(getReactApplicationContext(), EVENT_RESOLVE, service);
					} catch (JSONException e) {
						e.printStackTrace();
						sendEvent(getReactApplicationContext(), EVENT_ERROR, e);
					}
				}
			});
		} catch (IOException e) {
			e.printStackTrace();
			sendEvent(getReactApplicationContext(), EVENT_ERROR, e);
		}
	}

	@ReactMethod
	public void stop() {
		if (browserManager != null) {
			try {
				browserManager.close();

			} catch (IOException e) {
				Log.e(TAG, e.getMessage(), e);
				sendEvent(getReactApplicationContext(), EVENT_ERROR, e);
			}
			browserManager = null;
		}
	}

	protected void sendEvent(ReactContext reactContext,
	                         String eventName,
	                         @Nullable Object params) {
		reactContext
				.getJSModule(DeviceEventManagerModule.RCTDeviceEventEmitter.class)
				.emit(eventName, params);
	}


	private class RegistrationManager {

		private List<JmDNS> publishers = new ArrayList<JmDNS>();

		public RegistrationManager(List<InetAddress> addresses, String hostname) throws IOException {

			if (addresses == null || addresses.size() == 0) {
				publishers.add(JmDNS.create(null, hostname));
			} else {
				for (InetAddress addr : addresses) {
					publishers.add(JmDNS.create(addr, hostname));
				}
			}

		}

		public ServiceInfo register(String type, String domain, String name, int port, JSONObject props)
				throws JSONException, IOException {

			HashMap<String, String> txtRecord = new HashMap<String, String>();
			if (props != null) {
				Iterator<String> iter = props.keys();
				while (iter.hasNext()) {
					String key = iter.next();
					txtRecord.put(key, props.getString(key));
				}
			}

			ServiceInfo aService = null;
			for (JmDNS publisher : publishers) {
				ServiceInfo service = ServiceInfo.create(type + domain, name, port, 0, 0, txtRecord);
				try {
					publisher.registerService(service);
					aService = service;
				} catch (IOException e) {
					Log.e(TAG, e.getMessage(), e);
				}
			}
			// returns only one of the ServiceInfo instances!
			return aService;
		}

		public void unregister(String type, String domain, String name) {

			for (JmDNS publisher : publishers) {
				ServiceInfo serviceInfo = publisher.getServiceInfo(type + domain, name, 5000);
				if (serviceInfo != null) {
					publisher.unregisterService(serviceInfo);
				}
			}

		}

		public void stop() throws IOException {

			for (JmDNS publisher : publishers) {
				publisher.close();
			}

		}

	}

	private class BrowserManager implements ServiceListener {

		private List<JmDNS> browsers = new ArrayList<JmDNS>();

		private Map<String, DiscoveryListener> callbacks = new HashMap<String, DiscoveryListener>();

		public BrowserManager(List<InetAddress> addresses, String hostname) throws IOException {

			if (addresses == null || addresses.size() == 0) {
				browsers.add(JmDNS.create(null, hostname));
			} else {
				for (InetAddress addr : addresses) {
					browsers.add(JmDNS.create(addr, hostname));
				}
			}
		}

		private void watch(String type, String domain, DiscoveryListener listener) {

			callbacks.put(type + domain, listener);

			for (JmDNS browser : browsers) {
				browser.addServiceListener(type + domain, this);
			}
			listener.onDiscoveryStarted(type);

		}

		private void unwatch(String type, String domain) {
			DiscoveryListener listener = callbacks.get(type);
			listener.onDiscoveryStopped(type);
			callbacks.remove(type + domain);

			for (JmDNS browser : browsers) {
				browser.removeServiceListener(type + domain, this);
			}

		}

		private void close() throws IOException {

			Iterator<Map.Entry<String, DiscoveryListener>> it = callbacks.entrySet().iterator();
			while (it.hasNext()) {
				Map.Entry<String, DiscoveryListener> pair = it.next();

				DiscoveryListener listener = pair.getValue();
				listener.onDiscoveryStopped(pair.getKey());
			}
			callbacks.clear();

			for (JmDNS browser : browsers) {
				browser.close();
			}

		}

		@Override
		public void serviceResolved(ServiceEvent ev) {
			Log.d(TAG, "Resolved");

//			sendCallback("resolved", ev.getInfo());
			DiscoveryListener listener = callbacks.get(ev.getInfo().getType());
			listener.onServiceResolved(ev.getInfo());
		}

		@Override
		public void serviceRemoved(ServiceEvent ev) {
			Log.d(TAG, "Removed");

//			sendCallback("removed", ev.getInfo());
			DiscoveryListener listener = callbacks.get(ev.getInfo().getType());
			listener.onServiceLost(ev.getInfo());
		}

		@Override
		public void serviceAdded(ServiceEvent ev) {
			Log.d(TAG, "Added");
			DiscoveryListener listener = callbacks.get(ev.getInfo().getType());
			listener.onServiceFound(ev.getInfo());
		}

	}

	@Override
	public void onCatalystInstanceDestroy() {
		super.onCatalystInstanceDestroy();
		stop();
	}

	private static JSONObject jsonifyService(ServiceInfo service) throws JSONException {
		JSONObject obj = new JSONObject();

		String domain = service.getDomain() + ".";
		obj.put("domain", domain);
		obj.put("type", service.getType().replace(domain, ""));
		obj.put("name", service.getName());
		obj.put("port", service.getPort());
		obj.put("hostname", service.getServer());

		JSONArray ipv4Addresses = new JSONArray();
		InetAddress[] inet4Addresses = service.getInet4Addresses();
		for (int i = 0; i < inet4Addresses.length; i++) {
			if (inet4Addresses[i] != null) {
				ipv4Addresses.put(inet4Addresses[i].getHostAddress());
			}
		}
		obj.put("ipv4Addresses", ipv4Addresses);

		JSONArray ipv6Addresses = new JSONArray();
		InetAddress[] inet6Addresses = service.getInet6Addresses();
		for (int i = 0; i < inet6Addresses.length; i++) {
			if (inet6Addresses[i] != null) {
				ipv6Addresses.put(inet6Addresses[i].getHostAddress());
			}
		}
		obj.put("ipv6Addresses", ipv6Addresses);

		JSONObject props = new JSONObject();
		Enumeration<String> names = service.getPropertyNames();
		while (names.hasMoreElements()) {
			String name = names.nextElement();
			props.put(name, service.getPropertyString(name));
		}
		obj.put("txt", props);

		return obj;

	}

	private static WritableMap convertJsonToMap(JSONObject jsonObject) throws JSONException {
		WritableMap map = Arguments.createMap();

		Iterator<String> iterator = jsonObject.keys();
		while (iterator.hasNext()) {
			String key = iterator.next();
			Object value = jsonObject.get(key);
			if (value instanceof JSONObject) {
				map.putMap(key, convertJsonToMap((JSONObject) value));
			} else if (value instanceof JSONArray) {
				map.putArray(key, convertJsonToArray((JSONArray) value));
			} else if (value instanceof Boolean) {
				map.putBoolean(key, (Boolean) value);
			} else if (value instanceof Integer) {
				map.putInt(key, (Integer) value);
			} else if (value instanceof Double) {
				map.putDouble(key, (Double) value);
			} else if (value instanceof String) {
				map.putString(key, (String) value);
			} else {
				map.putString(key, value.toString());
			}
		}
		return map;
	}

	private static WritableArray convertJsonToArray(JSONArray jsonArray) throws JSONException {
		WritableArray array = new WritableNativeArray();

		for (int i = 0; i < jsonArray.length(); i++) {
			Object value = jsonArray.get(i);
			if (value instanceof JSONObject) {
				array.pushMap(convertJsonToMap((JSONObject) value));
			} else if (value instanceof JSONArray) {
				array.pushArray(convertJsonToArray((JSONArray) value));
			} else if (value instanceof Boolean) {
				array.pushBoolean((Boolean) value);
			} else if (value instanceof Integer) {
				array.pushInt((Integer) value);
			} else if (value instanceof Double) {
				array.pushDouble((Double) value);
			} else if (value instanceof String) {
				array.pushString((String) value);
			} else {
				array.pushString(value.toString());
			}
		}
		return array;
	}

	// http://stackoverflow.com/questions/21898456/get-android-wifi-net-hostname-from-code
	public static String getHostName() throws NoSuchMethodException, SecurityException, IllegalAccessException,
			IllegalArgumentException, InvocationTargetException {
		Method getString = Build.class.getDeclaredMethod("getString", String.class);
		getString.setAccessible(true);
		return getString.invoke(null, "net.hostname").toString();
	}
}
