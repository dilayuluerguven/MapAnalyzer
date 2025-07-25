#include "mainwindow.h"
#include <QMimeData>
#include <QUrl>
#include <QDebug>
#include <QMenuBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QFile>
#include <QTextStream>
#include "xlsxdocument.h" // QXlsx
#include <QGraphicsDropShadowEffect>
#include <QPropertyAnimation>
#include <QHeaderView>
#include <QPushButton>
#include <QToolBar>
#include <QDesktopServices>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),chartRow(nullptr) {

    setAcceptDrops(true);
    resize(1000, 600);
    setWindowTitle("Map Analyzer");
    QMenuBar *menuBar = new QMenuBar(this);

    setMenuBar(menuBar);
    QToolBar *toolBar = addToolBar("Araçlar");
      toolBar->addAction(QIcon(":/icons/open.png"), "Dosya Aç", this, &MainWindow::openFileDialog);
      toolBar->addAction(QIcon(":/icons/excel.png"), "Excel'e Kaydet", this, &MainWindow::exportToExcel);
      toolBar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);

      QAction *stackAction = toolBar->addAction("STACK");
      QAction *flashAction = toolBar->addAction("FLASH");
      QAction *ramAction = toolBar->addAction("RAM");




    QWidget *central = new QWidget(this);
    mainLayout = new QVBoxLayout(central);

    dropLabel = new ClickableLabel(this);
    dropLabel->setText("📁 Buraya .map dosyasını sürükleyebilirsiniz");
    dropLabel->setAlignment(Qt::AlignCenter);
    dropLabel->setStyleSheet("QLabel { border: 2px dashed #aaa; font-size: 16px; padding: 20px; color: #555; }");
    dropLabel->setFixedHeight(80);
    dropLabel->setAcceptDrops(true);
    mainLayout->addWidget(dropLabel);

    connect(dropLabel, &ClickableLabel::clicked, this, &MainWindow::openFileDialog);
    QHBoxLayout *thresholdLayout = new QHBoxLayout();

    greenMinSpin = new QSpinBox(this);
    greenMinSpin->setRange(0, 100);
    greenMinSpin->setValue(60);
    greenMinSpin->setStyleSheet(
        "QSpinBox {"
        "   background-color: #f8f9fa;"
        "   border: 1px solid #d3dce6;"
        "   border-radius: 4px;"
        "   padding: 5px;"
        "   min-width: 60px;"
        "margin-right: 15px;"
        "}"
        "QSpinBox::up-button {"
        "   subcontrol-origin: border;"
        "   subcontrol-position: top right;"
        "   width: 20px;"
        "   border-left: 1px solid #d3dce6;"
        "   border-bottom: 1px solid #d3dce6;"
        "   border-top-right-radius: 4px;"
        "   background-color: #e9ecef;"
        "}"
        "QSpinBox::down-button {"
        "   subcontrol-origin: border;"
        "   subcontrol-position: bottom right;"
        "   width: 20px;"
        "   border-left: 1px solid #d3dce6;"
        "   border-bottom-right-radius: 4px;"
        "   background-color: #e9ecef;"
        "}"
        "QSpinBox::up-button:hover, QSpinBox::down-button:hover {"
        "   background-color: #dee2e6;"
        "}"
        "QSpinBox::up-arrow, QSpinBox::down-arrow {"
        "   width: 7px;"
        "   height: 7px;"
        "}"
    );

    yellowMinSpin = new QSpinBox(this);
    yellowMinSpin->setRange(0, 100);
    yellowMinSpin->setValue(40);
    yellowMinSpin->setStyleSheet(greenMinSpin->styleSheet());

    QLabel *greenLabel = new QLabel("Yeşil sınır (%):", this);
    greenLabel->setStyleSheet("QLabel { color: #2d3748; font-weight: bold; }");

    QLabel *yellowLabel = new QLabel("Sarı sınır (%):", this);
    yellowLabel->setStyleSheet(greenLabel->styleSheet());

    thresholdLayout->addWidget(greenLabel);
    thresholdLayout->addWidget(greenMinSpin);
    thresholdLayout->addSpacing(15);
    thresholdLayout->addWidget(yellowLabel);
    thresholdLayout->addWidget(yellowMinSpin);
    thresholdLayout->addStretch();

    connect(greenMinSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MainWindow::updateMemoryTable);
    connect(yellowMinSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MainWindow::updateMemoryTable);

    mainLayout->insertLayout(1, thresholdLayout);

    memoryTable = new QTableWidget(this);
    memoryTable->setColumnCount(5);
    memoryTable->setHorizontalHeaderLabels({"Bellek Türü", "Toplam (KB)", "Kullanılan (KB)", "Boş (KB)", "Kullanım %"});
    memoryTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    memoryTable->setStyleSheet("QTableWidget { background-color: #f8f9fa; border: 1px solid #ddd; }"
                               "QHeaderView::section { background-color: #3498db; color: white; padding: 5px; }");
    memoryTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    memoryTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    memoryTable->setSelectionMode(QAbstractItemView::SingleSelection);

    initializeMemoryTable();
    mapContentView = new QTextEdit(this);
    mapContentView->setReadOnly(true);
    mapContentView->setStyleSheet(
        "QTextEdit {"
        "   background: #f5f7fa;"
        "   border: 1px solid #d3dce6;"
        "   border-radius: 4px;"
        "   font-family: 'Consolas', monospace;"
        "   font-size: 16px;"
        "   color: #2d3748;"
        "   padding: 6px;"
        "   line-height: 1.3;"
        "}"
        "QScrollBar:vertical { width: 10px; background: #edf2f7; }"
        "QScrollBar::handle:vertical { background: #c1ccdb; min-height: 30px; }"
    );
    mapContentView->setPlaceholderText("📋 .map dosyası içeriği burada gösterilecek...");
    mapContentView->setFixedHeight(150);
    mainLayout->addWidget(mapContentView);

    QPushButton *showChartsButton = new QPushButton("Grafikleri Göster", this);
    showChartsButton->setStyleSheet("QPushButton { background-color: #3498db; color: white; padding: 8px; border-radius: 4px; }"
                                   "QPushButton:hover { background-color: #2980b9; }");
    connect(showChartsButton, &QPushButton::clicked, this, &MainWindow::showCharts);

    chartRow = new QHBoxLayout();
    stackChartView = new QtCharts::QChartView();
    flashChartView = new QtCharts::QChartView();
    ramChartView = new QtCharts::QChartView();
    chartRow->addWidget(stackChartView);
    chartRow->addWidget(flashChartView);
    chartRow->addWidget(ramChartView);

    stackChartView->setVisible(false);
    flashChartView->setVisible(false);
    ramChartView->setVisible(false);

    connect(stackAction, &QAction::triggered, this, [this]() {
        MemoryDetailDialog dlg("STACK", lastStats.stackUsed, lastStats.stackTotal, this);
        dlg.exec();
    });
    connect(flashAction, &QAction::triggered, this, [this]() {
        MemoryDetailDialog dlg("FLASH", lastStats.flashUsed, lastStats.flashTotal, this);
        dlg.exec();
    });
    connect(ramAction, &QAction::triggered, this, [this]() {
        MemoryDetailDialog dlg("RAM", lastStats.ramUsed, lastStats.ramTotal, this);
        dlg.exec();
    });
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addWidget(showChartsButton);
    buttonLayout->addStretch();

    mainLayout->addWidget(memoryTable);
    mainLayout->addLayout(buttonLayout);
    mainLayout->addLayout(chartRow);
    setCentralWidget(central);
}

