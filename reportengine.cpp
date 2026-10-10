#include "reportengine.h"
#include <QLabel>
#include <QFile>
#include <QMessageBox>
#include <QTimer>
#include <QFrame>
#include <QScrollBar>
#include <QFont>
#include <QHeaderView>
#include <QFileDialog>
#include <QFileInfo>
#include <QPageSize>
#include <QPageLayout>
#include <QMarginsF>
#include <QDateTime>
#include <QPainter>
#include <QBrush>
#include <QPixmap>
#include <QUrl>

ReportEngine::ReportEngine(QWidget *parent) : QWidget(parent), currentStepIndex(0) {
    setupUI();
}

void ReportEngine::setupUI() {
    this->setLayoutDirection(Qt::RightToLeft);

    QFont mainFont("Vazir", 11);
    this->setFont(mainFont);

    mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(5, 5, 5, 10);
    mainLayout->setSpacing(10);

    scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setStyleSheet(
        "QScrollArea { border: none; background-color: #1e1e1e; }"
        "QScrollBar:vertical { background: #252526; width: 12px; margin: 0px; }"
        "QScrollBar::handle:vertical { background: #555; border-radius: 5px; min-height: 20px; }"
        "QScrollBar::handle:vertical:hover { background: #007ACC; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }"
    );

    scrollWidget = new QWidget();
    scrollWidget->setStyleSheet("QWidget { background-color: #1e1e1e; }");
    contentLayout = new QVBoxLayout(scrollWidget);
    contentLayout->setAlignment(Qt::AlignTop);
    contentLayout->setContentsMargins(15, 15, 15, 15);
    contentLayout->setSpacing(20);

    scrollWidget->setLayout(contentLayout);
    scrollArea->setWidget(scrollWidget);

    mainLayout->addWidget(scrollArea);

    btnSubmitStep = new QPushButton("ثبت پاسخ و رفتن به مرحله بعد", this);
    btnSubmitStep->setCursor(Qt::PointingHandCursor);
    btnSubmitStep->setMinimumHeight(45);
    btnSubmitStep->setMinimumWidth(250);
    QFont btnFont("Tahoma", 11, QFont::Bold);
    btnSubmitStep->setFont(btnFont);
    btnSubmitStep->setStyleSheet(
        "QPushButton { background-color: #007ACC; color: white; border: none; border-radius: 6px; padding: 10px; }"
        "QPushButton:hover { background-color: #005A9E; }"
        "QPushButton:pressed { background-color: #004578; }"
    );
    btnSubmitStep->hide();
    connect(btnSubmitStep, &QPushButton::clicked, this, &ReportEngine::onSubmitClicked);

    btnExportPdf = new QPushButton("دریافت گزارش تا این مرحله (PDF)", this);
    btnExportPdf->setCursor(Qt::PointingHandCursor);
    btnExportPdf->setMinimumHeight(45);
    btnExportPdf->setMinimumWidth(250);
    btnExportPdf->setFont(btnFont);
    btnExportPdf->setStyleSheet(
        "QPushButton { background-color: #d83b01; color: white; border: none; border-radius: 6px; padding: 10px; }"
        "QPushButton:hover { background-color: #ea4a1f; }"
    );
    btnExportPdf->hide();
    connect(btnExportPdf, &QPushButton::clicked, this, &ReportEngine::onExportPdfClicked);

    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    btnLayout->addWidget(btnExportPdf);
    btnLayout->addWidget(btnSubmitStep);
    btnLayout->addStretch();
    mainLayout->addLayout(btnLayout);
}

void ReportEngine::initializeSession(const QString &sessionPath, const QString &templatePath) {
    m_sessionPath = sessionPath;
    m_answersFilePath = sessionPath + "\\answers.json";

    loadAnswers(m_answersFilePath);
    loadReportTemplate(templatePath);
    renderState();

    if (!m_sessionPath.isEmpty()) {
        btnExportPdf->show();
    }
}

