#ifndef REPORTENGINE_H
#define REPORTENGINE_H

#include <QWidget>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QPushButton>
#include <QXmlStreamReader>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QTextEdit>
#include <QTableWidget>
#include <QHeaderView>
#include <QPdfWriter>
#include <QTextDocument>

struct ReportElement {
    QString type;
    QString id;
    QString content;
    int rows = 0;
    int cols = 0;
    QString headers;
};

struct ReportStep {
    QString id;
    QList<ReportElement> elements;
};

class ReportEngine : public QWidget {
    Q_OBJECT
public:
    explicit ReportEngine(QWidget *parent = nullptr);
    void initializeSession(const QString &sessionPath, const QString &templatePath);

private slots:
    void onSubmitClicked();
    void onExportPdfClicked();

private:
    void setupUI();
    void loadReportTemplate(const QString &filePath);
    void loadAnswers(const QString &sessionPath);
    void renderState();
    void renderStep(const ReportStep &step, bool isReadOnly);

    QVBoxLayout *mainLayout;
    QScrollArea *scrollArea;
    QWidget *scrollWidget;
    QVBoxLayout *contentLayout;
    QPushButton *btnSubmitStep;
    QPushButton *btnExportPdf;

    QString m_sessionPath;
    QString m_answersFilePath;
    QList<ReportStep> reportSteps;
    int currentStepIndex;
    QJsonObject answersJson;

    QList<QWidget*> currentActiveInputWidgets;
    QList<ReportElement> currentActiveElements;
};

#endif // REPORTENGINE_H
