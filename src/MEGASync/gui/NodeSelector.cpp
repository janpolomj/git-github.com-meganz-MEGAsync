#include "NodeSelector.h"
#include "ui_NodeSelector.h"

#include <QMessageBox>
#include <QPointer>
#include <QMenu>
#include "control/Utilities.h"


using namespace mega;

NodeSelector::NodeSelector(MegaApi *megaApi, int selectMode, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::NodeSelector)
{
    ui->setupUi(this);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    this->setWindowModality(Qt::ApplicationModal);


    this->megaApi = megaApi;
    this->model = NULL;
    folderIcon =  QIcon(QString::fromLatin1("://images/small_folder.png"));
    selectedFolder = mega::INVALID_HANDLE;
    selectedItem = QModelIndex();
    this->selectMode = selectMode;
    delegateListener = new QTMegaRequestListener(megaApi, this);
    ui->cbAlwaysUploadToLocation->hide();
    ui->bOk->setDefault(true);

    if (selectMode == NodeSelector::STREAM_SELECT)
    {
        setWindowTitle(tr("Select items"));
        ui->label->setText(tr("Select just one file."));
        ui->bNewFolder->setVisible(false);
    }
    else if (selectMode == NodeSelector::DOWNLOAD_SELECT)
    {
        ui->bNewFolder->setVisible(false);
    }

    nodesReady();

    ui->tMegaFolders->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->tMegaFolders, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onCustomContextMenu(const QPoint &)));
}

NodeSelector::~NodeSelector()
{
    delete delegateListener;
    delete ui;
    delete model;
}

void NodeSelector::nodesReady()
{
    if (!megaApi->isFilesystemAvailable())
    {
        ui->bOk->setEnabled(false);
        ui->bNewFolder->setEnabled(false);
        return;
    }

    model = new QMegaModel(megaApi);
    switch(selectMode)
    {
    case NodeSelector::UPLOAD_SELECT:
        model->setRequiredRights(MegaShare::ACCESS_READWRITE);
        model->showFiles(false);
        model->setDisableFolders(false);
        break;
    case NodeSelector::SYNC_SELECT:
        model->setRequiredRights(MegaShare::ACCESS_FULL);
        model->showFiles(false);
        model->setDisableFolders(false);
        break;
    case NodeSelector::DOWNLOAD_SELECT:
        model->setRequiredRights(MegaShare::ACCESS_READ);
        model->showFiles(true);
        model->setDisableFolders(false);
        break;
    case NodeSelector::STREAM_SELECT:
        model->setRequiredRights(MegaShare::ACCESS_READ);
        model->showFiles(true);
        model->setDisableFolders(false);
        break;
    }

    ui->tMegaFolders->setModel(model);
    connect(ui->tMegaFolders->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)),this, SLOT(onSelectionChanged(QItemSelection,QItemSelection)));

    ui->tMegaFolders->collapseAll();
    ui->tMegaFolders->header()->close();
//Disable animation for OS X due to problems showing the tree icons
#ifdef __APPLE__
    ui->tMegaFolders->setAnimated(false);
#endif

    QModelIndex defaultSelection = model->index(0, 0);
    ui->tMegaFolders->selectionModel()->select(defaultSelection, QItemSelectionModel::ClearAndSelect);
    ui->tMegaFolders->selectionModel()->setCurrentIndex(defaultSelection, QItemSelectionModel::ClearAndSelect);

    if (selectMode == NodeSelector::STREAM_SELECT)
    {
        ui->tMegaFolders->expandToDepth(0);
    }
}

void NodeSelector::showDefaultUploadOption(bool show)
{
    ui->cbAlwaysUploadToLocation->setVisible(show);
}

void NodeSelector::setDefaultUploadOption(bool value)
{
    ui->cbAlwaysUploadToLocation->setChecked(value);
}

long long NodeSelector::getSelectedFolderHandle()
{
    return selectedFolder;
}

void NodeSelector::setSelectedFolderHandle(long long selectedHandle)
{
    MegaNode *node = megaApi->getNodeByHandle(selectedHandle);
    if (!node)
    {
        return;
    }

    QList<MegaNode *> list;
    while (node)
    {
        list.append(node);
        node = megaApi->getParentNode(node);
    }

    if (!list.size())
    {
        return;
    }

    int index = list.size() - 1;
    QModelIndex modelIndex;
    QModelIndex parentModelIndex;
    node = list.at(index);

    for (int i = 0; i < model->rowCount(); i++)
    {
        QModelIndex tmp = model->index(i, 0);
        MegaNode *n = model->getNode(tmp);
        if (n && n->getHandle() == node->getHandle())
        {
            node = NULL;
            parentModelIndex = modelIndex;
            modelIndex = tmp;
            index--;
            ui->tMegaFolders->expand(parentModelIndex);
            break;
        }
    }

    if (node)
    {
        for (int k = 0; k < list.size(); k++)
        {
            delete list.at(k);
        }
        ui->tMegaFolders->collapseAll();
        return;
    }

    while (index >= 0)
    {
        node = list.at(index);
        for (int j = 0; j < model->rowCount(modelIndex); j++)
        {
            QModelIndex tmp = model->index(j, 0, modelIndex);
            MegaNode *n = model->getNode(tmp);
            if (n && n->getHandle() == node->getHandle())
            {
                node = NULL;
                parentModelIndex = modelIndex;
                modelIndex = tmp;
                index--;
                ui->tMegaFolders->expand(parentModelIndex);
                break;
            }
        }

        if (node)
        {
            for (int k = 0; k < list.size(); k++)
            {
                delete list.at(k);
            }
            ui->tMegaFolders->collapseAll();
            return;
        }
    }

    for (int k = 0; k < list.size(); k++)
    {
        delete list.at(k);
    }

    ui->tMegaFolders->selectionModel()->setCurrentIndex(modelIndex, QItemSelectionModel::ClearAndSelect);
    ui->tMegaFolders->selectionModel()->select(modelIndex, QItemSelectionModel::ClearAndSelect);
}

