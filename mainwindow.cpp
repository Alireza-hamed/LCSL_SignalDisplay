#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "consts.h"
#include <QDebug>
#include <QMetaType>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QMessageBox>
#include <QColorDialog>
#include <QFileDialog>
#include <QDateTime>
#include <QDialog>
#include <QFormLayout>
#include <QLineEdit>
#include <QDialogButtonBox>
#include <QTime>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QListWidgetItem>
#include <QStringList>
#include <QHash>
#include <QInputDialog>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_isMonitoring(false)
    , m_isScrollMode(false)
    , m_isFrozen(false)
    , m_isFindMode(false)
    , m_findModeChannel(0)
    , m_gridMode(1)
    , m_activeGroupPath("")
    , m_isBgImageLoaded(false)
    , m_isSyncingScroll(false)
{
    ui->setupUi(this);
    this->setWindowTitle("LCSL Oscilloscope - وضعیت: بدون گروه (لطفاً لاگین کنید)");
    this->setFocusPolicy(Qt::StrongFocus);
    ui->customPlot->setFocusPolicy(Qt::StrongFocus);

    for (int i = 0; i < 4; i++) {
        m_lastPlottedSample[i] = 0;
    }

    m_freezeTimer = new QTimer(this);
    m_freezeTimer->setSingleShot(true);
    connect(m_freezeTimer, &QTimer::timeout, this, [this]() {
        m_isFrozen = false;
    });

    m_tempLogFile = new QFile(QDir::tempPath() + "/lcsl_oscilloscope_temp.csv", this);
    m_tempLogStream = new QTextStream(m_tempLogFile);
    m_totalLoggedSamples = 0;

    this->setStyleSheet(this->styleSheet() + " QMessageBox QLabel, QDialog QLabel { color: black; font-weight: bold; }");
    m_channels.resize(Channel::NUMBER_OF_CHANNELS);

    setupPlotGraphics();
    setupUIElements();

    // راه‌اندازی تب‌ها و متصل کردن ReportEngine
    setupTabs();

    workerThread = new QThread(this);
    usbWorker = new UsbWorker(m_channels);
    usbWorker->moveToThread(workerThread);

    connect(workerThread, &QThread::started, usbWorker, &UsbWorker::startReading);
    connect(workerThread, &QThread::finished, usbWorker, &QObject::deleteLater);

    connect(usbWorker, &UsbWorker::errorOccurred, this, [this](QString msg){
        QMessageBox::critical(this, "خطای سخت‌افزار", msg + "\nلطفا دستگاه را متصل کنید.");
        m_isMonitoring = false;
    });

    qRegisterMetaType<QVector<unsigned char>>("QVector<unsigned char>");
    connect(this, &MainWindow::sendDeviceSettings, usbWorker, &UsbWorker::updateDeviceSettings);

    renderTimer = new QTimer(this);
    connect(renderTimer, &QTimer::timeout, this, &MainWindow::onDataReady);
    renderTimer->start(33);

    connect(ui->customPlot, &QCustomPlot::mousePress, this, &MainWindow::onGraphClicked);

    workerThread->start();
}

MainWindow::~MainWindow() {
    if (usbWorker) {
        usbWorker->stopReading();
    }
    workerThread->quit();
    workerThread->wait();

    if (m_tempLogFile && m_tempLogFile->exists()) {
        if (m_tempLogFile->isOpen()) m_tempLogFile->close();
        m_tempLogFile->remove();
    }
    delete ui;
}

void MainWindow::setupTabs() {
    // جداسازی محیط اسیلوسکوپ
    QWidget *oscWidget = new QWidget();
    oscWidget->setLayout(ui->gridLayout);

    mainTabWidget = new QTabWidget(this);

    // اصلاح استایل تب‌ها: افزودن min-width و min-height برای جلوگیری از بریدگی متن
    mainTabWidget->setStyleSheet(
        "QTabWidget::pane { border: 1px solid #444; }"
        "QTabBar::tab { "
        "   background: #333; "
        "   color: #ccc; "
        "   min-width: 160px; "  /* عریض‌تر کردن کادر تب */
        "   min-height: 40px; "  /* بلندتر کردن کادر تب */
        "   padding: 5px; "
        "   font-family: Tahoma; "
        "   font-size: 11pt; "
        "}"
        "QTabBar::tab:selected { "
        "   background: #007ACC; "
        "   color: #ffffff; "
        "   font-weight: bold; "
        "}"
    );

    mainTabWidget->addTab(oscWidget, "اسیلوسکوپ و کنترل");

    // نمونه‌سازی موتور گزارش کار و افزودن به تب دوم
    m_reportEngine = new ReportEngine(this);
    mainTabWidget->addTab(m_reportEngine, "گزارش کار و عملیات");

    this->setCentralWidget(mainTabWidget);
}

void MainWindow::setupUIElements() {
    ui_chkSelect[0] = ui->checkBox;     ui_chkSelect[1] = ui->checkBox_2;
    ui_chkSelect[2] = ui->checkBox_3;   ui_chkSelect[3] = ui->checkBox_4;

    ui_cmbVD[0] = ui->comboBox;         ui_cmbVD[1] = ui->comboBox_7;
    ui_cmbVD[2] = ui->comboBox_8;       ui_cmbVD[3] = ui->comboBox_9;

    ui_cmbOffset[0] = ui->comboBox_2;   ui_cmbOffset[1] = ui->comboBox_10;
    ui_cmbOffset[2] = ui->comboBox_11;  ui_cmbOffset[3] = ui->comboBox_12;

    for (int i = 0; i < 4; i++) {
        for (const QString& str : Consts::VD_STR) ui_cmbVD[i]->addItem(str);
        for (const QString& str : Consts::OFFSET_STR) ui_cmbOffset[i]->addItem(str);

        ui_cmbVD[i]->setCurrentIndex(3);
        ui_cmbOffset[i]->setCurrentIndex(6);
        ui_chkSelect[i]->setChecked(i == 0);

        connect(ui_chkSelect[i], &QCheckBox::stateChanged, this, &MainWindow::updateChannelSettings);
        connect(ui_cmbVD[i], QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::updateChannelSettings);
        connect(ui_cmbOffset[i], QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::updateChannelSettings);
    }

    for (const QString& str : Consts::TD_STR) {
        ui->cmbTimeBase->addItem(str);
    }

    ui->cmbTimeBase->setCurrentIndex(Consts::TD_Index);
    connect(ui->cmbTimeBase, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::on_cmbTimeBase_currentIndexChanged);

    updateChannelSettings();
}

