#include "TokenManager.h"
#include "Utilities.h"
#include "PathProvider.h"
#include "IThemeGenerator.h"
#include "QMLThemeGenerator.h"
#include "WidgetsDesignGenerator.h"

#include <QDebug>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStringBuilder>

using namespace DTI;

static const QString CoreMain = QString::fromLatin1("Core/Main");
static const QString CoreColors = QString::fromLatin1("Colors");
static const QString SemanticTokens = QString::fromLatin1("Semantic tokens");
static const QString ColorTokenStart = QString::fromLatin1("color-");

TokenManager::TokenManager()
{
    mCurrentDir = QDir::currentPath();
    qDebug() << __func__ << " Current working directory : " << mCurrentDir;
}

TokenManager* TokenManager::instance()
{
    static TokenManager manager;
    return &manager;
}

void TokenManager::run()
{
    // look for tokens.json file
    QString pathToDesignTokensFile = Utilities::resolvePath(mCurrentDir, PathProvider::RELATIVE_DESIGN_TOKENS_FILE_PATH);
    QFile designTokensFile(pathToDesignTokensFile);
    if (!designTokensFile.exists())
    {
        qCritical() << __func__ << " Error : No tokens.json file found in " << pathToDesignTokensFile;
        return;
    }

    if (!designTokensFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qDebug() << __func__ << " Error : opening tokens.json file " << pathToDesignTokensFile;
        return;
    }

    // load core data
    const CoreData& coreData = parseCore(designTokensFile);

    // parse json color themed files.
    const ThemedColorData& themesData = parseTheme(designTokensFile, coreData);

    // qml style generator entry point.
    std::unique_ptr<IThemeGenerator> qmlDesignGenerator{new QmlThemeGenerator()};
    qmlDesignGenerator->start(themesData);

    // qtwidget style generator entry point.
    std::unique_ptr<IThemeGenerator> widgetsDesignGenerator{new WidgetsDesignGenerator()};
    widgetsDesignGenerator->start(themesData);
}

void TokenManager::recurseCore(QString category, const QJsonObject& coreColors, CoreData& coreData)
{
    const QStringList tokenKeys = coreColors.keys();

    if (tokenKeys.contains(QLatin1String("value")) && tokenKeys.contains(QLatin1String("type")))
    {
        QJsonValue jType = coreColors["type"];
        QJsonValue jValue = coreColors["value"];

        if (!jType.isNull() && jValue.isString())
        {
            QString type = jType.toString();

            if (type == "color")
            {
                /*
                 * Core color hex value format is #RRGGBBAA,
                 * but we need to convert it to AARRGGBB.
                */
                QString coreColorHex = jValue.toString().remove(QChar('#'));
                if (coreColorHex.length() == 8)
                {
                    coreColorHex = coreColorHex.right(2) + coreColorHex.left(6);
                }

                coreData.insert(category, coreColorHex);
            }
        }
    }
    else
    {
        for (int index = 0; index < tokenKeys.size(); ++index)
        {
            QString subCategory = category + "." + tokenKeys[index];
            QJsonObject categoryObj = coreColors.value(tokenKeys[index]).toObject();

            recurseCore(subCategory, categoryObj, coreData);
        }
    }
}