void ReportEngine::loadAnswers(const QString &path) {
    answersJson = QJsonObject();
    QFile file(path);
    if (file.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        answersJson = doc.object();
        file.close();
    }
}

void ReportEngine::loadReportTemplate(const QString &filePath) {
    reportSteps.clear();
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "خطا", "فایل الگوی آزمایش (report_template.xml) یافت نشد.");
        return;
    }

    QXmlStreamReader xml(&file);
    ReportStep currentStep;
    QString currentExperimentTitle = "";

    while (!xml.atEnd() && !xml.hasError()) {
        QXmlStreamReader::TokenType token = xml.readNext();
        if (token == QXmlStreamReader::StartElement) {
            if (xml.name().toString() == "experiment") {
                currentExperimentTitle = xml.attributes().value("title").toString();
            } else if (xml.name().toString() == "step") {
                currentStep.id = xml.attributes().value("id").toString();
                currentStep.elements.clear();

                if (!currentExperimentTitle.isEmpty()) {
                    ReportElement el;
                    el.type = "title";
                    el.content = currentExperimentTitle;
                    currentStep.elements.append(el);
                    currentExperimentTitle = "";
                }
            } else if (xml.name().toString() == "text") {
                ReportElement el; el.type = "text";
                el.content = xml.readElementText();
                currentStep.elements.append(el);
            } else if (xml.name().toString() == "static_image") {
                ReportElement el; el.type = "static_image";
                el.content = xml.readElementText();
                currentStep.elements.append(el);
            } else if (xml.name().toString() == "question") {
                ReportElement el; el.type = "question";
                el.id = xml.attributes().value("id").toString();
                el.content = xml.readElementText();
                currentStep.elements.append(el);
            } else if (xml.name().toString() == "table") {
                ReportElement el; el.type = "table";
                el.id = xml.attributes().value("id").toString();
                el.rows = xml.attributes().value("rows").toInt();
                el.cols = xml.attributes().value("cols").toInt();
                el.headers = xml.attributes().value("headers").toString();
                currentStep.elements.append(el);
            } else if (xml.name().toString() == "image") {
                ReportElement el; el.type = "image";
                el.id = xml.attributes().value("id").toString();
                el.content = xml.readElementText();
                currentStep.elements.append(el);
            }
        } else if (token == QXmlStreamReader::EndElement) {
            if (xml.name().toString() == "step") {
                reportSteps.append(currentStep);
            }
        }
    }
}

void ReportEngine::renderState() {
    QLayoutItem *item;
    while ((item = contentLayout->takeAt(0)) != nullptr) {
        if(item->widget()) delete item->widget();
        delete item;
    }
    currentActiveInputWidgets.clear();
    currentActiveElements.clear();

    currentStepIndex = 0;

    for (int i = 0; i < reportSteps.size(); ++i) {
        bool isAnswered = false;
        for(const auto& el : reportSteps[i].elements) {
            if(el.type != "text" && el.type != "title" && el.type != "static_image" && answersJson.contains(el.id)) {
                isAnswered = true;
                break;
            }
        }

        if (isAnswered) {
            renderStep(reportSteps[i], true);
            currentStepIndex = i + 1;
        } else {
            renderStep(reportSteps[i], false);
            btnSubmitStep->show();

            QTimer::singleShot(100, [this](){
                scrollArea->verticalScrollBar()->setValue(scrollArea->verticalScrollBar()->maximum());
            });
            break;
        }
    }

    if (currentStepIndex >= reportSteps.size() && reportSteps.size() > 0) {
        btnSubmitStep->hide();
    }
}

