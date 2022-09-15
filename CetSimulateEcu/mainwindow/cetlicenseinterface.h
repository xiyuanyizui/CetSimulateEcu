#ifndef CETLICENSEINTERFACE_H
#define CETLICENSEINTERFACE_H

#include <QString>

class CetLicenseInterface
{
public:
    virtual ~CetLicenseInterface() { }
    enum AcitavteResult { 
        ACTIVATE_NONE = 0,  /**< 激活空，窗口关闭 */
        ACTIVATE_OK,        /**< 激活成功 */
        ACTIVATE_ER         /**< 激活失败 */
    };

    virtual int activate(const QString &productId) = 0;  /* 通过产品ID激活设备 */
    virtual int result() = 0;                               /* 获取激活结果 */
    virtual QString message() = 0;                          /* 激活信息 */
    virtual int exec() = 0;                                /* 使用激活窗口 */
};

Q_DECLARE_INTERFACE(CetLicenseInterface, "org.qter.myplugin.CetLicenseInterface")

#endif // CETLICENSEINTERFACE_H
