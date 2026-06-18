#include "mainwindow.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSpinBox>
#include <QComboBox>
#include <QPushButton>
#include <QTabWidget>
#include <QTimer>

// ─────────────────────────────────────────────────────────────────────────────
// Construction / destruction
// ─────────────────────────────────────────────────────────────────────────────

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      monitor_(soc_.os)
{
    setWindowTitle("Thermal Monitor Demo");
    resize(480, 480);

    auto* tabs = new QTabWidget(this);
    setCentralWidget(tabs);

    QWidget* cppTab = buildTab(cppW_);
    QWidget* cTab   = buildTab(cW_);
    tabs->addTab(cppTab, "C++ / Linux SoM");
    tabs->addTab(cTab,   "C / POSIX");

    // ── C++ tab wiring ───────────────────────────────────────────────────
    connect(cppW_.tempInput, QOverload<int>::of(&QSpinBox::valueChanged),
            [this](int v) {
                soc_.tempSensor->setRawValue(static_cast<int16_t>(v));
            });
    connect(cppW_.sensorTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            [this](int idx) {
                uint8_t b = static_cast<uint8_t>(cppW_.sensorTypeCombo->itemData(idx).toInt());
                soc_.eepromFile->setByte(0, b);
            });
    connect(cppW_.startBtn, &QPushButton::clicked, this, &MainWindow::onStartCpp);
    connect(cppW_.stopBtn,  &QPushButton::clicked, this, &MainWindow::onStopCpp);

    // ── C tab wiring ─────────────────────────────────────────────────────
    connect(cW_.tempInput, QOverload<int>::of(&QSpinBox::valueChanged),
            [this](int v) {
                cMonitor_.setRawValue(static_cast<int16_t>(v));
            });
    connect(cW_.sensorTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            [this](int idx) {
                auto t = static_cast<SensorType>(cW_.sensorTypeCombo->itemData(idx).toInt());
                cMonitor_.setSensorType(t);
            });
    connect(cW_.startBtn, &QPushButton::clicked, this, &MainWindow::onStartC);
    connect(cW_.stopBtn,  &QPushButton::clicked, this, &MainWindow::onStopC);

    // ── LED poll timer (shared, 50 ms) ────────────────────────────────────
    pollTimer_ = new QTimer(this);
    connect(pollTimer_, &QTimer::timeout, this, &MainWindow::onUpdateLEDs);
    pollTimer_->start(50);
}

MainWindow::~MainWindow() {
    monitor_.stop();
    cMonitor_.stop();
}

// ─────────────────────────────────────────────────────────────────────────────
// Tab builder  — identical layout for both tabs
// ─────────────────────────────────────────────────────────────────────────────

