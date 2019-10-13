#include "PasteMegaLinksDialog.h"
#include "ui_PasteMegaLinksDialog.h"

#include <QClipboard>
#include <QUrl>
#include <QMessageBox>

#include<iostream>
using namespace std;

PasteMegaLinksDialog::PasteMegaLinksDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::PasteMegaLinksDialog)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_QuitOnClose, false);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    const QClipboard *clipboard = QApplication::clipboard();
    QString text = clipboard->text();
    if (extractLinks(text).size() != 0)
    {
        ui->eLinks->setPlainText(text);
    }

    ui->bSubmit->setDefault(true);
    highDpiResize.init(this);
}

PasteMegaLinksDialog::~PasteMegaLinksDialog()
{
    delete ui;
}

QStringList PasteMegaLinksDialog::getLinks()
{
    return links;
}

void PasteMegaLinksDialog::on_bSubmit_clicked()
{
    QString text = ui->eLinks->toPlainText();
    links = extractLinks(text);
    links = links.toSet().toList();
    if (links.size() == 0)
    {
        if (!text.trimmed().size())
        {
            QMessageBox::warning(NULL, tr("Warning"), tr("Enter one or more MEGA file links"));
        }
        else
        {
            QMessageBox::warning(NULL, tr("Warning"), tr("Invalid MEGA Link"));
        }
        return;
    }

    accept();
}

void PasteMegaLinksDialog::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange)
    {
        ui->retranslateUi(this);
    }

    QDialog::changeEvent(event);
}

QStringList PasteMegaLinksDialog::extractLinks(QString text)
{
    QStringList finalLinks;
    QStringList linkHeaders;
    linkHeaders << QString::fromUtf8("#!") << QString::fromUtf8("#F!");

    for (int i = 0; i < linkHeaders.size(); i++)
    {
        QString linkHeader = linkHeaders[i];

        QStringList tempLinks = text.split(QString::fromLatin1("https://mega.co.nz/").append(linkHeader), QString::KeepEmptyParts, Qt::CaseInsensitive);
        tempLinks.removeAt(0);

        QStringList tempLinks2 = text.split(QString::fromLatin1("mega://").append(linkHeader), QString::KeepEmptyParts, Qt::CaseInsensitive);
        tempLinks2.removeAt(0);
        tempLinks.append(tempLinks2);

        QStringList tempLinksNewSite = text.split(QString::fromLatin1("https://mega.nz/").append(linkHeader), QString::KeepEmptyParts, Qt::CaseInsensitive);
        tempLinksNewSite.removeAt(0);
        tempLinks.append(tempLinksNewSite);

        QStringList tempLinksHttp = text.split(QString::fromLatin1("http://mega.co.nz/").append(linkHeader), QString::KeepEmptyParts, Qt::CaseInsensitive);
        tempLinksHttp.removeAt(0);
        tempLinks.append(tempLinksHttp);

        QStringList tempLinksNewSiteHttp = text.split(QString::fromLatin1("http://mega.nz/").append(linkHeader), QString::KeepEmptyParts, Qt::CaseInsensitive);
        tempLinksNewSiteHttp.removeAt(0);
        tempLinks.append(tempLinksNewSiteHttp);

        for (int i = 0; i < tempLinks.size(); i++)
        {
            QString link = checkLink(tempLinks[i].insert(0, QString::fromLatin1("https://mega.nz/").append(linkHeader)));
            if (!link.isNull())
            {
                finalLinks.append(link);
            }
        }
    }

    return finalLinks;
}

QString PasteMegaLinksDialog::checkLink(QString link)
{
    link = QUrl::fromPercentEncoding(link.toUtf8());
    link.replace(QChar::fromLatin1(' '), QChar::fromLatin1('+'));

    // File link
    if (link.at(26) == QChar::fromLatin1('!'))
    {
        if (link.length() < FILE_LINK_SIZE)
        {
            return QString();
        }

        link.truncate(FILE_LINK_SIZE);
        return link;
    }

    // Folder link
    if (link.at(27) == QChar::fromLatin1('!'))
    {
        if (link.length() >= FOLDER_LINK_WITH_SUBFOLDER_SIZE && link.count(QChar::fromLatin1('!')) == 3)
        {
            link.truncate(FOLDER_LINK_WITH_SUBFOLDER_SIZE);
        }
        else
        {
            if (link.length() < FOLDER_LINK_SIZE)
            {
                return QString();
            }

            link.truncate(FOLDER_LINK_SIZE);
        }

        return link;
    }

    return QString();
}
