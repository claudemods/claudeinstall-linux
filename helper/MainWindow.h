// SPDX-FileCopyrightText: no
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMap>
#include <QString>

class QCheckBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QRadioButton;
class QSpinBox;
class QStackedWidget;

/**
 * Setup helper for build.sh: collects the build choices and saves them to build.config
 * (KEY=value lines) next to build.sh, which "./build.sh build" then reads.
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow( const QString& projectDir, QWidget* parent = nullptr );

private:
    QWidget* createTypePage();
    QWidget* createInterfacePage();
    QWidget* createOptionsPage();
    QWidget* createReviewPage();

    QString configPath() const;
    QMap< QString, QString > values() const;
    QStringList legacyFlags() const;

    void loadConfig();
    void resetDefaults();
    bool saveConfig();
    void saveAndBuild();
    void refreshReview();
    void updateWarnings();

    QString m_dir;
    QListWidget* m_nav = nullptr;
    QStackedWidget* m_pages = nullptr;

    QRadioButton* m_modeButtons[ 3 ] = {};
    QRadioButton* m_interfaceButtons[ 3 ] = {};
    QLabel* m_distroWarning = nullptr;

    QCheckBox* m_deps = nullptr;
    QCheckBox* m_clean = nullptr;
    QCheckBox* m_test = nullptr;
    QCheckBox* m_install = nullptr;
    QCheckBox* m_package = nullptr;
    QSpinBox* m_jobs = nullptr;
    QLineEdit* m_buildDir = nullptr;
    QLineEdit* m_cmakeArgs = nullptr;
    QLineEdit* m_neededFiles = nullptr;

    QPlainTextEdit* m_review = nullptr;
};

#endif
