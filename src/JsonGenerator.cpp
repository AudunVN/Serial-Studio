/*
 * Copyright (c) 2020-2021 Alex Spataru <https://github.com/alex-spataru>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "Logger.h"
#include "CsvPlayer.h"
#include "JsonGenerator.h"
#include "SerialManager.h"
#include "ConsoleAppender.h"

#include <QFileInfo>
#include <QFileDialog>
#include <QMessageBox>

/*
 * Only instance of the class
 */
static JsonGenerator *INSTANCE = nullptr;

/*
 * Regular expresion used to check if there are still unmatched values
 * on the JSON map file.
 */
static const QRegExp UNMATCHED_VALUES_REGEX("(%\b([0-9]|[1-9][0-9])\b)");

/**
 * Shows a macOS-like message box with the given properties
 */
static int NiceMessageBox(QString text, QString informativeText,
                          QString windowTitle = qAppName(),
                          QMessageBox::StandardButtons bt = QMessageBox::Ok)
{
    // clang-format off
    auto icon = QPixmap(":/images/icon.png").scaled(64, 64,
                                                    Qt::IgnoreAspectRatio,
                                                    Qt::SmoothTransformation);
    // clang-format on

    // Create message box & set options
    QMessageBox box;
    box.setIconPixmap(icon);
    box.setStandardButtons(bt);
    box.setWindowTitle(windowTitle);
    box.setText("<h3>" + text + "</h3>");
    box.setInformativeText(informativeText);

    // Show message box & return user decision to caller
    return box.exec();
}

/**
 * Initializes the JSON Parser class and connects appropiate SIGNALS/SLOTS
 */
JsonGenerator::JsonGenerator()
{
    m_opMode = kAutomatic;
    m_dataFormatErrors = 0;
    auto sm = SerialManager::getInstance();
    connect(sm, SIGNAL(portChanged()), this, SLOT(reset()));
    connect(sm, SIGNAL(packetReceived(QByteArray)), this, SLOT(readData(QByteArray)));

    LOG_INFO() << "Initialized JsonParser module";
}

/**
 * Returns the only instance of the class
 */
JsonGenerator *JsonGenerator::getInstance()
{
    if (!INSTANCE)
        INSTANCE = new JsonGenerator();

    return INSTANCE;
}

/**
 * Returns the JSON map data from the loaded file as a string
 */
QString JsonGenerator::jsonMapData() const
{
    return m_jsonMapData;
}

/**
 * Returns the parsed JSON document from the received packet
 */
QJsonDocument JsonGenerator::document() const
{
    return m_document;
}

/**
 * Returns the file name (e.g. "JsonMap.json") of the loaded JSON map file
 */
QString JsonGenerator::jsonMapFilename() const
{
    if (m_jsonMap.isOpen())
    {
        auto fileInfo = QFileInfo(m_jsonMap.fileName());
        return fileInfo.fileName();
    }

    return "";
}

/**
 * Returns the file path of the loaded JSON map file
 */
QString JsonGenerator::jsonMapFilepath() const
{
    if (m_jsonMap.isOpen())
    {
        auto fileInfo = QFileInfo(m_jsonMap.fileName());
        return fileInfo.filePath();
    }

    return "";
}

/**
 * Returns the operation mode
 */
JsonGenerator::OperationMode JsonGenerator::operationMode() const
{
    return m_opMode;
}

/**
 * Creates a file dialog & lets the user select the JSON file map
 */
void JsonGenerator::loadJsonMap()
{
    auto file
        = QFileDialog::getOpenFileName(Q_NULLPTR, tr("Select JSON map file"),
                                       QDir::homePath(), tr("JSON files") + " (*.json)");

    if (!file.isEmpty())
        loadJsonMap(file);
}

/**
 * Opens, validates & loads into memory the JSON file in the given @a path.
 */
void JsonGenerator::loadJsonMap(const QString &path, const bool silent)
{
    // Log information
    LOG_INFO() << "Loading JSON file, silent flag set to" << silent;

    // Validate path
    if (path.isEmpty())
        return;

    // Close previous file (if open)
    if (m_jsonMap.isOpen())
    {
        m_jsonMap.close();
        emit jsonFileMapChanged();
    }

    // Try to open the file (read only mode)
    m_jsonMap.setFileName(path);
    if (m_jsonMap.open(QFile::ReadOnly))
    {
        // Read data & validate JSON from file
        QJsonParseError error;
        auto data = m_jsonMap.readAll();
        auto document = QJsonDocument::fromJson(data, &error);
        if (error.error != QJsonParseError::NoError)
        {
            LOG_INFO() << "JSON parse error" << error.errorString();

            m_jsonMap.close();
            writeSettings("");
            NiceMessageBox(tr("JSON parse error"), error.errorString());
        }

        // JSON contains no errors, load data & save settings
        else
        {
            LOG_INFO() << "JSON map loaded successfully";

            writeSettings(path);
            m_jsonMapData = QString::fromUtf8(data);
            if (!silent)
                NiceMessageBox(
                    tr("JSON map file loaded successfully!"),
                    tr("File \"%1\" loaded into memory").arg(jsonMapFilename()));
        }
    }

    // Open error
    else
    {
        LOG_INFO() << "JSON file error" << m_jsonMap.errorString();

        writeSettings("");
        NiceMessageBox(tr("Cannot read JSON file"),
                       tr("Please check file permissions & location"));
        m_jsonMap.close();
    }

    // Update UI
    emit jsonFileMapChanged();
}

