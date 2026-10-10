#ifndef USBWORKER_H
#define USBWORKER_H

#include <QObject>
#include <QString>
#include <vector>
#include "channel.h" // Channel map reference inclusion
#include <QVector>

class UsbWorker : public QObject {
    Q_OBJECT

public:
    // Class constructor: Now accepts a reference to the list of channels as input
    explicit UsbWorker(std::vector<Channel>& channels, QObject *parent = nullptr);

public slots:
    void startReading();
    void stopReading();
    void updateDeviceSettings(unsigned char tmr0h, unsigned char tmr0l, unsigned char psa, unsigned char channel, QVector<unsigned char> vdValues);

signals:
    void errorOccurred(QString message);
    void deviceConnected(bool status);
    void dataReady();

private:
    bool isRunning;

    // Reference to the application's primary channels for populating data
    std::vector<Channel>& m_channels;

    // Counter for rotating through active channels
    int chnum;
    QVector<unsigned char> pendingVdValues;

    // Configuration variables for the sampling device (PIC4550)
    unsigned char PSA_Value;
    unsigned char TMR0L_Value;
    unsigned char TMR0H_Value;
    unsigned char CHANNEL_Value;
    unsigned char VD_Value;
};

#endif // USBWORKER_H
