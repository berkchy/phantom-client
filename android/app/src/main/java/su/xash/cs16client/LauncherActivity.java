package su.xash.cs16client;

import android.app.Activity;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Intent;
import android.content.res.AssetManager;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Environment;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.provider.Settings;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.View;
import android.widget.EditText;
import android.widget.ImageView;
import android.widget.ProgressBar;
import android.widget.ScrollView;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import com.google.android.material.button.MaterialButton;
import com.google.android.material.button.MaterialButtonToggleGroup;
import com.google.android.material.dialog.MaterialAlertDialogBuilder;
import com.google.android.material.materialswitch.MaterialSwitch;
import com.google.android.material.textfield.TextInputEditText;
import com.google.android.material.textview.MaterialTextView;
import com.google.android.material.appbar.MaterialToolbar;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.atomic.AtomicInteger;

public class LauncherActivity extends AppCompatActivity {
    private static final String TAG = "CS16Launcher";
    private static final int REQ_STORAGE_ACCESS = 1201;
    public static final String EXTRA_GAME_DIR = "gameDir";
    public static final String EXTRA_GAME_TITLE = "gameTitle";

    private static final String PREFS = "launcher_prefs";
    private static final String KEY_CONSOLE = "console";
    private static final String KEY_LOG = "log";
    private static final String KEY_DEV = "dev";
    private static final String KEY_YAPB = "yapb";
    private static final String KEY_CUSTOM = "custom";

    private static final int PANEL_PLAY = 0;
    private static final int PANEL_SERVERS = 1;
    private static final int PANEL_OPTIONS = 2;
    private static final int PANEL_CONSOLE = 3;

    private final ExecutorService refreshExecutor = Executors.newSingleThreadExecutor();
    private final Handler mainHandler = new Handler(Looper.getMainLooper());
    private final AtomicInteger refreshGeneration = new AtomicInteger(0);
    private boolean storageAccessRequested;

    private String gameDir = "cstrike";
    private String gameTitle = "Counter-Strike";

    private MaterialToolbar toolbar;
    private MaterialButtonToggleGroup tabGroup;
    private ScrollView playPanel;
    private View serversPanel;
    private ScrollView optionsPanel;
    private View consolePanel;
    private MaterialButton playButton;
    private TextView selectedServerText;
    private TextView commandPreview;
    private TextView optionsPreview;
    private TextView serverStatus;
    private ProgressBar serverLoading;
    private TextInputEditText serverSearch;
    private TextInputEditText directConnectAddress;
    private TextInputEditText customArgs;
    private MaterialSwitch enableConsole;
    private MaterialSwitch enableLog;
    private MaterialSwitch enableDev;
    private MaterialSwitch enableYapb;
    private ImageView gameIcon;
    private TextView gameTitleView;
    private TextView gameSubtitle;
    private MaterialButton refreshServersButton;
    private MaterialButton directConnectButton;
    private MaterialButton copyMiniConsoleButton;
    private TextView miniConsole;
    private ScrollView miniConsoleScroll;
    private ServerAdapter serverAdapter;
    private final List<ServerEntry> allServers = new ArrayList<>();
    private ServerEntry selectedServer;
    private String serverFilter = "";
    private final StringBuilder miniConsoleBuffer = new StringBuilder();

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setContentView(R.layout.activity_launcher);

        if (getIntent() != null) {
            String extraDir = getIntent().getStringExtra(EXTRA_GAME_DIR);
            if (extraDir != null && !extraDir.isEmpty()) {
                gameDir = extraDir;
            }
            String extraTitle = getIntent().getStringExtra(EXTRA_GAME_TITLE);
            if (extraTitle != null && !extraTitle.isEmpty()) {
                gameTitle = extraTitle;
            }
        }

