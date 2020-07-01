package com.elfin.Boat;

import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetSocketAddress;
import java.net.Socket;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.List;

import android.content.SharedPreferences;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.net.wifi.WifiConfiguration;
import android.net.wifi.WifiInfo;
import android.net.wifi.WifiManager;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.util.Log;

/**
 * This class represents network queue.
 * 
 * @author Rafal Vonau <rafal.vonau@elfin-pe.pl>
 */
public class NetworkQueue 
{
	public class NetworkCMD
	{
		public Integer val0;
		public Integer val1;
		public boolean dir;
		public Integer light;

		public NetworkCMD(int val0, int val1, boolean dir, int light) 
		{
			this.val0 = val0;
			this.val1 = val1;
			this.dir = dir;
			this.light = light;
		}

		public Integer getVal0()
		{
			return this.val0;
		}

		public Integer getVal1() 
		{
			return this.val1;
		}
	}
	private LinkedList<NetworkCMD> cmds;
	private Thread thread;
	private boolean running;
	private boolean wifictl;
	private Runnable internalRunnable;

	private SharedPreferences prefs;
	private WifiManager wifiManager;
	private ConnectivityManager connectivityManager;

	private Handler handler;

	private final String ME = "NetworkQueue";

	/**
	 * Log text message and send it to parent (by handler).
	 * 
	 * @param s
	 *            - text message
	 */
	public void logMSG(String s) 
	{
		Log.d(ME, s);
		Message msg = Message.obtain(this.handler);
		Bundle bundle = new Bundle();
		bundle.putString("T", s);
		msg.setData(bundle);
		msg.what = 0;
		this.handler.sendMessage(msg);
	}

	public void logResult(String s) 
	{
		Log.d(ME, s);
		Message msg = Message.obtain(this.handler);
		Bundle bundle = new Bundle();
		bundle.putString("R", s);
		msg.setData(bundle);
		msg.what = 1;
		this.handler.sendMessage(msg);
	}

	/**
	 * Send result to parent.
	 * 
	 * @param id
	 *            - result type (id),
	 * @param s
	 *            - text.
	 */
	public void sendResult(int id, String s) 
	{
		Message msg = Message.obtain(this.handler);
		Bundle bundle = new Bundle();
		bundle.putString("R", s);
		msg.setData(bundle);
		msg.what = id;
		this.handler.sendMessage(msg);
	}
	
	private class InternalRunnable implements Runnable 
	{
		public void run() {
			internalRun();
		}
	}

	/**
	 * Constructor.
	 * 
	 * @param prefs
	 * @param handler
	 * @param wifiManager
	 * @param connectivityManager
	 */
	public NetworkQueue(SharedPreferences prefs, Handler handler,WifiManager wifiManager, ConnectivityManager connectivityManager) 
	{
		this.handler = handler;
		this.wifiManager = wifiManager;
		this.connectivityManager = connectivityManager;
		cmds = new LinkedList<NetworkCMD>();
		internalRunnable = new InternalRunnable();
		this.prefs = prefs;
		running = false;
	}

	/**
	 * Start thread.
	 */
	public void start() 
	{
		Log.d(ME, "THREAD Start");
		if (!running) {
			thread = new Thread(internalRunnable);
			thread.setDaemon(true);
			running = true;
			thread.start();
		}
	}

	/**
	 * Stop thread.
	 */
	public void stop() 
	{
		Log.d(ME, "THREAD Stop");
		running = false;
		synchronized (cmds) {
			while (cmds.isEmpty() == false) {
				cmds.removeFirst();
			}
			cmds.addLast(new NetworkCMD(0, 512, false, 0));
			cmds.notify(); // notify any waiting threads
		}
	}

	public void sendValues(Integer val0, Integer val1, boolean dir,Integer light) 
	{
		synchronized (cmds) {
			if (cmds.isEmpty() == false) {
				cmds.removeFirst();
			}
			cmds.addLast(new NetworkCMD(val0, val1, dir, light));
			cmds.notify(); // notify any waiting threads
		}
	}

	/**
	 * Get next command from network queue.
	 * 
	 * @return Command.
	 */
	private NetworkCMD getNextCMD() 
	{
		//Log.d(ME, "getNextCMD");
		synchronized (cmds) {
			if (cmds.isEmpty()) {
				try {
					cmds.wait();
				} catch (InterruptedException e) {
					Log.e(ME, "Task interrupted", e);
					stop();
				}
			}
			return cmds.removeFirst();
		}
	}

