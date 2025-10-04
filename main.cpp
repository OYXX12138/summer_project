#include <QApplication>
#include "mainwindow.h"
#include <QSqlDatabase>
#include <QSqlError>
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <QTranslator>

int main(int argc, char *argv[])
{
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling, true);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps, true);

    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

    QApplication app(argc, argv);

    // 加载翻译文件
    QTranslator translator;
    translator.load("translations/Language_CN.qm"); // 确保翻译文件路径正确
    QApplication::installTranslator(&translator);

    QFile file(":/res/qss/style.qss");
    file.open(QFile::ReadOnly);
    QTextStream filetext(&file);
    QString stylesheet = filetext.readAll();
    app.setStyleSheet(stylesheet);
    file.close();

    /*QStringList drivers = QSqlDatabase::drivers();
        qDebug() << "Available drivers:" << drivers;

   // 添加插件路径
        QApplication::addLibraryPath("D:/Postgraduate/Qt/5.14.1/msvc2017_64/plugins");

        QSqlDatabase db = QSqlDatabase::addDatabase("QMYSQL");
        db.setHostName("localhost");
        db.setDatabaseName("mysql");
        db.setUserName("root");
        db.setPassword("123456");

        if (!db.open()) {
            qDebug() << "无法连接到数据库: " << db.lastError().text();
        } else {
            qDebug() << "成功连接到数据库";
            db.close(); // 确保连接正常后关闭数据库
            QSqlDatabase::removeDatabase("qt_sql_default_connection");
        }*/
    MainWindow mainWindow;
    mainWindow.show();
    return app.exec();
}

