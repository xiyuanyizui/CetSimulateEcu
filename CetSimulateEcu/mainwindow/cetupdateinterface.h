#ifndef CETUPDATEINTERFACE_H
#define CETUPDATEINTERFACE_H

#include <QString>

typedef void ( *callback_t )(void);

class CetUpdateInterface
{
public:
    virtual ~CetUpdateInterface() { }
    /* 检测需要更新(服务器是否存在新版本) */
    virtual void checkUpdate(const QString &setup, const QString &oldver, callback_t callback = nullptr) = 0;
};

Q_DECLARE_INTERFACE(CetUpdateInterface, "org.qter.myplugin.CetUpdateInterface")

#endif // CETUPDATEINTERFACE_H
