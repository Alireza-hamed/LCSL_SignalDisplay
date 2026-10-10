#include "mainwindow.h"

#include <QFontDatabase>
#include <QFont>
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;

    // Add font file to the application database
    int fontId = QFontDatabase::addApplicationFont(":/fonts/Vazirmatn-UI-Medium.ttf");

    if (fontId != -1) {
    // Extract font family name
    QString fontFamily = QFontDatabase::applicationFontFamilies(fontId).at(0);

    // Create font instance with desired size (e.g., size 10)
    QFont mainFont(fontFamily, 10);

    // Apply font globally across the application
    a.setFont(mainFont);
        }

    w.show();
    return a.exec();
}
