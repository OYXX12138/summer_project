#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "mytask.h"
#include <QFile>
#include <QTextStream>
#include <QMessageBox>
#include <QStringList>
#include <QFileDialog>
#include <QDebug>
#include <QStyle>
#include <QSerialPortInfo>
#include <QResizeEvent>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),ui(new Ui::MainWindow), taskThread(nullptr), task(nullptr) {
    setWindowTitle("Trajectory Viewer");
    ui->setupUi(this);

    // 创建 Camera 对象
    //Camera *camera = new Camera(this);

    // 启动相机并将视图显示在 cameraWidget 中
    //camera->startCameraInWidget(ui->cameraWidget);

    //connect(btnCamera, &QPushButton::clicked, this, &MainWindow::on_btnCamera_clicked);

    initMapView(); // 调用初始化函数

    client = new QMqttClient;

    client->setHostname("broker.emqx.io");
    client->setPort(1883);
    client->setUsername("test");
    client->setPassword("test");
    client->setCleanSession(false);

    // 连接信号槽
    connect(client, &QMqttClient::connected, this, &MainWindow::onClientConnected);
    connect(client, &QMqttClient::messageReceived, this, &MainWindow::recvMessageSlot);
    connect(client, &QMqttClient::disconnected, [this]() {
        QMessageBox::warning(this, "连接提示", "服务器断开");
    });

    // 设置主题，连接后自动订阅
    QString topic = "testtopic/4G";
    client->connectToHost();

    //connect(client,&QMqttClient::connected,this,&MainWindow::connectSuccessSlot);

    // 创建一个场景
    QGraphicsScene *scene = new QGraphicsScene(this);
    //scene = new QGraphicsScene(this);
    scene->setSceneRect(0, 0, ui->mapView->width(), ui->mapView->height());
    ui->mapView->setScene(scene);  // 将场景设置为 mapView 的场景
    ui->mapView->setRenderHint(QPainter::Antialiasing);  // 开启抗锯齿渲染

    ui->mapView->resetTransform(); // 重置变换矩阵
    ui->mapView->fitInView(scene->sceneRect(), Qt::KeepAspectRatio); // 调整视图范围


    // 设置拖动和缩放
    ui->mapView->setDragMode(QGraphicsView::ScrollHandDrag);  // 设置为拖动模式
    ui->mapView->setRenderHint(QPainter::Antialiasing, true);

    // 设置窗口的显示名称
    this->setWindowTitle(tr(" Real time monitoring system for soil quality ") + " Alpha V5.3 ");

    // 初始化 UI 列表
    ui->dataTable->setColumnCount(4);  // 显示 4 列数据
    ui->dataTable->setHorizontalHeaderLabels(QStringList() <<"N"<< "时间" << "东经" << "北纬");

    // 创建串口对象
    m_pSerial = new QSerialPort(this);
    //m_pPosition = new QSerialPort(this);

    // 初始化宽度和高度
    //mapWidth = 371;  // 设定一个合适的宽度
    //mapHeight = 300; // 设定一个合适的高度

    // 初始化地图
    mapImage = QImage(mapWidth, mapHeight, QImage::Format_RGB32);
    mapImage.fill(Qt::white);
   // ui->mapLabel->setPixmap(QPixmap::fromImage(mapImage));

    // 设置默认显示物质
    currentSubstance = "N";

    // 添加定时器
    dataUpdateTimer = new QTimer(this);
    connect(dataUpdateTimer, &QTimer::timeout, this, &MainWindow::updateData);

    fileCheckTimer = new QTimer(this);
    connect(fileCheckTimer, &QTimer::timeout, this, &MainWindow::checkForFileUpdates); // 定期检查文件更新

    // 添加更新日期和时间的定时器
    dateTimeUpdateTimer = new QTimer(this);
    connect(dateTimeUpdateTimer, &QTimer::timeout, this, &MainWindow::updateDateTime);
    dateTimeUpdateTimer->start(1000); // 每秒更新一次

    // 连接按钮的点击信号到槽函数
    connect(ui->dataTable, &QTableWidget::cellClicked, this, &MainWindow::onRowClicked);

    bool isBtnStartConnected = false;
    if (!isBtnStartConnected) {
        connect(ui->btnStart, &QPushButton::clicked, this, &MainWindow::on_btnStart_clicked);
        isBtnStartConnected = true;
    }

    connect(ui->btnPause, &QPushButton::clicked, this, &MainWindow::on_btnPause_clicked);
    connect(ui->btnDelete, &QPushButton::clicked, this, &MainWindow::on_btnDelete_clicked);
    connect(ui->btnN, &QPushButton::clicked, this, &MainWindow::showN);
    connect(ui->btnP, &QPushButton::clicked, this, &MainWindow::showP);
    connect(ui->btnK, &QPushButton::clicked, this, &MainWindow::showK);

    //connect(ui->btnStart, &QPushButton::clicked, this, &MainWindow::startDrawing);
    //connect(ui->btnSave, &QPushButton::clicked, this, &MainWindow::sendToserial);

    //openPosition();
    openSerial();
    readSerialData();
    // 清空数据表格
    ui->dataTable->setRowCount(0); // 清空所有行
   // ui->mapLabel->setScaledContents(true); // 允许 QLabel 的内容缩放
    QPixmap pixmap(":/res/pic/location.png");
    ui->location->setPixmap(pixmap);
    ui->btnK->setEnabled(false);
    ui->btnN->setEnabled(false);
    ui->btnP->setEnabled(false);
}


MainWindow::~MainWindow() {
    if (taskThread) {
        taskThread->quit();
        taskThread->wait();
        delete taskThread;
    }
    delete ui;
}

void MainWindow::onClientConnected() {
    qDebug() << "MQTT client connected successfully.";

    // 设置自动订阅主题
    QString topic = "testtopic/4G";  // 替换为你的实际主题
    auto subscription = client->subscribe(topic);
    if (!subscription) {
        qDebug() << "Failed to subscribe to topic:" << topic;
    } else {
        qDebug() << "Subscribed to topic:" << topic;
    }
}