void MainWindow::updateChannelSettings() {
    unsigned char channelsMask = 0;
    QVector<unsigned char> vdValues;

    for (int i = 0; i < Channel::NUMBER_OF_CHANNELS; i++) {
        bool isSelected = ui_chkSelect[i]->isChecked();
        m_channels[i].selected.store(isSelected);
        m_channels[i].vdIndex.store(ui_cmbVD[i]->currentIndex());
        m_channels[i].offIndex.store(ui_cmbOffset[i]->currentIndex());

        if (isSelected) channelsMask |= (1 << i);

        unsigned char chVD = (unsigned char)(0x10 * (i + 1));
        chVD |= (unsigned char)ui_cmbVD[i]->currentIndex();
        vdValues.append(chVD);
    }

    emit sendDeviceSettings(m_tmr0h, m_tmr0l, m_psa, channelsMask, vdValues);
}

void MainWindow::on_pushButton_30_clicked() {
    m_isMonitoring = true;
    m_isFindMode = false;
    if (m_tempLogFile->open(QIODevice::WriteOnly | QIODevice::Text)) {
        m_totalLoggedSamples = 0;
        *m_tempLogStream << "SampleIndex,CH1(V),CH2(V),CH3(V),CH4(V)\n";
    }
}

void MainWindow::on_pushButton_31_clicked() {
    m_isMonitoring = false;
    if (m_tempLogFile->isOpen()) m_tempLogFile->close();
}

void MainWindow::onDataReady() {
    if (!m_isMonitoring) return;

    if (m_isScrollMode) {
        bool shouldReplot = false;
        long long maxTotalSamples = 0;

        for (int ch = 0; ch < Channel::NUMBER_OF_CHANNELS; ch++) {
            if (!m_channels[ch].selected.load()) {
                ui->customPlot->graph(ch)->data()->clear();
                continue;
            }
            long long currentTotal = m_channels[ch].totalSamples.load();
            if (currentTotal > maxTotalSamples) maxTotalSamples = currentTotal;
        }

        for (int ch = 0; ch < Channel::NUMBER_OF_CHANNELS; ch++) {
            if (!m_channels[ch].selected.load()) continue;

            long long currentTotal = m_channels[ch].totalSamples.load();
            int pointsToDraw = 1500;
            if (currentTotal < pointsToDraw) pointsToDraw = currentTotal;

            if (pointsToDraw > 0) {
                QVector<double> xData(pointsToDraw), yData(pointsToDraw);
                long long startAbsIndex = currentTotal - pointsToDraw;
                int currentWritePtr = m_channels[ch].writePtr.load();
                double offsetVal = Consts::OFFSET_VAL[m_channels[ch].offIndex.load()];
                double currentVD = Consts::VD_VAL[m_channels[ch].vdIndex.load()];
                int sign = m_channels[ch].sign;

                for (int i = 0; i < pointsToDraw; i++) {
                    int readIndex = (currentWritePtr - pointsToDraw + i + Channel::BUFFER_LENGTH) % Channel::BUFFER_LENGTH;
                    xData[i] = startAbsIndex + i;
                    yData[i] = (m_channels[ch].data[readIndex] * sign + offsetVal) / currentVD;
                }
                ui->customPlot->graph(ch)->setData(xData, yData);
                shouldReplot = true;
            }
        }

        if (shouldReplot) {
            double upperRange = ui->customPlot->xAxis->range().upper;
            double lowerRange = ui->customPlot->xAxis->range().lower;

            bool isTracking = (upperRange >= maxTotalSamples - 100);
            bool isOutOfView = (upperRange < maxTotalSamples - 1500) || (lowerRange > maxTotalSamples);

            if (maxTotalSamples < 500) {
                ui->customPlot->xAxis->setRange(0, 500);
            } else if (isTracking || isOutOfView) {
                ui->customPlot->xAxis->setRange(maxTotalSamples - 500, maxTotalSamples);
            } else {
                long long minVisible = qMax(0LL, maxTotalSamples - 1500);
                if (lowerRange < minVisible) {
                    ui->customPlot->xAxis->setRange(minVisible, minVisible + 500);
                }
            }
            ui->customPlot->replot();
        }
    } else {
        if (m_isFrozen) return;

        bool allReady = true;
        bool anyActive = false;

        for (int ch = 0; ch < Channel::NUMBER_OF_CHANNELS; ch++) {
            if (m_channels[ch].selected.load()) {
                anyActive = true;
                if (m_channels[ch].totalSamples.load() - m_lastPlottedSample[ch] < 500) {
                    allReady = false;
                    break;
                }
            } else {
                ui->customPlot->graph(ch)->data()->clear();
            }
        }

        if (anyActive && allReady) {
            for (int ch = 0; ch < Channel::NUMBER_OF_CHANNELS; ch++) {
                if (!m_channels[ch].selected.load()) continue;

                int pointsToDraw = 500;
                QVector<double> xData(pointsToDraw), yData(pointsToDraw);
                int currentWritePtr = m_channels[ch].writePtr.load();
                double offsetVal = Consts::OFFSET_VAL[m_channels[ch].offIndex.load()];
                double currentVD = Consts::VD_VAL[m_channels[ch].vdIndex.load()];
                int sign = m_channels[ch].sign;

                for (int i = 0; i < pointsToDraw; i++) {
                    int readIndex = (currentWritePtr - pointsToDraw + i + Channel::BUFFER_LENGTH) % Channel::BUFFER_LENGTH;
                    xData[i] = i;
                    yData[i] = (m_channels[ch].data[readIndex] * sign + offsetVal) / currentVD;
                }
                ui->customPlot->graph(ch)->setData(xData, yData);
                m_lastPlottedSample[ch] = m_channels[ch].totalSamples.load();
            }

            ui->customPlot->xAxis->setRange(0, 500);
            ui->customPlot->replot();

            m_isFrozen = true;
            m_freezeTimer->start(500);
        }
    }

    if (m_tempLogFile->isOpen()) {
        Channel::readSamples(m_channels);

        int maxFetched = 0;
        for (int ch = 0; ch < Channel::NUMBER_OF_CHANNELS; ch++) {
            if (m_channels[ch].selected.load() && m_channels[ch].numberOfFetchedData > maxFetched) {
                maxFetched = m_channels[ch].numberOfFetchedData;
            }
        }

        for (int i = 0; i < maxFetched; i++) {
            *m_tempLogStream << m_totalLoggedSamples++;

            for (int ch = 0; ch < Channel::NUMBER_OF_CHANNELS; ch++) {
                *m_tempLogStream << ",";
                if (m_channels[ch].selected.load() && i < m_channels[ch].numberOfFetchedData) {
                    *m_tempLogStream << Channel::fetchedData[ch][i];
                }
            }
            *m_tempLogStream << "\n";
        }
    }
}

