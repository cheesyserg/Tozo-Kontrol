#include "icon_data.h"
#include <QApplication>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSlider>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QGroupBox>
#include <QMessageBox>
#include <QInputDialog>
#include <QThread>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QIcon>
#include <QProcess>
#include <QSignalBlocker>
#include <QDir>
#include <QTimer>
#include <QMutex>
#include <QQueue>
#include <QPair>
#include <QVector>
#include <QMap>
#include <QCloseEvent>

#include <sys/socket.h>
#include <unistd.h>
#include <atomic>
#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>

static const QStringList FREQ_LABELS = {
    "20Hz", "50Hz", "100Hz", "200Hz", "400Hz", "800Hz", "1.6k", "3.2k", "6.4k", "12.8k"
};
static const uint8_t FREQ_CONSTS[10] = {
    0x05, 0x06, 0x07, 0x08, 0x0A, 0x0C, 0x0E, 0x10, 0x12, 0x14
};

/* --- Bluetooth Workers --- */

class BtTxWorker : public QThread {
    Q_OBJECT
public:
    BtTxWorker() : running(true) {}
    ~BtTxWorker() { stop(); }

    void sendAsync(int fd, const QByteArray &data, bool clearStale = false) {
        QMutexLocker locker(&mutex);
        if (clearStale) queue.clear();
        queue.enqueue(qMakePair(fd, data));
    }

    void stop() {
        running = false;
        quit();
        wait(500);
    }

protected:
    void run() override {
        while (running) {
            int fd = -1;
            QByteArray payload;
            {
                QMutexLocker locker(&mutex);
                if (!queue.isEmpty()) {
                    auto task = queue.dequeue();
                    fd = task.first;
                    payload = task.second;
                }
            }
            if (fd >= 0 && !payload.isEmpty()) {
                send(fd, payload.constData(), payload.size(), 0);
                msleep(80);
            } else {
                msleep(20);
            }
        }
    }

private:
    QMutex mutex;
    QQueue<QPair<int, QByteArray>> queue;
    std::atomic<bool> running;
};

class BtRxWorker : public QThread {
    Q_OBJECT
public:
    BtRxWorker(int fd) : sockFd(fd), running(true) {}
    ~BtRxWorker() { stop(); }

    void stop() {
        running = false;
        quit();
        wait(500);
    }

signals:
    void packetReceived(const QByteArray &data);
    void connectionLost();

protected:
    void run() override {
        uint8_t buffer[1024];
        while (running && sockFd >= 0) {
            ssize_t bytesRead = recv(sockFd, buffer, sizeof(buffer), 0);
            if (bytesRead > 0) {
                if (running) emit packetReceived(QByteArray(reinterpret_cast<char*>(buffer), bytesRead));
            } else {
                if (running) emit connectionLost();
                break;
            }
        }
    }

private:
    int sockFd;
    std::atomic<bool> running;
};

/* --- Main Application Window --- */

class TozoKdeApp : public QWidget {
    Q_OBJECT

public:
    TozoKdeApp(QWidget *parent = nullptr)
    : QWidget(parent), sockFd(-1), txWorker(nullptr), rxWorker(nullptr),
    isDisconnecting(false), activeCustomPreset("") {
        txWorker = new BtTxWorker();
        txWorker->start();

        initBuiltInPresets();
        initAncModes();
        initUi();
        loadPresetsConfig();
        scanDevices();
    }

    ~TozoKdeApp() {
        savePresetsConfig();
        handleDisconnect();
        if (txWorker) {
            txWorker->stop();
            delete txWorker;
            txWorker = nullptr;
        }
    }

protected:
    void closeEvent(QCloseEvent *event) override {
        savePresetsConfig();
        handleDisconnect();
        event->accept();
    }

private:
    int sockFd;
    BtTxWorker *txWorker;
    BtRxWorker *rxWorker;
    bool isDisconnecting;

