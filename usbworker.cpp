#include "usbworker.h"
#include "consts.h"
#include <QThread>
#include <QCoreApplication>
#include <libusb.h>

// Constructor: Initializes worker thread state and default PIC microcontroller registers
UsbWorker::UsbWorker(std::vector<Channel>& channels, QObject *parent)
    : QObject(parent), m_channels(channels) {
    isRunning = false;
    chnum = 0;

    // Initialize default hardware register configuration
    PSA_Value = 0x08;
    TMR0L_Value = 0x40;
    TMR0H_Value = 0xA2;
    CHANNEL_Value = 0x01;
    VD_Value = 0xff;
}

// Main worker execution loop: manages USB communication, packet transmission, and Auto-Reconnect
void UsbWorker::startReading() {
    isRunning = true;

    libusb_context *ctx = nullptr;
    libusb_init(&ctx);

    unsigned char readBuffer[64];
    int actual_length;

    // Outer Loop: Auto-Reconnect Mechanism
    while (isRunning) {
        QCoreApplication::processEvents();

        // Attempt to open target device using Vendor ID (VID) and Product ID (PID)
        libusb_device_handle *dev_handle = libusb_open_device_with_vid_pid(ctx, 0x04D8, 0x0204);

        // If device is not found, wait 500ms and try again quietly (Auto-Reconnect)
        if (dev_handle == nullptr) {
            emit deviceConnected(false);
            QThread::msleep(500);
            continue;
        }

        // Set configuration and claim interface 0 for bulk endpoints
        libusb_set_configuration(dev_handle, 1);
        libusb_claim_interface(dev_handle, 0);
        emit deviceConnected(true);

        bool deviceActive = true;

        // Inner Loop: Data Acquisition
        while (isRunning && deviceActive) {
            QCoreApplication::processEvents();

            // 1. Transmit updated channel configuration packets if changes are queued
            if (!pendingVdValues.isEmpty()) {
                for (unsigned char vd : pendingVdValues) {
                    unsigned char ConfigPacket[64] = {0};
                    ConfigPacket[0] = 0x81;
                    ConfigPacket[1] = TMR0H_Value;
                    ConfigPacket[2] = TMR0L_Value;
                    ConfigPacket[3] = PSA_Value;
                    ConfigPacket[4] = CHANNEL_Value;
                    ConfigPacket[5] = vd;

                    libusb_bulk_transfer(dev_handle, 0x01, ConfigPacket, 64, &actual_length, 1000);
                    QThread::msleep(2);
                }
                pendingVdValues.clear();
                VD_Value = 0xff; // Reset to continuous sampling mode
            }

            // 2. Dispatch standard sampling request packet to endpoint 0x01
            unsigned char OutputPacketBuffer[64] = {0};
            OutputPacketBuffer[0] = 0x81;
            OutputPacketBuffer[1] = TMR0H_Value;
            OutputPacketBuffer[2] = TMR0L_Value;
            OutputPacketBuffer[3] = PSA_Value;
            OutputPacketBuffer[4] = CHANNEL_Value;
            OutputPacketBuffer[5] = VD_Value;

            int writeResult = libusb_bulk_transfer(dev_handle, 0x01, OutputPacketBuffer, 64, &actual_length, 1000);

            // Check for physical disconnection during write
            if (writeResult == LIBUSB_ERROR_NO_DEVICE || writeResult == LIBUSB_ERROR_IO) {
                deviceActive = false;
                break;
            }

            // 3. Read incoming bulk data packet containing digitized ADC samples from endpoint 0x81
            int readResult = libusb_bulk_transfer(dev_handle, 0x81, readBuffer, sizeof(readBuffer), &actual_length, 100);

            // Check for physical disconnection during read
            if (readResult == LIBUSB_ERROR_NO_DEVICE || readResult == LIBUSB_ERROR_IO) {
                deviceActive = false;
                break;
            }

            if (readResult == 0 && actual_length > 0) {
                int ndata = 0;
                if (readBuffer[4] <= 58) {
                    ndata = readBuffer[4];
                }

                // Gather pointers to all currently enabled channels
                std::vector<Channel*> selectedChannels;
                for (size_t i = 0; i < m_channels.size(); i++) {
                    if (m_channels[i].selected.load()) {
                        selectedChannels.push_back(&m_channels[i]);
                    }
                }

                int numSelected = selectedChannels.size();

                // Demultiplex and convert raw ADC byte values to scaled physical voltages
                if (numSelected > 0) {
                    for (int i = 0; i < ndata; i++) {
                        unsigned char rawByte = readBuffer[5 + i];

                        if (chnum >= numSelected) {
                            chnum = 0;
                        }

                        // Calculate voltage using ADC conversion formula and channel gain scale
                        double usbData = (5.0 - (rawByte * 10.0 / 256.0));

                        // Store calculated sample into channel's circular buffer
                        int writePos = selectedChannels[chnum]->writePtr.load();
                        selectedChannels[chnum]->data[writePos] = usbData;

                        writePos++;

                        if (writePos >= Channel::BUFFER_LENGTH) {
                            writePos = 0;
                        }

                        selectedChannels[chnum]->writePtr.store(writePos);
                        selectedChannels[chnum]->totalSamples.fetch_add(1); // Increment total received samples counter for absolute timeline tracking
                        chnum++;
                    }
                }
            }
            QThread::msleep(1);
        }

        // Release resources cleanly before attempting to reconnect
        libusb_release_interface(dev_handle, 0);
        libusb_close(dev_handle);
    }

    libusb_exit(ctx);
    emit deviceConnected(false);
}

// Flags the worker loop to stop running cleanly
void UsbWorker::stopReading() {
    isRunning = false;
}

// Slot to update hardware timing registers, active channel mask, and voltage division values
void UsbWorker::updateDeviceSettings(unsigned char tmr0h, unsigned char tmr0l, unsigned char psa, unsigned char channel, QVector<unsigned char> vdValues) {
    TMR0H_Value = tmr0h;
    TMR0L_Value = tmr0l;
    PSA_Value = psa;
    CHANNEL_Value = channel;
    pendingVdValues = vdValues;
}