void MainWindow::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_G) {
        m_gridMode = (m_gridMode + 1) % 3;
        updateGridDisplay();
        return;
    }

    if (event->key() == Qt::Key_F) {
        if (m_isMonitoring) {
            QMessageBox::warning(this, "اخطار", "برای استفاده از ابزار اندازه‌گیری و Find Mode، لطفاً ابتدا مانیتورینگ را متوقف کنید.");
            return;
        }

        m_isFindMode = true;
        this->setFocus();

        if (!m_currentPoint.isValid) {
            long long centerX = qRound(ui->customPlot->xAxis->range().center());
            updateCursorPosition(centerX, true);
        } else {
            updateCursorPosition(m_currentPoint.xCoord, false);
        }
        return;
    }

    if (event->key() == Qt::Key_F) {
        m_isFindMode = false;
        if (m_textInfo->visible()) {
            QString infoText = m_textInfo->text();
            infoText.replace("Find Mode [ON]\n", "");
            m_textInfo->setText(infoText);
            ui->customPlot->replot();
        }
        return;
    }

    if (m_isFindMode) {
        if (event->key() >= Qt::Key_1 && event->key() <= Qt::Key_4) {
            int ch = event->key() - Qt::Key_1;
            if (m_channels[ch].selected.load()) {
                m_findModeChannel = ch;
                if (m_currentPoint.isValid) {
                    updateCursorPosition(m_currentPoint.xCoord, false);
                }
            }
            return;
        }

        if (m_currentPoint.isValid) {
            long long currentX = m_currentPoint.xCoord;

            if (event->key() == Qt::Key_Left) {
                updateCursorPosition(currentX - 1, false);
                return;
            } else if (event->key() == Qt::Key_Right) {
                updateCursorPosition(currentX + 1, false);
                return;
            }
        }
    }

    if (m_isScrollMode && !m_isFindMode && !m_isMonitoring) {
        if (event->key() == Qt::Key_Left) {
            m_scrollBar->setValue(m_scrollBar->value() - 50);
        } else if (event->key() == Qt::Key_Right) {
            m_scrollBar->setValue(m_scrollBar->value() + 50);
        }
    }

    QMainWindow::keyPressEvent(event);
}

void MainWindow::updateGridDisplay() {
    if (m_gridMode == 0) {
        ui->customPlot->xAxis->grid()->setVisible(false);
        ui->customPlot->yAxis->grid()->setVisible(false);
        ui->customPlot->xAxis->grid()->setSubGridVisible(false);
        ui->customPlot->yAxis->grid()->setSubGridVisible(false);
    } else if (m_gridMode == 1) {
        ui->customPlot->xAxis->grid()->setVisible(true);
        ui->customPlot->yAxis->grid()->setVisible(true);
        ui->customPlot->xAxis->grid()->setSubGridVisible(false);
        ui->customPlot->yAxis->grid()->setSubGridVisible(false);

        ui->customPlot->xAxis->grid()->setPen(QPen(QColor(70, 70, 70), 1, Qt::DotLine));
        ui->customPlot->yAxis->grid()->setPen(QPen(QColor(70, 70, 70), 1, Qt::DotLine));
    } else if (m_gridMode == 2) {
        ui->customPlot->xAxis->grid()->setVisible(true);
        ui->customPlot->yAxis->grid()->setVisible(true);
        ui->customPlot->xAxis->grid()->setSubGridVisible(true);
        ui->customPlot->yAxis->grid()->setSubGridVisible(true);

        ui->customPlot->xAxis->grid()->setPen(QPen(QColor(90, 90, 90), 1, Qt::SolidLine));
        ui->customPlot->yAxis->grid()->setPen(QPen(QColor(90, 90, 90), 1, Qt::SolidLine));
        ui->customPlot->xAxis->grid()->setSubGridPen(QPen(QColor(50, 50, 50), 1, Qt::DotLine));
        ui->customPlot->yAxis->grid()->setSubGridPen(QPen(QColor(50, 50, 50), 1, Qt::DotLine));
    }

    ui->customPlot->replot();
}