void MainWindow::recvMessageSlot(const QByteArray &ba, const QMqttTopicName &topic)
{
    qDebug() << "recived message:" << QString(ba);
    parseAndSaveData(QString(ba));
//    ui->textEdit->append(QString(ba));
//    ui->textEdit->moveCursor(QTextCursor::End);
}

//void MainWindow::on_subButton_clicked()
//{
//    ui->subTopicEdit->setText("1122113388");
//    client->subscribe(ui->subTopicEdit->text());
//}

/*void MainWindow::openPosition(){
    // 遍历所有可用串口，查找设备名包含 "Silicon Labs CP210x USB to UART Bridge" 的串口
    bool position = false;
    foreach (const QSerialPortInfo &info, QSerialPortInfo::availablePorts()) {
        qDebug() << "Port Name: " << info.portName();         // 例如 COM9
        qDebug() << "Description: " << info.description();   // 例如 USB-SERIAL CH340
        qDebug() << "Manufacturer: " << info.manufacturer(); // 例如 QinHeng Electronics

        // 如果设备名匹配 "Silicon Labs CP210x USB to UART Bridge"
        if (info.description().contains("Silicon Labs CP210x USB to UART Bridge")) {
            m_pPosition->setPort(info);
            position = true;
            break;
        }
    }

    if (position) {
        // 设置波特率、数据位、校验位、停止位等
            m_pPosition->setBaudRate(QSerialPort::Baud9600);
            m_pPosition->setDataBits(QSerialPort::Data8);
            m_pPosition->setParity(QSerialPort::NoParity);
            m_pPosition->setStopBits(QSerialPort::OneStop);
            m_pPosition->setFlowControl(QSerialPort::NoFlowControl);

            // 打开串口并连接 readyRead 信号
            if (m_pPosition->open(QIODevice::ReadWrite)) {
                connect(m_pPosition, &QSerialPort::readyRead, this, &MainWindow::readPositionData);
                qDebug() << "位置串口已打开 ";
            } else {
                qDebug() << "无法打开串口";
            }
      } else {
            qDebug() << "未找到 BDS 设备";
      }
}

void MainWindow::readPositionData(){
    if (!m_pPosition->isOpen()) {
        qDebug() << "位置未开，无法读数据";
        return;
    }
    QByteArray positionData = m_pPosition->readAll();
    if (positionData.isEmpty()) {
        qDebug() << "接收到空数据，继续等...";
        return;
    }
    // 将接收到的数据追加到 accumulatedData 中
    accumulatedData.append(QString::fromLatin1(positionData));

    // 查找是否包含完整的 NMEA 数据
    int startIdx = accumulatedData.indexOf('$'); // 查找开始符 '$'
    int endIdx = accumulatedData.indexOf('\n', startIdx); // 查找结束符 '\n'

    // 如果找到了完整的数据
    while (startIdx != -1 && endIdx != -1) {
        QString nmeaData = accumulatedData.mid(startIdx, endIdx - startIdx + 1);
        qDebug() << "接收到的位置数据:" << nmeaData;

        // 解析 GNRMC 语句
        if (nmeaData.startsWith("$GNRMC")) {
            parseRMC(nmeaData);
        }

        // 更新 accumulatedData，去掉已处理的数据
        accumulatedData.remove(0, endIdx + 1);
        startIdx = accumulatedData.indexOf('$');
        endIdx = accumulatedData.indexOf('\n', startIdx);
    }
}

void MainWindow::parseRMC(const QString &data) {
    QStringList parts = data.split(",");
    if (parts.size() > 9) {
        QString latitude = parts[3];
        QString latDirection = parts[4];
        QString longitude = parts[5];
        QString lonDirection = parts[6];
        QString speed = parts[7];  // 单位是节

        // 转换经纬度格式
        QString latDeg = convertToDegrees(latitude, latDirection);
        QString lonDeg = convertToDegrees(longitude, lonDirection);

        // 更新 UI 显示
        ui->latitude->setText("纬度: " + latDeg);
        ui->longitude->setText("经度: " + lonDeg);
        // ui->speedLabel->setText("速度: " + QString::number(speed.toDouble() * 1.852, 'f', 2) + " km/h");
    }
}

QString MainWindow::convertToDegrees(const QString &raw, const QString &direction) {
    // 将 NMEA 格式转换为度分格式 (DD°MM.MMMM')
    double decimal = raw.toDouble();
    int degrees = static_cast<int>(decimal / 100);
    double minutes = decimal - (degrees * 100);
    QString result = QString::number(degrees) + "°" + QString::number(minutes, 'f', 5) + "' " + direction;
    return result;
}*/

void MainWindow::openSerial(){
    // 遍历所有可用串口，查找设备名包含 "USB-SERIAL CH340" 的串口
    bool CH340 = false;
    foreach (const QSerialPortInfo &info, QSerialPortInfo::availablePorts()) {
        qDebug() << "Port Name: " << info.portName();         // 例如 COM9
        qDebug() << "Description: " << info.description();   // 例如 USB-SERIAL CH340
        qDebug() << "Manufacturer: " << info.manufacturer(); // 例如 QinHeng Electronics

        // 如果设备名匹配 "USB-SERIAL CH341A"
        if (info.description().contains("USB-SERIAL CH340")) {
            m_pSerial->setPort(info);
            CH340 = true;
            break;
        }
    }

    if (CH340) {
        // 设置波特率、数据位、校验位、停止位等
            m_pSerial->setBaudRate(QSerialPort::Baud9600);
            m_pSerial->setDataBits(QSerialPort::Data8);
            m_pSerial->setParity(QSerialPort::NoParity);
            m_pSerial->setStopBits(QSerialPort::OneStop);
            m_pSerial->setFlowControl(QSerialPort::NoFlowControl);

            // 打开串口并连接 readyRead 信号
            if (m_pSerial->open(QIODevice::ReadWrite)) {
                connect(m_pSerial, &QSerialPort::readyRead, this, &MainWindow::readSerialData);
                qDebug() << "串口已打开，正在监听数据 ";
            } else {
                qDebug() << "无法打开串口";
            }
      } else {
            qDebug() << "未找到 USB-SERIAL CH340 设备";
      }
}

