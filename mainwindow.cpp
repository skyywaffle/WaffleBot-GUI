#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QXmlStreamWriter>
#include <QCoreApplication>
#include <QDebug>
#include <QDomDocument>
#include <QMessageBox>
#include <QWidget>
#include <filesystem>
#include "sndfile.h"
#include "ClickFolder.h"
#include <algorithm>

namespace fs = std::filesystem;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    setFixedSize(QSize(712, 474));
    loadDefaultSettings();

    // set hint text for the click type threshold text fields
    ui->softClickThresholdTextEdit->setPlaceholderText(QString::number(0.15));
    ui->softClickAfterReleaseThresholdTextEdit->setPlaceholderText(QString::number(0.1));
    ui->hardClickThresholdTextEdit->setPlaceholderText(QString::number(2.0));
    ui->hardClickAfterReleaseThresholdTextEdit->setPlaceholderText(QString::number(1.5));
    ui->microClickThresholdTextEdit->setPlaceholderText(QString::number(0.05));
    ui->microClickAfterReleaseThresholdTextEdit->setPlaceholderText(QString::number(0.03));

    checkFields();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_loadMacrosButton_clicked()
{
    QStringList fileNames = QFileDialog::getOpenFileNames(this,
                                                          tr("Select Files"),
                                                          dialogOpenPath,
                                                          tr("Geometry Dash Macros (*.json)"));

    // Attempt to parse macros
    for (auto& file : fileNames) {
        QFileInfo fileInfo{file};
        dialogOpenPath = fileInfo.filePath();
        Macro macro{file.toStdString()};

        if (macro.isLoaded()) {
            file = fileInfo.fileName();
            ui->macrosListWidget->addItem(file);
            macroVector.push_back(std::move(macro));
        }
    }
    checkFields();
}


void MainWindow::on_unloadMacrosButton_clicked()
{
    for (auto item : ui->macrosListWidget->selectedItems())
    {
        int row = ui->macrosListWidget->row(item);

        // Remove corresponding index for the actual macro data
        macroVector.erase(macroVector.begin() + row); // this line can suck my cock 🖕

        // Then remove from list widget
        delete ui->macrosListWidget->takeItem(row);
    }
    checkFields();
}


void MainWindow::on_unloadAllMacrosButton_clicked()
{
    // Clear
    macroVector.clear();
    ui->macrosListWidget->clear();
    checkFields();
}

void MainWindow::getClicks(ClickFolder clickType, std::vector<const char*> folderPaths)
{
    for (auto& folderPath : folderPaths) {
        for (auto file : getAudioFiles(folderPath)) {
            clickFiles[clickType].emplace_back(file);
        }
    }
}

void MainWindow::on_loadClickpackButton_clicked()
{
    bool softclickLoaded = false;
    bool hardclickLoaded = false;
    // empty clickFiles array's previous values, if any
    clickFiles.fill({});
    QString previousWorkingPath = QDir::currentPath();

    // Get clickpack folder and set the working directory to it
    QString folderPath = QFileDialog::getExistingDirectory(this, tr("Select Folder"), dialogOpenPath);

    QDir::setCurrent(folderPath);

    // Get clicks lmfao
    for (std::size_t i{0}; i < clickFolderNames.size(); i++) {
        getClicks((ClickFolder)i, clickFolderNames[i]);
    }

    // noise.wav or whitenoise.wav in root, player1 or player2 folder
    noiseFile = getNoiseFile(folderPath.toStdString().c_str());

    if (noiseFile.buffer.empty()) {
        noiseFile = getNoiseFile(folderPath.toStdString().append("player1").c_str());
    }

    if (noiseFile.buffer.empty()) {
        noiseFile = getNoiseFile(folderPath.toStdString().append("player2").c_str());
    }

    QDir::setCurrent(previousWorkingPath);

    // Determine if clickpack loaded correctly
    std::string loadedClickpackText{""};

    for (auto& vector : clickFiles) {
        if (!vector.empty()) {
            QFileInfo folderInfo{folderPath};
            loadedClickpackText.append(folderInfo.fileName().toStdString());
            clickpackPath = folderInfo.absoluteFilePath();
            break;
        }
    }

    ui->clickpackNameLabel->setText(QString::fromStdString(loadedClickpackText));
    dialogOpenPath = folderPath;
    checkFields();
}