void MainWindow::updateCursorPosition(long long absX, bool updateOldPoint) {
    if (!m_channels[m_findModeChannel].selected.load()) {
        for(int i = 0; i < Channel::NUMBER_OF_CHANNELS; i++) {
            if(m_channels[i].selected.load()) {
                m_findModeChannel = i;
                break;
            }
        }
    }
    if (!m_channels[m_findModeChannel].selected.load()) return;

    long long currentTotal = m_channels[m_findModeChannel].totalSamples.load();
    if (currentTotal == 0) return;

    long long minValidX = qMax(0LL, currentTotal - Channel::BUFFER_LENGTH);
    long long maxValidX = qMax(0LL, currentTotal - 1);
    if (absX < minValidX) absX = minValidX;
    if (absX > maxValidX) absX = maxValidX;

    long long diff = currentTotal - absX;
    int writePtr = m_channels[m_findModeChannel].writePtr.load();
    int readIndex = (writePtr - diff) % Channel::BUFFER_LENGTH;
    if (readIndex < 0) readIndex += Channel::BUFFER_LENGTH;

    double offsetVal = Consts::OFFSET_VAL[m_channels[m_findModeChannel].offIndex.load()];
    double currentVD = Consts::VD_VAL[m_channels[m_findModeChannel].vdIndex.load()];
    int sign = m_channels[m_findModeChannel].sign;

    double rawY = m_channels[m_findModeChannel].data[readIndex];
    double finalY = (rawY * sign + offsetVal) / currentVD;

    if (updateOldPoint && m_currentPoint.isValid) {
        m_oldPoint = m_currentPoint;
        m_tracerOld->setGraph(nullptr);
        m_tracerOld->position->setCoords(m_oldPoint.xCoord, m_oldPoint.y);
        m_tracerOld->setVisible(true);
    }

    int tdIndex = Consts::TD_Index;
    double timeScale = Consts::TD_VAL[tdIndex];

    m_currentPoint.xCoord = absX;
    m_currentPoint.t = absX * timeScale / 50.0;
    m_currentPoint.y = finalY;
    m_currentPoint.isValid = true;
    m_currentPoint.chNumber = m_findModeChannel;

    m_tracerCurrent->setGraph(nullptr);
    m_tracerCurrent->position->setCoords(m_currentPoint.xCoord, m_currentPoint.y);
    m_tracerCurrent->setVisible(true);

    QString infoText = "";
    if (m_isFindMode) {
        infoText += "Find Mode [ON]\n";
    }

    infoText += QString("Current: t= %1  y= %2  (CH %3)")
                   .arg(m_currentPoint.t)
                   .arg(m_currentPoint.y)
                   .arg(m_findModeChannel + 1);

    if (m_oldPoint.isValid) {
        double dt = m_currentPoint.t - m_oldPoint.t;
        double dy = m_currentPoint.y - m_oldPoint.y;
        infoText += QString("\nAnchor: t= %1  y= %2")
                        .arg(m_oldPoint.t)
                        .arg(m_oldPoint.y);
        infoText += QString("\n dt= %1  dy= %2")
                        .arg(dt)
                        .arg(dy);
    }

    m_textInfo->setText(infoText);
    m_textInfo->setVisible(true);

    double lowerRange = ui->customPlot->xAxis->range().lower;
    double upperRange = ui->customPlot->xAxis->range().upper;

    if (absX < lowerRange || absX > upperRange) {
        double newLower = absX - 250;

        long long maxTotalSamples = 0;
        for (int ch = 0; ch < Channel::NUMBER_OF_CHANNELS; ch++) {
            if (m_channels[ch].selected.load()) {
                long long chTotal = m_channels[ch].totalSamples.load();
                if (chTotal > maxTotalSamples) maxTotalSamples = chTotal;
            }
        }

        long long minVisible = qMax(0LL, maxTotalSamples - 1500);
        if (newLower < minVisible) newLower = minVisible;
        if (newLower > maxTotalSamples - 500) newLower = qMax(0LL, maxTotalSamples - 500);

        ui->customPlot->xAxis->setRange(newLower, newLower + 500);

        m_scrollBar->blockSignals(true);
        m_scrollBar->setValue(qRound(newLower));
        m_scrollBar->blockSignals(false);
    }

    ui->customPlot->replot();
}

void MainWindow::onGraphClicked(QMouseEvent *event) {
    if (m_isMonitoring) return;

    double clickX = ui->customPlot->xAxis->pixelToCoord(event->pos().x());
    double clickY = ui->customPlot->yAxis->pixelToCoord(event->pos().y());

    int closestChannel = -1;
    double minDistanceY = 1e9;

    for (int ch = 0; ch < Channel::NUMBER_OF_CHANNELS; ch++) {
        if (!m_channels[ch].selected.load()) continue;

        QCPGraph *graph = ui->customPlot->graph(ch);
        if (!graph || graph->data()->isEmpty()) continue;

        auto it = graph->data()->findBegin(clickX);
        if (it != graph->data()->constEnd()) {
            double distanceY = qAbs(it->value - clickY);
            if (distanceY < minDistanceY && distanceY < 2.0) {
                minDistanceY = distanceY;
                closestChannel = ch;
            }
        }
    }

    if (closestChannel != -1) {
        m_findModeChannel = closestChannel;
    }

    m_isFindMode = true;
    this->setFocus();

    long long absX = qRound(clickX);
    updateCursorPosition(absX, true);
}

