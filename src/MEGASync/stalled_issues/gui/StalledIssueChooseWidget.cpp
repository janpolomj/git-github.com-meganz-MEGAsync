#include "StalledIssueChooseWidget.h"
#include "ui_StalledIssueChooseWidget.h"

#include "Utilities.h"
#include "PlatformStrings.h"
#include "MegaApplication.h"
#include "StalledIssueHeader.h"
#include "StalledIssuesModel.h"

static const int BUTTON_ID = 0;

StalledIssueChooseWidget::StalledIssueChooseWidget(QWidget *parent) :
    QFrame(parent),
    ui(new Ui::StalledIssueChooseWidget),
    mIsSolved(false),
    mPreviousSolveState(false)
{
    ui->setupUi(this);

    ui->name->removeBackgroundColor();

    ui->path->setIndent(StalledIssueHeader::GROUPBOX_CONTENTS_INDENT);
    ui->path->hideLocalOrRemoteTitle();

    ui->chooseTitle->addActionButton(QIcon(), tr("Choose"), BUTTON_ID, true);
    connect(ui->chooseTitle, &StalledIssueActionTitle::actionClicked, this, &StalledIssueChooseWidget::onActionClicked); }

StalledIssueChooseWidget::~StalledIssueChooseWidget()
{
    delete ui;
}

void StalledIssueChooseWidget::updateUi(StalledIssueDataPtr data)
{
    auto fileName = data->getFileName();

    ui->chooseTitle->setTitle(data->isCloud() ? tr("Remote Copy") : tr("Local Copy"));
    ui->chooseTitle->setIsCloud(data->isCloud());
    ui->chooseTitle->showIcon();

    ui->name->setTitle(fileName);
    ui->name->setPath(data->getNativeFilePath());
    ui->name->showIcon();

    ui->path->show();
    ui->path->updateUi(data);

    if(!data->isCloud())
    {
        QFileInfo file(data->getNativeFilePath());
        if(file.exists())
        {
            ui->name->updateLastTimeModified(file.lastModified());
#ifndef Q_OS_LINUX
            ui->name->updateCreatedTime(file.created());
#endif
            ui->name->updateSize(Utilities::getSizeString(file.size()));
        }
    }

    //bool discardItem(false);

    if(mPreviousSolveState != mIsSolved)
    {
        mPreviousSolveState = mIsSolved;

        if(mIsSolved)
        {
            ui->chooseTitle->hideActionButton(BUTTON_ID);

            QIcon icon;
            if(data->isSolved())
            {
                icon.addFile(QString::fromUtf8(":/images/StalledIssues/check_default.png"));
                ui->chooseTitle->addMessage(tr("Chosen"), icon.pixmap(24,24));
            }
            else
            {
                icon.addFile(QString::fromUtf8(":/images/StalledIssues/remove_default.png"));
                ui->chooseTitle->addMessage(PlatformStrings::movedFileToBin(), icon.pixmap(16,16));
            }
        }
    }

    setDisabled(data->isSolved());

    update();

    mData = data;
}

const StalledIssueDataPtr &StalledIssueChooseWidget::data()
{
    return mData;
}

bool StalledIssueChooseWidget::eventFilter(QObject *watched, QEvent *event)
{
    return QFrame::eventFilter(watched, event);
}

void StalledIssueChooseWidget::onActionClicked(int button_id)
{
    emit chooseButtonClicked(button_id);
}

void StalledIssueChooseWidget::setDisabled(bool solved)
{
    bool isDisabled(mIsSolved && !solved);

    ui->chooseTitle->setDisabled(isDisabled);
    ui->name->setDisabled(isDisabled);

    if(isDisabled && !ui->pathContainer->graphicsEffect())
    {
        auto pathEffect = new QGraphicsOpacityEffect(this);
        pathEffect->setOpacity(0.30);
        ui->pathContainer->setGraphicsEffect(pathEffect);
    }
}

void StalledIssueChooseWidget::setIssueSolved(bool newIssueSolved)
{
    mIsSolved = newIssueSolved;
}

void CloudStalledIssueChooseWidget::updateUi(CloudStalledIssueDataPtr cloudData)
{
    StalledIssueChooseWidget::updateUi(cloudData);

    auto node = cloudData->getNode();
    if(node)
    {
        ui->name->updateUser(cloudData->getUserFirstName());

        cloudData->getFileFolderAttributes()->requestModifiedTime(this, [this](const QDateTime& time){
            ui->name->updateLastTimeModified(time);
        });

#ifndef Q_OS_LINUX
        cloudData->getFileFolderAttributes()->requestCreatedTime(this, [this](const QDateTime& time){
            ui->name->updateCreatedTime(time);
        });
#endif
        cloudData->getFileFolderAttributes()->requestSize(this, [this](qint64 size){
            ui->name->updateSize(Utilities::getSizeString(size));
        });
    }
}