void MainWindow::readSerialData() {
    QByteArray data = m_pSerial->readAll();  // 读取所有数据
    qDebug() << "接收到的数据: " << data;

    if (data.isEmpty()) {
        qDebug() << "接收到空数据，继续等待...";
        return;
    }

    // 将数据转换为字符串并去除空白字符
    QString receivedData = QString::fromUtf8(data).trimmed();

    if (receivedData.startsWith("OK")) {
        qDebug() << "接收到确认信息，不保存到 CSV";
    } else {
        parseAndSaveData(receivedData);  // 解析并保存数据
    }
}

void MainWindow::parseAndSaveData(const QString &data) {
    // 打印原始数据
    qDebug() << "原始数据:" << data;

    // 去除外层的引号（如果有的话）
    QString cleanedData = data.trimmed().remove(QRegularExpression(R"(^\"|\"$)"));
    qDebug() << "清理后的数据:" << cleanedData;

    // 正则表达式解析
    QRegularExpression regex(R"(J(\d+)\.(\d+)\.(\d+)\.(\d+)W(\d+)\.(\d+)\.(\d+)\.(\d+)P(\d+\.\d+)N(\d+\.\d+)K(\d+\.\d+))");
    QRegularExpressionMatch match = regex.match(cleanedData);

    if (match.hasMatch()) {
        // 提取经度的度、分、秒
        int longitudeDegrees = match.captured(1).toInt();
        double longitudeMinutes = match.captured(2).toDouble();
        double longitudeSeconds = match.captured(3).toDouble() + match.captured(4).toDouble() / 100;

        // 提取纬度的度、分、秒
        int latitudeDegrees = match.captured(5).toInt();
        double latitudeMinutes = match.captured(6).toDouble();
        double latitudeSeconds = match.captured(7).toDouble() + match.captured(8).toDouble() / 100;

        // 计算十进制度格式的经度和纬度
        double longitude = longitudeDegrees + (longitudeMinutes / 60.0) + (longitudeSeconds / 3600.0);
        double latitude = latitudeDegrees + (latitudeMinutes / 60.0) + (latitudeSeconds / 3600.0);

        // 提取 P, N, K 值
        double pValue = match.captured(9).toDouble();
        double nValue = match.captured(10).toDouble();
        double kValue = match.captured(11).toDouble();

        // 保存数据到 CSV 文件
        saveDataToCSV(pValue, nValue, kValue, longitude, latitude);

        qDebug() << "Longitude:" << longitude
                 << "Latitude:" << latitude
                 << "P:" << pValue
                 << "N:" << nValue
                 << "K:" << kValue;
    } else {
        qDebug() << "接收到无效格式的数据:" << cleanedData;
    }
}

void MainWindow::saveDataToCSV(double pValue, double nValue, double kValue, double longitude, double latitude) {
    QString filePath = QCoreApplication::applicationDirPath() + "/cutton.csv";
    QFile file(filePath);

    // 打开 CSV 文件读取内容
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "无法打开 cutton.csv 文件";
        return;
    }

    QStringList lines;
    QTextStream in(&file);
    while (!in.atEnd()) {
        lines.append(in.readLine());
    }
    file.close();

    // 初始化列状态标志（判断是否有空位可填）
    bool pFilled = false, nFilled = false, kFilled = false, lonFilled = false, latFilled = false;

    // 遍历每一行，填补空位
    for (QString& line : lines) {
        QStringList cells = line.split(",");

        // 检查第二列（P列）是否为空
        if (cells.size() > 1 && cells[1].trimmed().isEmpty() && !pFilled) {
            cells[1] = QString::number(pValue, 'f', 5);  // 填充 P 列
            pFilled = true;
        }

        // 检查第三列（N列）是否为空
        if (cells.size() > 2 && cells[2].trimmed().isEmpty() && !nFilled) {
            cells[2] = QString::number(nValue, 'f', 5);  // 填充 N 列
            nFilled = true;
        }

        // 检查第四列（K列）是否为空
        if (cells.size() > 3 && cells[3].trimmed().isEmpty() && !kFilled) {
            cells[3] = QString::number(kValue, 'f', 5);  // 填充 K 列
            kFilled = true;
        }

        // 检查第五列（J列）是否为空
        if (cells.size() > 4 && cells[4].trimmed().isEmpty() && !lonFilled) {
            cells[4] = QString::number(longitude, 'f', 6);  // 填充 J 列
            lonFilled = true;
        }

        // 检查第六列（W列）是否为空
        if (cells.size() > 5 && cells[5].trimmed().isEmpty() && !latFilled) {
            cells[5] = QString::number(latitude, 'f', 6);  // 填充 W 列
            latFilled = true;
        }

        // 更新当前行
        line = cells.join(",");
    }

    // 如果有任何一列没有填补空位，则添加新行
    if (!pFilled || !nFilled || !kFilled || !lonFilled || !latFilled) {
        QString newRow = QString(",%1,%2,%3,%4,%5")
                             .arg(pFilled ? "" : QString::number(pValue, 'f', 5))
                             .arg(nFilled ? "" : QString::number(nValue, 'f', 5))
                             .arg(kFilled ? "" : QString::number(kValue, 'f', 5))
                             .arg(lonFilled ? "" : QString::number(longitude, 'f', 6))
                             .arg(latFilled ? "" : QString::number(latitude, 'f', 6));
        lines.append(newRow);
    }

    // 写回 CSV 文件
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        qDebug() << "无法写入 cutton.csv 文件";
        return;
    }

    QTextStream out(&file);
    for (const QString& line : lines) {
        out << line << "\n";
    }
    file.close();

    qDebug() << "数据写入成功: P=" << pValue << " N=" << nValue << " K=" << kValue
             << " J=" << longitude << " W=" << latitude;
}

void MainWindow::onRowClicked(int row, int column) {
    // 记录选中的行号
    selectedRow = row;
}

