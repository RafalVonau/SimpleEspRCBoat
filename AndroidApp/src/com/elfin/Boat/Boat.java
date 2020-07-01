package com.elfin.Boat;

import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.ScheduledFuture;
import java.util.concurrent.TimeUnit;

import android.app.Activity;
import android.app.Dialog;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.SharedPreferences.OnSharedPreferenceChangeListener;
import android.graphics.Color;
import android.graphics.Rect;
import android.net.ConnectivityManager;
import android.net.wifi.WifiManager;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.os.PowerManager;
import android.os.PowerManager.WakeLock;
import android.preference.PreferenceManager;
import android.util.Log;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.MotionEvent;
import android.widget.Toast;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;

public class Boat extends Activity  implements
OnSharedPreferenceChangeListener, SensorEventListener {
	private static final int SETTINGS_ID = Menu.FIRST;
	private SharedPreferences prefs;
	private WifiManager wifiManager;
	private NetworkQueue nq;
	private Handler handler;
	private ConnectivityManager connectivityManager;
	private static final String WAKE_LOCK_TAG = "rcboat";
	private WakeLock mWakeLock = null;
	private ScheduledExecutorService sservice;
	private Runnable notifyTask;
	private ScheduledFuture<?> notifyTaskHandle;
	private boolean calibrationActive;
	private boolean accActive;
	private boolean dirActive;
	private boolean lightActive;
	private Integer zero_offset;
	private Integer boatID;
	private SensorManager sManager;
	private float mGravity[] = new float[3];
	private float mGeomagnetic[] = new float[3];
	private VerticalTextView vtext1;
	private VerticalTextView vtext2;
	static final int TIME_DIALOG_ID = 999;
	ProgressImageWievH vSeekBar1;
	ProgressImageWiev vSeekBar2;
	ProgressImageWiev batBar;
	DirButton dirButton;
	String TAG="Boat";
	private Integer speedFingerId;
	private Integer angleFingerId;
	private Integer dirFingerId;
	
	@Override
	public void onCreate(Bundle savedInstanceState) 
	{
		super.onCreate(savedInstanceState);
		this.calibrationActive = false;
		this.accActive = false;
		this.lightActive = false;
		
		this.prefs = PreferenceManager.getDefaultSharedPreferences(this);
		this.prefs.registerOnSharedPreferenceChangeListener(this);
		
		final PowerManager powerManager = (PowerManager) getSystemService(POWER_SERVICE);
		mWakeLock = powerManager.newWakeLock(PowerManager.SCREEN_DIM_WAKE_LOCK, WAKE_LOCK_TAG);
		this.zero_offset = 0;
		this.boatID = -1;
		this.wifiManager = (WifiManager)this.getSystemService(Context.WIFI_SERVICE);
		this.handler = new Handler() {
			@Override
			public void handleMessage(Message msg) {
				switch (msg.what) {
					case 0: { Toast.makeText(getApplicationContext(), msg.getData().get("T").toString(), Toast.LENGTH_LONG).show(); } break;
					case 2: { 						
						String val = msg.getData().get("R").toString();
						Integer vali = Integer.parseInt(val);
						batBar.setLevel(vali);
						if (vali > 15) {
							batBar.setColor(Color.GREEN);
							vtext1.setTextColor(Color.WHITE);
							vtext2.setTextColor(Color.WHITE);
						} else {
							batBar.setColor(Color.RED);
							vtext1.setTextColor(Color.RED);	
							vtext2.setTextColor(Color.RED);
						}
						vtext1.setText(val+"%");						
					} break;
					case 3: { 
						Integer ID = Integer.parseInt(msg.getData().get("R").toString());
						if (ID != boatID) {
							boatID = ID;
							zero_offset = prefs.getInt("zeroOffset"+Integer.toString(boatID), 0);
						}
					} break;
					case 4: { 
						String val = msg.getData().get("R").toString();
						vtext2.setText(val+"mV");						
					} break;
					default:break;
				}
			}
		};
		this.connectivityManager = (ConnectivityManager)getSystemService(Context.CONNECTIVITY_SERVICE);		
		setContentView(R.layout.main);
		//tvDisplayTime = (TextView)findViewById(R.id.textView);

		this.nq = new NetworkQueue(this.prefs, this.handler, this.wifiManager, this.connectivityManager);
		this.nq.start();
		
		vSeekBar1 = (ProgressImageWievH)findViewById(R.id.touchValue1);
		vSeekBar1.setMaximalValue(1023);
		vSeekBar1.setLevel(0);

		vSeekBar2 = (ProgressImageWiev)findViewById(R.id.touchValue2);
		vSeekBar2.setMaximalValue(1023);
		vSeekBar2.setLevel(512);
		
		batBar = (ProgressImageWiev)findViewById(R.id.progressImageWiev1);
		batBar.setMaximalValue(100);
		batBar.setLevel(0);
		batBar.setColor(Color.GREEN);
		
		vtext1 = (VerticalTextView)findViewById(R.id.textView1);
		vtext1.setText("?");
		vtext2 = (VerticalTextView)findViewById(R.id.textView2);
		vtext2.setText("?");
		
		dirButton = (DirButton)findViewById(R.id.dirButton1);
		dirButton.setColor(Color.YELLOW);
		dirActive = false;
		
		this.calibrationActive = false;
		this.accActive = false;
		this.sManager = (SensorManager)getSystemService(SENSOR_SERVICE);
		this.zero_offset = prefs.getInt("zeroOffset"+Integer.toString(boatID), 0);
		this.speedFingerId = -1;
		this.angleFingerId = -1;	
		this.dirFingerId = -1;
	}

	public void execPeriodicSend() {
		this.sservice = Executors.newScheduledThreadPool(1);
		this.notifyTask = new Runnable() { @Override public void run() {
			if (calibrationActive) {
				/* Jestesmy w kalibracji :-) */
				//Log.d(TAG, "Kalibracja: "+ Integer.toString(512+zero_offset));
				nq.sendValues(vSeekBar1.getLevel(), 512+zero_offset, dirActive, lightActive?1:0);
			} else {
				/* Normalna praca */
				//Log.d(TAG, "Praca: "+ Integer.toString(vSeekBar2.getLevel()+zero_offset));
				nq.sendValues(vSeekBar1.getLevel(), vSeekBar2.getLevel()+zero_offset, dirActive, lightActive?1:0);
			}			
		}};
		this.notifyTaskHandle = this.sservice.scheduleWithFixedDelay(this.notifyTask, 20, 20, TimeUnit.MILLISECONDS); 
	}
	
	@Override
	public boolean dispatchTouchEvent (MotionEvent ev)
	{
		Rect sb1Rect = new Rect();
		Rect sb2Rect = new Rect();
		Rect dirRect = new Rect();
		vSeekBar1.getGlobalVisibleRect(sb1Rect);
		vSeekBar2.getGlobalVisibleRect(sb2Rect);
		dirButton.getGlobalVisibleRect(dirRect);
		int th = sb1Rect.bottom + ((sb2Rect.top - sb1Rect.bottom)/2); 
		int i, pId;		
		
		if (accActive == true) {
			for (i = 0; i < ev.getPointerCount(); i++) {
				int coordX = (int) ev.getX(i);
				int coordY = (int) ev.getY(i);
				switch (ev.getActionMasked()) {
			    	case MotionEvent.ACTION_DOWN:
			    	case MotionEvent.ACTION_POINTER_DOWN:
						if (dirRect.contains(coordX, coordY)) {
							if (dirActive) {
								dirActive = false;
								dirButton.setColor(Color.YELLOW);
								vSeekBar1.setColor(Color.YELLOW);
							} else {
								dirActive = true;
								dirButton.setColor(Color.RED);
								vSeekBar1.setColor(Color.RED);
							}
						}
					default:
			        break;
			    }
			}
	    	angleFingerId = -1;
	    	speedFingerId = -1;
	    	dirFingerId = -1;
			return true;
		}

		switch (ev.getActionMasked()) {
    	case MotionEvent.ACTION_DOWN:
    	case MotionEvent.ACTION_POINTER_DOWN:		    		
			i = ev.getActionIndex();
			pId = ev.getPointerId(i);
			int coordX = (int) ev.getX(i);
			int coordY = (int) ev.getY(i);
			if (dirRect.contains(coordX, coordY)) {
    			if (dirFingerId == -1) {
    				dirFingerId = pId;
    				if (dirActive) {
    					dirActive = false;
    					dirButton.setColor(Color.YELLOW);
    					vSeekBar1.setColor(Color.YELLOW);
    				} else {
    					dirActive = true;
    					dirButton.setColor(Color.RED);
    					vSeekBar1.setColor(Color.RED);
    				}
    				//vSeekBar1.setLevel(0);
    			}
			} else if (coordY < th) {
    			if (speedFingerId == -1) {
    				speedFingerId = pId;	
    			}
    		} else {
    			if (angleFingerId == -1) {
    				angleFingerId = pId;
    			}
    		}	
		break;
    		
    	case MotionEvent.ACTION_UP: // if action up
    		angleFingerId = -1;
    		speedFingerId = -1;
    		dirFingerId = -1;
    	break;
    	case MotionEvent.ACTION_POINTER_UP:
    		i = ev.getActionIndex();
    		pId = ev.getPointerId(i);
    		if (angleFingerId == pId) angleFingerId = -1;
    		if (speedFingerId == pId) speedFingerId = -1;
    		if (dirFingerId == pId) dirFingerId = -1;    	
    		break;
    	default:break;
		}

		for (i = 0; i < ev.getPointerCount(); i++) {
			pId = ev.getPointerId(i);
			if (pId == speedFingerId) {
				int coordX = (int) ev.getX(i);
	        	vSeekBar1.setLevel((coordX-sb1Rect.left)*1023/sb1Rect.width());
	        } else if (pId == angleFingerId) {
	        	int coordY = (int) ev.getY(i);
  				vSeekBar2.setLevel((sb2Rect.bottom - coordY)*1023/sb2Rect.height());
	        }
		}
		if (angleFingerId == -1) {
			if (calibrationActive == false) {
				vSeekBar2.setLevel(512);
			}
		}
		if (calibrationActive == true) {
			zero_offset = ((vSeekBar2.getLevel() - 512)/10);
		}		
		//AbsoluteLayout l =(AbsoluteLayout) this.findViewById(R.id.AbsoluteLayout1);
		//l.dispatchTouchEvent(ev);
		//super.dispatchTouchEvent(ev);
		return true;
	}

	public void onAccuracyChanged(Sensor sensor, int accuracy) {
	}
 
	public void onSensorChanged(SensorEvent event) {
		if (event.sensor.getType() == Sensor.TYPE_ACCELEROMETER)
			mGravity = event.values.clone();
		if (event.sensor.getType() == Sensor.TYPE_MAGNETIC_FIELD)
			mGeomagnetic = event.values.clone();
		if (mGravity != null && mGeomagnetic != null) {
			float R[] = new float[9];
			float Rout[] = new float[9];
			float I[] = new float[9];
			boolean success = SensorManager.getRotationMatrix(R, I, mGravity, mGeomagnetic);
			if (success) {
				//SensorManager.remapCoordinateSystem(R, SensorManager.AXIS_X,SensorManager.AXIS_Z, Rout);
				float orientation[] = new float[3];
				SensorManager.getOrientation(R, orientation);
				//float azimut = orientation[0]; // orientation contains: azimut, pitch and roll
				float pitch = orientation[1];
				float roll = orientation[2];
				/* Set seekbar value */
				if (accActive) {
					vSeekBar1.setLevel(1023 + Math.round((roll * 1023.0f)/1.2f));
					vSeekBar2.setLevel(512 + Math.round((pitch * 511.0f)/1.2f));
				}
			}
		}
	}

	@Override
	public void onPause() {
        if (this.notifyTaskHandle != null) {
        	this.notifyTaskHandle.cancel(true);
        	this.notifyTaskHandle = null;
        	this.sservice = null;
        }
		sManager.unregisterListener(this);
		this.nq.stop();
		mWakeLock.release();
		super.onPause();
	}
	
	@Override
	public void onResume() {
		this.nq.start();
		mWakeLock.acquire();		
		execPeriodicSend(); 
		if (accActive) {
			sManager.registerListener(this, sManager.getDefaultSensor(Sensor.TYPE_ACCELEROMETER),SensorManager.SENSOR_DELAY_GAME);
			sManager.registerListener(this, sManager.getDefaultSensor(Sensor.TYPE_MAGNETIC_FIELD),SensorManager.SENSOR_DELAY_GAME);
			//Manager.registerListener(this, sManager.getDefaultSensor(Sensor.TYPE_GAME_ROTATION_VECTOR),SensorManager.SENSOR_DELAY_GAME);
		}
		super.onResume();  // Always call the superclass method first
	}
	@Override
	public boolean onCreateOptionsMenu(Menu menu) {
		super.onCreateOptionsMenu(menu);
		new MenuInflater(this).inflate(R.menu.main, menu);
		//menu.add(Menu.NONE, SETTINGS_ID, Menu.NONE, R.string.menu_settings).setIcon(android.R.drawable.ic_menu_preferences);
		return true;
	}
	
	// Don't display the share menu item if the result overlay is showing.
	@Override
	public boolean onPrepareOptionsMenu(Menu menu) {
		super.onPrepareOptionsMenu(menu);
		MenuItem checkable = menu.findItem(R.id.calibrate_menu);
		checkable.setChecked(calibrationActive);
		checkable = menu.findItem(R.id.acc_menu);
		checkable.setChecked(accActive);
		checkable = menu.findItem(R.id.light_menu);
		checkable.setChecked(lightActive);
		return true;
	}

	/**
	 * Handle Options Menu button press.
	 */
	@Override
	public boolean onOptionsItemSelected(MenuItem item) {
		switch (item.getItemId()) {
			case R.id.menu_settings: {
				Intent intent = new Intent(Intent.ACTION_VIEW);
				intent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_WHEN_TASK_RESET);
				intent.setClassName(this, PreferencesActivity.class.getName());
				startActivity(intent);
			} break;
			case R.id.calibrate_menu: {
				/* Zapisz zero_offset */
				if (item.isChecked()) {										
					SharedPreferences.Editor editor = prefs.edit();
					editor.putInt("zeroOffset"+Integer.toString(boatID), zero_offset);
					editor.commit();
					Toast.makeText(getApplicationContext(), "Zapis offsetu = " + Integer.toString(zero_offset), Toast.LENGTH_LONG).show();
					calibrationActive = false;
					vSeekBar2.setLevel( 512 );					
				} else {					
					zero_offset = 0;
					vSeekBar2.setLevel( 512 );
					calibrationActive = true;
				}
				calibrationActive = !item.isChecked();
				item.setChecked(calibrationActive);
			} break;				
			case R.id.acc_menu: {
				accActive = !item.isChecked();
				if (accActive) {
					sManager.registerListener(this, sManager.getDefaultSensor(Sensor.TYPE_ACCELEROMETER),SensorManager.SENSOR_DELAY_GAME);
					sManager.registerListener(this, sManager.getDefaultSensor(Sensor.TYPE_MAGNETIC_FIELD),SensorManager.SENSOR_DELAY_GAME);
					//sManager.registerListener(this, sManager.getDefaultSensor(Sensor.TYPE_GAME_ROTATION_VECTOR),SensorManager.SENSOR_DELAY_GAME);
				} else {
					sManager.unregisterListener(this);
					vSeekBar1.setLevel(0);
					vSeekBar2.setLevel(512);					
				}
				item.setChecked(accActive);
			} break;
			case R.id.light_menu: {
				lightActive = !item.isChecked();
				item.setChecked(lightActive);
			} break;
			default:
				return super.onOptionsItemSelected(item);
		}
		return true;
	}

	@Override
	protected Dialog onCreateDialog(int id) {
		switch (id) {
		case TIME_DIALOG_ID:
			// set time picker as current time
			//return new TimePickerDialog(this, timePickerListener, hour, minute,false);
		}
		return null;
	}

	public synchronized void onSharedPreferenceChanged(SharedPreferences sharedPreferences, String key) { //
		//if (key.equals("espIP")) {
		//	this.nq.setServer( prefs.getString("espIP", "192.168.1.63"), 80 );
		//}
	}
}