void MainWindow::initializeMemoryTable() {
    memoryTable->setRowCount(0);
    lastStats = {};
}
void MainWindow::updateMemoryTable() {
    if (lastStats.stackTotal == 0 && lastStats.flashTotal == 0 && lastStats.ramTotal == 0) {
        memoryTable->setRowCount(0);
        return;
    }

    memoryTable->setRowCount(3);

    auto addRow = [&](int row, const QString &type, double used, double total) {
        double free = total - used;
        double percent = (total > 0) ? (used * 100.0 / total) : 0.0;

        memoryTable->setItem(row, 0, new QTableWidgetItem(type));
        memoryTable->setItem(row, 1, new QTableWidgetItem(QString::number(total, 'f', 2)));
        memoryTable->setItem(row, 2, new QTableWidgetItem(QString::number(used, 'f', 2)));
        memoryTable->setItem(row, 3, new QTableWidgetItem(QString::number(free, 'f', 2)));
        memoryTable->setItem(row, 4, new QTableWidgetItem(QString("%1%").arg(QString::number(percent, 'f', 2))));

        int greenMin = greenMinSpin->value();
        int yellowMin = yellowMinSpin->value();

        QTableWidgetItem *percentItem = memoryTable->item(row, 4);

        if (percent >= greenMin) {
            percentItem->setBackground(QColor("#06d6a0"));
        } else if (percent >= yellowMin) {
            percentItem->setBackground(QColor("#ffd166"));
        } else {
            percentItem->setBackground(QColor("#ff6b6b"));
        }

    };

    addRow(0, "STACK", lastStats.stackUsed, lastStats.stackTotal);
    addRow(1, "FLASH", lastStats.flashUsed, lastStats.flashTotal);
    addRow(2, "RAM", lastStats.ramUsed, lastStats.ramTotal);
}void MainWindow::showCharts()
{
    bool visible = !stackChartView->isVisible();

    stackChartView->setVisible(visible);
    flashChartView->setVisible(visible);
    ramChartView->setVisible(visible);

    if (visible) {
        setupCharts();

        showPieChart(stackChartView, "STACK", lastStats.stackUsed, lastStats.stackTotal);
        showPieChart(flashChartView, "FLASH", lastStats.flashUsed, lastStats.flashTotal);
        showPieChart(ramChartView, "RAM", lastStats.ramUsed, lastStats.ramTotal);

        layout()->update();
    }
}
void MainWindow::setupCharts()
{
    QSizePolicy chartSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    chartSizePolicy.setHorizontalStretch(1);

    const int chartSize = 350;
    QSize chartDimensions(chartSize, chartSize);

    auto setupChartView = [&](QtCharts::QChartView* view) {
        view->setSizePolicy(chartSizePolicy);
        view->setMinimumSize(chartDimensions);
        view->setRenderHint(QPainter::Antialiasing);
    };

    setupChartView(stackChartView);
    setupChartView(flashChartView);
    setupChartView(ramChartView);

    chartRow->setStretch(0, 1);
    chartRow->setStretch(1, 1);
    chartRow->setStretch(2, 1);
}
MainWindow::~MainWindow() {}