void MainWindow::loadCSVToTable() {
    QString appDirPath = QCoreApplication::applicationDirPath();
    QString filePath = appDirPath + "/cutton.csv";

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "无法打开文件";
        return;
    }

    // 清空当前表格
    ui->dataTable->setRowCount(0);

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed(); // 去除行首尾空白
                if (line.isEmpty()) {
                    continue; // 跳过空行
                }
                QStringList fields = line.split(","); // 假设 CSV 使用逗号分隔

                // 根据 currentSubstance 来决定显示哪一列 (N, P, K)
                int colIndex = -1;
                if (currentSubstance == "N") colIndex = 2;
                else if (currentSubstance == "P") colIndex = 1;
                else if (currentSubstance == "K") colIndex = 3;

                // 确保选择了有效的列
                if (colIndex != -1 && colIndex < fields.size()) {
                    QString data = fields[colIndex].trimmed(); // 获取当前列的数据
                    QString J = fields[4].trimmed();
                    QString W = fields[5].trimmed();
                    //qDebug() << "当前列: " << currentSubstance << ", 数据: " << data;

                    // 仅当数据不为空且不为 "N"、"P"、"K" 时，才将数据插入到表格
                    if (!data.isEmpty() && data != "N" && data != "P" && data != "K") {
                        int row = ui->dataTable->rowCount();
                        ui->dataTable->insertRow(row);
                        ui->dataTable->setItem(row, 0, new QTableWidgetItem(data));  // 插入有效数据
                        ui->dataTable->setItem(row, 2, new QTableWidgetItem(J));
                        ui->dataTable->setItem(row, 3, new QTableWidgetItem(W));

                        // 设置时间格式，例如 "yyyy-MM-dd HH:mm:ss"
                        QString timeFormat = "yyyy-MM-dd HH:mm:ss";
                        QString currentTime = QDateTime::currentDateTime().toString(timeFormat);
                        ui->dataTable->setItem(row, 1, new QTableWidgetItem(currentTime));  // 时间
                        // 确保滚动条始终在最底部
                        ui->dataTable->scrollToBottom();
                    }
                }

        // 将数据插入到表格的相应列中
     /*  for (int col = 0; col < fields.size(); ++col) {
            ui->dataTable->setItem(row, col, new QTableWidgetItem(fields[col]));
        }*/
    }

    // 只显示每列的最后一个非空值
    for (int i = 0; i < ui->dataTable->rowCount(); ++i) {
        QString lastValue = ui->dataTable->item(i, 0) ? ui->dataTable->item(i, 0)->text() : QString();
        if (lastValue.isEmpty()) {
            // 如果当前行的值为空，检查是否有上一行的值可用
            if (i > 0) {
                QString previousValue = ui->dataTable->item(i - 1, 0) ? ui->dataTable->item(i - 1, 0)->text() : QString();
                if (!previousValue.isEmpty()) {
                    ui->dataTable->setItem(i, 0, new QTableWidgetItem(previousValue)); // 复制上一个有效值
                }
            }
        }
    }

    file.close();
}

// 更新 CSV 文件中的指定单元格
void MainWindow::updateCSVCell(int row, int column, const QString& newValue) {
    QString appDirPath = QCoreApplication::applicationDirPath();
    QString filePath = appDirPath + "/cutton.csv";

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "无法打开CSV文件";
        return;
    }

    // 读取CSV文件的所有内容
    QStringList lines;
    QTextStream in(&file);
    while (!in.atEnd()) {
        lines.append(in.readLine());
    }
    file.close();

    // 检查行号是否超出范围
    if (row >= lines.size()) {
        qDebug() << "错误：行号超出范围，行号=" << row << ", 总行数=" << lines.size();
        return;
    }

    // 修改指定单元格的值
    QStringList cells = lines[row].split(",");
    if (column < cells.size()) {
        cells[column] = newValue;  // 更新指定单元格
        lines[row] = cells.join(",");  // 重新组合为CSV行
    } else {
        qDebug() << "错误：列号超出范围，列号=" << column << ", 总列数=" << cells.size();
        return;
    }

    // 写回CSV文件
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        qDebug() << "无法写入CSV文件";
        return;
    }

    QTextStream out(&file);
    for (const QString& line : lines) {
        out << line << "\n";
    }
    file.close();

    qDebug() << "CSV文件更新成功";
}



void MainWindow::on_btnDelete_clicked() {
    if (selectedRow < 0) {
        QMessageBox::warning(this, "警告", "删除成功");
        return;
    }

    // 确认是否删除
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, "删除确认", "你确定要删除该单元格数据",
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        int columnToDelete = getColumnForCurrentSubstance();  // 获取当前元素的列
        if (columnToDelete < 0) {
            QMessageBox::warning(this, "错误", "无效类型");
            return;
        }

        QString appDirPath = QCoreApplication::applicationDirPath();
        QString filePath = appDirPath + "/cutton.csv";

        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qDebug() << "无法打开CSV文件";
            return;
        }

        // 读取CSV文件的所有内容
        QStringList lines;
        QTextStream in(&file);
        while (!in.atEnd()) {
            lines.append(in.readLine());
        }
        file.close();

        // 将选中列的数据向上移动
        for (int i = selectedRow + 1; i < lines.size() - 1; ++i) {  // 跳过表头
            QStringList currentCells = lines[i].split(",");
            QStringList nextCells = lines[i + 1].split(",");

            // 将下一行的数据移到当前行
            if (columnToDelete < nextCells.size()) {
                currentCells[columnToDelete] = nextCells[columnToDelete];
            } else {
                currentCells[columnToDelete] = "";  // 防止越界，填空字符串
            }

            lines[i] = currentCells.join(",");
        }

        // 清空最后一行对应列的数据
        QStringList lastRowCells = lines.last().split(",");
        if (columnToDelete < lastRowCells.size()) {
            lastRowCells[columnToDelete] = "";
        }
        lines[lines.size() - 1] = lastRowCells.join(",");

        // 写回CSV文件
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            qDebug() << "无法写入CSV文件";
            return;
        }

        QTextStream out(&file);
        for (const QString& line : lines) {
            out << line << "\n";
        }
        file.close();

        // 重新加载表格数据
        ui->dataTable->clearContents();
        ui->dataTable->setRowCount(0);

        for (int i = 1; i < lines.size(); ++i) {  // 从第1行开始，跳过表头
            QStringList cells = lines[i].split(",");
            ui->dataTable->insertRow(i - 1);
            for (int col = 0; col < cells.size(); ++col) {
                ui->dataTable->setItem(i - 1, col, new QTableWidgetItem(cells[col]));
            }
        }

        // 重置选中的行索引
        selectedRow = -1;

       // 更新显示的最后一个值
        if (ui->dataTable->rowCount() > 0) {
            int lastRow = ui->dataTable->rowCount() - 1;
            QString lastNN = ui->dataTable->item(lastRow, 0)
                             ? ui->dataTable->item(lastRow, 0)->text()
                             : "";

            if (!lastNN.isEmpty()) {
                ui->NN->setText(lastNN);
            } else {
                ui->NN->clear();
            }
        } else {
            ui->NN->clear();
        }

        // 更新地图显示
        mapImage.fill(Qt::white);
       // drawMap();

        qDebug() << "成功删除数据";
    }
}