	/**
	 * Is WiFi connected ??
	 * 
	 * @return
	 */
	public boolean isConnected()
	{
		NetworkInfo networkInfo = null;
		if (this.connectivityManager != null) {
			networkInfo = this.connectivityManager.getActiveNetworkInfo();
		}
		return networkInfo == null ? false: networkInfo.getState() == NetworkInfo.State.CONNECTED;
	}

	/**
	 * Check WiFi state and enable WiFi when needed.
	 * 
	 * @return
	 */
	public boolean checkWiFi() 
	{
		int i = 60;
		//int gotIt = 0;
		if (!this.wifiManager.isWifiEnabled()) {
			if (this.prefs.getBoolean("enableWifi", true)) {
				Log.d(ME, "Enable WiFi");
				this.wifiManager.setWifiEnabled(true);
				this.wifictl = true;
				while ((!isConnected()) && ((i--) > 0)) {
					// Wait to connect
					Log.d(ME, "Wait for wifi i = " + i);
					try {
						Thread.sleep(1000);
						/*
						if ( gotIt == 0 ) {
							int netId = -1;
							List<WifiConfiguration> configs = this.wifiManager.getConfiguredNetworks();
							for (WifiConfiguration config : configs) {
								Log.d(ME, "wifi SSID = " + config.SSID.toString());
								if (config.SSID.toString().equals("\"SPACE\"")) {
									netId = config.networkId; 
								}
							}
							if (netId > -1) {
								Log.d(ME, "Enable wifi netID = " + netId);
								this.wifiManager.enableNetwork(netId, true);
								gotIt = 1;
							}
						}
						*/
					} catch (InterruptedException e) {
						// TODO Auto-generated catch block
						e.printStackTrace();
					}
				}
			}
		}
		
		return true;
	}