void MainWindow::setupPlotGraphics() {
    ui->customPlot->setBackground(QBrush(QColor(0, 0, 0)));
    ui->customPlot->xAxis->setBasePen(QPen(Qt::white, 1));
    ui->customPlot->yAxis->setBasePen(QPen(Qt::white, 1));
    ui->customPlot->xAxis->setTickPen(QPen(Qt::white, 1));
    ui->customPlot->yAxis->setTickPen(QPen(Qt::white, 1));
    ui->customPlot->xAxis->setSubTickPen(QPen(Qt::white, 1));
    ui->customPlot->yAxis->setSubTickPen(QPen(Qt::white, 1));
    ui->customPlot->xAxis->setTickLabelColor(Qt::white);
    ui->customPlot->yAxis->setTickLabelColor(Qt::white);

    ui->customPlot->xAxis->grid()->setZeroLinePen(Qt::NoPen);
    ui->customPlot->yAxis->grid()->setZeroLinePen(Qt::NoPen);
    updateGridDisplay();

    ui->customPlot->setInteractions(0);

    m_scrollBar = new QScrollBar(Qt::Horizontal, this);
    m_scrollBar->setStyleSheet(
        "QScrollBar:horizontal { background: #252526; height: 18px; border: 1px solid #3a3a3a; }"
        "QScrollBar::handle:horizontal { background: #333337; border: 1px solid #555555; border-radius: 4px; min-width: 40px; margin: 2px; }"
        "QScrollBar::handle:horizontal:hover { background: #3f3f46; border: 1px solid #007ACC; }"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0px; }"
        "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: none; }"
    );

    QVBoxLayout *plotLayout = new QVBoxLayout();
    QLayoutItem *plotItem = ui->gridLayout->itemAtPosition(0, 0);

    if (plotItem) {
        ui->gridLayout->removeItem(plotItem);
        plotLayout->addWidget(plotItem->widget());
    }
    plotLayout->addWidget(m_scrollBar);
    ui->gridLayout->addLayout(plotLayout, 0, 0);

    m_scrollBar->setVisible(false);
    connect(m_scrollBar, &QScrollBar::valueChanged, this, &MainWindow::onScrollBarChanged);
    connect(ui->customPlot->xAxis, QOverload<const QCPRange&>::of(&QCPAxis::rangeChanged), this, &MainWindow::onAxisRangeChanged);

    QColor channelColors[4] = {
        QColor(255, 165, 0),
        QColor(255, 255, 0),
        QColor(255, 0, 0),
        QColor(255, 222, 173)
    };

    QPushButton* colorButtons[4] = {
        ui->pushButton_2, ui->pushButton_3, ui->pushButton_4, ui->pushButton_5
    };

    for (int i = 0; i < 4; i++) {
        m_channels[i].color = channelColors[i];
        ui->customPlot->addGraph();
        ui->customPlot->graph(i)->setPen(QPen(channelColors[i], 2));
        colorButtons[i]->setStyleSheet(QString("background-color: %1").arg(channelColors[i].name()));
    }

    ui->customPlot->xAxis->setRange(0, 500);
    ui->customPlot->yAxis->setRange(-10, 10);

    m_tracerCurrent = new QCPItemTracer(ui->customPlot);
    m_tracerCurrent->setStyle(QCPItemTracer::tsCrosshair);
    m_tracerCurrent->setPen(QPen(Qt::green, 2));
    m_tracerCurrent->setSize(15);
    m_tracerCurrent->setVisible(false);

    m_tracerOld = new QCPItemTracer(ui->customPlot);
    m_tracerOld->setStyle(QCPItemTracer::tsCrosshair);
    m_tracerOld->setPen(QPen(QColor(128, 0, 128), 2));
    m_tracerOld->setSize(15);
    m_tracerOld->setVisible(false);

    m_textInfo = new QCPItemText(ui->customPlot);
    m_textInfo->setPositionAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_textInfo->position->setType(QCPItemPosition::ptAxisRectRatio);
    m_textInfo->position->setCoords(0.02, 0.02);
    m_textInfo->setColor(Qt::white);
    m_textInfo->setFont(QFont("Tahoma", 10, QFont::Bold));
    m_textInfo->setVisible(false);
}

void MainWindow::onAxisRangeChanged(const QCPRange &newRange) {
    if (m_isSyncingScroll || !m_isScrollMode) return;
    m_isSyncingScroll = true;

    long long maxTotalSamples = 0;
    for (int ch = 0; ch < Channel::NUMBER_OF_CHANNELS; ch++) {
        if (m_channels[ch].selected.load()) {
            long long currentTotal = m_channels[ch].totalSamples.load();
            if (currentTotal > maxTotalSamples) maxTotalSamples = currentTotal;
        }
    }

    long long minVisible = qMax(0LL, maxTotalSamples - 1500);
    m_scrollBar->setMinimum(minVisible);
    int maximum = qMax(0LL, maxTotalSamples - 500);
    m_scrollBar->setMaximum(maximum);
    m_scrollBar->setPageStep(500);
    m_scrollBar->setValue(qRound(newRange.lower));

    m_isSyncingScroll = false;
}

void MainWindow::onScrollBarChanged(int value) {
    if (m_isSyncingScroll || !m_isScrollMode) return;
    m_isSyncingScroll = true;

    ui->customPlot->xAxis->setRange(value, value + 500);
    ui->customPlot->replot();

    m_isSyncingScroll = false;
}

