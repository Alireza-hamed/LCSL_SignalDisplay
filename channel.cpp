#include "channel.h"

// Initialize static member variables for channel tracking and data extraction
int Channel::numberOfSelectedChannels = 1;
int Channel::count = 1;
std::vector<std::vector<double>> Channel::fetchedData(Channel::NUMBER_OF_CHANNELS, std::vector<double>(Channel::BUFFER_LENGTH, 0.0));

// Default constructor: Initializes ring buffer capacity and default channel parameters
Channel::Channel() {
    data.resize(BUFFER_LENGTH, 0.0);

    selected.store(false);
    vdIndex.store(3);
    offIndex.store(6);

    sign = 1;
    counter = 0;
    readPtr.store(0);
    writePtr.store(0);
    totalSamples.store(0); // Initialize absolute sample counter for time tracking
    numberOfFetchedData = 0;
    color = Qt::green;
}

// Extracts new samples from the circular ring buffer with decimation/downsampling support
void Channel::fetchData(int k) {
    int wr = writePtr.load();
    int rd = readPtr.load();

    size_t l = 0;
    numberOfFetchedData = 0;

    // Lambda helper to process, decimate, and store samples into the shared fetch buffer
    auto processSample = [&](size_t index) {
        counter++;
        if (counter >= count) {
            fetchedData[static_cast<size_t>(k)][l++] = data[index];
            numberOfFetchedData++;
            counter = 0;
        }
    };

    // Handle circular buffer wrap-around scenarios
    if (wr < rd) {
        // Read from current read pointer to the end of the buffer
        for (int j = rd; j < BUFFER_LENGTH; j++) processSample(static_cast<size_t>(j));
        // Wrap around and read from start of the buffer to the write pointer
        for (int j = 0; j < wr; j++) processSample(static_cast<size_t>(j));
    } else {
        // Linear read from read pointer to write pointer
        for (int j = rd; j < wr; j++) processSample(static_cast<size_t>(j));
    }

    // Advance read pointer to catch up with the current write position
    readPtr.store(wr);
}

// Iterates through all channels and retrieves newly acquired data for active channels
void Channel::readSamples(std::vector<Channel>& channels) {
    if (numberOfSelectedChannels == 0) return;

    for (size_t i = 0; i < channels.size(); i++) {
        if (channels[i].selected.load()) {
            channels[i].fetchData(static_cast<int>(i));
        }
    }
}

// Copy Constructor: Performs thread-safe atomic and standard member copying
Channel::Channel(const Channel& other) {
    data = other.data;
    sign = other.sign;
    counter = other.counter;
    color = other.color;
    numberOfFetchedData = other.numberOfFetchedData;

    selected.store(other.selected.load());
    vdIndex.store(other.vdIndex.load());
    offIndex.store(other.offIndex.load());
    readPtr.store(other.readPtr.load());
    writePtr.store(other.writePtr.load());
    totalSamples.store(other.totalSamples.load());
}

// Copy Assignment Operator: Safely copies state and atomic variables with self-assignment guard
Channel& Channel::operator=(const Channel& other) {
    if (this != &other) {
        data = other.data;
        sign = other.sign;
        counter = other.counter;
        color = other.color;
        numberOfFetchedData = other.numberOfFetchedData;

        selected.store(other.selected.load());
        vdIndex.store(other.vdIndex.load());
        offIndex.store(other.offIndex.load());
        readPtr.store(other.readPtr.load());
        writePtr.store(other.writePtr.load());
        totalSamples.store(other.totalSamples.load());
    }
    return *this;
}

// Move Constructor: Moves dynamically allocated resources and copies atomic values
Channel::Channel(Channel&& other) noexcept {
    data = std::move(other.data);
    sign = other.sign;
    counter = other.counter;
    color = std::move(other.color);
    numberOfFetchedData = other.numberOfFetchedData;

    selected.store(other.selected.load());
    vdIndex.store(other.vdIndex.load());
    offIndex.store(other.offIndex.load());
    readPtr.store(other.readPtr.load());
    writePtr.store(other.writePtr.load());
    totalSamples.store(other.totalSamples.load());
}

// Move Assignment Operator: Moves buffer/color resources and transfers atomic values
Channel& Channel::operator=(Channel&& other) noexcept {
    if (this != &other) {
        data = std::move(other.data);
        sign = other.sign;
        counter = other.counter;
        color = std::move(other.color);
        numberOfFetchedData = other.numberOfFetchedData;

        selected.store(other.selected.load());
        vdIndex.store(other.vdIndex.load());
        offIndex.store(other.offIndex.load());
        readPtr.store(other.readPtr.load());
        writePtr.store(other.writePtr.load());
        totalSamples.store(other.totalSamples.load());
    }
    return *this;
}
