#include <QApplication>
#include <QDateTime>
#include <QDebug>
#include <QMetaObject>
#include <QProcess>
#include <QTimer>
#include <atomic>
#include <memory>

#include <unistd.h>

#include <X11/Xlib.h>

#include "config_manager.hpp"
#include "ptt_input_backend.hpp"
#include "pulseaudio_controller.hpp"
#include "sound_controller.hpp"
#include "tray_icon_manager.hpp"

namespace {

/** Qt reads the session bus at QGuiApplication construction; align with Plasma before that. */
void ensureGraphicalSessionEnv() {
    if (qEnvironmentVariableIsEmpty("XDG_RUNTIME_DIR")) {
        const QByteArray rt = QByteArray("/run/user/") + QByteArray::number(getuid());
        qputenv("XDG_RUNTIME_DIR", rt);
    }
    const QByteArray rt = qgetenv("XDG_RUNTIME_DIR");
    if (!rt.isEmpty()) {
        qputenv("DBUS_SESSION_BUS_ADDRESS", QByteArray("unix:path=") + rt + "/bus");
    }
    if (!qEnvironmentVariableIsSet("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", "xcb");
    }
}

void openConfigFile(const QString &path) {
    const QList<QStringList> candidates = {
        {"xdg-open", path},
        {"gio", "open", path},
        {"flatpak-spawn", "--host", "xdg-open", path},
    };

    for (const QStringList &cmd : candidates) {
        if (cmd.isEmpty()) {
            continue;
        }
        const QString program = cmd.first();
        const QStringList args = cmd.mid(1);
        if (QProcess::startDetached(program, args)) {
            return;
        }
    }
    qCritical() << "Failed to open config file with any of the following commands:";
    qCritical() << "`xdg-open`, `gio open`, `flatpak spawn --host xdg-open`";
    qCritical() << "Please check if any of the commands is available in your PATH.";
    qCritical() << "You can manually open the config file at " << path;
}

} // namespace

int main(int argc, char *argv[]) {
    ensureGraphicalSessionEnv();
    QApplication app(argc, argv);
    app.setApplicationName("p2td");

    const AppConfig config = ConfigManager::readConfig();

    PulseAudioController pulse(config.cacheInputs);
    SoundController sound;
    TrayIconManager tray;

    std::atomic<bool> pttIsDown{false};
    std::atomic<qint64> suppressPressUntilMs{0};

    auto applyMute = [&]() {
        pulse.muteAllRecordingSources();
        sound.playMute();
        tray.setIconState(false);
    };

    auto onPress = [&](int) {
        QMetaObject::invokeMethod(
            &app,
            [&]() {
                if (QDateTime::currentMSecsSinceEpoch() < suppressPressUntilMs.load()) {
                    return;
                }
                const bool wasDown = pttIsDown.exchange(true);
                if (wasDown) {
                    return;
                }
                sound.playUnmute();
                pulse.unmuteAllRecordingSources();
                tray.setIconState(true);
            },
            Qt::QueuedConnection);
    };

    auto onRelease = [&](int) {
        QMetaObject::invokeMethod(
            &app,
            [&]() {
                if (!pttIsDown.exchange(false)) {
                    return;
                }
                suppressPressUntilMs.store(QDateTime::currentMSecsSinceEpoch() + 200);
                QTimer::singleShot(config.muteDelayMs, &app, [&]() {
                    if (!pttIsDown.load()) {
                        applyMute();
                    }
                });
            },
            Qt::QueuedConnection);
    };

    sound.ensureAndLoadSounds();
    applyMute();

    if (config.showTrayIcon) {
        tray.showIcon(
            [&]() { openConfigFile(ConfigManager::configPath()); },
            [&]() { QMetaObject::invokeMethod(&app, "quit", Qt::QueuedConnection); });
    }

    if (!qEnvironmentVariable("DISPLAY").isEmpty()) {
        XInitThreads();
    }

    std::unique_ptr<PttInputBackend> input = createPttInputBackend(config, onPress, onRelease);
    if (!input) {
        qCritical() << "No PTT input backend available.";
        qCritical() << "Prefer evdev: add your user to the 'input' group and set BIND_PTT in"
                    << ConfigManager::configPath();
        qCritical() << "On X11, XRecord fallback also requires DISPLAY and the XRecord extension.";
        return 1;
    }
    qInfo() << "PTT input:" << input->backendName();

    const int exitCode = app.exec();

    input->stop();
    tray.hideIcon();
    pulse.unmuteAllRecordingSources();
    return exitCode;
}