void MainWindow::checkFields() {
    bool softclickLoaded = false;
    bool hardclickLoaded = false;
    bool microclickLoaded = false;

    ui->unloadMacrosButton->setEnabled(ui->macrosListWidget->count() > 0);
    ui->unloadAllMacrosButton->setEnabled(ui->macrosListWidget->count() > 0);

    // Determine if soft clicks/hard clicks are loaded
    std::vector<int> softclickShitVector {
        PLAYER_1_SOFTCLICKS,
        PLAYER_1_SOFTRELEASES,
        PLAYER_1_LEFTSOFTCLICKS,
        PLAYER_1_LEFTSOFTRELEASES,
        PLAYER_1_LEFTSOFTCLICKS,
        PLAYER_1_LEFTSOFTRELEASES,

        PLAYER_2_SOFTCLICKS,
        PLAYER_2_SOFTRELEASES,
        PLAYER_2_LEFTSOFTCLICKS,
        PLAYER_2_LEFTSOFTRELEASES,
        PLAYER_2_LEFTSOFTCLICKS,
        PLAYER_2_LEFTSOFTRELEASES,
    };

    std::vector<int> hardclickShitVector {
        PLAYER_1_HARDCLICKS,
        PLAYER_1_HARDRELEASES,
        PLAYER_1_LEFTHARDCLICKS,
        PLAYER_1_LEFTHARDRELEASES,
        PLAYER_1_LEFTHARDCLICKS,
        PLAYER_1_LEFTHARDRELEASES,

        PLAYER_2_HARDCLICKS,
        PLAYER_2_HARDRELEASES,
        PLAYER_2_LEFTHARDCLICKS,
        PLAYER_2_LEFTHARDRELEASES,
        PLAYER_2_LEFTHARDCLICKS,
        PLAYER_2_LEFTHARDRELEASES,
    };

    std::vector<int> microclickShitVector {
        PLAYER_1_MICROCLICKS,
        PLAYER_1_MICRORELEASES,
        PLAYER_1_LEFTMICROCLICKS,
        PLAYER_1_LEFTMICRORELEASES,
        PLAYER_1_LEFTMICROCLICKS,
        PLAYER_1_LEFTMICRORELEASES,

        PLAYER_2_MICROCLICKS,
        PLAYER_2_MICRORELEASES,
        PLAYER_2_LEFTMICROCLICKS,
        PLAYER_2_LEFTMICRORELEASES,
        PLAYER_2_LEFTMICROCLICKS,
        PLAYER_2_LEFTMICRORELEASES,
    };

    for (int i : softclickShitVector) {
        if (!clickFiles[i].empty()) {
            softclickLoaded = true;
            break;
        }
    }

    for (int i : hardclickShitVector) {
        if (!clickFiles[i].empty()) {
            hardclickLoaded = true;
            break;
        }
    }

    for (int i : microclickShitVector) {
        if (!clickFiles[i].empty()) {
            microclickLoaded = true;
            break;
        }
    }

    ui->noiseCheckBox->setCheckable(!noiseFile.buffer.empty());
    ui->softClickThresholdTextEdit->setEnabled(softclickLoaded);
    ui->hardClickThresholdTextEdit->setEnabled(hardclickLoaded);
    ui->microClickThresholdTextEdit->setEnabled(microclickLoaded);
    ui->softClickAfterReleaseThresholdTextEdit->setEnabled(softclickLoaded);
    ui->hardClickAfterReleaseThresholdTextEdit->setEnabled(hardclickLoaded);
    ui->microClickAfterReleaseThresholdTextEdit->setEnabled(microclickLoaded);

    // Determine if render button is clickable
    bool allTextEditsValid;
    bool softClickThresholdValid = true;
    bool softClickAfterReleaseThresholdValid = true;
    bool hardClickThresholdValid = true;
    bool hardClickAfterReleaseThresholdValid = true;
    bool microClickThresholdValid = true;
    bool microClickAfterReleaseThresholdValid = true;

    if (ui->softClickThresholdTextEdit->isEnabled()) {
        ui->softClickThresholdTextEdit->document()->toPlainText().toDouble(&softClickThresholdValid);
    }

    if (ui->softClickAfterReleaseThresholdTextEdit->isEnabled()) {
        ui->softClickAfterReleaseThresholdTextEdit->document()->toPlainText().toDouble(&softClickAfterReleaseThresholdValid);
    }

    if (ui->hardClickThresholdTextEdit->isEnabled()) {
        ui->hardClickThresholdTextEdit->document()->toPlainText().toDouble(&hardClickThresholdValid);
    }

    if (ui->hardClickAfterReleaseThresholdTextEdit->isEnabled()) {
        ui->hardClickAfterReleaseThresholdTextEdit->document()->toPlainText().toDouble(&hardClickAfterReleaseThresholdValid);
    }

    if (ui->microClickThresholdTextEdit->isEnabled()) {
        ui->microClickThresholdTextEdit->document()->toPlainText().toDouble(&microClickThresholdValid);
    }

    if (ui->microClickAfterReleaseThresholdTextEdit->isEnabled()) {
        ui->microClickAfterReleaseThresholdTextEdit->document()->toPlainText().toDouble(&microClickAfterReleaseThresholdValid);
    }

    allTextEditsValid = (softClickThresholdValid && softClickAfterReleaseThresholdValid && hardClickThresholdValid && hardClickAfterReleaseThresholdValid && microClickThresholdValid && microClickAfterReleaseThresholdValid);

    ui->saveConfigButton->setEnabled(allTextEditsValid && !ui->clickpackNameLabel->text().isEmpty());
    ui->renderButton->setEnabled(!ui->clickpackNameLabel->text().isEmpty() && ui->unloadAllMacrosButton->isEnabled() && allTextEditsValid);

}


