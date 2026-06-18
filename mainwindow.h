#pragma once

#include <QMainWindow>
#include "thermal_monitor.h"
#include "platform/soc/mock_soc.hpp"
#include "c_impl/qt_adapter.hpp"

class QLabel;
class QSpinBox;
class QComboBox;
class QPushButton;
class QTabWidget;
class QTimer;

struct TabWidgets {
    QLabel*      ledGreen        = nullptr;
    QLabel*      ledYellow       = nullptr;
    QLabel*      ledRed          = nullptr;
    QSpinBox*    tempInput       = nullptr;
    QComboBox*   sensorTypeCombo = nullptr;
    QPushButton* startBtn        = nullptr;
    QPushButton* stopBtn         = nullptr;
};

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onStartCpp();
    void onStopCpp();
    void onStartC();
    void onStopC();
    void onUpdateLEDs();

private:
    QWidget* buildTab(TabWidgets& w);
    void     applyLED(QLabel* label, int value, const char* color);

    // ── C++ implementation ─────────────────────────────────────────────────
    MockSoC        soc_;
    ThermalMonitor monitor_;
    TabWidgets     cppW_;

    // ── C implementation ───────────────────────────────────────────────────
    CThermalMonitorAdapter cMonitor_;
    TabWidgets             cW_;

    QTimer* pollTimer_ = nullptr;
};