void MainWindow::deleteRowFromCSV(int rowToDelete) {
    QString appDirPath = QCoreApplication::applicationDirPath();
    QString filePath = appDirPath + "/cutton.csv";

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "无法打开文件";
        return;
    }

    QStringList fileContent;
    QTextStream in(&file);
    int currentRow = 0;

    while (!in.atEnd()) {
        QString line = in.readLine();

        // 如果当前行不是要删除的行，则保留
        if (currentRow != rowToDelete) {
            fileContent << line;
        }

        currentRow++;
    }
    file.close();

    // 将新的内容写回 CSV 文件
    if (file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        QTextStream out(&file);
        for (const QString &line : fileContent) {
            out << line << "\n";
        }
        qDebug() << "CSV file update successful";
        file.close();
    } else {
        qDebug() << "无法写入文件";
    }
}

void MainWindow::updateDateTime() {
    QDateTime currentDateTime = QDateTime::currentDateTime();
    QString dateTimeText = currentDateTime.toString("yyyy-MM-dd HH:mm:ss");
    ui->date->setText(dateTimeText);
}

void MainWindow::resetTask() {
    if (taskThread) {
        taskThread->quit();
        taskThread->wait();
        delete taskThread;
    }

    taskThread = new QThread(this);
    task = new MyTask;

    task->moveToThread(taskThread);

    connect(task, &MyTask::taskFinished, this, &MainWindow::handleTaskFinished);
    connect(task, &MyTask::newDataAvailable, this, &MainWindow::onNewDataAvailable); // 连接新数据可用信号到槽函数
    connect(taskThread, &QThread::started, task, &MyTask::run);
    connect(taskThread, &QThread::finished, task, &QObject::deleteLater);
}


void MainWindow::on_btnStart_clicked() {
    qDebug() << "on_btnStart_clicked called"; // 调试输出

    ui->btnK->setEnabled(true);
    ui->btnN->setEnabled(true);
    ui->btnP->setEnabled(true);

    client->connectToHost();

    // 获取应用程序根目录路径
    QString appDirPath = QCoreApplication::applicationDirPath();
    qDebug() << "Application directory path:" << appDirPath;

    // 拼接文件路径
    QString filePath = appDirPath + "/cutton.csv";
    qDebug() << "File path:" << filePath;

    // 仅在任务线程为 nullptr 时才重新初始化任务
    if (!taskThread) {
        taskThread = new QThread(this);
        task = new MyTask;

        task->moveToThread(taskThread);

        connect(task, &MyTask::taskFinished, this, &MainWindow::handleTaskFinished);
        connect(task, &MyTask::newDataAvailable, this, &MainWindow::onNewDataAvailable);
        connect(taskThread, &QThread::started, task, &MyTask::run);
        connect(taskThread, &QThread::finished, task, &QObject::deleteLater);
    }

    // 如果线程已经启动，则不再重复启动
    if (!taskThread->isRunning()) {
        task->setFileName(filePath);
        qDebug() << "Starting task thread";
        taskThread->start(); // 启动任务线程
    }

    // 启动定时器
        if (!dataUpdateTimer->isActive()) {
            dataUpdateTimer->start(100);  // 启动定时器，每0.1秒更新数据
            qDebug() << "Data update timer started.";
        }

        if (!fileCheckTimer->isActive()) {
            fileCheckTimer->start(100);  // 启动定时器，每0.1秒检查文件更新
            qDebug() << "File check timer started.";
        }
        // 确保文件读取完成后再调用绘制函数
        if (task->dataRows.isEmpty()) {
            qDebug() << "Data rows are still empty, skipping draw.";
            return;
        }
        //drawTrajectoryWithColor(task->dataRows, currentSubstance, lowerLimit1, lowerLimit2, lowerLimit3, upperLimit3);
}


void MainWindow::on_btnPause_clicked() {
    // 暂停任务或执行其他操作
    dataUpdateTimer->stop();
    fileCheckTimer->stop();
}

void MainWindow::handleTaskFinished() {
    // 调用保存数据到新的 CSV 文件的方法
        if (task) {
            task->saveToNewCSV();
            qDebug() << "Data saved to cutton_final.csv";
        }
}