void MainWindow::on_noiseCheckBox_clicked()
{
    checkFields();
}


void MainWindow::on_microClickThresholdTextEdit_textChanged()
{
    checkFields();
}


void MainWindow::on_microClickAfterReleaseThresholdTextEdit_textChanged()
{
    checkFields();
}


void MainWindow::on_softClickThresholdTextEdit_textChanged()
{
    checkFields();
}


void MainWindow::on_softClickAfterReleaseThresholdTextEdit_textChanged()
{
    checkFields();
}


void MainWindow::on_hardClickThresholdTextEdit_textChanged()
{
    checkFields();
}


void MainWindow::on_hardClickAfterReleaseThresholdTextEdit_textChanged()
{
    checkFields();
}


void MainWindow::loadClickpackFromConfig(const QString& absoluteFilePath) {
    bool softclickLoaded = false;
    bool hardclickLoaded = false;
    // empty clickFiles array's previous values, if any
    clickFiles.fill({});
    QString previousWorkingPath = QDir::currentPath();

    // Get clickpack folder and set the working directory to it
    QString folderPath = absoluteFilePath;

    QDir::setCurrent(folderPath);

    // Get clicks lmfao
    for (std::size_t i{0}; i < clickFolderNames.size(); i++) {
        getClicks((ClickFolder)i, clickFolderNames[i]);
    }

    // noise.wav or whitenoise.wav in root, player1 or player2 folder
    noiseFile = getNoiseFile(folderPath.toStdString().c_str());

    if (noiseFile.buffer.empty()) {
        noiseFile = getNoiseFile(folderPath.toStdString().append("player1").c_str());
    }

    if (noiseFile.buffer.empty()) {
        noiseFile = getNoiseFile(folderPath.toStdString().append("player2").c_str());
    }

    QDir::setCurrent(previousWorkingPath);

    // Determine if clickpack loaded correctly
    std::string loadedClickpackText{""};

    for (auto& vector : clickFiles) {
        if (!vector.empty()) {
            QFileInfo folderInfo{folderPath};
            loadedClickpackText.append(folderInfo.fileName().toStdString());
            clickpackPath = folderInfo.absoluteFilePath();
            break;
        }
    }

    ui->clickpackNameLabel->setText(QString::fromStdString(loadedClickpackText));
    checkFields();
}