void NodeSelector::onRequestFinish(MegaApi *, MegaRequest *request, MegaError *e)
{
    ui->bNewFolder->setEnabled(true);
    ui->bOk->setEnabled(true);

    if (e->getErrorCode() != MegaError::API_OK)
    {
        ui->tMegaFolders->setEnabled(true);
        QMessageBox::critical(NULL, QString::fromUtf8("MEGAsync"), tr("Error") + QString::fromUtf8(": ") + QCoreApplication::translate("MegaError", e->getErrorString()));
        return;
    }

    if(request->getType() == MegaRequest::TYPE_CREATE_FOLDER)
    {
        if (e->getErrorCode() == MegaError::API_OK)
        {
            MegaNode *node = megaApi->getNodeByHandle(request->getNodeHandle());
            if (node)
            {
                QModelIndex row = model->insertNode(node, selectedItem);
                setSelectedFolderHandle(node->getHandle());
                ui->tMegaFolders->selectionModel()->select(row, QItemSelectionModel::ClearAndSelect);
                ui->tMegaFolders->selectionModel()->setCurrentIndex(row, QItemSelectionModel::ClearAndSelect);
            }
        }
        else
        {
            ui->tMegaFolders->setEnabled(true);
            QMessageBox::critical(NULL, QString::fromUtf8("MEGAsync"), tr("Error") + QString::fromUtf8(": ") + QCoreApplication::translate("MegaError", e->getErrorString()));
            return;
        }
    }
    else if (request->getType() == MegaRequest::TYPE_REMOVE || request->getType() == MegaRequest::TYPE_MOVE)
    {
        if (e->getErrorCode() == MegaError::API_OK)
        {
            MegaNode *parent = model->getNode(selectedItem.parent());
            if (parent)
            {
                model->removeNode(selectedItem);
                setSelectedFolderHandle(parent->getHandle());
            }
        }
    }

    ui->tMegaFolders->setEnabled(true);
}

void NodeSelector::onCustomContextMenu(const QPoint &point)
{
    QMenu customMenu;

    MegaNode *node = megaApi->getNodeByHandle(selectedFolder);
    MegaNode *parent = megaApi->getParentNode(node);

    if (parent && node)
    {        
        int access = megaApi->getAccess(node);

        if (access == MegaShare::ACCESS_OWNER)
        {
            customMenu.addAction(tr("Get MEGA link"), this, SLOT(onGenMEGALinkClicked()));
        }

        if (access >= MegaShare::ACCESS_FULL)
        {
            customMenu.addAction(tr("Delete"), this, SLOT(onDeleteClicked()));
        }
    }

    if (!customMenu.actions().isEmpty())
    {
        customMenu.exec(ui->tMegaFolders->mapToGlobal(point));
    }

    delete parent;
    delete node;
}

void NodeSelector::onDeleteClicked()
{
    MegaNode *node = megaApi->getNodeByHandle(selectedFolder);
    int access = megaApi->getAccess(node);
    if (!node || access < MegaShare::ACCESS_FULL)
    {
        delete node;
        return;
    }

    QPointer<NodeSelector> currentDialog = this;
    if (QMessageBox::question(this,
                             QString::fromUtf8("MEGAsync"),
                             tr("Are you sure that you want to delete \"%1\"?")
                                .arg(QString::fromUtf8(node->getName())),
                             QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes)
    {
        if (!currentDialog)
        {
            delete node;
            return;
        }

        ui->tMegaFolders->setEnabled(false);
        ui->bNewFolder->setEnabled(false);
        ui->bOk->setEnabled(false);
        const char *name = node->getName();
        if (access == MegaShare::ACCESS_FULL
                || !strcmp(name, "NO_KEY")
                || !strcmp(name, "CRYPTO_ERROR")
                || !strcmp(name, "BLANK"))
        {
            megaApi->remove(node, delegateListener);
        }
        else
        {
            MegaNode *rubbish = megaApi->getRubbishNode();
            megaApi->moveNode(node, rubbish, delegateListener);
            delete rubbish;
        }
    }
    delete node;
}

void NodeSelector::onGenMEGALinkClicked()
{
    MegaNode *node = megaApi->getNodeByHandle(selectedFolder);
    if (!node || node->getType() == MegaNode::TYPE_ROOT
            || megaApi->getAccess(node) != MegaShare::ACCESS_OWNER)
    {
        delete node;
        return;
    }

    megaApi->exportNode(node);
    delete node;
}

void NodeSelector::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange)
    {
        ui->retranslateUi(this);
        nodesReady();
    }
    QDialog::changeEvent(event);
}

