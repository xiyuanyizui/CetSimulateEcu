#include "mainwindow.h"
#include <QApplication>
#include <QTextCodec>
#include <QStyleFactory>
#include "connection.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    if (!createConnection())
        return -1;

    QTextCodec::setCodecForLocale(QTextCodec::codecForLocale());
    QApplication::setStyle(QStyleFactory::create(QStyleFactory::keys().last()));

    MainWindow w;
    w.show();

    return a.exec();
}
