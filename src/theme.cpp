// Theme watching follows Omacut (MIT, David Heinemeier Hansson).
#include "theme.h"

#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStandardPaths>
#include <cmath>

namespace {

// Used for any key a theme leaves out, and for everything when Omarchy isn't
// installed. The accent is Omarchy's own default yellow.
const QHash<QString, QString> &darkFallback() {
    static const QHash<QString, QString> colors{
        {"accent", "#FFD60A"},          {"background", "#121212"}, {"darker_background", "#0b0b0b"},
        {"lighter_background", "#1f1f1f"}, {"foreground", "#e6e6e6"}, {"muted", "#8a8a8a"},
        {"selection", "#2c2c2c"}, {"red", "#ff6b6b"},
    };
    return colors;
}

const QHash<QString, QString> &lightFallback() {
    static const QHash<QString, QString> colors{
        {"accent", "#FFD60A"},          {"background", "#fafafa"}, {"darker_background", "#efefef"},
        {"lighter_background", "#ffffff"}, {"foreground", "#1a1a1a"}, {"muted", "#6e6e6e"},
        {"selection", "#dcdcdc"}, {"red", "#c62828"},
    };
    return colors;
}

QString readText(const QString &path) {
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString();
}

} // namespace

Theme::Theme(const QString &directory, QObject *parent, const QString &hyprctl)
    : QObject(parent),
      m_directory(directory.isEmpty() ? QDir::homePath() + "/.local/state/omarchy/current" : directory) {
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(80);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] { m_debounce.start(); });
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this] { m_debounce.start(); });
    connect(&m_debounce, &QTimer::timeout, this, &Theme::reload);

    // Corners follow Hyprland's effective rounding, which themes and personal
    // overrides can both set, so ask hyprctl rather than reading its config.
    // Without Hyprland the corners stay sharp, like Omarchy's default.
    m_hyprctl = !hyprctl.isEmpty() ? hyprctl
              : directory.isEmpty() && !qEnvironmentVariableIsEmpty("HYPRLAND_INSTANCE_SIGNATURE")
                  ? QStandardPaths::findExecutable("hyprctl") : QString();
    m_roundingTimeout.setSingleShot(true);
    m_roundingTimeout.setInterval(500);
    connect(&m_roundingTimeout, &QTimer::timeout, &m_roundingProcess, &QProcess::kill);
    connect(&m_roundingProcess, &QProcess::errorOccurred, this, [this] { m_roundingTimeout.stop(); });
    connect(&m_roundingProcess, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        m_roundingTimeout.stop();
        const auto value = QJsonDocument::fromJson(m_roundingProcess.readAllStandardOutput()).object().value("int");
        if (code != 0 || status != QProcess::NormalExit || !value.isDouble() || value.toInt(-1) < 0)
            return;
        if (value.toInt() != m_radius) {
            m_radius = value.toInt();
            emit changed();
        }
    });
    m_roundingPoll.setInterval(1000);
    connect(&m_roundingPoll, &QTimer::timeout, this, &Theme::refreshRounding);
    if (!m_hyprctl.isEmpty())
        m_roundingPoll.start();
    reload();
}

Theme::~Theme() {
    if (m_roundingProcess.state() != QProcess::NotRunning) {
        m_roundingProcess.kill();
        m_roundingProcess.waitForFinished(1000);
    }
}

void Theme::refreshRounding() {
    if (m_hyprctl.isEmpty() || m_roundingProcess.state() != QProcess::NotRunning)
        return;
    m_roundingProcess.start(m_hyprctl, {"-j", "getoption", "decoration:rounding"});
    m_roundingTimeout.start();
}

// Every `key = "#rrggbb"` line, which is all of colors.toml that matters here.
QHash<QString, QString> Theme::readColors(const QString &path) {
    QHash<QString, QString> colors;
    static const QRegularExpression expression(R"re(^\s*([a-z_]+)\s*=\s*["'](#[0-9a-fA-F]{6})["'])re",
                                               QRegularExpression::MultilineOption);
    auto it = expression.globalMatch(readText(path));
    while (it.hasNext()) {
        const auto match = it.next();
        colors.insert(match.captured(1), match.captured(2));
    }
    return colors;
}

QString Theme::readAccent(const QString &path) {
    return readColors(path).value("accent", "#FFD60A");
}

// Light themes say `mode = "light"`; anything else is dark, like Omarchy's
// default.
bool Theme::readDark(const QString &path) {
    static const QRegularExpression expression(R"re(^\s*mode\s*=\s*["']light["'])re",
                                               QRegularExpression::MultilineOption);
    return !expression.match(readText(path)).hasMatch();
}

// The rem root the shell sizes all its type from, under [font] in shell.toml.
int Theme::readFontBaseSize(const QString &path) {
    bool inFont = false;
    for (const auto &raw : readText(path).split('\n')) {
        const QString line = raw.section('#', 0, 0).trimmed();
        if (line.startsWith('[')) {
            inFont = line == "[font]";
            continue;
        }
        if (inFont && line.section('=', 0, 0).trimmed() == "base-size") {
            bool ok = false;
            const int size = line.section('=', 1).trimmed().toInt(&ok);
            if (ok && size > 0)
                return size;
        }
    }
    return 12;
}

// The shell's base size is tuned for a 26px bar; a window reads better a
// step up, so the app's body text is the shell's 14/12 scale, rounded.
int Theme::fontSize() const {
    return qMax(8, qRound(m_fontBase * 14.0 / 12.0));
}

QString Theme::color(const QString &key) const {
    const auto value = m_colors.value(key);
    if (!value.isEmpty())
        return value;
    if (key == "muted" && m_colors.contains("dark_foreground"))
        return m_colors.value("dark_foreground");
    return (m_dark ? darkFallback() : lightFallback()).value(key);
}

// Black or white, whichever reads on the accent (WCAG relative luminance).
QString Theme::accentForeground() const {
    const QColor c(accent());
    auto linear = [](double v) { return v <= .04045 ? v / 12.92 : std::pow((v + .055) / 1.055, 2.4); };
    const double luminance = .2126 * linear(c.redF()) + .7152 * linear(c.greenF()) + .0722 * linear(c.blueF());
    return luminance > .179 ? "black" : "white";
}

void Theme::reload() {
    refreshRounding();
    const auto paths = m_watcher.files() + m_watcher.directories();
    if (!paths.isEmpty())
        m_watcher.removePaths(paths);

    // A theme switch replaces symlinks and files, so watch the parents too and
    // re-arm after every change. Watching the nearest existing ancestor means
    // installing Omarchy later works without restarting the app.
    QString ancestor = m_directory;
    while (!QFileInfo::exists(ancestor) && ancestor != "/")
        ancestor = QFileInfo(ancestor).absolutePath();
    const QString colorsPath = m_directory + "/theme/colors.toml";
    const QString shellPath = m_directory + "/theme/shell.toml";
    QStringList candidates{ancestor, QFileInfo(ancestor).absolutePath(), m_directory,
                           m_directory + "/theme", colorsPath, shellPath};
    candidates.removeDuplicates();
    for (const auto &path : candidates)
        if (QFileInfo::exists(path))
            m_watcher.addPath(path);

    const auto colors = readColors(colorsPath);
    const bool dark = readDark(colorsPath);
    const int fontBase = readFontBaseSize(shellPath);
    if (colors != m_colors || dark != m_dark || fontBase != m_fontBase) {
        m_colors = colors;
        m_dark = dark;
        m_fontBase = fontBase;
        emit changed();
    }
}
