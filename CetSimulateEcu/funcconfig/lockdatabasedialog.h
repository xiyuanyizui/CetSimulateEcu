#ifndef LOCKDATABASEDIALOG_H
#define LOCKDATABASEDIALOG_H

#include <QDialog>

namespace Ui {
class LockDatabaseDialog;
}

class QSqlQueryModel;
class QSortFilterProxyModel;

class LockDatabaseDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LockDatabaseDialog(QWidget *parent = 0);
    ~LockDatabaseDialog();

public:
    void showDialog();
    void updateDatabase();
    void setEcuType(const QString &ecuBrand, const QString &engineVendor);

signals:
    void signalsVehicleVendor(const QString &vehicleVendor);
    
private slots:
    void on_deleteButton_clicked();

    void on_okButton_clicked();

    void on_cancelButton_clicked();

private:
    Ui::LockDatabaseDialog *ui;
    QSqlQueryModel *model;
    QSortFilterProxyModel *sqlproxy;

    QString m_ecuBrand;
    QString m_engineVendor;
};

#endif // LOCKDATABASEDIALOG_H
