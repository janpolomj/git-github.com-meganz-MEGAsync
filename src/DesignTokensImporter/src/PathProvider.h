#ifndef PATHPROVIDER_H
#define PATHPROVIDER_H

#include <QString>

namespace DTI
{
class PathProvider
{
public:
    PathProvider() = delete;

    //paths
    static const QString RELATIVE_GENERATED_PATH;
    static const QString RELATIVE_CORE_FILE_PATH;
    static const QString RELATIVE_COLOR_TOKENS_PATH;
    static const QString RELATIVE_UI_PATH;
    static const QString RELATIVE_IMAGES_PATH;
    static const QString RELATIVE_SVG_PATH;
    static const QString RELATIVE_SVG_QRC_PATH;
    static const QString RELATIVE_GENERATED_SVG_DIR_PATH;
    static const QString RELATIVE_GUI_PRI_PATH;
    static const QString RELATIVE_RESOURCE_FILE_IMAGES_PATH;
    static const QString RELATIVE_UI_WIN_PATH;
    static const QString RELATIVE_UI_LINUX_PATH;
    static const QString RELATIVE_UI_MAC_PATH;
    static const QString RELATIVE_QRC_MAC_PATH;
    static const QString RELATIVE_QRC_WINDOWS_PATH;
    static const QString RELATIVE_QRC_LINUX_PATH;
    static const QString RELATIVE_THEMES_DIR_PATH;
    static const QString RELATIVE_STYLES_DIR_PATH;
    static const QString RELATIVE_CSS_WIN_PATH;
    static const QString RELATIVE_CSS_LINUX_PATH;
    static const QString RELATIVE_CSS_MAC_PATH;
    static const QString RELATIVE_HASHES_PATH;
    static const QString RELATIVE_CMAKE_FILE_LIST_DIR_PATH;

    //filters
    static const QString JSON_NAME_FILTER;
    static const QString UI_NAME_FILTER;
    static const QString SVG_NAME_FILTER;
    static const QString CSS_NAME_FILTER;

    //file extensions
    static const QString SVG_FILE_EXTENSION;
    static const QString CSS_FILE_EXTENSION;
    static const QString UI_FILE_EXTENSION;
};
}

#endif // PATHPROVIDER_H