void MainWindow::on_saveConfigButton_clicked()
{
    QString configFileName = QFileDialog::getSaveFileName(this, tr("Save File"), dialogOpenPath, tr("XML files (*.xml)"));
    if (!configFileName.isEmpty() && !configFileName.endsWith(".xml")) {
        configFileName.append(".xml");
    }

    QFile file(configFileName);

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qDebug() << "Failed to open file for writing:" << file.errorString();
        return;
    }

    QXmlStreamWriter xmlWriter(&file);
    xmlWriter.setAutoFormatting(true);  // Makes output human-readable

    xmlWriter.writeStartDocument();

    xmlWriter.writeStartElement("Configuration");

    xmlWriter.writeTextElement("clickpackPath", clickpackPath);
    xmlWriter.writeTextElement("microclickThreshold", ui->microClickThresholdTextEdit->document()->toPlainText());
    xmlWriter.writeTextElement("microclickAfterReleaseThreshold", ui->microClickAfterReleaseThresholdTextEdit->document()->toPlainText());
    xmlWriter.writeTextElement("softclickThreshold", ui->softClickThresholdTextEdit->document()->toPlainText());
    xmlWriter.writeTextElement("softclickAfterReleaseThreshold", ui->softClickAfterReleaseThresholdTextEdit->document()->toPlainText());
    xmlWriter.writeTextElement("hardclickThreshold", ui->hardClickThresholdTextEdit->document()->toPlainText());
    xmlWriter.writeTextElement("hardclickAfterReleaseThreshold", ui->hardClickAfterReleaseThresholdTextEdit->document()->toPlainText());
    xmlWriter.writeTextElement("noiseEnabled", (ui->noiseCheckBox->isChecked()) ? "true" : "false");

    xmlWriter.writeEndElement(); // Configuration
    xmlWriter.writeEndDocument();

    file.close();
    QFileInfo fileInfo{configFileName};
    dialogOpenPath = fileInfo.filePath();
}


void MainWindow::on_loadConfigButton_clicked()
{
    QString configFileName = QFileDialog::getOpenFileName(this, tr("Open File"), dialogOpenPath, tr("XML files (*.xml)"));

    QFile file(configFileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "Failed to open file.";
        checkFields();
        return;
    }

    QDomDocument doc;
    if (!doc.setContent(&file)) {
        qDebug() << "Failed to parse XML.";
        checkFields();
        return;
    }
    file.close();

    // Access elements
    QDomElement root = doc.documentElement();

    clickpackPath = root.firstChildElement("clickpackPath").text();
    loadClickpackFromConfig(clickpackPath);
    ui->microClickThresholdTextEdit->setText(root.firstChildElement("microclickThreshold").text());
    ui->microClickAfterReleaseThresholdTextEdit->setText(root.firstChildElement("microclickAfterReleaseThreshold").text());
    ui->softClickThresholdTextEdit->setText(root.firstChildElement("softclickThreshold").text());
    ui->softClickAfterReleaseThresholdTextEdit->setText(root.firstChildElement("softclickAfterReleaseThreshold").text());
    ui->hardClickThresholdTextEdit->setText(root.firstChildElement("hardclickThreshold").text());
    ui->hardClickAfterReleaseThresholdTextEdit->setText(root.firstChildElement("hardclickAfterReleaseThreshold").text());
    ui->noiseCheckBox->setChecked(root.firstChildElement("noiseEnabled").text() == "true");

    QFileInfo fileInfo{configFileName};
    dialogOpenPath = fileInfo.filePath();

    checkFields();
}

