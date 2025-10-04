package git.gxosty.tunmode.interceptor.activities;

import androidx.appcompat.app.AppCompatActivity;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.view.View;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ListView;
import android.widget.Toast;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import git.gxosty.tunmode.R;
import git.gxosty.tunmode.interceptor.services.TunModeService;

public class MainActivity extends AppCompatActivity implements TunModeService.EventListener {
    // 原有变量保留
    private Button toggleButton;
    // 新增变量
    private Button saveButton;
    private EditText ipInput;
    private ListView ipListView;
    private ArrayAdapter<String> ipAdapter;
    private List<String> ipList = new ArrayList<>();
    private static final String PREFS_NAME = "IpBlockList";
    private static final String IP_LIST_KEY = "blocked_ips";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        // 原有初始化逻辑保留
        toggleButton = findViewById(R.id.tun_switch_button);
        
        // 新增：初始化IP列表相关控件
        saveButton = findViewById(R.id.save_button);
        ipInput = findViewById(R.id.ip_input);
        ipListView = findViewById(R.id.ip_list_view);
        ipAdapter = new ArrayAdapter<>(this, android.R.layout.simple_list_item_1, ipList);
        ipListView.setAdapter(ipAdapter);
        loadIpList();  // 加载保存的IP列表

        // 原有切换按钮逻辑保留（不修改）
        toggleButton.setOnClickListener(v -> {
            TunModeService.State state = TunModeService.getState();
            if (state.equals(TunModeService.State.CONNECTED)) {
                startTunMode(TunModeService.Operation.DISCONNECT);
            } else if (state.equals(TunModeService.State.DISCONNECTED)) {
                startTunMode(TunModeService.Operation.CONNECT);
            } else {
                Toast.makeText(this, "请重试", Toast.LENGTH_SHORT).show();
            }
        });

        // 新增：保存按钮逻辑
        saveButton.setOnClickListener(v -> {
            String newIp = ipInput.getText().toString().trim();
            if (!newIp.isEmpty() && !ipList.contains(newIp)) {
                ipList.add(newIp);
                ipAdapter.notifyDataSetChanged();
                ipInput.setText("");
                saveIpList();  // 保存到本地
                updateBlockedIpsInNative();  // 同步到C++层
                Toast.makeText(this, "添加成功", Toast.LENGTH_SHORT).show();
            }
        });

        // 原有初始化逻辑保留
        TunModeService.setEventListener(this);
        TunModeService.setActivity(this);
        startTunMode(TunModeService.Operation.INITIALIZE);
    }

    // 新增：加载保存的IP列表
    private void loadIpList() {
        SharedPreferences prefs = getSharedPreferences(PREFS_NAME, MODE_PRIVATE);
        String ipStr = prefs.getString(IP_LIST_KEY, "");
        if (!ipStr.isEmpty()) {
            ipList.addAll(Arrays.asList(ipStr.split(",")));
            ipAdapter.notifyDataSetChanged();
        }
    }

    // 新增：保存IP列表到本地
    private void saveIpList() {
        StringBuilder sb = new StringBuilder();
        for (String ip : ipList) {
            sb.append(ip).append(",");
        }
        SharedPreferences prefs = getSharedPreferences(PREFS_NAME, MODE_PRIVATE);
        prefs.edit().putString(IP_LIST_KEY, sb.toString()).apply();
    }

    // 新增：供JNI调用的IP列表同步方法
    public String getIpListAsString() {
        SharedPreferences prefs = getSharedPreferences(PREFS_NAME, MODE_PRIVATE);
        return prefs.getString(IP_LIST_KEY, "");
    }

    // 新增：JNI方法声明（同步到C++层）
    private native void updateBlockedIpsInNative();

    // 原有生命周期和事件处理逻辑完全保留（不修改）
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
                    toggleButton.setText("连接中...");
                    break;
                case CONNECTED:
                    toggleButton.setText("已连接（点击断开）");
                    break;
                case DISCONNECTING:
                    toggleButton.setText("断开中...");
                    break;
                case INITIALIZED:
                case DISCONNECTED:
                    toggleButton.setText("未连接（点击启动）");
                    break;
                case NETWORK_ERROR:
                    toggleButton.setText("未连接（网络错误）");
                    Toast.makeText(this, "网络错误", Toast.LENGTH_SHORT).show();
                    break;
                case COULDNT_INITIALIZE:
                    toggleButton.setText("初始化失败");
                    toggleButton.setEnabled(false);
                    Toast.makeText(this, "初始化失败", Toast.LENGTH_SHORT).show();
                    break;
                default:
                    break;
            }
        });
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        if (requestCode == 1337) {
            if (resultCode == RESULT_CANCELED) {
                Toast.makeText(this, "权限被拒绝", Toast.LENGTH_SHORT).show();
            }
        }
        super.onActivityResult(requestCode, resultCode, data);
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
                toggleButton.setText("连接中...");
                break;
            case CONNECTED:
                toggleButton.setText("已连接（点击断开）");
                break;
            case DISCONNECTING:
                toggleButton.setText("断开中...");
                break;
            case DISCONNECTED:
                toggleButton.setText("未连接（点击启动）");
                break;
        }
    }
}
