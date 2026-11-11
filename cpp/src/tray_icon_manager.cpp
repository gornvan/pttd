#include "tray_icon_manager.hpp"

#include <QAction>
#include <QColor>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMenu>
#include <QPainter>
#include <QPixmap>
#include <QRectF>
#include <QtSvg/QSvgRenderer>
#include <QSystemTrayIcon>
#include <QTimer>

namespace {

QString trayIconsDirPath() {
    return QDir::homePath() + "/.local/p2td/icons";
}

QString activeTrayIconPath() {
    return trayIconsDirPath() + "/unmuted.svg";
}

QString inactiveTrayIconPath() {
    return trayIconsDirPath() + "/muted.svg";
}

QString defaultCircleSvg(const QString &color) {
    return QStringLiteral(
               R"(<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" width="64" height="64" viewBox="0 0 64 64">
<circle cx="32" cy="32" r="28" fill="%1"/>
</svg>
)")
        .arg(color);
}

void ensureIconFile(const QString &path, const QString &svgContent) {
    if (QFileInfo::exists(path)) {
        return;
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        qWarning() << "Failed to create tray icon file:" << path;
        return;
    }
    file.write(svgContent.toUtf8());
}

void ensureTrayIconFiles() {
    QDir dir(trayIconsDirPath());
    if (!dir.exists() && !dir.mkpath(".")) {
        qWarning() << "Failed to create tray icon directory:" << trayIconsDirPath();
        return;
    }

    ensureIconFile(activeTrayIconPath(), defaultCircleSvg("#4CAF50"));
    ensureIconFile(inactiveTrayIconPath(), defaultCircleSvg("#6666aa"));
}

/** Rasterize SVG with Qt Svg (linked). Avoids reliance on Qt's svg imageformats plugin — required for bundled AppDirs. */
QIcon iconFromSvgPath(const QString &path, const QIcon &fallback) {
    if (!QFileInfo::exists(path)) {
        return fallback;
    }

    QSvgRenderer renderer(path);
    if (!renderer.isValid()) {
        qWarning() << "Invalid or unreadable tray SVG:" << path;
        return fallback;
    }

    QIcon icon;
    for (const int size : {16, 22, 24, 32, 48, 64, 128, 256}) {
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing, true);
        renderer.render(&painter, QRectF(0.0, 0.0, static_cast<double>(size), static_cast<double>(size)));
        icon.addPixmap(pixmap);
    }
    return icon;
}

} // namespace

TrayIconManager::TrayIconManager(QObject *parent)
    : QObject(parent) {
    loadIcons();
}

void TrayIconManager::loadIcons() {
    const QIcon defaultActiveIcon = makeCircleIcon(QColor("#4CAF50"));
    const QIcon defaultInactiveIcon = makeCircleIcon(QColor("#6666aa"));

    ensureTrayIconFiles();

    activeIcon_ = iconFromSvgPath(activeTrayIconPath(), defaultActiveIcon);
    inactiveIcon_ = iconFromSvgPath(inactiveTrayIconPath(), defaultInactiveIcon);
}

TrayIconManager::~TrayIconManager() {
    hideIcon();
}

void TrayIconManager::showIcon(const std::function<void()> &onOpenConfig,
                               const std::function<void()> &onTerminate) {
    if (trayIcon_) {
        return;
    }
    onOpenConfig_ = onOpenConfig;
    onTerminate_ = onTerminate;
    trayShowAttempts_ = 0;
    tryShowIcon();
}

void TrayIconManager::tryShowIcon() {
    if (trayIcon_) {
        return;
    }

    constexpr int kMaxAttempts = 12;
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        if (trayShowAttempts_ < kMaxAttempts) {
            ++trayShowAttempts_;
            qWarning() << "System tray not yet available; retry" << trayShowAttempts_ << "of" << kMaxAttempts;
            QTimer::singleShot(1500, this, [this]() { tryShowIcon(); });
            return;
        }
        qWarning() << "System tray is not available on this desktop. Continuing without tray icon.";
        return;
    }

    trayIcon_ = new QSystemTrayIcon(this);
    menu_ = new QMenu();

    QAction *openConfigAction = menu_->addAction("Open config");
    QObject::connect(openConfigAction, &QAction::triggered, this, [this]() {
        if (onOpenConfig_) {
            onOpenConfig_();
        }
    });

    QAction *terminateAction = menu_->addAction("Terminate");
    QObject::connect(terminateAction, &QAction::triggered, this, [this]() {
        if (onTerminate_) {
            onTerminate_();
        }
    });

    trayIcon_->setIcon(inactiveIcon_);
    trayIcon_->setToolTip("PaX Push-to-Talk");
    trayIcon_->setContextMenu(menu_);
    trayIcon_->show();

    if (!trayIcon_->isVisible() && trayShowAttempts_ < kMaxAttempts) {
        ++trayShowAttempts_;
        qWarning() << "Tray icon not visible yet; retry" << trayShowAttempts_;
        trayIcon_->deleteLater();
        trayIcon_ = nullptr;
        menu_->deleteLater();
        menu_ = nullptr;
        QTimer::singleShot(1500, this, [this]() { tryShowIcon(); });
    }
}

bool TrayIconManager::isShown() const {
    return trayIcon_ != nullptr;
}

void TrayIconManager::hideIcon() {
    if (!trayIcon_) {
        return;
    }
    trayIcon_->hide();
    trayIcon_->deleteLater();
    trayIcon_ = nullptr;
    if (menu_) {
        menu_->deleteLater();
        menu_ = nullptr;
    }
}

bool TrayIconManager::isAvailable() const {
    return QSystemTrayIcon::isSystemTrayAvailable();
}

void TrayIconManager::setIconState(bool active) {
    if (!trayIcon_) {
        return;
    }
    trayIcon_->setIcon(active ? activeIcon_ : inactiveIcon_);
}

QIcon TrayIconManager::makeCircleIcon(const QColor &color) {
    constexpr int size = 64;
    constexpr int margin = 4;

    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    painter.drawEllipse(margin, margin, size - margin * 2, size - margin * 2);
    painter.end();

    return QIcon(pixmap);
}
