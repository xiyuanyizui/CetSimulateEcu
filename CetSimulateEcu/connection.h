#ifndef CONNECTION_H
#define CONNECTION_H

#include <QtSql>
#include <QMessageBox>
#include <QDebug>

static bool createConnection()
{
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");

    db.setHostName("www.cetxiyuan.com");
    db.setDatabaseName("database.db");
    db.setConnectOptions("QSQLITE_CREATE_KEY");
    db.setUserName("cetxiyuan");
    db.setPassword("cet123456");

    if (!db.open()) {
        QMessageBox::warning(0, QObject::tr("数据库"),
            QObject::tr("打开 %1-%2 数据库失败").arg("database.db", db.lastError().text()));
        return false;
    }

    return true;
}

#endif // CONNECTION_H