void MainWindow::updateData() {
    qDebug() << "updateData:";

    ui->dataTable->setColumnWidth(1, 150);  // 设置时间列的宽度为150像素

    QString lastValidValue;  // 存储当前列中最后一个非空的数据
    int validRowCount = 0;   // 用于跟踪有效数据行的计数

    // 遍历所有行，逐行检查数据是否有更新
    for (int i = 0; i < task->dataRows.size(); ++i) {
        QStringList currentData = task->dataRows[i];  // 获取第 i 行的数据

        if (!currentData.isEmpty()) {
            QString newValue;

            // 根据当前选择的元素决定要检查的列
            if (currentSubstance == "N") {
                newValue = currentData[2];  // N 列
            } else if (currentSubstance == "P") {
                newValue = currentData[1];  // P 列
            } else if (currentSubstance == "K") {
                newValue = currentData[3];  // K 列
            }

            // 仅当该列的值不为空时，处理数据
            if (!newValue.isEmpty()) {
                lastValidValue = newValue;  // 保存最后一个有效值

                // 如果该行不存在，插入新行
                if (validRowCount >= ui->dataTable->rowCount()) {
                    ui->dataTable->insertRow(validRowCount);
                }

                // 检查该列是否有新数据，并更新界面
                if (ui->dataTable->item(validRowCount, 0) == nullptr ||
                    ui->dataTable->item(validRowCount, 0)->text() != newValue) {

                    // 更新表格中的数据
                    ui->dataTable->setItem(validRowCount, 0, new QTableWidgetItem(newValue));

                    // 插入第五列数据到第三列
                    ui->dataTable->setItem(validRowCount, 2, new QTableWidgetItem(currentData[4]));
                    // 插入第六列数据到第四列
                    ui->dataTable->setItem(validRowCount, 3, new QTableWidgetItem(currentData[5]));

                    // 设置时间格式
                    QString timeFormat = "yyyy-MM-dd HH:mm:ss";
                    QString currentTime = QDateTime::currentDateTime().toString(timeFormat);
                    ui->dataTable->setItem(validRowCount, 1, new QTableWidgetItem(currentTime));

                    qDebug() << "Row " << validRowCount << " updated with value: " << newValue;

                    ui->dataTable->scrollToBottom();
                }

                validRowCount++;  // 仅在插入了有效数据后增加计数
                //ui->dataTable->scrollToBottom();
            }
        }
    }

    // 如果没有任何有效数据，则清空 QLabel
    /*if (!lastValidValue.isEmpty()) {
        ui->NN->setText(lastValidValue);
    } else {
        ui->NN->clear();
    }*/

    // 删除多余的空行（如果有的话）
    while (ui->dataTable->rowCount() > validRowCount) {
        ui->dataTable->removeRow(ui->dataTable->rowCount() - 1);
    }

    // 更新地图（延时100ms执行）
    // QTimer::singleShot(100, this, &MainWindow::drawTrajectoryWithColor(task->dataRows, currentSubstance, lowerLimit1, lowerLimit2, lowerLimit3, upperLimit3));
}

void MainWindow::onNewDataAvailable() {
    qDebug() << "onNewDataAvailable: currentSubstance = " << currentSubstance;

    if (task->currentIndex < task->dataRows.size()) {
        QStringList newData = task->getCurrentData();  // 获取当前行的数据
        qDebug() << "newData = " << newData;

        if (!newData.isEmpty()) {
            QString displayData;

            // 根据当前选择的元素决定显示的数据
            if (currentSubstance == "N" && newData.size() > 2) {
                displayData = newData[2];  // N 列
            } else if (currentSubstance == "P" && newData.size() > 1) {
                displayData = newData[1];  // P 列
            } else if (currentSubstance == "K" && newData.size() > 3) {
                displayData = newData[3];  // K 列
            }

            qDebug() << "Inserting data into QLabel NN: " << displayData;

            // 更新 QLabel (NN)
            if (!displayData.isEmpty()) {
                ui->NN->setText(displayData);  // 显示数据
            } else {
                ui->NN->clear();  // 如果数据为空则清空
            }

            // 如果是最新行的数据，则保存到 CSV
            if (task->currentIndex == task->dataRows.size() - 1) {
                task->appendDataToCSV(newData);  // 追加数据到 CSV
            }
        }

        // 确保在数据加载完后触发绘制
        if (!task->dataRows.isEmpty()) {
            drawTrajectoryWithColor(task->dataRows, currentSubstance, lowerLimit1, lowerLimit2, lowerLimit3, upperLimit3);
        }
    }

    // 确保定时器继续运行
    if (!dataUpdateTimer->isActive()) {
        dataUpdateTimer->start(10);
    }
}

void MainWindow::checkForFileUpdates() {
    if (task) {
        task->checkForUpdates();
        if (task->isUpdated) {
            qDebug() << "File updated, checking for new data.";
            task->isUpdated = false;  // 重置更新标志

            if (task->currentIndex < task->dataRows.size()) {
                // 继续更新数据和地图
//                dataUpdateTimer->start(10);
            } else {
                qDebug() << "No new data available yet.";
            }
        }
//        else{
//            qDebug() << "NO new data.checkForFileUpdates";
//        }
    }
}

void MainWindow::startDrawing()
{
    // 模拟轨迹更新，你可以在这里用实时经纬度数据替换
    QList<QPointF> points = {{0, 0}, {10, 20}, {20, 30}, {30, 10}};
    updateTrajectory(points);
}

void MainWindow::updateTrajectory(const QList<QPointF>& points)
{
    scene->clear(); // 清空之前的轨迹
    for (int i = 0; i < points.size() - 1; ++i) {
        scene->addLine(QLineF(points[i], points[i + 1]), QPen(Qt::blue, 2));
    }
}

void MainWindow::initMapView() {

    if (ui->mapView->scene()) {
        QRectF viewportRect = ui->mapView->mapToScene(ui->mapView->viewport()->geometry()).boundingRect();
        ui->mapView->scene()->setSceneRect(viewportRect);
    }

    // 禁用滚动条
    ui->mapView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui->mapView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
}


