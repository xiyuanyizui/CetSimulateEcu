#ifndef CANIDFILTERDIALOG_H
#define CANIDFILTERDIALOG_H

#include <QDialog>

namespace Ui {
class CanIdFilterDialog;
}

class CanIdFilterDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CanIdFilterDialog(QWidget *parent = 0);
    ~CanIdFilterDialog();
    bool isFilterCanId(const QString &frameid);

public slots:
    void on_clearButton_clicked();
    void on_importButton_clicked();
    void on_exportButton_clicked();
    void on_addButton_clicked();
    void on_modeComboBox_activated(int index);

private:
    bool isInRange(const QString &frameid);

private slots:
    void doDelTableWidget(int row, int column);

private:
    Ui::CanIdFilterDialog *ui;
};

#endif // CANIDFILTERDIALOG_H
