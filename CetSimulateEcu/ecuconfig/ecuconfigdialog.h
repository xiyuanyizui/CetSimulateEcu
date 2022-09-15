#ifndef ECUCONFIGDIALOG_H
#define ECUCONFIGDIALOG_H

#include <QDialog>

namespace Ui {
class EcuConfigDialog;
}

// 发动机品牌
#define ECUBRAND_BOSCH      "BOSCH"     /**< 博世 */
#define ECUBRAND_DENSO      "DENSO"     /**< 电装 */
#define ECUBRAND_VDO        "VDO"       /**< 大陆 */

// 发动机产商
#define ECUVENDOR_MAN           "MAN"           /**< 大曼 */
#define ECUVENDOR_WEICHAI       "WEICHAI"       /**< 潍柴 */
#define ECUVENDOR_YUCHAI        "YUCHAI"        /**< 玉柴 */
#define ECUVENDOR_XICHAI        "XICHAI"        /**< 锡柴 */
#define ECUVENDOR_YUNNEI        "YUNNEI"        /**< 云内 */
#define ECUVENDOR_KANGMINGSI    "KANGMINGSI"    /**< 康明斯 */


class EcuConfigDialog : public QDialog
{
    Q_OBJECT

public:
    explicit EcuConfigDialog(QWidget *parent = 0);
    ~EcuConfigDialog();
    
signals:
    void signalEcuTypeChanged(const QString &ecuBrand, const QString &engineVendor);

public slots:
    void showDialog();
    void slotControlStateChanged(bool acc, bool ig, bool can);

private slots:
    void doSlaveEcuParamConfig();

private:
    Ui::EcuConfigDialog *ui;

    QString m_ecuBrand;
    QString m_engineVendor;
    //QString m_vehicleVendor;
};

#endif // ECUCONFIGDIALOG_H
