#include "client_info.h"
#include "ui/pages/ui_clientinfo.h"

Q_LOGGING_CATEGORY(logClientInfo, "pcm.ClientInfo")

ClientInfo::ClientInfo(std::shared_ptr<QClientModel> model, QWidget *parent)
    : QWidget(parent), mUi(std::make_unique<Ui::ClientInfo>()),
      mClientModel(std::move(model)) {
  mUi->setupUi(this);

  mLoadingLabel = new QLabel(tr(": CLIENTS_LOADING"), this);
  mLoadingLabel->setAlignment(Qt::AlignCenter);
  mUi->verticalLayout->insertWidget(0, mLoadingLabel);

  mUi->listView->setModel(mClientModel.get());
  mUi->listView->setViewMode(QListView::ListMode);

  const auto delegate = new QClientDelegate(mUi->listView);
  connectSignals(delegate);

  mUi->listView->setItemDelegate(delegate);

  connect(mClientModel.get(), &QClientModel::loadingStateChanged, this,
          [this](const bool isLoading) {
            if (isLoading) {
              mLoadingLabel->setText(tr(": CLIENTS_LOADING"));
            }
            mLoadingLabel->setVisible(isLoading);
            mUi->listView->setEnabled(!isLoading);
          });

  connect(mClientModel.get(), &QClientModel::loadFailed, this,
          [this](const QString &message) {
            mLoadingLabel->setText(tr(": CLIENTS_LOADING_ERROR %1")
                                       .arg(message));
            mLoadingLabel->setVisible(true);
          });

  mLoadingLabel->setVisible(mClientModel->isLoading());
  mUi->listView->setEnabled(!mClientModel->isLoading());
}

ClientInfo::~ClientInfo() = default;

void ClientInfo::connectSignals(const QClientDelegate *delegate) {
  connect(
      delegate, &QClientDelegate::displayButtonClicked, [&](const auto index) {
        qCDebug(logClientInfo) << "Display button clicked for index: " << index;

        const QVariant clientVar =
            index.data(QClientModel::ClientRoles::Full_object);
        const auto client = clientVar.value<DuckClient>();

        emit displayButtonClicked(client);
      });

  connect(
      delegate, &QClientDelegate::removeButtonClicked, [&](const auto index) {
        qCDebug(logClientInfo) << "Remove button clicked for index: " << index;
      });
}