    // UI Widgets
    QComboBox *macCombo;
    QLabel *batteryLbl;
    QPushButton *connectBtn;
    QGroupBox *modesGroup;
    QComboBox *ancCombo;
    QCheckBox *rememberChk;
    QCheckBox *spatialChk;
    QCheckBox *latencyChk;

    QGroupBox *eqGroup;
    QComboBox *presetCombo;
    QPushButton *btnAddPreset;
    QPushButton *btnSavePreset;
    QPushButton *btnDelPreset;
    QVector<QSlider*> sliders;
    QVector<QDoubleSpinBox*> spinboxes;

    QLabel *statusLbl;
    QPushButton *btnReset;
    QPushButton *btnCommit;

    // Presets & Modes
    QVector<QPair<QString, QVector<int>>> builtInPresets;
    QMap<QString, QVector<int>> customPresets;
    QString activeCustomPreset;

    struct AncEntry {
        QString name;
        QString cmdLive;
        QString cmdPersist;
    };
    QVector<AncEntry> ancList;

    void initBuiltInPresets() {
        builtInPresets.append({"Custom / Flat",  QVector<int>(10, 0)});
        builtInPresets.append({"Bass Boost",     {30, 25, 15, 5, 0, 0, 0, 5, 10, 15}});
        builtInPresets.append({"Dance",          {25, 20, 10, 0, 5, 15, 20, 20, 15, 10}});
        builtInPresets.append({"Rock / Pop",     {20, 15, 0, -10, -5, 10, 20, 25, 20, 15}});
        builtInPresets.append({"Vocal Clarity",  {-10, -5, 0, 10, 20, 20, 10, 0, -5, -10}});
    }

    void initAncModes() {
        ancList.append({"Normal Mode",       "1004010000", "1012010000"});
        ancList.append({"Noise Cancelling",  "1004010101", "1012010101"});
        ancList.append({"Transparency",      "1005010101", "1012010202"});
        ancList.append({"Reduce Wind Noise", "1007010101", "1012010303"});
        ancList.append({"Leisure Mode",      "1008010101", "1012010404"});
        ancList.append({"Adaptive Mode",     "1011010101", "1012010606"});
    }

