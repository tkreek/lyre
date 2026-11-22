#pragma once

#include <QFileSystemWatcher>
#include <QHash>
#include <QObject>
#include <QProcess>
#include <QTimer>

// The current Omarchy theme, live: the palette from colors.toml, the type
// scale from the shell's shell.toml, and Hyprland's window rounding. Every
// color the window paints comes from here, so a theme switch repaints the
// whole app the way it repaints the terminal and the bar.
class Theme : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool dark READ dark NOTIFY changed)
    Q_PROPERTY(QString accent READ accent NOTIFY changed)
    Q_PROPERTY(QString accentForeground READ accentForeground NOTIFY changed)
    Q_PROPERTY(QString background READ background NOTIFY changed)
    Q_PROPERTY(QString surface READ surface NOTIFY changed)
    Q_PROPERTY(QString raised READ raised NOTIFY changed)
    Q_PROPERTY(QString foreground READ foreground NOTIFY changed)
    Q_PROPERTY(QString muted READ muted NOTIFY changed)
    Q_PROPERTY(QString selection READ selection NOTIFY changed)
    Q_PROPERTY(QString danger READ danger NOTIFY changed)
    Q_PROPERTY(QString fontFamily READ fontFamily CONSTANT)
    Q_PROPERTY(int fontSize READ fontSize NOTIFY changed)
    Q_PROPERTY(int radius READ radius NOTIFY changed)

public:
    explicit Theme(const QString &currentDirectory = {}, QObject *parent = nullptr, const QString &hyprctl = {});
    ~Theme() override;

    bool dark() const { return m_dark; }
    QString accent() const { return color("accent"); }
    QString accentForeground() const;
    QString background() const { return color("background"); }
    QString surface() const { return color("darker_background"); }
    QString raised() const { return color("lighter_background"); }
    QString foreground() const { return color("foreground"); }
    QString muted() const { return color("muted"); }
    QString selection() const { return color("selection"); }
    QString danger() const { return color("red"); }
    // The fontconfig alias `omarchy font set` writes, the same family the bar
    // and the Omarchy menus draw with.
    QString fontFamily() const { return QStringLiteral("monospace"); }
    int fontSize() const;
    int radius() const { return m_radius; }

    static QString readAccent(const QString &path);
    static QHash<QString, QString> readColors(const QString &path);
    static bool readDark(const QString &path);
    static int readFontBaseSize(const QString &path);

signals:
    void changed();

private:
    QString color(const QString &key) const;
    void reload();
    void refreshRounding();

    QString m_directory;
    QHash<QString, QString> m_colors;
    bool m_dark = true;
    int m_fontBase = 12;
    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
    QString m_hyprctl;
    int m_radius = 0;
    QProcess m_roundingProcess;
    QTimer m_roundingPoll, m_roundingTimeout;
};