void MainWindow::drawTrajectoryWithColor(const QList<QStringList>& dataRows, const QString& currentSubstance,
                                         qreal lowerLimit1, qreal lowerLimit2, qreal lowerLimit3, qreal upperLimit3) {

    try {
        qDebug() << "drawTrajectoryStart";

        // 检查 scene 是否为 nullptr，并在必要时初始化
        if (ui->mapView->scene() == nullptr) {
            qDebug() << "Scene is null, setting a new scene.";
            ui->mapView->setScene(new QGraphicsScene(this));
        }

        // 清空当前场景
        ui->mapView->scene()->clear();
        qDebug() << "finishClear";

        const int blockSize = 10; // 每个矩形的大小
        int dataIndex = 0;

        // 获取地图视图大小，用于调整显示范围
        int mapWidth = ui->mapView->width();
        int mapHeight = ui->mapView->height();
        qDebug() << "width:" << mapWidth;
        qDebug() << "height:" << mapHeight;

        QSize size = ui->mapView->size();
        qDebug() << "mapView size:" << size;
        QSize viewportSize = ui->mapView->viewport()->size();
        qDebug() << "mapView viewport size:" << viewportSize;
        QRectF sceneRect = ui->mapView->scene()->sceneRect();
        qDebug() << "Scene Rect:" << sceneRect;

        // 检查数据行数
        qDebug() << "dataRows size:" << dataRows.size();
        if (dataRows.isEmpty()) {
            qDebug() << "Data rows are empty. Exiting drawMap.";
            return; // 如果没有数据，直接退出函数
        }

        QVector<QPointF> coordinates;

        // 遍历数据，根据经纬度位置绘制
        for (const QStringList& currentData : dataRows) {
            if (currentData.size() < 6) {
                continue; // 跳过不完整数据
            }

            // 获取经纬度数据
            bool latOk, lonOk;
            qreal lon = currentData[4].toDouble(&lonOk);  // 经度
            qreal lat = currentData[5].toDouble(&latOk);  // 纬度

            if (!latOk || !lonOk) {
                continue; // 跳过无效经纬度数据
            }

            // 获取当前物质的值
            qreal value = 0;
            bool validData = false;
            if (currentSubstance == "N" && currentData.size() >= 3 && !currentData[2].isEmpty()) {
                value = currentData[2].toDouble();
                validData = true;
            } else if (currentSubstance == "P" && currentData.size() >= 2 && !currentData[1].isEmpty()) {
                value = currentData[1].toDouble();
                validData = true;
            } else if (currentSubstance == "K" && currentData.size() >= 4 && !currentData[3].isEmpty()) {
                value = currentData[3].toDouble();
                validData = true;
            }

            if (validData) {
                // 根据浓度值设置颜色
                QColor color;
                if (value < lowerLimit1) {
                    color = QColor(227, 232, 192);
                } else if (value < lowerLimit2) {
                    color = QColor(255, 226, 168);
                } else if (value < lowerLimit3) {
                    color = QColor(246, 165, 118);
                } else if (value < upperLimit3) {
                    color = QColor(233, 109, 81);
                } else {
                    color = QColor(212, 49, 42);
                }

                // 将经纬度转换为场景坐标
                qreal sceneX = (lon + 180.0) * (mapWidth / 360.0);
                qreal sceneY = (90.0 - lat) * (mapHeight / 180.0);

                // 绘制矩形并绑定数据
                QGraphicsRectItem* rect = ui->mapView->scene()->addRect(sceneX, sceneY, blockSize, blockSize, QPen(), QBrush(color));
                rect->setData(0, dataIndex); // 绑定行号

                // 存储坐标
                coordinates.append(QPointF(sceneX, sceneY));
            }

            dataIndex++;
        }

        // 确保所有绘制的点都在视图内显示
        if (!coordinates.isEmpty()) {
            qreal minX = coordinates[0].x(), maxX = coordinates[0].x();
            qreal minY = coordinates[0].y(), maxY = coordinates[0].y();

            for (const QPointF& point : coordinates) {
                minX = qMin(minX, point.x());
                maxX = qMax(maxX, point.x());
                minY = qMin(minY, point.y());
                maxY = qMax(maxY, point.y());
            }

            ui->mapView->scene()->setSceneRect(minX, minY, maxX - minX, maxY - minY);
            ui->mapView->fitInView(minX, minY, maxX - minX, maxY - minY, Qt::KeepAspectRatio);
        }

        qDebug() << "drawTrajectory finished without exceptions.";
    } catch (const std::exception &e) {
        qDebug() << "Exception occurred in drawTrajectory:" << e.what();
    } catch (...) {
        qDebug() << "Unknown exception occurred in drawTrajectory.";
    }
}


void MainWindow::showN() {
    ui->btnN->setProperty("selected", true);   // 设置 N 按钮为选中状态
    ui->btnP->setProperty("selected", false);  // 设置 P 按钮为非选中状态
    ui->btnK->setProperty("selected", false);  // 设置 K 按钮为非选中状态
    ui->btnN->style()->unpolish(ui->btnN);     // 刷新样式
    ui->btnN->style()->polish(ui->btnN);
    ui->btnP->style()->unpolish(ui->btnP);
    ui->btnP->style()->polish(ui->btnP);
    ui->btnK->style()->unpolish(ui->btnK);
    ui->btnK->style()->polish(ui->btnK);
    currentSubstance = "N";
    ui->dataTable->clear();  // 清空表格
    loadCSVToTable();        // 重新加载 CSV 数据，只显示 N 列
    if (!task->dataRows.isEmpty()) {
        drawTrajectoryWithColor(task->dataRows, currentSubstance, lowerLimit1, lowerLimit2, lowerLimit3, upperLimit3);
    }
    ui->dataTable->setHorizontalHeaderLabels(QStringList() << "N" << "时间" << "东经" << "北纬");
    ui->tableTitle->setText(tr("Nitrogen Concentration Record Table (g/KG)"));
    ui->nText->setText(tr("Nitrogen content"));
    //ui->mapLabel->setPixmap(QPixmap::fromImage(mapImage));
}

void MainWindow::showP() {
    ui->btnP->setProperty("selected", true);   // 设置 P 按钮为选中状态
    ui->btnN->setProperty("selected", false);  // 设置 N 按钮为非选中状态
    ui->btnK->setProperty("selected", false);  // 设置 K 按钮为非选中状态
    ui->btnN->style()->unpolish(ui->btnN);     // 刷新样式
    ui->btnN->style()->polish(ui->btnN);
    ui->btnP->style()->unpolish(ui->btnP);
    ui->btnP->style()->polish(ui->btnP);
    ui->btnK->style()->unpolish(ui->btnK);
    ui->btnK->style()->polish(ui->btnK);
    currentSubstance = "P";
    ui->dataTable->clear();  // 清空表格
    loadCSVToTable();        // 重新加载 CSV 数据，只显示 P 列
    if (!task->dataRows.isEmpty()) {
        drawTrajectoryWithColor(task->dataRows, currentSubstance, lowerLimit1, lowerLimit2, lowerLimit3, upperLimit3);
    }
    ui->dataTable->setHorizontalHeaderLabels(QStringList() << "P" << "时间" << "东经" << "北纬");
    ui->tableTitle->setText(tr("Phosphorus Concentration Record Table (g/KG)"));
    ui->nText->setText(tr("Phosphorus content"));
    //ui->mapLabel->setPixmap(QPixmap::fromImage(mapImage));
}

