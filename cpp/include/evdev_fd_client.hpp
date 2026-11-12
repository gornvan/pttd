#pragma once

#include <QStringList>

#include <vector>

/** Open evdev nodes via setgid helper (no setgid on the Qt GUI process — tray needs that). */
bool acquireEvdevFdsViaHelper(const QStringList &devicePaths, std::vector<int> &outFds);
