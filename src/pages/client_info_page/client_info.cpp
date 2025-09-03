#include "client_info.h"
#include "ui/pages/ui_clientinfo.h"

Q_LOGGING_CATEGORY(logClientInfo, "pcm.ClientInfo")

ClientInfo::ClientInfo(std::shared_ptr<QClientModel> model, QWidget *parent)
    : QWidget(parent), mUi(std::make_unique<Ui::ClientInfo>()),
      mClientModel(std::move(model)) {
  mUi->setupUi(this);

  mUi->listView->setModel(mClientModel.get());
  mUi->listView->setViewMode(QListView::ListMode);

  const auto delegate = new QClientDelegate(mUi->listView);
  connectSignals(delegate);

  mUi->listView->setItemDelegate(delegate);
}

ClientInfo::~ClientInfo() = default;

void ClientInfo::connectSignals(const QClientDelegate *delegate) {
  connect(
      delegate, &QClientDelegate::displayButtonClicked, [&](const auto index) {
        qCDebug(logClientInfo) << "Display button clicked for index: " << index;

        const QVariant clientVar =
            index.data(QClientModel::ClientRoles::Full_object);
        const auto client = clientVar.value<ObxClient>();

        emit displayButtonClicked(client);
      });

  connect(
      delegate, &QClientDelegate::removeButtonClicked, [&](const auto index) {
        qCDebug(logClientInfo) << "Remove button clicked for index: " << index;
      });
}