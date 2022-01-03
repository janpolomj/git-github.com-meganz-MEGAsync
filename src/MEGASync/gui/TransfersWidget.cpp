#include "TransfersWidget.h"
#include "ui_TransfersWidget.h"
#include "MegaApplication.h"
#include <QTimer>

using namespace mega;

TransfersWidget::TransfersWidget(QWidget* parent) :
    QWidget (parent),
    ui (new Ui::TransfersWidget),
    model (nullptr),
    model2 (nullptr),
    tDelegate (nullptr),
    tDelegate2 (nullptr),
    mIsPaused (false),
    app (qobject_cast<MegaApplication*>(qApp)),
    mHeaderNameState (0),
    mHeaderSizeState (0),
    mFilterMutex(new QMutex(QMutex::NonRecursive)),
    mThreadPool(ThreadPoolSingleton::getInstance())
{
    ui->setupUi(this);
}

void TransfersWidget::setupTransfers(std::shared_ptr<MegaTransferData> transferData, QTransfersModel::ModelType type)
{
    mType = type;
    model = new QActiveTransfersModel(type, transferData);

    connect(model, SIGNAL(noTransfers()), this, SLOT(noTransfers()));
    connect(model, SIGNAL(onTransferAdded()), this, SLOT(onTransferAdded()));

    configureTransferView();

    if ((type == MegaTransfer::TYPE_DOWNLOAD && transferData->getNumDownloads())
            || (type == MegaTransfer::TYPE_UPLOAD && transferData->getNumUploads()))
    {
        onTransferAdded();
    }
}

void TransfersWidget::setupTransfers()
{
    model2 = new QTransfersModel2(this);
    mProxyModel = new TransfersSortFilterProxyModel(this);
    mProxyModel->setDynamicSortFilter(false);
    mProxyModel->setSourceModel(model2);

    configureTransferView();
    model2->initModel();

//    onTransferAdded();
}

void TransfersWidget::setupFinishedTransfers(QList<MegaTransfer* > transferData, QTransfersModel::ModelType modelType)
{
    mType = modelType;
    model = new QFinishedTransfersModel(transferData, modelType);
    connect(model, SIGNAL(noTransfers()), this, SLOT(noTransfers()));
    connect(model, SIGNAL(onTransferAdded()), this, SLOT(onTransferAdded()));
    // Subscribe to MegaApplication for changes on finished transfers generated by other finished model to keep consistency
    connect(app, SIGNAL(clearAllFinishedTransfers()), model, SLOT(removeAllTransfers()));
    connect(app, SIGNAL(clearFinishedTransfer(int)),  model, SLOT(removeTransferByTag(int)));

    configureTransferView();

    if (transferData.size())
    {
        onTransferAdded();
    }
}

void TransfersWidget::refreshTransferItems()
{
    if (model) model->refreshTransfers();
}

TransfersWidget::~TransfersWidget()
{
    delete ui;
    if (tDelegate) delete tDelegate;
    if (tDelegate2) delete tDelegate2;
    if (model) delete model;
    if (model2) delete model2;
    if (mProxyModel) delete mProxyModel;
}

bool TransfersWidget::areTransfersActive()
{
    return model && model->rowCount(QModelIndex()) != 0;
}

void TransfersWidget::configureTransferView()
{
    if (!model && !model2)
    {
        return;
    }

    if (model)
    {
        tDelegate = new MegaTransferDelegate(model, this);
        ui->tvTransfers->setup(mType);
        ui->tvTransfers->setItemDelegate(tDelegate);
        ui->tvTransfers->setModel(model);
    }
    else
    {
        tDelegate2 = new MegaTransferDelegate2(mProxyModel, ui->tvTransfers, this);
        ui->tvTransfers->setup(this);
        ui->tvTransfers->setModel(mProxyModel);
        ui->tvTransfers->setItemDelegate(tDelegate2);
        onPauseStateChanged(model2->areAllPaused());

//        QObject::connect(this, &TransfersWidget::updateSearchFilter,
////                         mProxyModel,static_cast<void (TransfersSortFilterProxyModel::*)(const QRegularExpression&)>(&TransfersSortFilterProxyModel::setFilterRegularExpression),
//                         mProxyModel, &TransfersSortFilterProxyModel::setFilterFixedString,
//                Qt::QueuedConnection);
    }

    ui->tvTransfers->setDragEnabled(true);
    ui->tvTransfers->viewport()->setAcceptDrops(true);
    ui->tvTransfers->setDropIndicatorShown(true);
    ui->tvTransfers->setDragDropMode(QAbstractItemView::InternalMove);
}

void TransfersWidget::pausedTransfers(bool paused)
{
    mIsPaused = paused;
    if (model && model->rowCount(QModelIndex()) == 0)
    {
    }
    else
    {
        ui->sWidget->setCurrentWidget(ui->pTransfers);
    }
}

void TransfersWidget::disableGetLink(bool disable)
{
    ui->tvTransfers->disableGetLink(disable);
}

QTransfersModel *TransfersWidget::getModel()
{
    return model;
}

QTransfersModel2* TransfersWidget::getModel2()
{
    return model2;
}

void TransfersWidget::on_pHeaderName_clicked()
{
    Qt::SortOrder order (Qt::AscendingOrder);
    int column (-1);

    switch (mHeaderNameState)
    {
        case 0:
        {
            order = Qt::DescendingOrder;
            column = 0;
            break;
        }
        case 1:
        {
            order = Qt::AscendingOrder;
            column = 0;
            break;
        }
        case 2:
        default:
        {
            break;
        }
    }

    if (mHeaderSizeState != 0)
    {
        setHeaderState(ui->pHeaderSize, 2);
        mHeaderSizeState = 0;
        mProxyModel->sort(-1, order);
    }

    mProxyModel->setSortBy(TransfersSortFilterProxyModel::SORT_BY::NAME);
    mProxyModel->sort(column, order);

    setHeaderState(ui->pHeaderName, mHeaderNameState);
    mHeaderNameState = (mHeaderNameState + 1) % 3;
}