void MainWindow::changeChannelColor(int chIndex, QPushButton* btn) {
    QColor color = QColorDialog::getColor(m_channels[chIndex].color, this, "انتخاب رنگ کانال");
    if (color.isValid()) {
        m_channels[chIndex].color = color;
        ui->customPlot->graph(chIndex)->setPen(QPen(color, 2));
        ui->customPlot->replot();
        btn->setStyleSheet(QString("background-color: %1").arg(color.name()));
    }
}

void MainWindow::on_pushButton_2_clicked() { changeChannelColor(0, ui->pushButton_2); }
void MainWindow::on_pushButton_3_clicked() { changeChannelColor(1, ui->pushButton_3); }
void MainWindow::on_pushButton_4_clicked() { changeChannelColor(2, ui->pushButton_4); }
void MainWindow::on_pushButton_5_clicked() { changeChannelColor(3, ui->pushButton_5); }

void MainWindow::toggleChannelSign(int chIndex, QPushButton* btn) {
    if (m_channels[chIndex].sign == 1) {
        m_channels[chIndex].sign = -1;
        btn->setText("-");
    } else {
        m_channels[chIndex].sign = 1;
        btn->setText("+");
    }
}

void MainWindow::on_pushButton_clicked()    { toggleChannelSign(0, ui->pushButton); }
void MainWindow::on_pushButton_10_clicked() { toggleChannelSign(1, ui->pushButton_10); }
void MainWindow::on_pushButton_11_clicked() { toggleChannelSign(2, ui->pushButton_11); }
void MainWindow::on_pushButton_12_clicked() { toggleChannelSign(3, ui->pushButton_12); }

void MainWindow::on_pushButton_32_clicked() {
    m_isMonitoring = false;
    m_isFindMode = false;

    if (m_isBgImageLoaded) {
        ui->customPlot->setBackground(QPixmap());
        ui->customPlot->setBackground(QBrush(QColor(0, 0, 0)));
        ui->customPlot->xAxis->setVisible(true);
        ui->customPlot->yAxis->setVisible(true);
        m_isBgImageLoaded = false;
    }

    m_gridMode = 1;
    updateGridDisplay();

    if (m_tracerCurrent) m_tracerCurrent->setVisible(false);
    if (m_tracerOld) m_tracerOld->setVisible(false);
    if (m_textInfo) m_textInfo->setVisible(false);
    m_currentPoint.isValid = false;
    m_oldPoint.isValid = false;

    QColor defaultColors[4] = {
        QColor(255, 165, 0),
        QColor(255, 255, 0),
        QColor(255, 0, 0),
        QColor(255, 222, 173)
    };

    QPushButton* colorButtons[4] = {
        ui->pushButton_2, ui->pushButton_3, ui->pushButton_4, ui->pushButton_5
    };

    m_isFrozen = false;
    if(m_freezeTimer->isActive()) m_freezeTimer->stop();

    for (int i = 0; i < Channel::NUMBER_OF_CHANNELS; i++) {
        ui_chkSelect[i]->setChecked(i == 0);
        ui_cmbVD[i]->setCurrentIndex(3);
        ui_cmbOffset[i]->setCurrentIndex(6);

        m_channels[i].sign = 1;
        m_channels[i].color = defaultColors[i];
        m_channels[i].totalSamples.store(0);
        m_lastPlottedSample[i] = 0;

        ui->customPlot->graph(i)->setPen(QPen(defaultColors[i], 2));
        colorButtons[i]->setIcon(QIcon());
        colorButtons[i]->setStyleSheet(QString("background-color: %1").arg(defaultColors[i].name()));
        ui->customPlot->graph(i)->data()->clear();
    }

    ui->pushButton->setText("+");
    ui->pushButton_10->setText("+");
    ui->pushButton_11->setText("+");
    ui->pushButton_12->setText("+");
    ui->cmbTimeBase->setCurrentIndex(7);
    ui->customPlot->replot();
    updateChannelSettings();
}

void MainWindow::on_pushButton_34_clicked() {
    if (m_activeGroupPath.isEmpty()) {
        QMessageBox::warning(this, "اخطار", "هیچ گروهی لاگین نکرده است!\nلطفاً ابتدا از طریق دکمه «ورود گروه» وارد شوید.");
        return;
    }

    if (m_isMonitoring) {
        QMessageBox::warning(this, "اخطار", "لطفاً ابتدا مانیتورینگ را متوقف کنید (دکمه پایان) تا داده‌ها بسته شوند.");
        return;
    }

    if (!m_tempLogFile->exists() || m_tempLogFile->size() == 0) {
        QMessageBox::information(this, "خطا", "داده‌ای برای ذخیره وجود ندارد! ابتدا مانیتورینگ را آغاز کنید.");
        return;
    }

    QString timeStr = QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss");
    QString csvPath = m_activeGroupPath + "\\Record_" + timeStr + ".csv";
    QString pngPath = m_activeGroupPath + "\\Record_" + timeStr + ".png";

    if (QFile::exists(csvPath)) {
        QFile::remove(csvPath);
    }

    if (m_tempLogFile->copy(csvPath)) {
        ui->customPlot->savePng(pngPath, 1280, 720, 2.0, 100);
        QMessageBox::information(this, "موفقیت", "داده‌ها و تصویر با موفقیت ذخیره شدند:\n\nفایل دیتا: " + csvPath + "\nفایل عکس: " + pngPath);
        m_tempLogFile->remove();
    } else {
        QMessageBox::critical(this, "خطا", "متأسفانه در ذخیره‌سازی فایل خطایی رخ داد!");
    }
}