/**
 * Changes the operation mode of the JSON parser. There are two possible op.
 * modes:
 *
 * @c kManual serial data only contains the comma-separated values, and we need
 *            to use a JSON map file (given by the user) to know what each value
 *            means. This method is recommended when we need to transfer &
 *            display a large amount of information from the microcontroller
 *            unit to the computer.
 *
 * @c kAutomatic serial data contains the JSON data frame, good for simple
 *               applications or for prototyping.
 */
void JsonGenerator::setOperationMode(const OperationMode mode)
{
    m_opMode = mode;
    emit operationModeChanged();

    LOG_INFO() << "Operation mode set to" << mode;
}

/**
 * Loads the last saved JSON map file (if any)
 */
void JsonGenerator::readSettings()
{
    auto path = m_settings.value("json_map_location", "").toString();
    if (!path.isEmpty())
        loadJsonMap(path, true);
}

/**
 * Saves the location of the last valid JSON map file that was opened (if any)
 */
void JsonGenerator::writeSettings(const QString &path)
{
    m_settings.setValue("json_map_location", path);
}

/**
 * Changes the JSON document to be used to generate the user interface.
 * This function is set to public in order to allow the CSV-replay feature to
 * work by replacing the data/json input source.
 */
void JsonGenerator::setJsonDocument(const QJsonDocument &document)
{
    m_document = document;
    if (!m_document.object().isEmpty())
        emit packetReceived();
}

/**
 * Resets all the statistics related to the current serial port device and
 * the JSON map file
 */
void JsonGenerator::reset()
{
    m_dataFormatErrors = 0;
    setJsonDocument(QJsonDocument::fromJson(QByteArray("{}")));
}

/**
 * Tries to parse the given data as a JSON document according to the selected
 * operation mode.
 *
 * Possible operation modes:
 * - Auto:   serial data contains the JSON data frame
 * - Manual: serial data only contains the comma-separated values, and we need
 *           to use a JSON map file (given by the user) to know what each value
 *           means
 *
 * If JSON parsing is successfull, then the class shall notify the rest of the
 * application in order to process packet data.
 */
void JsonGenerator::readData(const QByteArray &data)
{
    // CSV-replay active, abort
    if (CsvPlayer::getInstance()->isOpen())
        return;

    // Data empty, abort
    if (data.isEmpty())
        return;

    // Init variables
    QJsonParseError error;
    QJsonDocument document;

    // Serial device sends JSON (auto mode)
    if (operationMode() == kAutomatic)
        document = QJsonDocument::fromJson(data, &error);

    // We need to use a map file, check if its loaded & replace values into map
    else
    {
        // Empty JSON map data
        if (jsonMapData().isEmpty())
            return;

        // Init conversion status boolean
        bool ok = true;

        // Separate incoming data & add it to the JSON map
        auto json = jsonMapData();
        auto list = QString::fromUtf8(data).split(',');
        for (int i = 0; i < list.count(); ++i)
        {
            // Get value at i & insert it into json
            auto str = list.at(i);
            auto mod = json.arg(str);

            // If JSON after insertion is different we're good to go
            if (json != mod)
                json = mod;

            // JSON is the same after insertion -> format error
            else
            {
                ok = false;
                break;
            }
        }

        // Test that JSON does not contain unmatched values
        if (ok)
            ok = !(json.contains(UNMATCHED_VALUES_REGEX));

        // There was an error & the JSON map is incomplete (or misses received
        // info from the microcontroller).
        if (!ok)
        {
            // Increment error counter
            ++m_dataFormatErrors;

            // Avoid nagging the user too much (only display once, and only
            // after two continous errors have been detected)
            if (m_dataFormatErrors == 2)
            {
                NiceMessageBox(tr("JSON/serial data format mismatch"),
                               tr("The format of the received data does not "
                                  "correspond to the selected JSON map file."));
            }

            // Stop executing function
            return;
        }

        // Create json document
        document = QJsonDocument::fromJson(json.toUtf8(), &error);
    }

    // No parse error, update UI & reset error counter
    if (error.error == QJsonParseError::NoError)
    {
        setJsonDocument(document);
        m_dataFormatErrors = 0;
    }
}