void NodeSelector::onSelectionChanged(QItemSelection, QItemSelection)
{
    if (ui->tMegaFolders->selectionModel()->selectedIndexes().size())
    {
        selectedItem = ui->tMegaFolders->selectionModel()->selectedIndexes().at(0);
        MegaNode *node = model->getNode(selectedItem);
        if (node)
        {
            selectedFolder =  node->getHandle();
        }
        else
        {
            selectedFolder = mega::INVALID_HANDLE;
        }
    }
    else
    {
        selectedItem = QModelIndex();
        selectedFolder = mega::INVALID_HANDLE;
    }
}

void NodeSelector::on_bNewFolder_clicked()
{
    QPointer<QInputDialog> id = new QInputDialog(this);
    id->setWindowFlags(id->windowFlags() & ~Qt::WindowContextHelpButtonHint);
    id->setWindowTitle(tr("New folder"));
    id->setLabelText(tr("Enter the new folder name:"));
    int result = id->exec();

    if (!id || !result)
    {
        delete id;
        return;
    }

    QString text = id->textValue();
    text = text.trimmed();
    if (!text.isEmpty())
    {
        MegaNode *parent = megaApi->getNodeByHandle(selectedFolder);
        if (!parent)
        {
            parent = megaApi->getRootNode();
            if (!parent)
            {
                delete id;
                return;
            }
            selectedFolder = parent->getHandle();
            selectedItem = QModelIndex();
        }

        MegaNode *node = megaApi->getNodeByPath(text.toUtf8().constData(), parent);
        if (!node || node->isFile())
        {
            ui->bNewFolder->setEnabled(false);
            ui->bOk->setEnabled(false);
            ui->tMegaFolders->setEnabled(false);
            megaApi->createFolder(text.toUtf8().constData(), parent, delegateListener);
        }
        else
        {
            for (int i = 0; i < model->rowCount(selectedItem); i++)
            {
                QModelIndex row = model->index(i, 0, selectedItem);
                MegaNode *node = model->getNode(row);

                if (node && text.compare(QString::fromUtf8(node->getName())) == 0)
                {
                    setSelectedFolderHandle(node->getHandle());
                    ui->tMegaFolders->selectionModel()->select(row, QItemSelectionModel::ClearAndSelect);
                    ui->tMegaFolders->selectionModel()->setCurrentIndex(row, QItemSelectionModel::ClearAndSelect);
                    break;
                }
            }
        }
        delete parent;
        delete node;
    }
    else
    {
        QMessageBox::critical(NULL, QString::fromUtf8("MEGAsync"), tr("Please enter a valid folder name"));
        if (!id)
        {
            return;
        }
    }
    delete id;
}

void NodeSelector::on_bOk_clicked()
{
    MegaNode *node = megaApi->getNodeByHandle(selectedFolder);
    if (!node)
    {
        reject();
        return;
    }

    int access = megaApi->getAccess(node);
    if ((selectMode == NodeSelector::UPLOAD_SELECT) && ((access < MegaShare::ACCESS_READWRITE)))
    {
        delete node;
        QMessageBox::warning(NULL, tr("Error"), tr("You need Read & Write or Full access rights to be able to upload to the selected folder."), QMessageBox::Ok);
        return;

    }
    else if ((selectMode == NodeSelector::SYNC_SELECT) && (access < MegaShare::ACCESS_FULL))
    {
        delete node;
        QMessageBox::warning(NULL, tr("Error"), tr("You need Full access right to be able to sync the selected folder."), QMessageBox::Ok);
        return;
    }
    else if ((selectMode == NodeSelector::STREAM_SELECT) && node->isFolder())
    {
        delete node;
        QMessageBox::warning(NULL, tr("Error"), tr("Only files can be used for streaming."), QMessageBox::Ok);
        return;
    }

    if (selectMode == NodeSelector::SYNC_SELECT)
    {
        const char* path = megaApi->getNodePath(node);
        MegaNode *check = megaApi->getNodeByPath(path);
        delete [] path;
        if (!check)
        {
            delete node;
            QMessageBox::warning(NULL, tr("Warning"), tr("Invalid folder for synchronization.\n"
                                                         "Please, ensure that you don't use characters like '\\' '/' or ':' in your folder names."),
                                 QMessageBox::Ok);
            return;
        }
        delete check;
    }

    delete node;
    accept();
}

bool NodeSelector::getDefaultUploadOption()
{
   return ui->cbAlwaysUploadToLocation->isChecked();
}
