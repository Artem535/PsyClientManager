#include "qclient_model.h"

#include <QString>

Q_LOGGING_CATEGORY(logClientModel, "pcm.ClientModel")

namespace {

class ClientLoaderWorker final : public QObject {
  Q_OBJECT

public:
  explicit ClientLoaderWorker(std::shared_ptr<pcm::database::Database> db)
      : m_db(std::move(db)) {}

public slots:
  void loadClients() {
    if (!m_db) {
      emit loadFailed(QStringLiteral(": DATABASE_NOT_INITIALIZED"));
      return;
    }

    std::vector<DuckClient> clients;
    const auto loaded = m_db->get_clients();
    clients.reserve(loaded.size());
    for (const auto &client : loaded) {
      if (!client) {
        continue;
      }
      clients.push_back(*client);
    }

    emit clientsLoaded(clients);
  }

signals:
  void clientsLoaded(const std::vector<DuckClient> &clients);
  void loadFailed(const QString &message);

private:
  std::shared_ptr<pcm::database::Database> m_db;
};

} // namespace

QClientModel::QClientModel(std::shared_ptr<pcm::database::Database> db,
                           QObject *parent)
    : QAbstractListModel(parent), m_db{db} {
  qRegisterMetaType<std::vector<DuckClient>>("std::vector<DuckClient>");

  m_loaderThread = new QThread(this);
  const auto worker = new ClientLoaderWorker(m_db);
  worker->moveToThread(m_loaderThread);

  connect(this, &QClientModel::requestLoad, worker,
          &ClientLoaderWorker::loadClients, Qt::QueuedConnection);

  connect(worker, &ClientLoaderWorker::clientsLoaded, this,
          [this](const std::vector<DuckClient> &clients) {
            beginResetModel();
            m_clients = clients;
            m_client_ids.clear();
            m_client_ids.reserve(m_clients.size());
            for (const auto &client : m_clients) {
              m_client_ids.push_back(client.id);
            }
            endResetModel();

            setLoading(false);
          });

  connect(worker, &ClientLoaderWorker::loadFailed, this,
          [this](const QString &message) {
            qCWarning(logClientModel) << "Client load failed:" << message;
            setLoading(false);
            emit loadFailed(message);
          });

  connect(m_loaderThread, &QThread::finished, worker, &QObject::deleteLater);
  m_loaderThread->start();
  reload();
}

QClientModel::~QClientModel() {
  if (m_loaderThread && m_loaderThread->isRunning()) {
    m_loaderThread->quit();
    m_loaderThread->wait();
  }
}

void QClientModel::reload() {
  if (!m_loaderThread || !m_loaderThread->isRunning() || m_isLoading) {
    return;
  }

  setLoading(true);
  emit requestLoad();
}

bool QClientModel::isLoading() const { return m_isLoading; }

int QClientModel::rowCount(const QModelIndex &parent) const {
  if (parent.isValid()) {
    return 0;
  }

  return static_cast<int>(m_clients.size());
}

QVariant QClientModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid()) {
    qCWarning(logClientModel) << "ClientModel::data: index is invalid";
    return QVariant();
  }

  if (index.row() >= static_cast<int>(m_clients.size())) {
    qCWarning(logClientModel) << "ClientModel::data: index is out of range";
    return QVariant();
  }

  const auto &client = m_clients.at(index.row());

  QVariant result;
  switch (role) {
  case QClientModel::ClientRoles::Id:
    result = QVariant::fromValue(client.id);
    break;
  case Qt::DisplayRole: {
    const auto name = client.name != std::nullopt
                          ? QString::fromStdString(client.name.value())
                          : tr(": VALUE_UNDEFINED");
    const auto last_name = client.last_name != std::nullopt
                               ? QString::fromStdString(client.last_name.value())
                               : tr(": VALUE_UNDEFINED");
    result = QString("%1 %2").arg(name, last_name);
    break;
  }
  case QClientModel::ClientRoles::Full_object:
    result = QVariant::fromValue(client);
    break;
  default:
    qCWarning(logClientModel)
        << "ClientModel::data: wrong role, selected role = " << role;
    break;
  }

  return result;
}

bool QClientModel::setData(const QModelIndex &index, const QVariant &value,
                           int role) {
  if (!index.isValid()) {
    qCWarning(logClientModel) << "ClientModel::updateClient: index is invalid";
    return false;
  }

  if (index.row() >= static_cast<int>(m_clients.size())) {
    qCWarning(logClientModel)
        << "ClientModel::updateClient: index is out of range";
    return false;
  }

  if (role != Qt::EditRole) {
    qCWarning(logClientModel) << "ClientModel::updateClient: wrong role";
    return false;
  }

  // const auto old_client = *m_db->get_client(m_client_ids.at(index.row()));
  const auto client = value.value<DuckClient>();

  m_db->add_client(client);
  m_clients.at(index.row()) = client;
  m_client_ids.at(index.row()) = client.id;
  emit dataChanged(index, index,
                   {QClientModel::ClientRoles::Id,
                    QClientModel::ClientRoles::Full_object, Qt::DisplayRole});
  return true;
}

void QClientModel::add_new_client(const DuckClient &client) {
  int new_row_index = static_cast<int>(m_clients.size());
  const auto id = m_db->add_client(client);
  auto new_client = client;
  new_client.id = id;

  beginInsertRows(QModelIndex(), new_row_index, new_row_index);
  m_clients.push_back(new_client);
  m_client_ids.push_back(id);
  endInsertRows();
}

void QClientModel::setLoading(const bool isLoading) {
  if (m_isLoading == isLoading) {
    return;
  }

  m_isLoading = isLoading;
  emit loadingStateChanged(m_isLoading);
}

#include "qclient_model.moc"
