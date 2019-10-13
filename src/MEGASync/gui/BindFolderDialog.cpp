#include "BindFolderDialog.h"
#include "ui_BindFolderDialog.h"
#include "MegaApplication.h"
#include "control/Utilities.h"
#include <QInputDialog>

using namespace mega;

BindFolderDialog::BindFolderDialog(MegaApplication *app, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::BindFolderDialog)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_QuitOnClose, false);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    this->app = app;
    Preferences *preferences = Preferences::instance();
    syncNames = preferences->getSyncNames();
    localFolders = preferences->getLocalFolders();
    megaFolderHandles = preferences->getMegaFolderHandles();
    ui->bOK->setDefault(true);
    highDpiResize.init(this);
}

BindFolderDialog::BindFolderDialog(MegaApplication *app, QStringList syncNames,
                                   QStringList localFolders,
                                   QList<long long> megaFolderHandles,
                                   QWidget *parent) :
    QDialog(parent),
    ui(new Ui::BindFolderDialog)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_QuitOnClose, false);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    this->app = app;
    this->syncNames = syncNames;
    this->localFolders = localFolders;
    this->megaFolderHandles = megaFolderHandles;
    ui->bOK->setDefault(true);
}

BindFolderDialog::~BindFolderDialog()
{
    delete ui;
}

long long BindFolderDialog::getMegaFolder()
{
    return ui->wBinder->selectedMegaFolder();
}

void BindFolderDialog::setMegaFolder(MegaHandle handle)
{
    ui->wBinder->setSelectedMegaFolder(handle);
}

QString BindFolderDialog::getLocalFolder()
{
    return ui->wBinder->selectedLocalFolder();
}

QString BindFolderDialog::getSyncName()
{
    return syncName;
}

void BindFolderDialog::on_bOK_clicked()
{
    QString localFolderPath = ui->wBinder->selectedLocalFolder();
    MegaApi *megaApi = app->getMegaApi();
    MegaHandle handle = ui->wBinder->selectedMegaFolder();

    MegaNode *node = megaApi->getNodeByHandle(handle);
    if (!localFolderPath.length() || !node)
    {
        delete node;
        QMessageBox::warning(NULL, tr("Error"), tr("Please select a local folder and a MEGA folder"), QMessageBox::Ok);
        return;
    }

    localFolderPath = QDir::toNativeSeparators(QDir(localFolderPath).canonicalPath());
    if (!localFolderPath.size())
    {
        accept();
        return;
    }

    for (int i = 0; i < localFolders.size(); i++)
    {
        QString c = QDir::toNativeSeparators(QDir(localFolders[i]).canonicalPath());
        if (!c.size())
        {
            continue;
        }

        if (localFolderPath.startsWith(c)
                && ((c.size() == localFolderPath.size())
                    || (localFolderPath[c.size()] == QDir::separator())))
        {
            delete node;
            QMessageBox::warning(NULL, tr("Error"), tr("The selected local folder is already synced"), QMessageBox::Ok);
            return;
        }
        else if (c.startsWith(localFolderPath)
                 && c[localFolderPath.size()] == QDir::separator())
        {
            delete node;
            QMessageBox::warning(NULL, tr("Error"), tr("A synced folder cannot be inside another synced folder"), QMessageBox::Ok);
            return;
        }
    }

    for (int i = 0; i < megaFolderHandles.size(); i++)
    {
        MegaNode *n = megaApi->getNodeByHandle(megaFolderHandles[i]);
        if (n)
        {
            const char *cPath = megaApi->getNodePath(node);
            if (!cPath)
            {
                delete n;
                continue;
            }

            QString megaPath = QString::fromUtf8(cPath);
            delete [] cPath;

            const char *nPath = megaApi->getNodePath(n);
            if (!nPath)
            {
                delete n;
                continue;
            }

            QString p = QString::fromUtf8(nPath);
            delete [] nPath;

            if (megaPath.startsWith(p) && ((p.size() == megaPath.size()) || p.size() == 1 || megaPath[p.size()] == QChar::fromLatin1('/')))
            {
                delete n;
                delete node;
                QMessageBox::warning(NULL, tr("Error"), tr("The selected MEGA folder is already synced"), QMessageBox::Ok);
                return;
            }
            else if (p.startsWith(megaPath) && ((p.size() == megaPath.size()) || megaPath.size() == 1 || p[megaPath.size()] == QChar::fromLatin1('/')))
            {
                delete n;
                delete node;
                QMessageBox::warning(NULL, tr("Error"), tr("A synced folder cannot be inside another synced folder"), QMessageBox::Ok);
                return;
            }
            delete n;
        }
    }
    delete node;

   bool repeated;
   syncName = QFileInfo(localFolderPath).fileName();
   do
   {
       repeated = false;
       for (int i = 0; i < syncNames.size(); i++)
       {
           if (!syncName.compare(syncNames[i]))
           {
                repeated = true;

                QPointer<QInputDialog> id = new QInputDialog(this);
                id->setWindowFlags(id->windowFlags() & ~Qt::WindowContextHelpButtonHint);
                id->setWindowTitle(tr("Sync name"));
                id->setLabelText(tr("The name \"%1\" is already in use for another sync\n"
                                    "Please enter a different name to identify this synced folder:").arg(syncName));
                int result = id->exec();

                if (!id || !result)
                {
                    delete id;
                    return;
                }

                QString text = id->textValue();
                text = text.trimmed();
                delete id;

                if (text.isEmpty())
                {
                    return;
                }
                syncName = text;
           }
       }
   } while (repeated);

   accept();
}

void BindFolderDialog::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange)
    {
        ui->retranslateUi(this);
    }
    QDialog::changeEvent(event);
}
