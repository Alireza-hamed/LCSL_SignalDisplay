#ifndef CHANNEL_H
#define CHANNEL_H

#include <vector>
#include <QColor>
#include <atomic>

// Represents an individual data acquisition channel
class Channel {
public:
    // Default constructor
    Channel();

    // Copy and move constructors
    Channel(const Channel& other);
    Channel(Channel&& other) noexcept;

    // Copy and move assignment operators
    Channel& operator=(const Channel& other);
    Channel& operator=(Channel&& other) noexcept;

    // Constants for buffer size and total channel capacity
    static const int BUFFER_LENGTH = 100000;
    static const int NUMBER_OF_CHANNELS = 4;

    std::vector<double> data;           // Internal ring buffer / sample storage
    std::atomic<bool> selected;         // Selection/active state flag (thread-safe)
    int sign;                           // Polarity/inversion multiplier for display
    std::atomic<int> vdIndex;           // Volts/division scale index (thread-safe)
    std::atomic<int> offIndex;          // Voltage offset index (thread-safe)
    int counter;                        // Internal event/processing counter
    QColor color;                       // Waveform rendering color

    std::atomic<int> readPtr;           // Ring buffer read pointer (thread-safe)
    std::atomic<int> writePtr;          // Ring buffer write pointer (thread-safe)

    std::atomic<long long> totalSamples;// Total received samples counter for absolute time tracking

    int numberOfFetchedData;            // Count of samples retrieved in the last fetch cycle

    // Shared static state across all channels
    static int numberOfSelectedChannels;                 // Number of currently active channels
    static int count;                                    // Global sample/cycle counter
    static std::vector<std::vector<double>> fetchedData; // Shared buffer for extracted plot data

    // Fetches and scales buffered data for a specific channel into the shared fetchedData buffer
    void fetchData(int channelIndex);

    // Reads incoming raw hardware samples and distributes them across channels
    static void readSamples(std::vector<Channel>& channels);
};

#endif // CHANNEL_H
