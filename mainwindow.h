#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QThread>
#include <QTimer>
#include <QMouseEvent>
#include <QKeyEvent>
#include <vector>
#include "usbworker.h"
#include "channel.h"
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QVector>
#include <QScrollBar>
#include <QTabWidget>

// اینکلود کردن کلاس جدید گزارش‌ساز
#include "reportengine.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

struct SelectedPointData {
    double t = 0.0;
    double y = 0.0;
    long long xCoord = 0;
    int chNumber = -1;
    bool isValid = false;
};

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void keyPressEvent(QKeyEvent *event) override;
    bool focusNextPrevChild(bool next) override;

private slots:
    void onDataReady();
    void on_pushButton_30_clicked();
    void on_pushButton_31_clicked();
    void on_pushButton_33_clicked();
    void on_pushButton_login_clicked();
    void updateChannelSettings();
    void on_pushButton_34_clicked();
    void on_pushButton_35_clicked();
    void onGraphClicked(QMouseEvent *event);
    void on_pushButton_2_clicked();
    void on_pushButton_3_clicked();
    void on_pushButton_4_clicked();
    void on_pushButton_5_clicked();
    void on_pushButton_clicked();
    void on_pushButton_10_clicked();
    void on_pushButton_11_clicked();
    void on_pushButton_12_clicked();
    void on_pushButton_32_clicked();
    void on_cmbTimeBase_currentIndexChanged(int index);
    void onScrollBarChanged(int value);
    void onAxisRangeChanged(const class QCPRange &newRange);

signals:
    void sendDeviceSettings(unsigned char tmr0h, unsigned char tmr0l, unsigned char psa, unsigned char channel, QVector<unsigned char> vdValues);

private:
    Ui::MainWindow *ui;
    void setupPlotGraphics();
    void setupUIElements();

    // تابع تنظیم تب‌ها
    void setupTabs();

    void updateCursorPosition(long long absX, bool updateOldPoint = false);
    void updateGridDisplay();

    std::vector<Channel> m_channels;
    QThread *workerThread;
    UsbWorker *usbWorker;
    QTimer *renderTimer;

    bool m_isMonitoring;
    bool m_isScrollMode;
    bool m_isFrozen;
    QTimer *m_freezeTimer;
    long long m_lastPlottedSample[4];

    bool m_isFindMode;
    int m_findModeChannel;
    int m_gridMode;
    QString m_activeGroupPath;
    bool m_isBgImageLoaded;

    QScrollBar *m_scrollBar;
    bool m_isSyncingScroll;

    SelectedPointData m_currentPoint;
    SelectedPointData m_oldPoint;

    QCheckBox* ui_chkSelect[4];
    QComboBox* ui_cmbVD[4];
    QComboBox* ui_cmbOffset[4];

    void changeChannelColor(int chIndex, QPushButton* btn);
    void toggleChannelSign(int chIndex, QPushButton* btn);

    class QCPItemTracer *m_tracerCurrent;
    class QCPItemTracer *m_tracerOld;
    class QCPItemText *m_textInfo;

    class QFile *m_tempLogFile;
    class QTextStream *m_tempLogStream;
    long long m_totalLoggedSamples;

    unsigned char m_tmr0h = 0xA2;
    unsigned char m_tmr0l = 0x40;
    unsigned char m_psa = 0x08;

    void applyTimebaseSettings(int index);

    // پوینترهای مدیریت تب‌ها و موتور گزارش‌کار
    QTabWidget *mainTabWidget;
    ReportEngine *m_reportEngine;
};

#endif // MAINWINDOW_H