	/**
	 * Working thread.
	 */
	private void internalRun() 
	{
		this.wifictl = false;
		byte[] lMsg = new byte[4096];
		DatagramPacket dp = new DatagramPacket(lMsg, lMsg.length);
		DatagramPacket dps = null;
		DatagramSocket ds = null;
		InetSocketAddress espaddr = new InetSocketAddress("192.168.1.25", 28800);
		Integer len;
		int l, dir;
		int boatType = -1;
		/* Create datagram socket. */
		
		try {
			ds = new DatagramSocket(null);
			ds.setReuseAddress(true);
			ds.bind(new InetSocketAddress(28800));
			ds.setSoTimeout(1);
		} catch (Throwable t) {
			Log.e(ME, "Task threw an exception", t);
			return;
		}
		
		// Check WiFi
		if (checkWiFi()) {
			// connectToHost();
			String ssid = null;
			WifiInfo wifiInfo = this.wifiManager.getConnectionInfo();
			if ( wifiInfo.getNetworkId() != -1 ) {
				ssid = wifiInfo.getSSID();
				Log.d(ME, "WiFi Connected to "+ssid);
			}
			if ((ssid != null) && (ssid.contains("myboat"))) {
			} else {
				int found = 0;
				int found1 = 0;
				int found3 = 0;
				int netId = 0;
				
				Log.d(ME, "WiFi - force connect to myboat :-)");
				List<WifiConfiguration> networks = this.wifiManager.getConfiguredNetworks();
				Iterator<WifiConfiguration> iterator = networks.iterator();
				while (iterator.hasNext()) {
					WifiConfiguration wifiConfig = iterator.next();
					if (wifiConfig.SSID.contains("myboat1")) {
						found1 = 1;
						this.wifiManager.enableNetwork(wifiConfig.networkId, true);
						netId = wifiConfig.networkId;
					} else if (wifiConfig.SSID.contains("myboat3")) {
						found3 = 1;
						this.wifiManager.enableNetwork(wifiConfig.networkId, true);
						netId = wifiConfig.networkId;
					} else if (wifiConfig.SSID.contains("myboat")) {
						found = 1;
						this.wifiManager.enableNetwork(wifiConfig.networkId, true);
						netId = wifiConfig.networkId;
					} else {
						this.wifiManager.disableNetwork(wifiConfig.networkId);
					}
				}
				//remember id
				if (found == 0) {
					WifiConfiguration conf = new WifiConfiguration();
					conf.SSID = "\"myboat\"";   // Please note the quotes. String should contain ssid in quotes
					conf.status = WifiConfiguration.Status.ENABLED;
					conf.priority = 100000;
					conf.allowedProtocols.set(WifiConfiguration.Protocol.RSN);
					conf.allowedProtocols.set(WifiConfiguration.Protocol.WPA);
					conf.allowedKeyManagement.set(WifiConfiguration.KeyMgmt.WPA_PSK);
					conf.allowedPairwiseCiphers.set(WifiConfiguration.PairwiseCipher.CCMP);
					conf.allowedPairwiseCiphers.set(WifiConfiguration.PairwiseCipher.TKIP);
					conf.allowedGroupCiphers.set(WifiConfiguration.GroupCipher.WEP40);
					conf.allowedGroupCiphers.set(WifiConfiguration.GroupCipher.WEP104);
					conf.allowedGroupCiphers.set(WifiConfiguration.GroupCipher.CCMP);
					conf.allowedGroupCiphers.set(WifiConfiguration.GroupCipher.TKIP);
					conf.preSharedKey = "\"rctymek123\"";
					netId = this.wifiManager.addNetwork(conf);
					this.wifiManager.enableNetwork(netId, true);
				}
				if (found1 == 0) {
					WifiConfiguration conf = new WifiConfiguration();
					conf.SSID = "\"myboat1\"";   // Please note the quotes. String should contain ssid in quotes
					conf.status = WifiConfiguration.Status.ENABLED;
					conf.priority = 100000;
					conf.allowedProtocols.set(WifiConfiguration.Protocol.RSN);
					conf.allowedProtocols.set(WifiConfiguration.Protocol.WPA);
					conf.allowedKeyManagement.set(WifiConfiguration.KeyMgmt.WPA_PSK);
					conf.allowedPairwiseCiphers.set(WifiConfiguration.PairwiseCipher.CCMP);
					conf.allowedPairwiseCiphers.set(WifiConfiguration.PairwiseCipher.TKIP);
					conf.allowedGroupCiphers.set(WifiConfiguration.GroupCipher.WEP40);
					conf.allowedGroupCiphers.set(WifiConfiguration.GroupCipher.WEP104);
					conf.allowedGroupCiphers.set(WifiConfiguration.GroupCipher.CCMP);
					conf.allowedGroupCiphers.set(WifiConfiguration.GroupCipher.TKIP);
					conf.preSharedKey = "\"rctymek123\"";
					netId = this.wifiManager.addNetwork(conf);
					this.wifiManager.enableNetwork(netId, true);
				}
				if (found3 == 0) {
					WifiConfiguration conf = new WifiConfiguration();
					conf.SSID = "\"myboat3\"";   // Please note the quotes. String should contain ssid in quotes
					conf.status = WifiConfiguration.Status.ENABLED;
					conf.priority = 100000;
					conf.allowedProtocols.set(WifiConfiguration.Protocol.RSN);
					conf.allowedProtocols.set(WifiConfiguration.Protocol.WPA);
					conf.allowedKeyManagement.set(WifiConfiguration.KeyMgmt.WPA_PSK);
					conf.allowedPairwiseCiphers.set(WifiConfiguration.PairwiseCipher.CCMP);
					conf.allowedPairwiseCiphers.set(WifiConfiguration.PairwiseCipher.TKIP);
					conf.allowedGroupCiphers.set(WifiConfiguration.GroupCipher.WEP40);
					conf.allowedGroupCiphers.set(WifiConfiguration.GroupCipher.WEP104);
					conf.allowedGroupCiphers.set(WifiConfiguration.GroupCipher.CCMP);
					conf.allowedGroupCiphers.set(WifiConfiguration.GroupCipher.TKIP);
					conf.preSharedKey = "\"rctymek123\"";
					netId = this.wifiManager.addNetwork(conf);
					this.wifiManager.enableNetwork(netId, true);
				}
				this.wifiManager.disconnect();
				try {
					Thread.sleep(1000);
				} catch (Throwable t) {
					Log.e(ME, "Task threw an exception", t);
				}
				//this.wifiManager.enableNetwork(netId, true);
				//this.wifiManager.saveConfiguration();
				this.wifiManager.reconnect();
			}
		}
		while (running) {
			String ssid = null;
			WifiInfo wifiInfo = this.wifiManager.getConnectionInfo();
			if ( wifiInfo.getNetworkId() != -1 ) {
				ssid = wifiInfo.getSSID();
				Log.d(ME, "WiFi Connected to "+ssid);
			}			
			if ((ssid != null) && (ssid.contains("myboat"))) break;
			try {
				Thread.sleep(1000);
			} catch (Throwable t) {
				Log.e(ME, "Task threw an exception", t);
			}
		}
		while (running) {
			// Get command from network queue
			NetworkCMD cmd = getNextCMD();
			// Execute command
			try {
				if (cmd.dir) dir = 1; else dir = 0;
				/* Send UDP packet */
				switch (boatType) {
					case 1: {
						/* Typ 1 - 1xPWM, 1XServo, 1xdir, 1xlight */
						String requestmsg = "cp0 "+Integer.toString(cmd.getVal0()) + "\nch0 " + Integer.toString(1000 + ((cmd.getVal1()*1000)/1023)) + "\ncd0 " + Integer.toString(dir) + "\ncl0 " + Integer.toString(cmd.light) + "\n";
						dps = new DatagramPacket(requestmsg.getBytes(), requestmsg.length(), espaddr);
						ds.setBroadcast(true);
						ds.send(dps);
					} break;
					case 2: {
						/* Typ 2 - 2xPWM (cx0 - Motor speed (0-1023), cx1 - rotation (0-2000) , 1xdir, 1xlight */
						String requestmsg = "cx0 "+Integer.toString(cmd.getVal0()) + "\ncx1 " + Integer.toString((cmd.getVal1()*2000)/1023) + "\ncd0 " + Integer.toString(dir) + "\ncl0 " + Integer.toString(cmd.light) + "\n";
						dps = new DatagramPacket(requestmsg.getBytes(), requestmsg.length(), espaddr);
						ds.setBroadcast(true);
						ds.send(dps);
					} break;
					case 3: {
						/* Typ 3 - 2xServo (Speed limited to 50%) (ch0 - Motor speed (1000-1500), ch2 - rotation (1000-2000) , 1xdir, 1xlight */
						String requestmsg = "ch0 "+Integer.toString(1000 + ((cmd.getVal0()*500)/1023)) + "\nch1 " + Integer.toString(1000 + (1000 - ((cmd.getVal1()*1000)/1023))) + "\ncd0 " + Integer.toString(dir) + "\ncl0 " + Integer.toString(cmd.light) + "\n";
						dps = new DatagramPacket(requestmsg.getBytes(), requestmsg.length(), espaddr);
						ds.setBroadcast(true);
						ds.send(dps);
					} break;
					default:break;
				}
			} catch (Throwable t) {
				Log.e(ME, "Task threw an exception", t);
			}
			try {
				/* Receive data */
				ds.receive(dp);
				len = dp.getLength();
				if (len > 0) {
					String s="";
					lMsg[len] = '\0';
					/* TODO: Analize data lines */
					Log.d(ME, "Got message "+ new String(lMsg, 0, len));
					l = 0;
					for (l = 0; l < len; ++l) {
						if (lMsg[l] =='\n') {
							if (s.startsWith("bat")) {
								sendResult(2, s.substring(4));
							}
							if (s.startsWith("boattype")) {
								String k = s.substring(9);
								int bt = Integer.parseInt( k );
								if (bt !=boatType ) {
									Log.d(ME, "Got new boat type: "+ k);
									boatType = bt;
									sendResult(3, k);
								}
							}
							if (s.startsWith("volt")) {
								sendResult(4, s.substring(5));
							}
							//Log.d(ME, "Got line "+ s);
							s="";
						} else {
							s+=(char)lMsg[l];
						}
					}
				}
				Thread.yield();
			} catch (Throwable t) {
				//Log.e(ME, "Task threw an exception", t);
			}
		}
		/* Shut down WiFi */
		if (this.wifiManager.isWifiEnabled()) { 
			if ((this.wifictl) && (this.prefs.getBoolean("disableWifi", false))) {
				Log.d(ME, "Disable WiFi");
				this.wifiManager.setWifiEnabled( false ); 
			}
		}
	}
}