ColorData TokenManager::parseColorTheme(const QJsonObject& jsonThemeObject, const CoreData& coreData)
{
    ColorData colourData;

    const QStringList categoryKeys = jsonThemeObject.keys();
    for (int index = 0; index < categoryKeys.size(); ++index)
    {
        const QString& category = categoryKeys[index];
        QJsonObject categoryObject = jsonThemeObject.value(category).toObject();

        const QStringList tokenKeys = categoryObject.keys();
        for (int index = 0; index < tokenKeys.size(); ++index)
        {
            const QString& token = tokenKeys[index];

            QJsonObject tokenObject = categoryObject[token].toObject();
            QJsonValue jType = tokenObject["type"];
            QJsonValue jValue = tokenObject["value"];
            //QJsonValue jAlpha = tokenObject["alpha"];

            if (!jType.isNull() && !jValue.isNull()) // && !jAlpha.isNull())
            {
                QString type = jType.toString();

                if (type == "color")
                {
                    QString value = jValue.toString();
                    value.remove("{").remove("}");
                    //float alpha = jAlpha.toString().toFloat();

                    if (coreData.contains(value))
                    {
                        QString coreColor = coreData[value];
                        //QString alphaString = QString::number(static_cast<uint>(alpha * 255), 16).rightJustified(2, '0');
                        //QString color = "#" + alphaString + coreColor;
                        QString color = "#" + coreColor;

                        // Strip "--color-" or "-color-" from beginning of token
                        int indexPrefix = token.indexOf(ColorTokenStart);
                        if (indexPrefix != -1)
                        {
                            colourData.insert(token.mid(indexPrefix + ColorTokenStart.size()), color);
                        }
                    }
                    else
                    {
                        qDebug() << __func__ << " Core map doesn't contain the color id " << value;
                    }
                }
            }
        }
    }

    return colourData;
}

CoreData TokenManager::parseCore(QFile& designTokensFile)
{
    const QString errorPrefix = __func__ + QString(" Error : parsing design tokens file, ");

    CoreData coreData;

    designTokensFile.seek(0);
    QJsonDocument jsonDocument = QJsonDocument::fromJson(designTokensFile.readAll());
    if (jsonDocument.isNull())
    {
        qDebug() << errorPrefix << "couldn't read data.";
        return coreData;
    }

    QJsonObject jsonObject = jsonDocument.object();
    QStringList childKeys = jsonObject.keys();
    auto foundMatchingChildKeyIt = std::find_if(childKeys.constBegin(), childKeys.constEnd(), [](const QString& key){
        return key == CoreMain;
    });

    if (foundMatchingChildKeyIt == childKeys.constEnd())
    {
        qDebug() << errorPrefix << "key not found " << CoreMain;
        return coreData;
    }

    jsonObject = jsonObject.value(CoreMain).toObject();
    childKeys = jsonObject.keys();
    foundMatchingChildKeyIt = std::find_if(childKeys.constBegin(), childKeys.constEnd(), [](const QString& key){
        return key == CoreColors;
    });

    if (foundMatchingChildKeyIt == childKeys.constEnd())
    {
        qDebug() << errorPrefix << "key not found " << CoreColors;
        return coreData;
    }

    QJsonObject coreColors = jsonObject.value(CoreColors).toObject();
    recurseCore(CoreColors, coreColors, coreData);

    return coreData;
}

ThemedColorData TokenManager::parseTheme(QFile& designTokensFile, const CoreData& coreData)
{
    const QString errorPrefix = __func__ + QString(" Error : parsing design tokens file, ");

    ThemedColorData themedColorData;

    designTokensFile.seek(0);
    QJsonDocument jsonDocument = QJsonDocument::fromJson(designTokensFile.readAll());
    if (jsonDocument.isNull())
    {
        qDebug() << errorPrefix << "couldn't read data.";
        return themedColorData;
    }

    QJsonObject jsonObject = jsonDocument.object();
    QStringList childKeys = jsonObject.keys();
    std::for_each(childKeys.constBegin(), childKeys.constEnd(), [&](const QString& key)
    {
        if (key.contains(SemanticTokens))
        {
            QJsonObject themeObject = jsonObject.value(key).toObject();

            ColorData colorData = parseColorTheme(themeObject, coreData);
            if (colorData.isEmpty())
            {
                qDebug() << errorPrefix << "no color theme data for " << key;
                return;
            }

            QString theme = key.right(key.size() - key.lastIndexOf('/') - 1);
            if (theme.isEmpty())
            {
                qDebug() << errorPrefix << " Error : No valid theme found on key " << key;
                return;
            }

            themedColorData.insert(theme, colorData);
        }
    });

    return themedColorData;
}


