![Uploading Screenshot (542).png…]()
<img width="1920" height="1080" alt="Screenshot (586)" src="https://github.com/user-attachments/assets/09f0dc99-db88-4bb5-aff7-d71b598eab46" />
# ⚡ LCSL Oscilloscope (Linear Control Systems Laboratory)

![Qt](https://img.shields.io/badge/Qt-5.15%2B-41CD52?style=flat-square&logo=qt&logoColor=white)
![C++](https://img.shields.io/badge/C++-11%2F14%2F17-00599C?style=flat-square&logo=c%2B%2B&logoColor=white)
![License](https://img.shields.io/badge/License-MIT-blue.svg?style=flat-square)

A real-time, multi-channel oscilloscope and data acquisition software built with C++ and Qt. Designed specifically for the Linear Control Systems Laboratory to interface with hardware via USB, monitor signals, and automatically generate comprehensive student laboratory reports.

---

## 📸 Screenshots & Output

*(Note: Replace the placeholder links below with actual screenshots of your running application. You can drag and drop images directly into GitHub to get the link!)*

### Main Oscilloscope Interface
![Main Interface Placeholder](https://via.placeholder.com/800x400/1e1e1e/007ACC?text=Drop+Main+Oscilloscope+Screenshot+Here)
*Real-time signal monitoring with up to 4 simultaneous channels, featuring custom grid modes and voltage/timebase adjustments.*

### Measurement & Find Mode
![Find Mode Placeholder](https://via.placeholder.com/800x400/1e1e1e/4eb0ff?text=Drop+Find+Mode+Screenshot+Here)
*Interactive crosshair cursors for precise amplitude and time measurements.*

### Automated Report Generation
![Report Engine Placeholder](https://via.placeholder.com/800x400/1e1e1e/d83b01?text=Drop+Report+Engine+Screenshot+Here)
*Built-in XML-based report engine allowing students to answer questions, fill tables, attach scope captures, and export to PDF.*

---

## ✨ Key Features

### 📡 Data Acquisition & Signal Processing
* **Real-Time Monitoring:** High-performance rendering using `QCustomPlot` capable of displaying continuous high-speed data.
* **4-Channel Support:** Independent controls for Voltage/Division (VD), Offset, and display colors for up to four channels.
* **Hardware Interfacing:** Direct USB bulk transfer communication using `libusb`, managed in a dedicated background `QThread` to guarantee a completely responsive UI.
* **Thread-Safe Architecture:** Utilizes `std::atomic` and circular ring buffers to safely exchange high-frequency sample data between the hardware thread and the GUI rendering thread.

### 🛠️ Interactive Analysis Tools
* **Dynamic Timebase (TD):** Adjustable time scales with an integrated **Scroll Mode** for reviewing historical waveform data.
* **Find Mode:** Precision analysis tool to anchor points and calculate $\Delta t$ and $\Delta V$ between signal peaks/troughs.
* **Customizable Display:** Multiple grid overlay modes and signal inversion (polarity toggling).

### 📝 Automated Laboratory Reports
* **Student Session Management:** Authentication and group session tracking via CSV registries.
* **XML Template Engine:** Dynamically generates interactive laboratory worksheets based on predefined XML templates.
* **PDF Export:** Consolidates student answers, data tables, and oscilloscope graph snapshots into a formatted, ready-to-print PDF report using `QPdfWriter`.
* **Data Logging:** One-click exporting of raw waveform data into `.csv` files for external analysis (e.g., in MATLAB or Excel).

---

## 💻 Technology Stack

* **Language:** C++
* **Framework:** Qt (Widgets, Core, Gui, PrintSupport)
* **Plotting Library:** [QCustomPlot](https://www.qcustomplot.com/)
* **USB Communication:** `libusb-1.0`
* **Concurrency:** `QThread`, `std::atomic`, Mutex-free Ring Buffers

---

## 🚀 Getting Started

### Prerequisites
To compile and run this project, you need:
1.  **Qt Environment:** Qt 5 or Qt 6 installed (tested with MinGW/MSVC).
2.  **libusb-1.0:** The `libusb` library must be configured in your Qt `.pro` file.
3.  **Hardware Drivers:** Appropriate USB drivers (e.g., WinUSB via Zadig) installed for the target data acquisition board (PIC microcontroller).

### Build Instructions
1. Clone the repository:
   ```bash
   git clone https://github.com/YourUsername/LCSL_SignalDisplay.git
   ```
2. Open the `LCSL_SignalDisplay.pro` file in Qt Creator.
3. Verify that the library paths for `libusb` and `QCustomPlot` in the `.pro` file match your local system structure.
4. Build and Run the project (`Ctrl + R`).

---

## 👨‍💻 Author

**Alireza Hamed Roohbakhsh**  
*Computer Engineering | Software Engineering*
