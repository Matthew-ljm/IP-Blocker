package com.matthew.ipblocker.interceptor.activities;

import androidx.appcompat.app.AppCompatActivity;

import android.content.Intent;
import android.content.SharedPreferences;

import android.widget.Toast;
import android.widget.Button;
import android.widget.EditText;
import android.view.View;

import android.os.Bundle;

import com.matthew.ipblocker.R;
import com.matthew.ipblocker.interceptor.services.TunModeService;

public class MainActivity extends AppCompatActivity implements TunModeService.EventListener {

	private Button toggle_button;
	private Button save_ips_button;
	private EditText blocked_ips_edittext;

	private static final String PREFS_NAME = "TunModePrefs";
	private static final String BLOCKED_IPS_KEY = "blocked_ips";
	private static final String DEFAULT_BLOCKED_IPS = "192.168.0.102";

	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		setContentView(R.layout.activity_main);

		this.toggle_button = this.findViewById(R.id.tun_switch_button);
		this.save_ips_button = this.findViewById(R.id.save_ips_button);
		this.blocked_ips_edittext = this.findViewById(R.id.blocked_ips_edittext);

		// 加载保存的拦截IP列表
		loadBlockedIPs();

		this.toggle_button.setOnClickListener((View v) -> {
			TunModeService.State state = TunModeService.getState();
			if (state.equals(TunModeService.State.CONNECTED)) {
				MainActivity.this.startTunMode(TunModeService.Operation.DISCONNECT);
			} else if (state.equals(TunModeService.State.DISCONNECTED)) {
				MainActivity.this.startTunMode(TunModeService.Operation.CONNECT);
				// 【开启VPN时传递拦截列表】
				setBlockedIPsToNative();
			} else {
				Toast.makeText(this, "Try Again", Toast.LENGTH_SHORT).show();
			}
		});

		this.save_ips_button.setOnClickListener((View v) -> {
			saveBlockedIPs();
			// 【保存时直接传递给Native层】
			setBlockedIPsToNative();
			Toast.makeText(this, "拦截列表已保存", Toast.LENGTH_SHORT).show();
		});

		TunModeService.setEventListener(this);
		TunModeService.setActivity(this);

		this.startTunMode(TunModeService.Operation.INITIALIZE);
	}

	@Override
	protected void onResume() {
		super.onResume();
		this.syncButtonText();
	}

	@Override
	protected void onDestroy() {
		TunModeService.setEventListener(null);
		super.onDestroy();
	}

	@Override
	public void onEvent(TunModeService.Event event) {
		runOnUiThread(() -> {
			switch (event) {
				case CONNECTING:
					MainActivity.this.toggle_button.setText("准备拦截中...");
					break;

				case CONNECTED:
					MainActivity.this.toggle_button.setText("拦截中");
					break;

				case DISCONNECTING:
					MainActivity.this.toggle_button.setText("停止拦截中...");
					break;

				case INITIALIZED:
				case DISCONNECTED:
					MainActivity.this.toggle_button.setText("未拦截");
					break;

				case NETWORK_ERROR:
					MainActivity.this.toggle_button.setText("未拦截");
					Toast.makeText(this, "Network Error", Toast.LENGTH_SHORT).show();
					break;

				case COULDNT_INITIALIZE:    // Doubt it will come here, but still
					MainActivity.this.toggle_button.setText("未拦截");
					MainActivity.this.toggle_button.setEnabled(false);
					Toast.makeText(this, "Couldn't initialize", Toast.LENGTH_SHORT).show();
					break;

				default:
					break;
			}
		});
	}

	@Override
	protected void onActivityResult(int requestCode, int resultCode, Intent data) {
		super.onActivityResult(requestCode, resultCode, data);
		if (requestCode == 1337) {
			switch (resultCode) {
				case RESULT_OK:
					break;
				case RESULT_CANCELED:
					Toast.makeText(this, "Access Denied", Toast.LENGTH_SHORT).show();
					break;
			}
		}
	}

	private void startTunMode(TunModeService.Operation operation) {
		Intent intent = new Intent(this, TunModeService.class);
		intent.putExtra(TunModeService.INTENT_EXTRA_OPERATION, operation);
		startService(intent);
	}

	private void syncButtonText() {
		TunModeService.State state = TunModeService.getState();

		switch (state) {
			case CONNECTING:
				this.toggle_button.setText("准备拦截中...");
				break;
			case CONNECTED:
				this.toggle_button.setText("拦截中");
				break;
			case DISCONNECTING:
				this.toggle_button.setText("停止拦截中...");
				break;
			case DISCONNECTED:
				this.toggle_button.setText("未拦截");
				break;
		}
	}

	private void loadBlockedIPs() {
		SharedPreferences prefs = getSharedPreferences(PREFS_NAME, MODE_PRIVATE);
		String blockedIPs = prefs.getString(BLOCKED_IPS_KEY, DEFAULT_BLOCKED_IPS);
		blocked_ips_edittext.setText(blockedIPs);
	}

	private void saveBlockedIPs() {
		SharedPreferences prefs = getSharedPreferences(PREFS_NAME, MODE_PRIVATE);
		SharedPreferences.Editor editor = prefs.edit();
		String blockedIPs = blocked_ips_edittext.getText().toString().trim();
		editor.putString(BLOCKED_IPS_KEY, blockedIPs);
		editor.apply();
	}

	// 【直接传递给Native层】
	private void setBlockedIPsToNative() {
		String blockedIPs = blocked_ips_edittext.getText().toString().trim();
		setBlockedIPsNative(blockedIPs);
	}

	// Native方法声明
	private native void setBlockedIPsNative(String blockedIPs);

	static {
		System.loadLibrary("tunmode");
	}
}