void MainWindow::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasUrls()) {
        QList<QUrl> urls = event->mimeData()->urls();
        if (!urls.isEmpty() && urls.first().toLocalFile().endsWith(".map")) {
            event->acceptProposedAction();
        }
    }
}

void MainWindow::dragMoveEvent(QDragMoveEvent *event) {
    event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent *event) {
    QList<QUrl> urls = event->mimeData()->urls();
    if (!urls.isEmpty()) {
        QString filePath = urls.first().toLocalFile();
        if (filePath.endsWith(".map")) {
            openFile(filePath);
        }
    }
}

void MainWindow::openFileDialog() {
    QString filePath = QFileDialog::getOpenFileName(this, "Map Dosyası Seç", "", "Map Files (*.map);;All Files (*)");
    if (!filePath.isEmpty()) {
        openFile(filePath);
    }
}

void MainWindow::openFile(const QString &filePath) {
    lastStats = {};

    QFile file(filePath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString fileContents = QTextStream(&file).readAll();
        mapContentView->setPlainText(fileContents);
        file.close();
    } else {
        mapContentView->setPlainText("Dosya okunamadı!");
    }

    if (!parseMapFile(filePath, lastStats)) {
        QMessageBox::warning(this, "Hata", "Dosya işlenirken bir hata oluştu.");
        return;
    }

    lastStats.stackUsed /= 1024.0;
    lastStats.stackTotal /= 1024.0;
    lastStats.flashUsed /= 1024.0;
    lastStats.flashTotal /= 1024.0;
    lastStats.ramUsed /= 1024.0;
    lastStats.ramTotal /= 1024.0;

    updateMemoryTable();

    if (stackChartView->isVisible()) {
        showPieChart(stackChartView, "STACK", lastStats.stackUsed, lastStats.stackTotal);
        showPieChart(flashChartView, "FLASH", lastStats.flashUsed, lastStats.flashTotal);
        showPieChart(ramChartView, "RAM", lastStats.ramUsed, lastStats.ramTotal);
    }

    QFileInfo fileInfo(filePath);
    setWindowTitle("Map Analyzer - " + fileInfo.fileName());
    QMessageBox::information(this, "Başarılı",
            QString("Dosya başarıyla yüklendi:\n%1").arg(fileInfo.fileName()));
}

