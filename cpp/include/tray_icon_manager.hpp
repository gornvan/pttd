#pragma once

#include <QIcon>
#include <QObject>
#include <functional>

class QMenu;
class QSystemTrayIcon;

class TrayIconManager final : public QObject {
public:
    explicit TrayIconManager(QObject *parent = nullptr);
    ~TrayIconManager() override;

    void showIcon(const std::function<void()> &onOpenConfig,
                  const std::function<void()> &onTerminate);
    void hideIcon();
    bool isAvailable() const;
    bool isShown() const;
    void setIconState(bool active);

private:
    void loadIcons();
    void tryShowIcon();
    static QIcon makeCircleIcon(const QColor &color);

    std::function<void()> onOpenConfig_;
    std::function<void()> onTerminate_;
    int trayShowAttempts_ = 0;

    QIcon activeIcon_;
    QIcon inactiveIcon_;
    QSystemTrayIcon *trayIcon_ = nullptr;
    QMenu *menu_ = nullptr;
};