        bindViews();
        bindTabs();
        bindOptions();
        bindServers();
        loadPreferences();
        applyGameHeader();
        startAssetSyncIfPossible();
        updateCommandPreview();
        showPanel(PANEL_PLAY);
        appendMiniConsole("launcher ready");
    }

    @Override
    protected void onResume() {
        super.onResume();
        startAssetSyncIfPossible();
    }

    @Override
    protected void onDestroy() {
        refreshExecutor.shutdownNow();
        super.onDestroy();
    }

    private void bindViews() {
        toolbar = findViewById(R.id.toolbar);
        tabGroup = findViewById(R.id.tabGroup);
        playPanel = findViewById(R.id.playPanel);
        serversPanel = findViewById(R.id.serversPanel);
        optionsPanel = findViewById(R.id.optionsPanel);
        consolePanel = findViewById(R.id.consolePanel);
        playButton = findViewById(R.id.playButton);
        selectedServerText = findViewById(R.id.selectedServerText);
        commandPreview = findViewById(R.id.commandPreview);
        optionsPreview = findViewById(R.id.optionsPreview);
        serverStatus = findViewById(R.id.serverStatus);
        serverLoading = findViewById(R.id.serverLoading);
        serverSearch = findViewById(R.id.serverSearch);
        directConnectAddress = findViewById(R.id.directConnectAddress);
        customArgs = findViewById(R.id.customArgs);
        enableConsole = findViewById(R.id.enableConsole);
        enableLog = findViewById(R.id.enableLog);
        enableDev = findViewById(R.id.enableDev);
        enableYapb = findViewById(R.id.enableYapb);
        gameIcon = findViewById(R.id.gameIcon);
        gameTitleView = findViewById(R.id.gameTitle);
        gameSubtitle = findViewById(R.id.gameSubtitle);
        refreshServersButton = findViewById(R.id.refreshServersButton);
        directConnectButton = findViewById(R.id.directConnectButton);
        copyMiniConsoleButton = findViewById(R.id.copyMiniConsoleButton);
        miniConsole = findViewById(R.id.miniConsole);
        miniConsoleScroll = findViewById(R.id.miniConsoleScroll);

        toolbar.setTitle(gameTitle);
        toolbar.setSubtitle(getString(R.string.app_name));
        setSupportActionBar(toolbar);
    }

    private void bindTabs() {
        tabGroup.check(R.id.tabPlay);
        tabGroup.addOnButtonCheckedListener((group, checkedId, isChecked) -> {
            if (!isChecked) {
                return;
            }
            if (checkedId == R.id.tabPlay) {
                showPanel(PANEL_PLAY);
            } else if (checkedId == R.id.tabServers) {
                showPanel(PANEL_SERVERS);
            } else if (checkedId == R.id.tabOptions) {
                showPanel(PANEL_OPTIONS);
            } else if (checkedId == R.id.tabConsole) {
                showPanel(PANEL_CONSOLE);
            }
        });

        playButton.setOnClickListener(v -> launchSelectedAction());
    }

    private void bindOptions() {
        TextWatcher watcher = new SimpleTextWatcher(this::updateCommandPreview);
        customArgs.addTextChangedListener(watcher);
        serverSearch.addTextChangedListener(new SimpleTextWatcher(() -> {
            serverFilter = textOf(serverSearch);
            applyFilter();
        }));
        enableConsole.setOnCheckedChangeListener((buttonView, isChecked) -> updateCommandPreview());
        enableLog.setOnCheckedChangeListener((buttonView, isChecked) -> updateCommandPreview());
        enableDev.setOnCheckedChangeListener((buttonView, isChecked) -> updateCommandPreview());
        enableYapb.setOnCheckedChangeListener((buttonView, isChecked) -> updateCommandPreview());

        refreshServersButton.setOnClickListener(v -> refreshServers());
        directConnectButton.setOnClickListener(v -> {
            String address = textOf(directConnectAddress);
            if (address.isEmpty()) {
                return;
            }
            selectedServer = new ServerEntry(address);
            selectedServer.name = address;
            updateSelectedServerUi();
            launchSelectedAction();
        });
        copyMiniConsoleButton.setOnClickListener(v -> copyMiniConsoleToClipboard());
    }

    private void bindServers() {
        RecyclerView recyclerView = findViewById(R.id.serverList);
        recyclerView.setLayoutManager(new LinearLayoutManager(this));
        serverAdapter = new ServerAdapter(entry -> {
            selectedServer = entry;
            updateSelectedServerUi();
            tabGroup.check(R.id.tabPlay);
        });
        recyclerView.setAdapter(serverAdapter);
    }

    private void loadPreferences() {
        enableConsole.setChecked(prefs().getBoolean(KEY_CONSOLE, true));
        enableLog.setChecked(prefs().getBoolean(KEY_LOG, true));
        enableDev.setChecked(prefs().getBoolean(KEY_DEV, true));
        enableYapb.setChecked(prefs().getBoolean(KEY_YAPB, true));
        customArgs.setText(prefs().getString(KEY_CUSTOM, ""));
    }

    private void savePreferences() {
        prefs().edit()
                .putBoolean(KEY_CONSOLE, enableConsole.isChecked())
                .putBoolean(KEY_LOG, enableLog.isChecked())
                .putBoolean(KEY_DEV, enableDev.isChecked())
                .putBoolean(KEY_YAPB, enableYapb.isChecked())
                .putString(KEY_CUSTOM, textOf(customArgs))
                .apply();
    }

    private android.content.SharedPreferences prefs() {
        return getSharedPreferences(PREFS, MODE_PRIVATE);
    }

    private void applyGameHeader() {
        gameTitleView.setText(gameTitle);
        gameSubtitle.setText(gameDir.toUpperCase(Locale.US) + " launcher");
        if ("czero".equalsIgnoreCase(gameDir)) {
            gameIcon.setImageResource(R.mipmap.ic_launcher_cz);
        } else {
            gameIcon.setImageResource(R.mipmap.ic_launcher);
        }
        selectedServerText.setText(getString(R.string.launcher_no_servers));
    }

    private void showPanel(int panel) {
        playPanel.setVisibility(panel == PANEL_PLAY ? View.VISIBLE : View.GONE);
        serversPanel.setVisibility(panel == PANEL_SERVERS ? View.VISIBLE : View.GONE);
        optionsPanel.setVisibility(panel == PANEL_OPTIONS ? View.VISIBLE : View.GONE);
        consolePanel.setVisibility(panel == PANEL_CONSOLE ? View.VISIBLE : View.GONE);
    }

    private void updateSelectedServerUi() {
        if (selectedServer == null) {
            selectedServerText.setText(getString(R.string.launcher_no_servers));
            playButton.setText(getString(R.string.launcher_launch));
            playButton.setIconResource(android.R.drawable.ic_media_play);
            updateCommandPreview();
            return;
        }

        StringBuilder sb = new StringBuilder();
        sb.append(selectedServer.name == null || selectedServer.name.isEmpty() ? selectedServer.address : selectedServer.name);
        if (selectedServer.map != null && !selectedServer.map.isEmpty()) {
            sb.append("\n").append(selectedServer.map);
        }
        if (selectedServer.players > 0 || selectedServer.maxPlayers > 0) {
            sb.append("\n").append(selectedServer.playersText());
        }
        selectedServerText.setText(sb.toString());
        playButton.setText(R.string.launcher_connect);
        playButton.setIconResource(android.R.drawable.ic_menu_send);
        updateCommandPreview();
    }

    private void updateCommandPreview() {
        String argv = buildArgv(selectedServer != null ? selectedServer.address : null);
        commandPreview.setText(argv);
        optionsPreview.setText(argv);
        if (selectedServer == null) {
            playButton.setText(R.string.launcher_launch);
            playButton.setIconResource(android.R.drawable.ic_media_play);
        } else {
            playButton.setText(R.string.launcher_connect);
            playButton.setIconResource(android.R.drawable.ic_menu_send);
        }
    }

    private String buildArgv(@Nullable String connectAddress) {
        StringBuilder sb = new StringBuilder();
        if (enableConsole.isChecked()) {
            appendArg(sb, "-console");
        }
        if (enableLog.isChecked()) {
            appendArg(sb, "-log");
        }
        if (enableDev.isChecked()) {
            appendArg(sb, "-dev");
            appendArg(sb, "2");
        }
        if (enableYapb.isChecked()) {
            appendArg(sb, "-dll");
            appendArg(sb, "@yapb");
        }

        String custom = textOf(customArgs);
        if (!custom.isEmpty()) {
            appendRaw(sb, custom);
        }

        if (connectAddress != null && !connectAddress.isEmpty()) {
            appendArg(sb, "+connect");
            appendArg(sb, connectAddress);
        }

        return sb.toString().trim();
    }

    private void launchSelectedAction() {
        savePreferences();
        String argv = buildArgv(selectedServer != null ? selectedServer.address : null);
        startAssetSyncIfPossible();
        EngineLauncher.launchEngine(this, gameDir, argv);
    }

    private void startAssetSyncIfPossible() {
        if (!hasAssetWriteAccess()) {
            appendMiniConsole("launcher: storage access missing");
            if (!storageAccessRequested) {
                storageAccessRequested = true;
                requestAssetWriteAccess();
            }
            return;
        }
        storageAccessRequested = false;
        appendMiniConsole("launcher: asset sync starting");
        new Thread(this::ensureGameAssetsInstalled, "AssetSync").start();
    }

    private boolean hasAssetWriteAccess() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            return Environment.isExternalStorageManager();
        }
        return ContextCompat.checkSelfPermission(this, android.Manifest.permission.WRITE_EXTERNAL_STORAGE)
                == PackageManager.PERMISSION_GRANTED;
    }

    private void requestAssetWriteAccess() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            try {
                Intent intent = new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION);
                intent.setData(Uri.parse("package:" + getPackageName()));
                startActivity(intent);
            } catch (Exception e) {
                appendMiniConsole("launcher: storage settings unavailable " + e.getMessage());
            }
            return;
        }
        ActivityCompat.requestPermissions(this,
                new String[]{android.Manifest.permission.WRITE_EXTERNAL_STORAGE},
                REQ_STORAGE_ACCESS);
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, @Nullable String[] permissions, @Nullable int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        if (requestCode != REQ_STORAGE_ACCESS) {
            return;
        }
        boolean granted = grantResults != null && grantResults.length > 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED;
        appendMiniConsole(granted ? "launcher: storage access granted" : "launcher: storage access denied");
        if (granted) {
            startAssetSyncIfPossible();
        }
    }

    private void ensureGameAssetsInstalled() {
        File gameRoot = new File(Environment.getExternalStorageDirectory(), "xash/" + gameDir);
        try (ZipFile apk = new ZipFile(getPackageCodePath())) {
            syncAssetTree(apk, "sprites", new File(gameRoot, "sprites"));
            syncAssetTree(apk, "sound/vox", new File(gameRoot, "sound/vox"));
            syncAssetTree(apk, "gfx/shell", new File(gameRoot, "gfx/shell"));
            syncAssetTree(apk, "fonts.wad", new File(gameRoot, "fonts.wad"));
            syncAssetTree(apk, "gfx.wad", new File(gameRoot, "gfx.wad"));
            syncAssetTree(apk, "resource/gameui_english.txt", new File(gameRoot, "resource/gameui_english.txt"));
        } catch (IOException e) {
            appendMiniConsole("launcher: asset sync failed open apk " + e.getMessage());
        }
        appendMiniConsole("launcher: asset sync finished");
    }

    private void syncAssetTree(ZipFile apk, String assetPath, File target) {
        AssetManager assets = getAssets();
        try {
            String[] children = assets.list(assetPath);
            if (children != null && children.length > 0) {
                if (!target.exists() && !target.mkdirs()) {
                    appendMiniConsole("launcher: failed to create dir " + target.getAbsolutePath());
                    return;
                }
                for (String child : children) {
                    String childAssetPath = assetPath.isEmpty() ? child : assetPath + "/" + child;
                    syncAssetTree(apk, childAssetPath, new File(target, child));
                }
                return;
            }

            File parent = target.getParentFile();
            if (parent != null && !parent.exists() && !parent.mkdirs()) {
                appendMiniConsole("launcher: failed to create dir " + parent.getAbsolutePath());
                return;
            }

            ZipEntry entry = apk.getEntry("assets/" + assetPath);
            long srcSize = entry != null ? entry.getSize() : -1;
            long srcTime = entry != null ? entry.getTime() : -1;
            if (target.exists() && target.isFile()) {
                boolean sizeMatches = srcSize >= 0 && target.length() == srcSize;
                boolean timeMatches = srcTime < 0 || target.lastModified() == srcTime;
                if (sizeMatches && timeMatches) {
                    appendMiniConsole("launcher: asset up to date " + assetPath);
                    return;
                }
            }

            try (InputStream in = assets.open(assetPath);
                 FileOutputStream out = new FileOutputStream(target)) {
                byte[] buffer = new byte[8192];
                int read;
                while ((read = in.read(buffer)) != -1) {
                    out.write(buffer, 0, read);
                }
                if (srcTime > 0) {
                    target.setLastModified(srcTime);
                }
                appendMiniConsole("launcher: synced asset " + assetPath);
            }
        } catch (IOException e) {
            appendMiniConsole("launcher: asset sync failed " + assetPath + " " + e.getMessage());
        }
    }

    private void refreshServers() {
        final int generation = refreshGeneration.incrementAndGet();
        android.util.Log.d(TAG, "refreshServers: start generation=" + generation + " gameDir=" + gameDir);
        appendMiniConsole("launcher: refresh start gameDir=" + gameDir);
        serverLoading.setVisibility(View.VISIBLE);
        serverStatus.setText(R.string.launcher_loading_servers);
        refreshServersButton.setEnabled(false);
        allServers.clear();
        serverAdapter.submit(new ArrayList<>());

        refreshExecutor.execute(() -> {
            ServerBrowserClient client = new ServerBrowserClient(this::appendMiniConsole);
            List<ServerEntry> servers = client.refresh(gameDir);
            mainHandler.post(() -> {
                if (generation != refreshGeneration.get()) {
                    android.util.Log.d(TAG, "refreshServers: stale generation ignored generation=" + generation);
                    appendMiniConsole("launcher: stale refresh ignored generation=" + generation);
                    return;
                }
                refreshServersButton.setEnabled(true);
                serverLoading.setVisibility(View.GONE);
                allServers.clear();
                if (servers != null) {
                    allServers.addAll(servers);
                }
                android.util.Log.d(TAG, "refreshServers: result size=" + allServers.size());
                appendMiniConsole("launcher: refresh result size=" + allServers.size());
                applyFilter();
                serverStatus.setText(getString(R.string.launcher_no_servers));
                if (allServers.isEmpty()) {
                    serverStatus.setText(R.string.launcher_no_servers);
                } else {
                    serverStatus.setText(allServers.size() + " servers");
                }
            });
        });
    }

    private void applyFilter() {
        String filter = serverFilter == null ? "" : serverFilter.trim().toLowerCase(Locale.US);
        android.util.Log.d(TAG, "applyFilter: filter=\"" + filter + "\" total=" + allServers.size());
        List<ServerEntry> filtered = new ArrayList<>();
        for (ServerEntry entry : allServers) {
            if (filter.isEmpty()) {
                filtered.add(entry);
                continue;
            }
            String haystack = ((entry.name == null ? "" : entry.name) + " "
                    + (entry.map == null ? "" : entry.map) + " "
                    + (entry.address == null ? "" : entry.address)).toLowerCase(Locale.US);
            if (haystack.contains(filter)) {
                filtered.add(entry);
            }
        }
        android.util.Log.d(TAG, "applyFilter: filtered=" + filtered.size());
        serverAdapter.submit(filtered);
    }

    private void appendMiniConsole(String line) {
        final String text = line == null ? "" : line.trim();
        if (text.isEmpty()) {
            return;
        }
        mainHandler.post(() -> {
            if (miniConsole == null || miniConsoleScroll == null) {
                return;
            }
            if (miniConsoleBuffer.length() > 0) {
                miniConsoleBuffer.append('\n');
            }
            miniConsoleBuffer.append(text);
            final int maxChars = 8000;
            if (miniConsoleBuffer.length() > maxChars) {
                int cut = miniConsoleBuffer.length() - maxChars;
                int newline = miniConsoleBuffer.indexOf("\n", cut);
                if (newline >= 0) {
                    miniConsoleBuffer.delete(0, newline + 1);
                } else {
                    miniConsoleBuffer.delete(0, cut);
                }
            }
            miniConsole.setText(miniConsoleBuffer.toString());
            miniConsoleScroll.post(() -> miniConsoleScroll.fullScroll(View.FOCUS_DOWN));
        });
    }

    private void copyMiniConsoleToClipboard() {
        String text = miniConsoleBuffer.toString();
        if (text.isEmpty()) {
            appendMiniConsole("launcher: copy requested but console is empty");
            return;
        }
        ClipboardManager clipboard = (ClipboardManager) getSystemService(CLIPBOARD_SERVICE);
        if (clipboard == null) {
            appendMiniConsole("launcher: clipboard unavailable");
            return;
        }
        clipboard.setPrimaryClip(ClipData.newPlainText("CS16 debug log", text));
        appendMiniConsole("launcher: log copied to clipboard");
    }

    private void appendArg(StringBuilder sb, String arg) {
        if (arg == null || arg.isEmpty()) {
            return;
        }
        if (sb.length() > 0) {
            sb.append(' ');
        }
        sb.append(arg);
    }

    private void appendRaw(StringBuilder sb, String raw) {
        if (raw == null || raw.trim().isEmpty()) {
            return;
        }
        if (sb.length() > 0) {
            sb.append(' ');
        }
        sb.append(raw.trim());
    }

    private String textOf(TextInputEditText editText) {
        Editable text = editText.getText();
        return text == null ? "" : text.toString().trim();
    }

    private static final class SimpleTextWatcher implements TextWatcher {
        private final Runnable onChange;

        SimpleTextWatcher(Runnable onChange) {
            this.onChange = onChange;
        }

        @Override
        public void beforeTextChanged(CharSequence s, int start, int count, int after) { }

        @Override
        public void onTextChanged(CharSequence s, int start, int before, int count) {
            onChange.run();
        }

        @Override
        public void afterTextChanged(Editable s) { }
    }
}
