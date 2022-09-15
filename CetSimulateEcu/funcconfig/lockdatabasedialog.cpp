#include "lockdatabasedialog.h"
#include "ui_lockdatabasedialog.h"

#include <QSqlQueryModel>
#include <QSortFilterProxyModel>
#include <QMessageBox>

#include <QDebug>

LockDatabaseDialog::LockDatabaseDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::LockDatabaseDialog)
{
    ui->setupUi(this);
    
    model = new QSqlQueryModel(this);
    sqlproxy = new QSortFilterProxyModel(this);
    sqlproxy->setSourceModel(model);

    ui->tableView->setModel(sqlproxy);
    ui->tableView->setSortingEnabled(true);
    ui->tableView->verticalHeader()->hide();
    ui->tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableView->setSelectionMode(QAbstractItemView::SingleSelection);

    int column = 0;
    ui->tableView->setColumnWidth(column++, 60);
    ui->tableView->setColumnWidth(column++, 80);
    ui->tableView->setColumnWidth(column++, 160);
    ui->tableView->setColumnWidth(column++, 200);
    ui->tableView->setColumnWidth(column++, 160);
    ui->tableView->setColumnWidth(column++, 110);
    ui->tableView->setColumnWidth(column++, 240);
}

LockDatabaseDialog::~LockDatabaseDialog()
{
    delete model;
    delete ui;
}

void LockDatabaseDialog::updateDatabase()
{
    model->setQuery(QString("select * from %1_%2_LOCKCFG order by id asc").arg(m_ecuBrand, m_engineVendor));
    
    int column = 0;
    model->setHeaderData(column++, Qt::Horizontal, tr("序列ID"));
    model->setHeaderData(column++, Qt::Horizontal, tr("车厂产商"));
    model->setHeaderData(column++, Qt::Horizontal, tr("SEED接收CANID"));
    model->setHeaderData(column++, Qt::Horizontal, tr("SEED发送CANID"));
    model->setHeaderData(column++, Qt::Horizontal, tr("SEED接收数据"));
    model->setHeaderData(column++, Qt::Horizontal, tr("SEED发送数据"));
    model->setHeaderData(column++, Qt::Horizontal, tr("KEY接收数据"));
    model->setHeaderData(column++, Qt::Horizontal, tr("KEY发送数据"));
    model->setHeaderData(column++, Qt::Horizontal, tr("绑定接收CANID"));
    model->setHeaderData(column++, Qt::Horizontal, tr("绑定发送CANID"));
    model->setHeaderData(column++, Qt::Horizontal, tr("绑定接收数据"));
    model->setHeaderData(column++, Qt::Horizontal, tr("绑定发送数据"));
    model->setHeaderData(column++, Qt::Horizontal, tr("解绑接收数据"));
    model->setHeaderData(column++, Qt::Horizontal, tr("解绑发送数据"));
    model->setHeaderData(column++, Qt::Horizontal, tr("锁车接收CANID"));
    model->setHeaderData(column++, Qt::Horizontal, tr("锁车发送CANID"));
    model->setHeaderData(column++, Qt::Horizontal, tr("锁车接收数据"));
    model->setHeaderData(column++, Qt::Horizontal, tr("锁车发送数据"));
    model->setHeaderData(column++, Qt::Horizontal, tr("解锁接收数据"));
    model->setHeaderData(column++, Qt::Horizontal, tr("解锁发送数据"));
    model->setHeaderData(column++, Qt::Horizontal, tr("心跳接收CANID"));
    model->setHeaderData(column++, Qt::Horizontal, tr("心跳发送CANID"));
    model->setHeaderData(column++, Qt::Horizontal, tr("心跳接收数据"));
    model->setHeaderData(column++, Qt::Horizontal, tr("心跳发送数据"));

    sqlproxy->sort(0);
}


void LockDatabaseDialog::showDialog()
{
    setWindowTitle(tr("%1_%2_LOCKCFG 表预览").arg(m_ecuBrand, m_engineVendor));
    updateDatabase();
    show();
}

void LockDatabaseDialog::setEcuType(const QString &ecuBrand, const QString &engineVendor)
{
    m_ecuBrand = ecuBrand;
    m_engineVendor = engineVendor;
}

void LockDatabaseDialog::on_deleteButton_clicked()
{
    int res = QMessageBox::question(this, tr("删除"), tr("确定要删除该条目？\t"), 
                            QMessageBox::Ok, QMessageBox::No);
    if (QMessageBox::No == res)
        return;

    int row = ui->tableView->currentIndex().row();
    QModelIndex modelIndex = model->index(row, 0);

    qDebug() << "row" << row << "modelIndex" << modelIndex.data().toInt();
    model->setQuery(QString("delete from %1_%2_LOCKCFG where id=%3")
        .arg(m_ecuBrand, m_engineVendor).arg(modelIndex.data().toInt()));

    updateDatabase();
}

void LockDatabaseDialog::on_okButton_clicked()
{
    int row = ui->tableView->currentIndex().row();
    if (row >= 0) {
        QModelIndex modelIndex = model->index(row, 1);
        qDebug() << "row" << row << "modelIndex" << modelIndex.data().toString();
        emit signalsVehicleVendor(modelIndex.data().toString());
    }

    accept();
}

void LockDatabaseDialog::on_cancelButton_clicked()
{
    reject();
}
