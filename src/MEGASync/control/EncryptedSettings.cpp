#include "EncryptedSettings.h"
#include "platform/Platform.h"

EncryptedSettings::EncryptedSettings(QString file) :
    QSettings(file, QSettings::IniFormat)
{
    QByteArray fixedSeed("$JY/X?o=h·&%v/M(");
    QByteArray localKey = Platform::getLocalStorageKey();
    QByteArray xLocalKey = XOR(fixedSeed, localKey);
    QByteArray hLocalKey = QCryptographicHash::hash(xLocalKey, QCryptographicHash::Sha1);
    encryptionKey = hLocalKey;
}

void EncryptedSettings::setValue(const QString &key, const QVariant &value)
{
    QSettings::setValue(hash(key), encrypt(key, value.toString()));
}

QVariant EncryptedSettings::value(const QString &key, const QVariant &defaultValue)
{
    return QVariant(decrypt(key, QSettings::value(hash(key), encrypt(key, defaultValue.toString())).toString()));
}

void EncryptedSettings::beginGroup(const QString &prefix)
{
    QSettings::beginGroup(hash(prefix));
}

void EncryptedSettings::beginGroup(int numGroup)
{
     QSettings::beginGroup(QSettings::childGroups().at(numGroup));
}

void EncryptedSettings::endGroup()
{
    QSettings::endGroup();
}

int EncryptedSettings::numChildGroups()
{
    return QSettings::childGroups().size();
}

bool EncryptedSettings::containsGroup(QString groupName)
{
    return QSettings::childGroups().contains(hash(groupName));
}

bool EncryptedSettings::isGroupEmpty()
{
    return QSettings::group().isEmpty();
}

void EncryptedSettings::remove(const QString &key)
{
    if (!key.length())
    {
        QSettings::remove(QString::fromLatin1(""));
    }
    else
    {
        QSettings::remove(hash(key));
    }
}

void EncryptedSettings::clear()
{
    QSettings::clear();
}

void EncryptedSettings::sync()
{
    QSettings::sync();

    QFile::remove(this->fileName().append(QString::fromUtf8(".bak")));
    QFile::copy(this->fileName(),this->fileName().append(QString::fromUtf8(".bak")));
}

//Simplified XOR fun
QByteArray EncryptedSettings::XOR(const QByteArray& key, const QByteArray& data) const
{
    int keyLen = key.length();
    if (!keyLen)
    {
        return data;
    }

    QByteArray result;
    int rotation = abs(key[keyLen/3]*key[keyLen/5])%keyLen;
    int increment = abs(key[keyLen/2]*key[keyLen/7])%keyLen;
    for (int i = 0, j = rotation; i < data.length(); i++, j -= increment)
    {
        if (j < 0)
        {
            j += keyLen;
        }
        result.append(data[i] ^ key[j]);
    }
    return result;
}

QString EncryptedSettings::encrypt(const QString key, const QString value) const
{
    if (value.isEmpty())
    {
        return value;
    }

    QByteArray k = hash(key).toAscii();
    QByteArray xValue = XOR(k, value.toUtf8());
    QByteArray xKey = XOR(k, group().toAscii());
    QByteArray xEncrypted = XOR(k, Platform::encrypt(xValue, xKey));
    return QString::fromLatin1(xEncrypted.toBase64());
}

QString EncryptedSettings::decrypt(const QString key, const QString value) const
{
    if (value.isEmpty())
    {
        return value;
    }

    QByteArray k = hash(key).toAscii();
    QByteArray xValue = XOR(k, QByteArray::fromBase64(value.toAscii()));
    QByteArray xKey = XOR(k, group().toAscii());
    QByteArray xDecrypted = XOR(k, Platform::decrypt(xValue, xKey));
    return QString::fromUtf8(xDecrypted);
}

QString EncryptedSettings::hash(const QString key) const
{
    QByteArray xPath = XOR(encryptionKey, (key+group()).toUtf8());
    QByteArray keyHash = QCryptographicHash::hash(xPath, QCryptographicHash::Sha1);
    QByteArray xKeyHash = XOR(key.toUtf8(), keyHash);
    return QString::fromLatin1(xKeyHash.toHex());
}