    void initUi() {
        setWindowTitle("TOZO HT3 Kontrol Panel");
        setMinimumWidth(680);

        setupAppIcon();

        auto *mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(8, 8, 8, 8);

        // 1. Connection Section
        auto *connGroup = new QGroupBox("Device Connection", this);
        auto *connLayout = new QHBoxLayout(connGroup);
        connLayout->addWidget(new QLabel("MAC:", this));

        macCombo = new QComboBox(this);
        macCombo->setEditable(true);
        macCombo->setMinimumWidth(220);
        connLayout->addWidget(macCombo);

        auto *btnScan = new QPushButton("Scan", this);
        connect(btnScan, &QPushButton::clicked, this, &TozoKdeApp::scanDevices);
        connLayout->addWidget(btnScan);

        batteryLbl = new QLabel("Battery: --%", this);
        batteryLbl->setStyleSheet("font-weight: bold;");
        connLayout->addWidget(batteryLbl);
        connLayout->addStretch(1);

        connectBtn = new QPushButton("Connect", this);
        connect(connectBtn, &QPushButton::clicked, this, &TozoKdeApp::toggleConnection);
        connLayout->addWidget(connectBtn);
        mainLayout->addWidget(connGroup);

        // 2. Hardware Modes Section
        modesGroup = new QGroupBox("Audio & Hardware Modes", this);
        auto *modesLayout = new QHBoxLayout(modesGroup);
        modesLayout->addWidget(new QLabel("ANC:", this));

        ancCombo = new QComboBox(this);
        for (const auto &entry : ancList) {
            ancCombo->addItem(entry.name);
        }
        connect(ancCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &TozoKdeApp::changeAncMode);
        modesLayout->addWidget(ancCombo);

        rememberChk = new QCheckBox("Remember Mode", this);
        rememberChk->setChecked(true);
        modesLayout->addWidget(rememberChk);
        modesLayout->addStretch(1);

        spatialChk = new QCheckBox("Spatial Audio", this);
        connect(spatialChk, &QCheckBox::toggled, this, &TozoKdeApp::toggleSpatialAudio);
        modesLayout->addWidget(spatialChk);

        latencyChk = new QCheckBox("Low Latency", this);
        connect(latencyChk, &QCheckBox::toggled, this, &TozoKdeApp::toggleLowLatency);
        modesLayout->addWidget(latencyChk);
        mainLayout->addWidget(modesGroup);

        // 3. 10-Band Equalizer Section
        eqGroup = new QGroupBox("10-Band Equalizer", this);
        auto *eqLayout = new QVBoxLayout(eqGroup);

        auto *presetLayout = new QHBoxLayout();
        presetLayout->addWidget(new QLabel("Preset:", this));

        presetCombo = new QComboBox(this);
        connect(presetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &TozoKdeApp::onPresetIndexChanged);
        presetLayout->addWidget(presetCombo, 1);

        btnAddPreset = new QPushButton("Add", this);
        connect(btnAddPreset, &QPushButton::clicked, this, &TozoKdeApp::addCustomPreset);
        presetLayout->addWidget(btnAddPreset);

        btnSavePreset = new QPushButton("Save", this);
        connect(btnSavePreset, &QPushButton::clicked, this, &TozoKdeApp::saveCurrentPreset);
        presetLayout->addWidget(btnSavePreset);

        btnDelPreset = new QPushButton("Delete", this);
        connect(btnDelPreset, &QPushButton::clicked, this, &TozoKdeApp::deleteCustomPreset);
        presetLayout->addWidget(btnDelPreset);

        eqLayout->addLayout(presetLayout);

        auto *slidersLayout = new QHBoxLayout();
        for (int i = 0; i < 10; ++i) {
            auto *col = new QVBoxLayout();
            auto *lbl = new QLabel(FREQ_LABELS[i], this);
            lbl->setAlignment(Qt::AlignCenter);
            col->addWidget(lbl);

            auto *slider = new QSlider(Qt::Vertical, this);
            slider->setRange(-50, 50);
            slider->setValue(0);
            slider->setMinimumHeight(140);
            col->addWidget(slider, 0, Qt::AlignCenter);

            auto *spin = new QDoubleSpinBox(this);
            spin->setRange(-5.0, 5.0);
            spin->setSingleStep(0.1);
            spin->setValue(0.0);
            spin->setSuffix(" dB");
            col->addWidget(spin);

            connect(slider, &QSlider::valueChanged, this, [this, spin, i](int val) {
                const QSignalBlocker blocker(spin);
                spin->setValue(val / 10.0);
                onSliderMoved();
            });

            connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this, slider, i](double val) {
                const QSignalBlocker blocker(slider);
                slider->setValue(static_cast<int>(val * 10.0));
                onSliderMoved();
            });