QWidget* MainWindow::buildTab(TabWidgets& w) {
    auto* page = new QWidget;
    auto* root = new QVBoxLayout(page);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(10);

    // ── Sensor simulation group ───────────────────────────────────────────
    auto* sensorGroup  = new QGroupBox("Sensor Simulation (mock)");
    auto* sensorLayout = new QVBoxLayout(sensorGroup);

    auto* typeRow = new QHBoxLayout;
    typeRow->addWidget(new QLabel("Sensor Type:"));
    w.sensorTypeCombo = new QComboBox;
    w.sensorTypeCombo->addItem("Type A  (1 °C / unit)",
                               static_cast<int>(SensorType::TypeA_1_Per_Unit));
    w.sensorTypeCombo->addItem("Type B  (0.1 °C / unit)",
                               static_cast<int>(SensorType::TypeB_0_1_Per_Unit));
    typeRow->addWidget(w.sensorTypeCombo);
    sensorLayout->addLayout(typeRow);

    auto* adrRow = new QHBoxLayout;
    adrRow->addWidget(new QLabel("Raw ADC Value:"));
    w.tempInput = new QSpinBox;
    w.tempInput->setRange(-32768, 32767);
    w.tempInput->setValue(250);
    adrRow->addWidget(w.tempInput);
    sensorLayout->addLayout(adrRow);

    root->addWidget(sensorGroup);

    // ── LED panel group ───────────────────────────────────────────────────
    auto* ledGroup  = new QGroupBox("Operator Panel (GPIO mock)");
    auto* ledLayout = new QVBoxLayout(ledGroup);
    auto* ledRow    = new QHBoxLayout;

    auto makeLed = [](const QString& letter) {
        auto* lbl = new QLabel(letter);
        lbl->setAlignment(Qt::AlignCenter);
        lbl->setFixedSize(64, 64);
        lbl->setStyleSheet("QLabel { background-color:#333; color:white; "
                           "border-radius:32px; font-weight:bold; font-size:20px; }");
        return lbl;
    };

    w.ledGreen  = makeLed("G");
    w.ledYellow = makeLed("Y");
    w.ledRed    = makeLed("R");

    ledRow->addStretch();
    ledRow->addWidget(w.ledGreen);
    ledRow->addSpacing(8);
    ledRow->addWidget(w.ledYellow);
    ledRow->addSpacing(8);
    ledRow->addWidget(w.ledRed);
    ledRow->addStretch();
    ledLayout->addLayout(ledRow);
    root->addWidget(ledGroup);

    // ── Start / Stop buttons ──────────────────────────────────────────────
    auto* btnRow = new QHBoxLayout;
    w.startBtn = new QPushButton("Start Monitor");
    w.stopBtn  = new QPushButton("Stop Monitor");
    w.stopBtn->setEnabled(false);
    btnRow->addStretch();
    btnRow->addWidget(w.startBtn);
    btnRow->addWidget(w.stopBtn);
    btnRow->addStretch();
    root->addLayout(btnRow);

    root->addStretch();
    return page;
}

// ─────────────────────────────────────────────────────────────────────────────
// Slots — C++ tab
// ─────────────────────────────────────────────────────────────────────────────

void MainWindow::onStartCpp() {
    monitor_.start();
    cppW_.startBtn->setEnabled(false);
    cppW_.stopBtn ->setEnabled(true);
}

void MainWindow::onStopCpp() {
    monitor_.stop();
    cppW_.startBtn->setEnabled(true);
    cppW_.stopBtn ->setEnabled(false);
}

// ─────────────────────────────────────────────────────────────────────────────
// Slots — C tab
// ─────────────────────────────────────────────────────────────────────────────

void MainWindow::onStartC() {
    cMonitor_.start();
    cW_.startBtn->setEnabled(false);
    cW_.stopBtn ->setEnabled(true);
}

void MainWindow::onStopC() {
    cMonitor_.stop();
    cW_.startBtn->setEnabled(true);
    cW_.stopBtn ->setEnabled(false);
}

// ─────────────────────────────────────────────────────────────────────────────
// LED poll — updates both tabs from their respective panels
// ─────────────────────────────────────────────────────────────────────────────

void MainWindow::applyLED(QLabel* label, int value, const char* color) {
    if (value) {
        label->setStyleSheet(
            QString("QLabel { background-color:%1; color:black; "
                    "border-radius:32px; font-weight:bold; font-size:20px; }").arg(color));
    } else {
        label->setStyleSheet(
            "QLabel { background-color:#333; color:white; "
            "border-radius:32px; font-weight:bold; font-size:20px; }");
    }
}

void MainWindow::onUpdateLEDs() {
    // C++ tab — read from mock GPIO files
    applyLED(cppW_.ledGreen,  soc_.gpioGreen->getValue(),  "#00ff00");
    applyLED(cppW_.ledYellow, soc_.gpioYellow->getValue(), "#ffff00");
    applyLED(cppW_.ledRed,    soc_.gpioRed->getValue(),    "#ff0000");

    // C tab — read from MockOs gpio array
    applyLED(cW_.ledGreen,  cMonitor_.getGpioValue(10), "#00ff00");
    applyLED(cW_.ledYellow, cMonitor_.getGpioValue(11), "#ffff00");
    applyLED(cW_.ledRed,    cMonitor_.getGpioValue(12), "#ff0000");
}
