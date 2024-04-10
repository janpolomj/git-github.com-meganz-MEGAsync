#include "QMLColorThemeTarget.h"

#include "QMLTargetUtils.h"
#include "QMLThemeTargetFactory.h"

#include <QDir>
#include <QDebug>
#include <QFileInfo>
#include <QTextStream>

#include <iostream>

using namespace DTI;

static const QString qmlColorThemeTargetPath = "%1/gui/qml/common/themes/%2";
static const QString qmlColorThemeFileName = "%1/Colors.qml";
static const QString colorThemeHeader = QString::fromUtf8("import QtQuick 2.15\n\nQtObject {\n\n");
static const QString colorThemeLine = QString::fromUtf8("\treadonly property color %1: \"%2\"\n");
static const QString colorThemeFooter = QString::fromUtf8("}\n");

bool QMLColorThemeTarget::registered = ConcreteQMLThemeFactory<QMLColorThemeTarget>::Register("qmlColorTheme");

void QMLColorThemeTarget::deploy(const ThemedColourMap& themeColorData) const
{
    if (themeColorData.isEmpty())
    {
        qWarning() << "Error : No themed colour data to deploy/process.";

        return;
    }

    for (auto themeIt = themeColorData.constBegin(); themeIt != themeColorData.constEnd(); ++themeIt)
    {
        const auto& colourMap = themeIt.value();

        auto themeName = themeIt.key().toLower();
        if (!themeName.isEmpty() && !colourMap.isEmpty())
        {
            auto qmlColorStyleFilePath = qmlColorThemeTargetPath.arg(QDir::currentPath(), themeName);

            QDir dir(qmlColorStyleFilePath);
            if (!dir.exists() && !dir.mkpath(qmlColorStyleFilePath))
            {
                qWarning() << "couldn't create directory " << qmlColorStyleFilePath;

                return;
            }

            QFile data(qmlColorThemeFileName.arg(qmlColorStyleFilePath));
            if (data.open(QFile::WriteOnly | QFile::Truncate))
            {
                QTextStream stream(&data);
                stream << colorThemeHeader;

                for (auto it = colourMap.constBegin(); it != colourMap.constEnd(); ++it)
                {
                    const auto& tokenId = it.key();
                    const auto& color = it.value().toUpper();
                    stream << colorThemeLine.arg(normalizeTokenId(tokenId), color);
                }

                stream << colorThemeFooter;
            }
        }
    }
}
