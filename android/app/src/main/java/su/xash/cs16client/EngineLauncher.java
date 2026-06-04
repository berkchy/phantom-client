package su.xash.cs16client;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.Uri;

import com.google.android.material.dialog.MaterialAlertDialogBuilder;

public final class EngineLauncher {
    private EngineLauncher() {}

    public static String findEnginePackage(Context context) {
        String[] packages = new String[] { "su.xash.engine.test", "su.xash.engine" };
        for (String pkg : packages) {
            try {
                context.getPackageManager().getPackageInfo(pkg, 0);
                return pkg;
            } catch (PackageManager.NameNotFoundException ignored) {
                // keep searching
            }
        }
        return null;
    }

    public static void promptInstall(Context context) {
        new MaterialAlertDialogBuilder(context)
                .setTitle(R.string.launcher_no_engine)
                .setMessage(R.string.launcher_no_engine_message)
                .setPositiveButton(R.string.launcher_engine_install, (d, w) -> {
                    Intent i = new Intent(Intent.ACTION_VIEW,
                            Uri.parse("https://github.com/FWGS/xash3d-fwgs/releases/tag/continuous"));
                    i.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TASK);
                    context.startActivity(i);
                })
                .setNegativeButton(R.string.launcher_engine_later, null)
                .show();
    }

    public static void launchEngine(Context context, String gameDir, String argv) {
        String pkg = findEnginePackage(context);
        if (pkg == null) {
            promptInstall(context);
            return;
        }

        Intent intent = new Intent().setComponent(new ComponentName(pkg, "su.xash.engine.XashActivity"))
                .setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TASK)
                .putExtra("gamedir", gameDir)
                .putExtra("gamelibdir", context.getApplicationInfo().nativeLibraryDir)
                .putExtra("argv", argv)
                .putExtra("package", context.getPackageName());

        context.startActivity(intent);
    }
}