            sliders.append(slider);
            spinboxes.append(spin);
            slidersLayout->addLayout(col);
        }
        eqLayout->addLayout(slidersLayout);
        mainLayout->addWidget(eqGroup);

        // 4. Footer Section
        auto *footerLayout = new QHBoxLayout();
        statusLbl = new QLabel("Status: Disconnected.", this);
        footerLayout->addWidget(statusLbl);
        footerLayout->addStretch(1);

        auto *btnSync = new QPushButton("Sync States", this);
        connect(btnSync, &QPushButton::clicked, this, &TozoKdeApp::requestMasterState);
        footerLayout->addWidget(btnSync);

        btnReset = new QPushButton("Reset Flat", this);
        connect(btnReset, &QPushButton::clicked, this, &TozoKdeApp::resetFlat);
        footerLayout->addWidget(btnReset);

        btnCommit = new QPushButton("Commit to Device", this);
        connect(btnCommit, &QPushButton::clicked, this, &TozoKdeApp::commitToDevice);
        footerLayout->addWidget(btnCommit);
        mainLayout->addLayout(footerLayout);

        setControlsEnabled(false);
    }

    void setupAppIcon() {
        QPixmap pixmap;
        if (pixmap.loadFromData(icon_jpg, icon_jpg_len)) {
            QIcon icon(pixmap);
            setWindowIcon(icon);
            QApplication::setWindowIcon(icon);
        } else {
            QIcon fallback = QIcon::fromTheme("audio-headphones");
            setWindowIcon(fallback);
            QApplication::setWindowIcon(fallback);
        }
    }

    void setControlsEnabled(bool enabled) {
        modesGroup->setEnabled(enabled);
        bool eqActive = enabled && !spatialChk->isChecked();
        eqGroup->setEnabled(eqActive);
        btnReset->setEnabled(eqActive);
        btnCommit->setEnabled(enabled);
    }

    QByteArray generatePayload() {
        QByteArray payload;
        payload.append(static_cast<char>(0x10));
        payload.append(static_cast<char>(0x0B));
        payload.append(static_cast<char>(0x15));

        uint32_t sum = 0;
        for (auto *s : sliders) {
            int v = s->value();
            uint8_t b = static_cast<uint8_t>(v < 0 ? 256 + v : v);
            payload.append(static_cast<char>(b));
            sum += b;
        }
        for (int i = 0; i < 10; ++i) {
            payload.append(static_cast<char>(FREQ_CONSTS[i]));
            sum += FREQ_CONSTS[i];
        }
        payload.append(static_cast<char>(0x01));
        sum += 0x01;
        payload.append(static_cast<char>(sum & 0xFF));
        return payload;
    }

    void pushEqRam() {
        if (sockFd >= 0) txWorker->sendAsync(sockFd, generatePayload(), true);
    }

    QVector<int> currentEqValues() const {
        QVector<int> v;
        for (auto *s : sliders) v.append(s->value());
        return v;
    }

    void setEqValues(const QVector<int> &vals, bool syncHw = false) {
        for (int i = 0; i < 10 && i < vals.size(); ++i) {
            const QSignalBlocker b1(sliders[i]);
            const QSignalBlocker b2(spinboxes[i]);
            sliders[i]->setValue(vals[i]);
            spinboxes[i]->setValue(vals[i] / 10.0);
        }
        if (syncHw) pushEqRam();
    }

    void onSliderMoved() {
        pushEqRam();
        checkExactMatchPreset();
    }

    void checkExactMatchPreset() {
        QVector<int> cur = currentEqValues();
        const QSignalBlocker blocker(presetCombo);

        for (int i = 0; i < builtInPresets.size(); ++i) {
            if (builtInPresets[i].second == cur) {
                int idx = presetCombo->findData("builtin:" + QString::number(i));
                presetCombo->setCurrentIndex(idx);
                activeCustomPreset.clear();
                updatePresetButtons(false);
                return;
            }
        }

        for (auto it = customPresets.begin(); it != customPresets.end(); ++it) {
            if (it.value() == cur) {
                int idx = presetCombo->findData("custom:" + it.key());
                presetCombo->setCurrentIndex(idx);
                activeCustomPreset = it.key();
                updatePresetButtons(true);
                return;
            }
        }

        if (!activeCustomPreset.isEmpty()) {
            updatePresetButtons(true);
        } else {
            updatePresetButtons(false);
        }
    }

    void refreshPresetDropdown(const QString &selectKey = QString()) {
        const QSignalBlocker blocker(presetCombo);
        presetCombo->clear();

        for (int i = 0; i < builtInPresets.size(); ++i) {
            presetCombo->addItem(builtInPresets[i].first, "builtin:" + QString::number(i));
        }

        if (!customPresets.isEmpty()) {
            presetCombo->insertSeparator(presetCombo->count());
            for (auto it = customPresets.begin(); it != customPresets.end(); ++it) {
                presetCombo->addItem(it.key() + " (Custom)", "custom:" + it.key());
            }
        }

        if (!selectKey.isEmpty()) {
            int idx = presetCombo->findData(selectKey);
            if (idx >= 0) presetCombo->setCurrentIndex(idx);
        } else {
            presetCombo->setCurrentIndex(0);
        }

        onPresetIndexChanged(presetCombo->currentIndex());
    }

    void updatePresetButtons(bool isCustom) {
        btnAddPreset->setEnabled(true);
        btnSavePreset->setEnabled(isCustom);
        btnDelPreset->setEnabled(isCustom);
    }

    void loadPresetsConfig() {
        QFile file(QDir::homePath() + "/.config/tozo_config.json");
        if (file.open(QIODevice::ReadOnly)) {
            QJsonObject obj = QJsonDocument::fromJson(file.readAll()).object();
            QJsonObject custom = obj["custom_presets"].toObject();
            for (const QString &k : custom.keys()) {
                QJsonArray arr = custom[k].toArray();
                QVector<int> vals;
                for (auto v : arr) vals.append(v.toInt());
                if (vals.size() == 10) customPresets[k] = vals;
            }
        }
        refreshPresetDropdown();
    }

    void savePresetsConfig() {
        QJsonObject root;
        QJsonObject custom;
        for (auto it = customPresets.begin(); it != customPresets.end(); ++it) {
            QJsonArray arr;
            for (int v : it.value()) arr.append(v);
            custom[it.key()] = arr;
        }
        root["custom_presets"] = custom;
        QFile file(QDir::homePath() + "/.config/tozo_config.json");
        if (file.open(QIODevice::WriteOnly)) {
            file.write(QJsonDocument(root).toJson());
        }
    }