void MainWindow::on_pushButton_33_clicked() {
    QHash<QString, QString> studentData;
    QFile csvFile("D:\\Classes\\list_.csv");
    if (csvFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&csvFile);
        in.setCodec("UTF-8");
        bool isFirstLine = true;
        while (!in.atEnd()) {
            QString line = in.readLine();
            if (isFirstLine) { isFirstLine = false; continue; }

            QStringList parts = line.split(',');
            if (parts.size() >= 3) {
                QString name = parts[0].trimmed();
                QString family = parts[1].trimmed();
                QString stNum = parts[2].trimmed();
                studentData[stNum] = name + " " + family;
            }
        }
        csvFile.close();
    } else {
        QMessageBox::warning(this, "خطا", "فایل list_.csv در مسیر D:\\Classes یافت نشد!\nلطفاً فایل لیست دانشجویان را در این مسیر قرار دهید.");
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle("ثبت گروه");
    dialog.setLayoutDirection(Qt::RightToLeft);
    dialog.setMinimumWidth(400);

    QVBoxLayout *mainLayout = new QVBoxLayout(&dialog);
    QHBoxLayout *inputLayout = new QHBoxLayout();
    QLineEdit *txtId = new QLineEdit(&dialog);
    txtId->setPlaceholderText("شماره دانشجویی را وارد کنید...");
    txtId->setValidator(new QRegExpValidator(QRegExp("^[0-9]*$"), txtId));

    QPushButton *btnAdd = new QPushButton("افزودن", &dialog);
    inputLayout->addWidget(txtId);
    inputLayout->addWidget(btnAdd);
    QListWidget *listWidget = new QListWidget(&dialog);

    mainLayout->addLayout(inputLayout);
    mainLayout->addWidget(listWidget);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, &dialog);
    buttonBox->button(QDialogButtonBox::Ok)->setText("ثبت نهایی");
    buttonBox->button(QDialogButtonBox::Cancel)->setText("انصراف");
    mainLayout->addWidget(buttonBox);

    connect(btnAdd, &QPushButton::clicked, [&]() {
        QString id = txtId->text().trimmed();
        if (id.isEmpty()) return;

        if (studentData.contains(id)) {
            bool alreadyAdded = false;
            for (int i = 0; i < listWidget->count(); ++i) {
                if (listWidget->item(i)->text().contains(id)) {
                    alreadyAdded = true;
                    break;
                }
            }

            if (alreadyAdded) {
                QMessageBox::information(&dialog, "تکراری", "این دانشجو قبلاً به گروه اضافه شده است.");
            } else {
                QString fullName = studentData[id];
                listWidget->addItem(id + "," + fullName);
                txtId->clear();
            }
        } else {
            QMessageBox::warning(&dialog, "نامعتبر", "شماره دانشجویی وارد شده در لیست یافت نشد!");
        }
    });

    connect(txtId, &QLineEdit::returnPressed, btnAdd, &QPushButton::click);
    connect(buttonBox, &QDialogButtonBox::accepted, [&]() {
        if (listWidget->count() == 0) {
            QMessageBox::warning(&dialog, "خطا", "لیست گروه نمی‌تواند خالی باشد! لطفاً حداقل یک دانشجو را اضافه کنید.");
        } else {
            dialog.accept();
        }
    });
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        QString classNum = "6";
        QDateTime now = QDateTime::currentDateTime();
        int dayOfWeek = now.date().dayOfWeek();

        if (dayOfWeek == Qt::Saturday) classNum = "0";
        else if (dayOfWeek == Qt::Sunday) classNum = "1";
        else if (dayOfWeek == Qt::Monday) classNum = "2";
        else if (dayOfWeek == Qt::Tuesday) classNum = "3";
        else if (dayOfWeek == Qt::Thursday) classNum = "4";

        QTime currentTime = now.time();
        QTime intervals[6] = {
            QTime(8, 30), QTime(10, 30), QTime(12, 30),
            QTime(15, 30), QTime(17, 30), QTime(19, 30)
        };

        for (int i = 0; i < 6; i++) {
            if (intervals[i] > currentTime) {
                classNum += QString("_%1_3").arg(i + 1);
                break;
            }
        }

        QString basePath = "D:\\Classes\\" + classNum;
        QDir dir;
        if (!dir.mkpath(basePath)) {
            QMessageBox::critical(this, "خطای دسترسی", "امکان ساخت پوشه در درایو D وجود ندارد!");
            return;
        }

        QFile regFile("D:\\Classes\\groups_registry.csv");
        if (regFile.open(QIODevice::Append | QIODevice::Text)) {
            QTextStream out(&regFile);
            out.setCodec("UTF-8");

            for (int i = 0; i < listWidget->count(); ++i) {
                QString itemText = listWidget->item(i)->text();
                out << itemText << "," << classNum << "\n";
            }
            regFile.close();
            QMessageBox::information(this, "موفق", "ثبت گروه با موفقیت در فایل مرکزی انجام شد.\n\nتعداد اعضا: " + QString::number(listWidget->count()) + "\nشماره گروه: " + classNum);
        } else {
            QMessageBox::critical(this, "خطا", "خطا در نوشتن فایل مرکزی (groups_registry.csv)!");
        }
    }
}

