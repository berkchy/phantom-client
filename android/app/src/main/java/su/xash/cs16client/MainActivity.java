package su.xash.cs16client;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;

public class MainActivity extends Activity {
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        startActivity(new Intent(this, LauncherActivity.class)
                .putExtra(LauncherActivity.EXTRA_GAME_DIR, "cstrike")
                .putExtra(LauncherActivity.EXTRA_GAME_TITLE, getString(R.string.launcher_title_cs)));
        finish();
    }
}