private slots:
    void onPresetIndexChanged(int index) {
        if (index < 0) return;
        QString data = presetCombo->itemData(index).toString();
        if (data.startsWith("builtin:")) {
            int bIdx = data.section(':', 1).toInt();
            if (bIdx >= 0 && bIdx < builtInPresets.size()) {
                activeCustomPreset.clear();
                setEqValues(builtInPresets[bIdx].second, true);
                updatePresetButtons(false);
            }
        } else if (data.startsWith("custom:")) {
            QString cName = data.section(':', 1);
            if (customPresets.contains(cName)) {
                activeCustomPreset = cName;
                setEqValues(customPresets[cName], true);
                updatePresetButtons(true);
            }
        }
    }

    void addCustomPreset() {
        bool ok;
        QString name = QInputDialog::getText(this, "Add Custom Preset", "Enter Preset Name:", QLineEdit::Normal, "", &ok);
        if (ok && !name.trimmed().isEmpty()) {
            QString clean = name.trimmed();
            customPresets[clean] = currentEqValues();
            savePresetsConfig();
            activeCustomPreset = clean;
            refreshPresetDropdown("custom:" + clean);
            statusLbl->setText(QString("Status: Preset '%1' added.").arg(clean));
        }
    }

    void saveCurrentPreset() {
        if (!activeCustomPreset.isEmpty() && customPresets.contains(activeCustomPreset)) {
            customPresets[activeCustomPreset] = currentEqValues();
            savePresetsConfig();
            statusLbl->setText(QString("Status: Preset '%1' saved.").arg(activeCustomPreset));
        }
    }

    void deleteCustomPreset() {
        if (!activeCustomPreset.isEmpty() && customPresets.contains(activeCustomPreset)) {
            if (QMessageBox::question(this, "Confirm Delete", QString("Delete preset '%1'?").arg(activeCustomPreset)) == QMessageBox::Yes) {
                customPresets.remove(activeCustomPreset);
                activeCustomPreset.clear();
                savePresetsConfig();
                refreshPresetDropdown("builtin:0");
                setEqValues(builtInPresets[0].second, true);
                statusLbl->setText("Status: Preset deleted.");
            }
        }
    }

    void scanDevices() {
        statusLbl->setText("Status: Scanning...");
        macCombo->clear();

        QProcess proc;
        proc.start("bluetoothctl", {"devices", "Paired"});
        proc.waitForFinished(3000);
        QString out = proc.readAllStandardOutput();

        QStringList lines = out.split('\n',
                                      #if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
                                      Qt::SkipEmptyParts
                                      #else
                                      QString::SkipEmptyParts
                                      #endif
        );

        int count = 0;
        for (const QString &line : lines) {
            if (line.contains("TOZO") || line.contains("Device")) {
                QStringList parts = line.split(' ');
                if (parts.size() >= 3) {
                    QString mac = parts[1];
                    QString name = line.section(' ', 2);
                    macCombo->addItem(QString("%1 (%2)").arg(name, mac));
                    count++;
                }
            }
        }
        statusLbl->setText(QString("Status: Found %1 device(s).").arg(count));
    }

    void toggleConnection() {
        if (sockFd >= 0) {
            handleDisconnect();
            return;
        }

        QString text = macCombo->currentText();
        QString mac = text.contains('(') ? text.section('(', 1).remove(')') : text;
        if (mac.isEmpty()) return;

        struct sockaddr_rc addr = { 0 };
        addr.rc_family = AF_BLUETOOTH;
        addr.rc_channel = 1;
        str2ba(mac.toLatin1().constData(), &addr.rc_bdaddr);

        sockFd = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
        if (sockFd < 0 || ::connect(sockFd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            if (sockFd >= 0) { ::close(sockFd); sockFd = -1; }
            QMessageBox::critical(this, "Connection Failed", "Could not connect to the RFCOMM socket.");
            return;
        }

        rxWorker = new BtRxWorker(sockFd);
        connect(rxWorker, &BtRxWorker::packetReceived, this, &TozoKdeApp::handleIncomingPacket);
        connect(rxWorker, &BtRxWorker::connectionLost, this, &TozoKdeApp::handleDisconnect);
        rxWorker->start();

        connectBtn->setText("Disconnect");
        macCombo->setEnabled(false);
        setControlsEnabled(true);
        requestMasterState();
    }

    void handleDisconnect() {
        if (isDisconnecting) return;
        isDisconnecting = true;

        // 1. Force unblock blocking recv() syscall
        if (sockFd >= 0) {
            ::shutdown(sockFd, SHUT_RDWR);
            ::close(sockFd);
            sockFd = -1;
        }

        // 2. Stop and safely schedule deletion of worker thread
        if (rxWorker) {
            rxWorker->disconnect();
            rxWorker->stop();
            rxWorker->deleteLater();
            rxWorker = nullptr;
        }

        connectBtn->setText("Connect");
        macCombo->setEnabled(true);
        setControlsEnabled(false);
        batteryLbl->setText("Battery: --%");
        statusLbl->setText("Status: Disconnected.");

        isDisconnecting = false;
    }

    void handleIncomingPacket(const QByteArray &data) {
        QString hex = data.toHex();
        if (hex.startsWith("10")) return;

        // ANC Mode Decoding
        {
            const QSignalBlocker b(ancCombo);
            int idx = hex.indexOf("003001");
            if (idx != -1 && hex.length() >= idx + 8) {
                QString modeCode = hex.mid(idx + 6, 2);
                if (modeCode == "00") ancCombo->setCurrentText("Normal Mode");
                else if (modeCode == "01") ancCombo->setCurrentText("Noise Cancelling");
                else if (modeCode == "02") ancCombo->setCurrentText("Transparency");
                else if (modeCode == "03") ancCombo->setCurrentText("Reduce Wind Noise");
                else if (modeCode == "04") ancCombo->setCurrentText("Leisure Mode");
                else if (modeCode == "06") ancCombo->setCurrentText("Adaptive Mode");
            } else if (hex.contains("110101") || hex.contains("040101")) {
                ancCombo->setCurrentText("Noise Cancelling");
            } else if (hex.contains("100101") || hex.contains("050101")) {
                ancCombo->setCurrentText("Transparency");
            } else if (hex.contains("110100") || hex.contains("040100")) {
                ancCombo->setCurrentText("Normal Mode");
            }
        }

        // Battery
        int bIdx = hex.indexOf("000201");
        if (bIdx != -1 && hex.length() >= bIdx + 8) {
            bool ok;
            int bat = hex.mid(bIdx + 6, 2).toInt(&ok, 16);
            if (ok) batteryLbl->setText(QString("Battery: %1%").arg(bat));
        }

        // Low Latency
        int lIdx = hex.indexOf("000601");
        if (lIdx != -1 && hex.length() >= lIdx + 8) {
            const QSignalBlocker b(latencyChk);
            latencyChk->setChecked(hex.mid(lIdx + 6, 2) == "01");
        }

        // Spatial Audio
        int sIdx = hex.indexOf("001301");
        if (sIdx != -1 && hex.length() >= sIdx + 8) {
            const QSignalBlocker b(spatialChk);
            bool spatial = (hex.mid(sIdx + 6, 2) == "01");
            spatialChk->setChecked(spatial);
            eqGroup->setEnabled(!spatial && sockFd >= 0);
            btnReset->setEnabled(!spatial && sockFd >= 0);
        }

        // EQ Telemetry
        int eqIdx = hex.indexOf("000b14");
        if (eqIdx != -1 && hex.length() >= eqIdx + 26) {
            QVector<int> parsed;
            for (int i = 0; i < 10; ++i) {
                bool ok;
                int byteVal = hex.mid(eqIdx + 6 + i * 2, 2).toInt(&ok, 16);
                int gain = (byteVal > 127) ? byteVal - 256 : byteVal;
                parsed.append(qBound(-50, gain, 50));
            }
            setEqValues(parsed, false);
            checkExactMatchPreset();
        }
        statusLbl->setText("Status: State synchronized.");
    }

    void requestMasterState() {
        if (sockFd < 0) return;
        const QStringList queries = {"00020000", "000b0000", "00060000", "00120000", "00300000", "00130000"};
        for (const QString &q : queries) {
            txWorker->sendAsync(sockFd, QByteArray::fromHex(q.toLatin1()));
        }
        statusLbl->setText("Status: Querying telemetry...");
    }

    void changeAncMode() {
        if (sockFd < 0) return;
        int idx = ancCombo->currentIndex();
        if (idx >= 0 && idx < ancList.size()) {
            txWorker->sendAsync(sockFd, QByteArray::fromHex(ancList[idx].cmdLive.toLatin1()));
            if (rememberChk->isChecked()) {
                txWorker->sendAsync(sockFd, QByteArray::fromHex(ancList[idx].cmdPersist.toLatin1()));
            }
        }
    }

    void toggleSpatialAudio(bool chk) {
        bool eqActive = !chk && sockFd >= 0;
        eqGroup->setEnabled(eqActive);
        btnReset->setEnabled(eqActive);
        if (sockFd >= 0) {
            txWorker->sendAsync(sockFd, QByteArray::fromHex(chk ? "1013010101" : "1013010000"));
            if (!chk) QTimer::singleShot(200, this, [this]() {
                txWorker->sendAsync(sockFd, QByteArray::fromHex("000b0000"));
            });
        }
    }

    void toggleLowLatency(bool chk) {
        if (sockFd >= 0) {
            txWorker->sendAsync(sockFd, QByteArray::fromHex(chk ? "1006010101" : "1006010000"));
        }
    }

    void resetFlat() {
        activeCustomPreset.clear();
        setEqValues(builtInPresets[0].second, true);
        checkExactMatchPreset();
    }

    void commitToDevice() {
        pushEqRam();
        changeAncMode();
    }
};

int main(int argc, char *argv[]) {
    QApplication::setDesktopFileName("tozo-kontrol");
    QApplication app(argc, argv);

    TozoKdeApp window;
    window.show();
    return app.exec();
}

#include "main.moc"