void TransfersWidget::on_pHeaderSize_clicked()
{
    Qt::SortOrder order (Qt::AscendingOrder);
    int column (-1);

    switch (mHeaderSizeState)
    {
        case 0:
        {
            order = Qt::DescendingOrder;
            column = 0;
            break;
        }
        case 1:
        {
            order = Qt::AscendingOrder;
            column = 0;
            break;
        }
        case 2:
        default:
        {
            break;
        }
    }

    if (mHeaderNameState != 0)
    {
        setHeaderState(ui->pHeaderName, 2);
        mHeaderNameState = 0;
        mProxyModel->sort(-1, order);
    }

    mProxyModel->setSortBy(TransfersSortFilterProxyModel::SORT_BY::TOTAL_SIZE);
    mProxyModel->sort(column, order);

    setHeaderState(ui->pHeaderSize, mHeaderSizeState);
    mHeaderSizeState = (mHeaderSizeState + 1) % 3;
}

void TransfersWidget::on_tPauseResumeAll_clicked()
{
    onPauseStateChanged(!mIsPaused);
    emit pauseResumeAllRows(mIsPaused);
}

void TransfersWidget::on_tCancelAll_clicked()
{
    emit cancelClearAllRows(true, true);
}

void TransfersWidget::onTransferAdded()
{
    ui->sWidget->setCurrentWidget(ui->pTransfers);
    ui->tvTransfers->scrollToTop();
}

void TransfersWidget::onShowCompleted(bool showCompleted)
{
    if (showCompleted)
    {
        ui->lHeaderTime->setText(tr("Time"));
        ui->tCancelAll->setToolTip(tr("Clear All"));
        ui->lHeaderSpeed->setText(tr("Avg. speed"));
    }
    else
    {
        ui->lHeaderTime->setText(tr("Time left"));
        ui->tCancelAll->setToolTip(tr("Cancel or Clear All"));
        ui->lHeaderSpeed->setText(tr("Speed"));
    }

    ui->tPauseResumeAll->setVisible(!showCompleted);
}

void TransfersWidget::onPauseStateChanged(bool pauseState)
{
    ui->tPauseResumeAll->setIcon(pauseState ?
                                     QIcon(QString::fromUtf8(":/images/lists_resume_all_ico.png"))
                                   : QIcon(QString::fromUtf8(":/images/lists_pause_all_ico.png")));
    ui->tPauseResumeAll->setToolTip(pauseState ?
                                        tr("Resume visible transfers")
                                      : tr("Pause visible transfers"));
    mIsPaused = pauseState;
}

void TransfersWidget::textFilterChanged(const QString& pattern)
{
//    QtConcurrent::run([=]
    mThreadPool->push([=]
    {
        QMutexLocker lock (mFilterMutex);
        std::unique_ptr<mega::MegaApiLock> apiLock (app->getMegaApi()->getMegaApiLock(true));
        mProxyModel->setFilterFixedString(pattern);
    });

    ui->tvTransfers->scrollToTop();
}

void TransfersWidget::fileTypeFilterChanged(const TransferData::FileTypes fileTypes)
{
    mThreadPool->push([=]
    {
        mProxyModel->setFileTypes(fileTypes);
    });
}

void TransfersWidget::transferStateFilterChanged(const TransferData::TransferStates transferStates)
{
    mThreadPool->push([=]
    {
        mProxyModel->setTransferStates(transferStates);
    });
}

void TransfersWidget::transferTypeFilterChanged(const TransferData::TransferTypes transferTypes)
{
    mThreadPool->push([=]
    {
        mProxyModel->setTransferTypes(transferTypes);
    });
}

void TransfersWidget::transferFilterReset()
{
    mFilterMutex->lock();
    mThreadPool->push([=]
    {
        mProxyModel->resetAllFilters();
        mFilterMutex->unlock();
    });
}

void TransfersWidget::transferFilterApply(bool invalidate)
{
    if (!mProxyModel->dynamicSortFilter())
    {
        std::unique_ptr<mega::MegaApiLock> apiLock (app->getMegaApi()->getMegaApiLock(true));
        mProxyModel->applyFilters(false);
        mProxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
        mProxyModel->setDynamicSortFilter(true);
    }
    else
    {
        mThreadPool->push([=]
        {
            std::unique_ptr<mega::MegaApiLock> apiLock (app->getMegaApi()->getMegaApiLock(true));
            mProxyModel->resetNumberOfItems();
            mProxyModel->applyFilters(invalidate);
        });
    }
    ui->tvTransfers->scrollToTop();
}

int TransfersWidget::rowCount()
{
    return ui->tvTransfers->model()->rowCount();
}

void TransfersWidget::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange)
    {
        ui->retranslateUi(this);
    }
    QWidget::changeEvent(event);
}

void TransfersWidget::setHeaderState(QPushButton* header, int state)
{
    QIcon icon;
    switch (state)
    {
        case 0:
        {
            icon = Utilities::getCachedPixmap(QLatin1Literal(":/images/sort_descending.png"));
            break;
        }
        case 1:
        {
            icon = Utilities::getCachedPixmap(QLatin1Literal(":/images/sort_ascending.png"));
            break;
        }
        case 2:
        default:
        {
            icon = QIcon();
            break;
        }
    }
    header->setIcon(icon);
}