void ReportEngine::renderStep(const ReportStep &step, bool isReadOnly) {
    QFrame *stepFrame = new QFrame();
    stepFrame->setFrameShape(QFrame::StyledPanel);

    if (isReadOnly) {
        stepFrame->setStyleSheet("QFrame { background-color: #252526; border: 1px solid #333; border-radius: 8px; padding: 15px; }");
    } else {
        stepFrame->setStyleSheet("QFrame { background-color: #2a2d33; border: 1px solid #007ACC; border-radius: 8px; padding: 15px; }");
    }

    QVBoxLayout *frameLayout = new QVBoxLayout(stepFrame);
    frameLayout->setSpacing(15);

    for (const auto &el : step.elements) {
        if (el.type == "title") {
            QLabel *lbl = new QLabel(el.content);
            lbl->setWordWrap(true);
            lbl->setStyleSheet("color: #007ACC; font-size: 15pt; font-weight: bold; border: none; padding-bottom: 10px;");
            lbl->setAlignment(Qt::AlignCenter);
            frameLayout->addWidget(lbl);
        } else if (el.type == "static_image") {
            QLabel *lbl = new QLabel();
            QString imgPath = el.content;
            QFileInfo fi(imgPath);
            if (!fi.isAbsolute()) {
                imgPath = "D:/Classes/" + el.content;
            }
            QPixmap pix(imgPath);
            if (!pix.isNull()) {
                lbl->setPixmap(pix.scaled(650, 450, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                lbl->setAlignment(Qt::AlignCenter);
            } else {
                lbl->setText("❌ تصویر یافت نشد: " + imgPath);
                lbl->setStyleSheet("color: #ff4444; font-size: 11pt; border: 1px dashed #ff4444; padding: 10px;");
                lbl->setAlignment(Qt::AlignCenter);
            }
            frameLayout->addWidget(lbl);
        } else if (el.type == "text") {
            QLabel *lbl = new QLabel(el.content);
            lbl->setWordWrap(true);
            lbl->setStyleSheet("color: #cccccc; font-size: 11pt; border: none; padding: 0px;");
            frameLayout->addWidget(lbl);
        } else if (el.type == "question") {
            QLabel *lbl = new QLabel(el.content);
            lbl->setWordWrap(true);
            lbl->setStyleSheet("color: #4eb0ff; font-size: 12pt; font-weight: bold; border: none; padding: 0px;");
            frameLayout->addWidget(lbl);

            QTextEdit *txt = new QTextEdit();
            txt->setMinimumHeight(100);
            QFont txtFont("Tahoma", 11);
            txt->setFont(txtFont);

            if (isReadOnly) {
                txt->setReadOnly(true);
                txt->setText(answersJson[el.id].toString());
                txt->setStyleSheet("background-color: #1e1e1e; color: #a0a0a0; border: 1px solid #3a3a3a; border-radius: 5px; padding: 5px;");
            } else {
                currentActiveInputWidgets.append(txt);
                currentActiveElements.append(el);
                txt->setStyleSheet("background-color: #333337; color: white; border: 1px solid #555; border-radius: 5px; padding: 5px;");
            }
            frameLayout->addWidget(txt);
        } else if (el.type == "table") {
            QTableWidget *tbl = new QTableWidget(el.rows, el.cols);
            QStringList headers = el.headers.split(",");
            tbl->setHorizontalHeaderLabels(headers);

            tbl->horizontalHeader()->setMinimumHeight(55);
            tbl->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            tbl->verticalHeader()->setDefaultSectionSize(40);
            tbl->verticalHeader()->setVisible(false);
            tbl->setMinimumHeight(220);
            tbl->setAlternatingRowColors(true);

            QFont tableFont("Tahoma", 11);
            tbl->setFont(tableFont);

            if (isReadOnly) {
                tbl->setEditTriggers(QAbstractItemView::NoEditTriggers);
                tbl->setStyleSheet(
                    "QTableWidget { background-color: #1e1e1e; alternate-background-color: #252526; color: #a0a0a0; gridline-color: #333; border: 1px solid #333; border-radius: 5px; }"
                    "QHeaderView::section { background-color: #2d2d30; color: #cccccc; font-weight: bold; border: 1px solid #333; min-height: 45px; }"
                );
                QJsonArray rowsArr = answersJson[el.id].toArray();
                for(int r=0; r<rowsArr.size() && r<el.rows; ++r) {
                    QJsonArray colsArr = rowsArr[r].toArray();
                    for(int c=0; c<colsArr.size() && c<el.cols; ++c) {
                        QTableWidgetItem *item = new QTableWidgetItem(colsArr[c].toString());
                        item->setTextAlignment(Qt::AlignCenter);
                        tbl->setItem(r, c, item);
                    }
                }
            } else {
                currentActiveInputWidgets.append(tbl);
                currentActiveElements.append(el);
                tbl->setStyleSheet(
                    "QTableWidget { background-color: #252526; alternate-background-color: #2d2d30; color: white; gridline-color: #555; border: 1px solid #555; border-radius: 5px; }"
                    "QHeaderView::section { background-color: #007ACC; color: #ffffff; font-weight: bold; border: 1px solid #005A9E; min-height: 45px; }"
                );

                for(int r=0; r<el.rows; ++r) {
                    for(int c=0; c<el.cols; ++c) {
                        QTableWidgetItem *item = new QTableWidgetItem("");
                        item->setTextAlignment(Qt::AlignCenter);
                        item->setForeground(QBrush(Qt::white));
                        tbl->setItem(r, c, item);
                    }
                }
            }
            frameLayout->addWidget(tbl);
        } else if (el.type == "image") {
            QLabel *lbl = new QLabel(el.content);
            lbl->setWordWrap(true);
            lbl->setStyleSheet("color: #4eb0ff; font-size: 11pt; font-weight: bold; border: none; padding: 0px;");
            frameLayout->addWidget(lbl);

            if (isReadOnly) {
                QString savedPath = answersJson[el.id].toString();
                QFileInfo fi(savedPath);

                QLabel *imgVal = new QLabel("✔ تصویر پیوست شده: " + fi.fileName());
                imgVal->setToolTip(savedPath);
                imgVal->setStyleSheet("color: #28a745; font-size: 11pt; font-weight: bold; border: none; padding-top: 5px;");
                frameLayout->addWidget(imgVal);
            } else {
                QPushButton *btnImg = new QPushButton("انتخاب تصویر از پوشه نتایج (اسیلوسکوپ)", this);
                btnImg->setCursor(Qt::PointingHandCursor);
                btnImg->setMinimumHeight(40);
                QFont btnFont("Tahoma", 10, QFont::Bold);
                btnImg->setFont(btnFont);
                btnImg->setStyleSheet(
                    "QPushButton { background-color: #333337; color: #ddd; border: 1px solid #555; border-radius: 4px; padding: 5px; }"
                    "QPushButton:hover { background-color: #444; border: 1px solid #007ACC; }"
                );
                currentActiveInputWidgets.append(btnImg);
                currentActiveElements.append(el);
                btnImg->setProperty("selectedImage", "");

                connect(btnImg, &QPushButton::clicked, this, [btnImg, this]() {
                    QString filePath = QFileDialog::getOpenFileName(this, "انتخاب تصویر", m_sessionPath, "PNG Images (*.png)");

                    if (!filePath.isEmpty()) {
                        btnImg->setProperty("selectedImage", filePath);
                        QFileInfo fileInfo(filePath);
                        QString fileName = fileInfo.fileName();

                        btnImg->setText("✔ تصویر انتخاب شد: " + fileName + " (برای تغییر کلیک کنید)");
                        btnImg->setStyleSheet("QPushButton { background-color: #28a745; border: 1px solid #218838; color: white; border-radius: 4px; font-weight: bold; }");
                    }
                });
                frameLayout->addWidget(btnImg);
            }
        }
    }
    contentLayout->addWidget(stepFrame);
}

void ReportEngine::onSubmitClicked() {
    for (int i = 0; i < currentActiveInputWidgets.size(); ++i) {
        QWidget *w = currentActiveInputWidgets[i];
        ReportElement el = currentActiveElements[i];

        if (el.type == "question") {
            QTextEdit *txt = qobject_cast<QTextEdit*>(w);
            if(txt) answersJson[el.id] = txt->toPlainText();
        } else if (el.type == "table") {
            QTableWidget *tbl = qobject_cast<QTableWidget*>(w);
            if(tbl) {
                QJsonArray rowsArr;
                for(int r=0; r<tbl->rowCount(); ++r) {
                    QJsonArray colsArr;
                    for(int c=0; c<tbl->columnCount(); ++c) {
                        QTableWidgetItem *item = tbl->item(r, c);
                        colsArr.append(item ? item->text() : "");
                    }
                    rowsArr.append(colsArr);
                }
                answersJson[el.id] = rowsArr;
            }
        } else if (el.type == "image") {
            QPushButton *btn = qobject_cast<QPushButton*>(w);
            if(btn) answersJson[el.id] = btn->property("selectedImage").toString();
        }
    }

    QFile file(m_answersFilePath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(answersJson).toJson());
        file.close();
    }

    renderState();
}

void ReportEngine::onExportPdfClicked() {
    if (m_sessionPath.isEmpty()) {
        QMessageBox::warning(this, "خطا", "ابتدا باید گروه خود را لاگین/انتخاب کنید.");
        return;
    }

    QString timeStr = QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss");
    QString pdfPath = m_sessionPath + "\\Final_Report_" + timeStr + ".pdf";

    QPdfWriter pdfWriter(pdfPath);
    pdfWriter.setPageSize(QPageSize(QPageSize::A4));
    pdfWriter.setResolution(300);
    pdfWriter.setPageMargins(QMarginsF(15, 15, 15, 15), QPageLayout::Millimeter);

    QTextDocument doc;

    QString html = "<html dir='rtl'><body style=\"font-family: 'B Nazanin', 'B Koodak', Tahoma, Arial; font-size: 13pt; line-height: 1.8; text-align: justify; color: #000000;\">";

    html += "<h1 style='text-align: center; font-size: 22pt; font-weight: bold; margin-bottom: 0px;'>گزارش کار آزمایشگاه کنترل خطی</h1>";
    html += "<p style='text-align: center; font-size: 12pt; color: #555; margin-top: 5px;'>مربوط به مسیر: " + m_sessionPath + "</p>";
    html += "<hr style='border: 1px solid #000; margin-bottom: 20px;'>";

    for (const auto &step : reportSteps) {
        for (const auto &el : step.elements) {
            if (el.type == "title") {
                html += "<h2 style='font-size: 16pt; font-weight: bold; color: #000000; border-bottom: 2px solid #000000; padding-bottom: 5px; margin-top: 30px; margin-bottom: 15px;'>" + el.content + "</h2>";
            } else if (el.type == "static_image") {
                QString imgPath = el.content;
                QFileInfo fi(imgPath);
                if (!fi.isAbsolute()) {
                    imgPath = "D:/Classes/" + el.content;
                }
                QString fileUrl = QUrl::fromLocalFile(imgPath).toString();
                html += "<div align='center' style='margin-top: 15px; margin-bottom: 15px;'><img src='" + fileUrl + "' width='450'></div>";
            } else if (el.type == "text") {
                QString formattedText = el.content;
                formattedText.replace("\n", "<br>");
                html += "<p style='margin-bottom: 10px; margin-top: 10px;'>" + formattedText + "</p>";
            } else if (el.type == "question") {
                html += "<p style='font-size: 14pt; font-weight: bold; margin-top: 20px; margin-bottom: 10px;'>" + el.content + "</p>";

                QString ans = answersJson.contains(el.id) ? answersJson[el.id].toString() : "";
                if(ans.trimmed().isEmpty()) {
                    ans = ".......................................................................................................................................................";
                } else {
                    ans.replace("\n", "<br>");
                }

                html += "<div style='background-color: #f8f9fa; border: 1px dashed #666; padding: 15px; color: #003366; margin-bottom: 20px; font-family: Tahoma; font-size: 12pt;'>" + ans + "</div>";
            } else if (el.type == "table") {
                html += "<div align='center'>";
                html += "<table border='1' cellspacing='0' cellpadding='6' style='width: 90%; margin-left: auto; margin-right: auto; border-collapse: collapse; margin-top: 15px; margin-bottom: 15px; border: 1px solid #000;'>";
                html += "<tr style='background-color: #e0e0e0; font-weight: bold; text-align: center;'>";
                QStringList headers = el.headers.split(",");
                for (const QString &h : headers) {
                    html += "<th style='border: 1px solid #000; padding: 8px;'>" + h + "</th>";
                }
                html += "</tr>";

                if (answersJson.contains(el.id)) {
                    QJsonArray rowsArr = answersJson[el.id].toArray();
                    for (int r = 0; r < rowsArr.size() && r < el.rows; ++r) {
                        html += "<tr>";
                        QJsonArray colsArr = rowsArr[r].toArray();
                        for (int c = 0; c < colsArr.size() && c < el.cols; ++c) {
                            html += "<td style='border: 1px solid #000; padding: 8px; text-align: center; color: #003366;'>" + colsArr[c].toString() + "</td>";
                        }
                        html += "</tr>";
                    }
                } else {
                    for (int r = 0; r < el.rows; ++r) {
                        html += "<tr>";
                        for (int c = 0; c < el.cols; ++c) {
                            html += "<td style='border: 1px solid #000; padding: 8px; text-align: center; color: #999;'></td>";
                        }
                        html += "</tr>";
                    }
                }
                html += "</table>";
                html += "</div>";
            } else if (el.type == "image") {
                html += "<p style='font-size: 14pt; font-weight: bold; margin-top: 20px; margin-bottom: 10px;'>" + el.content + "</p>";
                if (answersJson.contains(el.id)) {
                    QString imgPath = answersJson[el.id].toString();
                    if (!imgPath.isEmpty()) {
                        QString fileUrl = QUrl::fromLocalFile(imgPath).toString();
                        html += "<div align='center' style='margin-top: 15px; margin-bottom: 15px; border: 2px solid #000; padding: 5px;'><img src='" + fileUrl + "' width='550'></div>";
                    } else {
                        html += "<div style='text-align: center; border: 1px dashed #000; padding: 20px; color: #999; margin-top: 10px; margin-bottom: 10px;'>محل قرارگیری تصویر دانشجو</div>";
                    }
                } else {
                    html += "<div style='text-align: center; border: 1px dashed #000; padding: 20px; color: #999; margin-top: 10px; margin-bottom: 10px;'>محل قرارگیری تصویر دانشجو</div>";
                }
            }
        }
        html += "<hr style='border: 0.5px solid #ccc; margin-top: 20px; margin-bottom: 20px;'>";
    }
    html += "</body></html>";

    doc.setHtml(html);

    QPainter painter(&pdfWriter);
    QSizeF pageSize(pdfWriter.width(), pdfWriter.height() - 200);
    doc.setPageSize(pageSize);

    int pageCount = doc.pageCount();
    for (int i = 0; i < pageCount; ++i) {
        if (i > 0) pdfWriter.newPage();

        QRectF clip(0, i * pageSize.height(), pageSize.width(), pageSize.height());
        painter.save();
        painter.translate(0, -i * pageSize.height());
        doc.drawContents(&painter, clip);
        painter.restore();

        painter.save();
        painter.setPen(QColor(100, 100, 100));
        painter.setFont(QFont("Tahoma", 10));

        QRect footerRect(0, pdfWriter.height() - 100, pdfWriter.width(), 100);
        painter.drawText(footerRect, Qt::AlignCenter | Qt::AlignBottom, "صفحه " + QString::number(i + 1));
        painter.restore();
    }

    QMessageBox::information(this, "گزارش نهایی تولید شد", "فایل PDF گزارش با فرمت استاندارد تولید شد و در مسیر زیر ذخیره گردید:\n\n" + pdfPath);
}