void MainWindow::on_pushButton_login_clicked() {
    bool ok;
    QString stId = QInputDialog::getText(this, "ورود گروه",
                                         "شماره دانشجویی خود را وارد کنید:",
                                         QLineEdit::Normal,
                                         "", &ok);

    if (ok && !stId.trimmed().isEmpty()) {
        QString idToSearch = stId.trimmed();
        bool found = false;
        QString groupFolder = "";
        QString studentName = "";

        QFile regFile("D:\\Classes\\groups_registry.csv");
        if (regFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&regFile);
            in.setCodec("UTF-8");
            while (!in.atEnd()) {
                QString line = in.readLine();
                QStringList parts = line.split(',');
                if (parts.size() >= 3) {
                    if (parts[0].trimmed() == idToSearch) {
                        found = true;
                        studentName = parts[1].trimmed();
                        groupFolder = parts[2].trimmed();
                        break;
                    }
                }
            }
            regFile.close();
        } else {
            QMessageBox::warning(this, "خطا", "فایل اطلاعات گروه‌ها (groups_registry.csv) یافت نشد.\nلطفاً ابتدا گروه خود را ثبت کنید.");
            return;
        }

        if (found) {
            QString groupRoot = "D:\\Classes\\" + groupFolder;
            QString sessionFolderName = "Session_" + QDateTime::currentDateTime().toString("yyyy-MM-dd");
            m_activeGroupPath = groupRoot + "\\" + sessionFolderName;

            QDir dir;
            if (!dir.exists(m_activeGroupPath)) {
                dir.mkpath(m_activeGroupPath);
            }

            this->setWindowTitle("LCSL Oscilloscope - وضعیت: گروه " + groupFolder + " (" + studentName + ")");
            QMessageBox::information(this, "ورود موفق", "خوش آمدید " + studentName + "!\nپوشه فعال امروز شما: \n" + groupFolder + "\\" + sessionFolderName);

            // راه‌اندازی و تزریق اطلاعات به موتور گزارش‌کار
            m_reportEngine->initializeSession(m_activeGroupPath, "D:\\Classes\\report_template.xml");

        } else {
            QMessageBox::warning(this, "خطا در ورود", "شماره دانشجویی وارد شده در هیچ گروهی ثبت نشده است.");
        }
    }
}

void MainWindow::on_pushButton_35_clicked() {
    if (m_isBgImageLoaded) {
        ui->customPlot->setBackground(QPixmap());
        ui->customPlot->setBackground(QBrush(QColor(0, 0, 0)));
        ui->customPlot->xAxis->setVisible(true);
        ui->customPlot->yAxis->setVisible(true);
        ui->customPlot->replot();
        m_isBgImageLoaded = false;
        return;
    }

    QString searchPath = m_activeGroupPath.isEmpty() ? "D:\\Classes" : m_activeGroupPath;
    QString loadPath = QFileDialog::getOpenFileName(this, "انتخاب تصویر مرجع", searchPath, "PNG Image (*.png)");

    if (loadPath.isEmpty()) {
        return;
    }

    QPixmap bg(loadPath);
    if (bg.isNull()) {
        QMessageBox::critical(this, "خطا", "فایل انتخاب شده معتبر نیست!");
        return;
    }

    ui->customPlot->setBackground(bg, true, Qt::IgnoreAspectRatio);
    ui->customPlot->xAxis->setVisible(false);
    ui->customPlot->yAxis->setVisible(false);
    ui->customPlot->replot();
    m_isBgImageLoaded = true;
}

void MainWindow::on_cmbTimeBase_currentIndexChanged(int index) {
    applyTimebaseSettings(index);
}

void MainWindow::applyTimebaseSettings(int index) {
    Channel::count = 1;
    m_isScrollMode = (index >= 5);
    m_isFrozen = false;
    if (m_freezeTimer->isActive()) m_freezeTimer->stop();

    for (int i = 0; i < Channel::NUMBER_OF_CHANNELS; i++) {
        m_lastPlottedSample[i] = m_channels[i].totalSamples.load();
    }

    if (m_isScrollMode) {
        ui->customPlot->setInteractions(0);
        m_scrollBar->setVisible(true);
    } else {
        ui->customPlot->setInteractions(0);
        m_scrollBar->setVisible(false);
    }

    switch (index) {
        case 0:  m_psa = 0x08; m_tmr0l = 0x10; m_tmr0h = 0xFF; break;
        case 1:  m_psa = 0x08; m_tmr0l = 0x20; m_tmr0h = 0xFE; break;
        case 2:  m_psa = 0x08; m_tmr0l = 0x50; m_tmr0h = 0xFB; break;
        case 3:  m_psa = 0x08; m_tmr0l = 0xA0; m_tmr0h = 0xF6; break;
        case 4:  m_psa = 0x08; m_tmr0l = 0x40; m_tmr0h = 0xED; break;
        case 5:  m_psa = 0x08; m_tmr0l = 0x20; m_tmr0h = 0xD1; break;
        case 6:  m_psa = 0x08; m_tmr0l = 0x40; m_tmr0h = 0xA2; break;
        case 7:  m_psa = 0x08; m_tmr0l = 0x40; m_tmr0h = 0xA2; Channel::count = 2;   break;
        case 8:  m_psa = 0x08; m_tmr0l = 0x40; m_tmr0h = 0xA2; Channel::count = 5;   break;
        case 9:  m_psa = 0x08; m_tmr0l = 0x40; m_tmr0h = 0xA2; Channel::count = 10;  break;
        case 10: m_psa = 0x08; m_tmr0l = 0x40; m_tmr0h = 0xA2; Channel::count = 20;  break;
        case 11: m_psa = 0x08; m_tmr0l = 0x40; m_tmr0h = 0xA2; Channel::count = 50;  break;
        case 12: m_psa = 0x08; m_tmr0l = 0x40; m_tmr0h = 0xA2; Channel::count = 100; break;
    }

    Consts::TD_Index = index;
    updateChannelSettings();
}

bool MainWindow::focusNextPrevChild(bool next) {
    if (m_isFindMode) {
        if (m_currentPoint.isValid) {
            long long currentX = m_currentPoint.xCoord;
            if (next) {
                updateCursorPosition(currentX + 5, false);
            } else {
                updateCursorPosition(currentX - 5, false);
            }
        }
        return true;
    }
    return QMainWindow::focusNextPrevChild(next);
}