void MainWindow::showK() {
    ui->btnK->setProperty("selected", true);   // 设置 K 按钮为选中状态
    ui->btnP->setProperty("selected", false);  // 设置 P 按钮为非选中状态
    ui->btnN->setProperty("selected", false);  // 设置 K 按钮为非选中状态
    ui->btnK->style()->unpolish(ui->btnK);     // 刷新样式
    ui->btnK->style()->polish(ui->btnK);
    ui->btnP->style()->unpolish(ui->btnP);
    ui->btnP->style()->polish(ui->btnP);
    ui->btnN->style()->unpolish(ui->btnN);
    ui->btnN->style()->polish(ui->btnN);
    currentSubstance = "K";
    ui->dataTable->clear();  // 清空表格
    loadCSVToTable();        // 重新加载 CSV 数据，只显示 K 列
    if (!task->dataRows.isEmpty()) {
        drawTrajectoryWithColor(task->dataRows, currentSubstance, lowerLimit1, lowerLimit2, lowerLimit3, upperLimit3);
    }
    ui->dataTable->setHorizontalHeaderLabels(QStringList() << "K" << "时间" << "东经" << "北纬");
    ui->tableTitle->setText(tr("kalium Concentration Record Table (g/KG)"));
    ui->nText->setText(tr("kalium content"));
    //ui->mapLabel->setPixmap(QPixmap::fromImage(mapImage));
}

void MainWindow::resizeEvent(QResizeEvent *event) {
    QMainWindow::resizeEvent(event);
    if (ui->mapView->scene()) {
        ui->mapView->scene()->setSceneRect(0, 0, ui->mapView->width(), ui->mapView->height());
        ui->mapView->fitInView(ui->mapView->scene()->sceneRect(), Qt::KeepAspectRatio);
    }
}

// 获取当前选择的元素对应的列号（假设 N 在第2列，P 在第1列，K 在第3列）
int MainWindow::getColumnForCurrentSubstance() {
    if (currentSubstance == "N") return 2;
    if (currentSubstance == "P") return 1;
    if (currentSubstance == "K") return 3;
    return -1;  // 如果元素无效，返回-1
}

void MainWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        // 转换全局坐标到 mapView 的本地坐标
        QPointF localPos = ui->mapView->mapFromGlobal(event->globalPos());

        // 检查是否在 mapView 内
        if (ui->mapView->rect().contains(localPos.toPoint())) {
            // 将本地坐标转换为场景坐标
            QPointF scenePos = ui->mapView->mapToScene(localPos.toPoint());
            qDebug() << "Mouse clicked at scene position:" << scenePos;

            // 获取场景中的图元
            QList<QGraphicsItem*> items = ui->mapView->scene()->items(scenePos);
            if (!items.isEmpty()) {
                QGraphicsItem* clickedItem = items.first();
                if (clickedItem) {
                    // 读取绑定的数据（假设绑定的是行索引）
                    bool ok;
                    int rowIndex = clickedItem->data(0).toInt(&ok);
                    if (ok) {
                        // 跳转到对应行并显示数据
                        if (rowIndex >= 0 && rowIndex < ui->dataTable->rowCount()) {
                            ui->dataTable->selectRow(rowIndex);
                            ui->dataTable->scrollToItem(ui->dataTable->item(rowIndex, 0));

                            // 更新 QLabel（假设第一列数据）
                            QTableWidgetItem* item = ui->dataTable->item(rowIndex, 0);
                            if (item) {
                                ui->NN->setText(item->text());
                                qDebug() << "NN updated with:" << item->text();
                            } else {
                                ui->NN->setText("No Data");
                                qDebug() << "Selected row has no valid data.";
                            }
                            qDebug() << "Navigated to row:" << rowIndex;
                        } else {
                            qDebug() << "Row index out of range.";
                        }
                    } else {
                        qDebug() << "Invalid data in clicked item.";
                    }
                }
            } else {
                qDebug() << "No items clicked.";
            }
        }
    }

    // 调用父类鼠标事件处理
    QMainWindow::mousePressEvent(event);
}


void MainWindow::mouseMoveEvent(QMouseEvent *event)
{
    // 如果正在拖动并且鼠标在 mapView 内部
    if (dragging && ui->mapView->geometry().contains(event->pos())) {
        QPoint delta = event->pos() - lastMousePos;  // 计算鼠标移动的偏移量
        ui->mapView->horizontalScrollBar()->setValue(ui->mapView->horizontalScrollBar()->value() - delta.x());
        ui->mapView->verticalScrollBar()->setValue(ui->mapView->verticalScrollBar()->value() - delta.y());
        lastMousePos = event->pos();  // 更新鼠标位置
    }
    QMainWindow::mouseMoveEvent(event);  // 调用父类事件处理
}

void MainWindow::mouseReleaseEvent(QMouseEvent *event)
{
    // 如果鼠标在 mapView 内部，并且按下的是左键
    if (ui->mapView->geometry().contains(event->pos()) && event->button() == Qt::LeftButton) {
        dragging = false;  // 停止拖动
        unsetCursor();  // 恢复鼠标光标
    }
    QMainWindow::mouseReleaseEvent(event);  // 调用父类事件处理
}

void MainWindow::wheelEvent(QWheelEvent* event) {
    // 将全局坐标转换为 mapView 的局部坐标
    QPointF localPos = ui->mapView->mapFromGlobal(event->globalPos());
    if (ui->mapView->rect().contains(localPos.toPoint())) {
        // 计算缩放因子
        qreal factor = (event->angleDelta().y() > 0) ? 1.1 : 0.9;
        ui->mapView->scale(factor, factor);

        // 调整场景矩形
        QGraphicsScene* scene = ui->mapView->scene();
        if (scene) {
            QRectF sceneRect = scene->sceneRect();
            scene->setSceneRect(sceneRect.x(), sceneRect.y(), sceneRect.width() * factor, sceneRect.height() * factor);
        }
    } else {
        // 不在 mapView 内时调用父类事件
        QMainWindow::wheelEvent(event);
    }
}

/*void MainWindow::on_btnCamera_clicked() {
    // 获取 Camera 的实例，并保存图像
    Camera *cameraWindow = new Camera(this);

    // 保存图像
    cameraWindow->captureImage();
}*/