void MainWindow::on_renderButton_clicked()
{
    QString previousWorkingPath = QDir::currentPath();

    // Get clickpack folder and set the working directory to it
    QString folderPath = QFileDialog::getExistingDirectory(this, tr("Save to..."), dialogOpenPath);

    QDir::setCurrent(folderPath);

    double microClickTime = (ui->microClickThresholdTextEdit->isEnabled()) ? ui->microClickThresholdTextEdit->document()->toPlainText().toDouble() : 0.0;
    double microClickAfterReleaseTime = (ui->microClickAfterReleaseThresholdTextEdit->isEnabled()) ? ui->microClickAfterReleaseThresholdTextEdit->document()->toPlainText().toDouble() : 0.0;
    double softClickTime = (ui->softClickThresholdTextEdit->isEnabled()) ? ui->softClickThresholdTextEdit->document()->toPlainText().toDouble() : 0.0;
    double softClickAfterReleaseTime = (ui->softClickAfterReleaseThresholdTextEdit->isEnabled()) ? ui->softClickAfterReleaseThresholdTextEdit->document()->toPlainText().toDouble() : 0.0;
    double hardClickTime = (ui->hardClickThresholdTextEdit->isEnabled()) ? ui->hardClickThresholdTextEdit->document()->toPlainText().toDouble() : std::numeric_limits<double>::max();
    double hardClickAfterReleaseTime = (ui->hardClickAfterReleaseThresholdTextEdit->isEnabled()) ? ui->hardClickAfterReleaseThresholdTextEdit->document()->toPlainText().toDouble() : std::numeric_limits<double>::max();

    int generatedFilesCount = 0;
    for (Macro& macro : macroVector) {
        macro.determineClickTypes(microClickTime, microClickAfterReleaseTime, softClickTime, softClickAfterReleaseTime, hardClickTime, hardClickAfterReleaseTime);
        if (generateAudio(macro)) {
            generatedFilesCount++;
        }
    }

    QString generatedText{"Generated "};
    generatedText.append(QString::number(generatedFilesCount));
    generatedText.append(" of ");
    generatedText.append(QString::number(ui->macrosListWidget->count()));
    generatedText.append(" files!");

    QMessageBox::information(this, "Info", generatedText);

    QDir::setCurrent(previousWorkingPath);
    dialogOpenPath = folderPath;
}

void MainWindow::closeEvent(QCloseEvent *event) {
    // Save current settings to a default settings xml
    QString configFileName {"settings.xml"};

    QFile file(configFileName);

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qDebug() << "Failed to open file for writing:" << file.errorString();
        return;
    }

    QXmlStreamWriter xmlWriter(&file);
    xmlWriter.setAutoFormatting(true);  // Makes output human-readable

    xmlWriter.writeStartDocument();

    xmlWriter.writeStartElement("Configuration");

    xmlWriter.writeTextElement("clickpackPath", clickpackPath);
    xmlWriter.writeTextElement("microclickThreshold", ui->microClickThresholdTextEdit->document()->toPlainText());
    xmlWriter.writeTextElement("microclickAfterReleaseThreshold", ui->microClickAfterReleaseThresholdTextEdit->document()->toPlainText());
    xmlWriter.writeTextElement("softclickThreshold", ui->softClickThresholdTextEdit->document()->toPlainText());
    xmlWriter.writeTextElement("softclickAfterReleaseThreshold", ui->softClickAfterReleaseThresholdTextEdit->document()->toPlainText());
    xmlWriter.writeTextElement("hardclickThreshold", ui->hardClickThresholdTextEdit->document()->toPlainText());
    xmlWriter.writeTextElement("hardclickAfterReleaseThreshold", ui->hardClickAfterReleaseThresholdTextEdit->document()->toPlainText());
    xmlWriter.writeTextElement("noiseEnabled", (ui->noiseCheckBox->isChecked()) ? "true" : "false");

    xmlWriter.writeEndElement(); // Configuration
    xmlWriter.writeEndDocument();

    file.close();
}

void MainWindow::loadDefaultSettings() {
    QString configFileName {"settings.xml"};

    QFile file(configFileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "Failed to open file.";
        checkFields();
        return;
    }

    QDomDocument doc;
    if (!doc.setContent(&file)) {
        qDebug() << "Failed to parse XML.";
        checkFields();
        return;
    }
    file.close();

    // Access elements
    QDomElement root = doc.documentElement();

    clickpackPath = root.firstChildElement("clickpackPath").text();
    loadClickpackFromConfig(clickpackPath);
    ui->microClickThresholdTextEdit->setText(root.firstChildElement("microclickThreshold").text());
    ui->microClickAfterReleaseThresholdTextEdit->setText(root.firstChildElement("microclickAfterReleaseThreshold").text());
    ui->softClickThresholdTextEdit->setText(root.firstChildElement("softclickThreshold").text());
    ui->softClickAfterReleaseThresholdTextEdit->setText(root.firstChildElement("softclickAfterReleaseThreshold").text());
    ui->hardClickThresholdTextEdit->setText(root.firstChildElement("hardclickThreshold").text());
    ui->hardClickAfterReleaseThresholdTextEdit->setText(root.firstChildElement("hardclickAfterReleaseThreshold").text());
    ui->noiseCheckBox->setChecked(root.firstChildElement("noiseEnabled").text() == "true");

    checkFields();
}