void MainWindow::updateCharts(const QVector<QString> &lines) {

    lastStats.stackUsed = 120.0;
    lastStats.stackTotal = 200.0;
    lastStats.flashUsed = 150.0;
    lastStats.flashTotal = 300.0;
    lastStats.ramUsed = 180.0;
    lastStats.ramTotal = 256.0;

    updateMemoryTable();

    if (stackChartView->isVisible()) {
        showPieChart(stackChartView, "STACK", lastStats.stackUsed, lastStats.stackTotal);
        showPieChart(flashChartView, "FLASH", lastStats.flashUsed, lastStats.flashTotal);
        showPieChart(ramChartView, "RAM", lastStats.ramUsed, lastStats.ramTotal);
    }
}

void MainWindow::showPieChart(QtCharts::QChartView *view, const QString &title, double used, double total) {
    using namespace QtCharts;

    QPieSeries *series = new QPieSeries();
    double free = total - used;

    QPieSlice *usedSlice = series->append("Used", used);
    QPieSlice *freeSlice = series->append("Free", free);

    QLinearGradient usedGradient(0, 0, 1, 1);
    usedGradient.setColorAt(0, QColor("#3498db"));
    usedGradient.setColorAt(1, QColor("#2980b9"));
    usedSlice->setBrush(usedGradient);

    QLinearGradient freeGradient(0, 0, 1, 1);
    freeGradient.setColorAt(0, QColor("#bdc3c7"));
    freeGradient.setColorAt(1, QColor("#95a5a6"));
    freeSlice->setBrush(freeGradient);

    QPen pen(Qt::white);
    pen.setWidth(2);
    usedSlice->setPen(pen);
    freeSlice->setPen(pen);

    series->setPieSize(0.7);  // Daha küçük boyut
    series->setHorizontalPosition(0.5);
    series->setVerticalPosition(0.5);

    series->setLabelsPosition(QPieSlice::LabelOutside);
    series->setLabelsVisible(true);

    for (auto slice : series->slices()) {
        slice->setLabelVisible(true);
        double percent = slice->percentage() * 100.0;
        slice->setLabel(QString("%1\n%2% (%3 KB)")
                        .arg(slice->label())
                        .arg(percent, 0, 'f', 1)
                        .arg(slice->value(), 0, 'f', 2));

        QFont font = slice->labelFont();
        font.setFamily("Segoe UI");
        font.setPointSize(10);
        font.setBold(true);
        slice->setLabelFont(font);
        slice->setLabelColor(QColor("#333333"));

        slice->setLabelArmLengthFactor(0.1);
        slice->setLabelPosition(QPieSlice::LabelOutside); // Enum değeri kullanılıyor
    }

    connect(series, &QPieSeries::hovered, [=](QPieSlice *slice, bool state){
        QPropertyAnimation *explodeAnim = new QPropertyAnimation(slice, "explodeDistanceFactor");
        explodeAnim->setDuration(300);
        explodeAnim->setEasingCurve(QEasingCurve::OutBack);

        if(state) {
            explodeAnim->setStartValue(slice->explodeDistanceFactor());
            explodeAnim->setEndValue(0.15);  // Patlama miktarı
            slice->setLabelFont(QFont("Segoe UI", 11, QFont::Bold));
        } else {
            explodeAnim->setStartValue(slice->explodeDistanceFactor());
            explodeAnim->setEndValue(0);    // Orijinal pozisyon
            slice->setLabelFont(QFont("Segoe UI", 10, QFont::Bold));
        }

        explodeAnim->start(QPropertyAnimation::DeleteWhenStopped);
    });

    QChart *chart = new QChart();
    chart->addSeries(series);
    chart->setTitle(QString("<b>%1 Memory Usage</b><br><span style='font-size:10pt; color:#555'>Total: %2 KB</span>")
                    .arg(title)
                    .arg(total, 0, 'f', 2));

    chart->setTitleBrush(QColor("#333333"));
    QFont titleFont("Segoe UI", 12, QFont::Bold);
    titleFont.setWeight(QFont::DemiBold);
    chart->setTitleFont(titleFont);

    chart->setAnimationOptions(QChart::AllAnimations);
    chart->setAnimationDuration(1200);

    chart->setBackgroundBrush(QColor("#f8f9fa"));
    chart->setBackgroundRoundness(10);
    chart->setMargins(QMargins(15, 15, 15, 15));
    chart->setContentsMargins(-10, -10, -10, -10);

    chart->legend()->setVisible(true);
    chart->legend()->setAlignment(Qt::AlignBottom);
    chart->legend()->setFont(QFont("Segoe UI", 9));
    chart->legend()->setLabelColor(QColor("#555555"));
    chart->legend()->setMarkerShape(QLegend::MarkerShapeCircle);

    view->setRenderHint(QPainter::Antialiasing, true);
    view->setRenderHint(QPainter::TextAntialiasing, true);
    view->setRenderHint(QPainter::SmoothPixmapTransform, true);
    view->setBackgroundBrush(QColor("#f8f9fa"));
    view->setChart(chart);

    QGraphicsDropShadowEffect *shadowEffect = new QGraphicsDropShadowEffect();
    shadowEffect->setBlurRadius(15);
    shadowEffect->setColor(QColor(0, 0, 0, 160));
    shadowEffect->setOffset(3, 3);
    view->setGraphicsEffect(shadowEffect);
}
void MainWindow::exportToExcel() {
    if (lastStats.stackTotal == 0 && lastStats.flashTotal == 0 && lastStats.ramTotal == 0) {
        QMessageBox::warning(this, "Uyarı", "Dışa aktarılacak veri bulunamadı!");
        return;
    }

    QString path = QFileDialog::getSaveFileName(this,
        "Excel Dosyasını Kaydet",
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/memory_stats.xlsx",
        "Excel Files (*.xlsx)");

    if (path.isEmpty()) return;

    QXlsx::Document xlsx;

    QXlsx::Format headerFormat;
    headerFormat.setFontBold(true);
    headerFormat.setFontSize(12);
    headerFormat.setFillPattern(QXlsx::Format::PatternSolid);
    headerFormat.setPatternBackgroundColor(QColor("#3498db"));
    headerFormat.setFontColor(Qt::white);
    headerFormat.setHorizontalAlignment(QXlsx::Format::AlignHCenter);

    QXlsx::Format dataFormat;
    dataFormat.setFontSize(11);
    dataFormat.setHorizontalAlignment(QXlsx::Format::AlignRight);

    xlsx.write(1, 1, "Bellek Türü", headerFormat);
    xlsx.write(1, 2, "Toplam (KB)", headerFormat);
    xlsx.write(1, 3, "Kullanılan (KB)", headerFormat);
    xlsx.write(1, 4, "Boş (KB)", headerFormat);
    xlsx.write(1, 5, "Kullanım %", headerFormat);

    xlsx.setColumnWidth(1, 20);
    xlsx.setColumnWidth(2, 15);
    xlsx.setColumnWidth(3, 15);
    xlsx.setColumnWidth(4, 15);
    xlsx.setColumnWidth(5, 15);

    auto writeRow = [&](int row, const QString &type, double used, double total) {
        double free = total - used;
        double percent = (total > 0) ? (used * 100.0 / total) : 0.0;

        // Kullanım yüzdesine göre renk formatı
        QXlsx::Format percentFormat = dataFormat;
        if (percent > 80) {
            percentFormat.setPatternBackgroundColor(QColor("#ff6b6b"));
        } else if (percent > 60) {
            percentFormat.setPatternBackgroundColor(QColor("#ffd166"));
        } else {
            percentFormat.setPatternBackgroundColor(QColor("#06d6a0"));
        }

        xlsx.write(row, 1, type, dataFormat);
        xlsx.write(row, 2, total, dataFormat);
        xlsx.write(row, 3, used, dataFormat);
        xlsx.write(row, 4, free, dataFormat);
        xlsx.write(row, 5, QString("%1%").arg(percent, 0, 'f', 2), percentFormat);
    };

    writeRow(2, "STACK", lastStats.stackUsed, lastStats.stackTotal);
    writeRow(3, "FLASH", lastStats.flashUsed, lastStats.flashTotal);
    writeRow(4, "RAM",   lastStats.ramUsed,   lastStats.ramTotal);

    if (xlsx.saveAs(path)) {
          #ifdef Q_OS_WIN
              QDesktopServices::openUrl(QUrl::fromLocalFile(path));
          #endif

          QMessageBox::information(this, "Başarılı",
              QString("Excel dosyası başarıyla kaydedildi ve açılıyor:\n%1").arg(path));
      } else {
          QMessageBox::warning(this, "Hata", "Excel dosyası kaydedilemedi!");
      }
}
