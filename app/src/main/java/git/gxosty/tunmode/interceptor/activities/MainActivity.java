package git.gxosty.tunmode.interceptor.activities;

import androidx.appcompat.app.AppCompatActivity;

import android.content.SharedPreferences;
import android.os.Bundle;
import android.widget.Button;
import android.widget.EditText;
import android.widget.TextView;
import android.widget.Toast;

import java.util.HashSet;
import java.util.Set;

import git.gxosty.tunmode.R;
import git.gxosty.tunmode.interceptor.services.TunModeService;

public class MainActivity extends AppCompatActivity implements TunModeService.EventListener {

    private Button toggle_button;
    private Button add_ip_button;
    private Button save_button;
    private EditText ip_input;
    private TextView ip_list_text;
    
    private Set<String> blockedIps;
    private SharedPreferences sharedPreferences;
    private static final String PREFS_NAME = "TunModePrefs";
    private static final String BLOCKED_IPS_KEY = "blocked_ips";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        initViews();
        loadBlockedIps();
        updateIpListDisplay();

        toggle_button.setOnClickListener(v -> {
            TunModeService.State state = TunModeService.getState();
            if (state.equals(TunModeService.State.CONNECTED)) {
                startTunMode(TunModeService.Operation.DISCONNECT);
            } else if (state.equals(TunModeService.State.DISCONNECTED)) {
                startTunMode(TunModeService.Operation.CONNECT);
            } else {
                Toast.makeText(this, "请重试", Toast.LENGTH_SHORT).show();
            }
        });

        add_ip_button.setOnClickListener(v -> {
            String ip = ip_input.getText().toString().trim();
            if (ip.isEmpty()) {
                Toast.makeText(this, "请输入IP地址", Toast.LENGTH_SHORT).show();
                return;
            }
            
            // 简单的IP格式验证
            if (!ip.matches("^(\\d{1,3}\\.){3}\\d{1,3}$")) {
                Toast.makeText(this, "IP格式不正确", Toast.LENGTH_SHORT).show();
                return;
            }
            
            blockedIps.add(ip);
            ip_input.setText("");
            updateIpListDisplay();
            Toast.makeText(this, "已添加: " + ip, Toast.LENGTH_SHORT).show();
        });

        save_button.setOnClickListener(v -> {
            saveBlockedIps();
            Toast.makeText(this, "名单已保存", Toast.LENGTH_SHORT).show();
        });

        TunModeService.setEventListener(this);
        TunModeService.setActivity(this);
        startTunMode(TunModeService.Operation.INITIALIZE);
    }

    private void initViews() {
        toggle_button = findViewById(R.id.tun_switch_button);
        add_ip_button = findViewById(R.id.add_ip_button);
        save_button = findViewById(R.id.save_button);
        ip_input = findViewById(R.id.ip_input);
        ip_list_text = findViewById(R.id.ip_list_text);
        
        sharedPreferences = getSharedPreferences(PREFS_NAME, MODE_PRIVATE);
    }

    private void loadBlockedIps() {
        blockedIps = sharedPreferences.getStringSet(BLOCKED_IPS_KEY, new HashSet<>());
    }

    private void saveBlockedIps() {
        sharedPreferences.edit().putStringSet(BLOCKED_IPS_KEY, blockedIps).apply();
    }

    private void updateIpListDisplay() {
        StringBuilder sb = new StringBuilder("拦截IP列表：\n");
        for (String ip : blockedIps) {
            sb.append("• ").append(ip).append("\n");
        }
        ip_list_text.setText(sb.toString());
    }

    @Override
    protected void onResume() {
        super.onResume();
        syncButtonText();
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
                    toggle_button.setText("连接中...");
                    break;
                case CONNECTED:
                    toggle_button.setText("已连接");
                    break;
                case DISCONNECTING:
                    toggle_button.setText("断开中...");
                    break;
                case INITIALIZED:
                case DISCONNECTED:
                    toggle_button.setText("启动VPN");
                    break;
                case NETWORK_ERROR:
                    toggle_button.setText("启动VPN");
                    Toast.makeText(this, "网络错误", Toast.LENGTH_SHORT).show();
                    break;
                case COULDNT_INITIALIZE:
                    toggle_button.setText("启动VPN");
                    toggle_button.setEnabled(false);
                    Toast.makeText(this, "初始化失败", Toast.LENGTH_SHORT).show();
                    break;
            }
        });
    }

    private void startTunMode(TunModeService.Operation operation) {
        android.content.Intent intent = new android.content.Intent(this, TunModeService.class);
        intent.putExtra(TunModeService.INTENT_EXTRA_OPERATION, operation);
        startService(intent);
    }

    private void syncButtonText() {
        TunModeService.State state = TunModeService.getState();
        switch (state) {
            case CONNECTING:
                toggle_button.setText("连接中...");
                break;
            case CONNECTED:
                toggle_button.setText("已连接");
                break;
            case DISCONNECTING:
                toggle_button.setText("断开中...");
                break;
            case DISCONNECTED:
                toggle_button.setText("启动VPN");
                break;
        }
    }
    
    // 获取拦截IP列表供native层使用
    public String[] getBlockedIpsArray() {
        return blockedIps.toArray(new String[0]);
    }
}
