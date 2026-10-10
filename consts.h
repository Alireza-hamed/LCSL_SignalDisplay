#ifndef CONSTS_H
#define CONSTS_H

#include <QVector>
#include <QString>

class Consts {
public:
    // Voltage scale constant values (Voltage / Division)
    inline static const QVector<QString> VD_STR = {"50mv", "100mv", "200mv", "500mv", "1v", "2v", "5v", "10v"};
    inline static const QVector<double> VD_VAL = {0.05, 0.1, 0.2, 0.5, 1.0, 2.0, 5.0, 10.0};

    // Voltage offset constant values (Offset)
    inline static const QVector<QString> OFFSET_STR = {"-5v", "-2v", "-1v", "-500mv", "-200mv", "-100mv", "0", "100mv", "200mv", "500mv", "1v", "2v", "5v"};
    inline static const QVector<double> OFFSET_VAL = {-5.0, -2.0, -1.0, -0.5, -0.2, -0.1, 0.0, 0.1, 0.2, 0.5, 1.0, 2.0, 5.0};

    // Timebase constant values (Time / Division)
    inline static const QVector<QString> TD_STR = {"1ms", "2ms", "5ms", "10ms", "20ms", "50ms", "100ms", "200ms", "500ms", "1s", "2s", "5s", "10s"};
    inline static const QVector<double> TD_VAL = {0.001, 0.002, 0.005, 0.01, 0.02, 0.05, 0.1, 0.2, 0.5, 1.0, 2.0, 5.0, 10.0};

    inline static int TD_Index = 7; // Default value
};

#endif // CONSTS_H
