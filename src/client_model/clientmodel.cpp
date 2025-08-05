#include "clientmodel.h"
#include <qabstractitemmodel.h>
#include <qloggingcategory.h>
#include <qnamespace.h>

Q_LOGGING_CATEGORY(logClientModel, "pcm.ClientModel")

ClientModel::ClientModel(std::shared_ptr<pcm::database::Database> db,
                         QObject *parent)
    : QAbstractListModel(parent), m_db{db} {

  m_client_ids = m_db->get_client_ids();
}

int ClientModel::rowCount(const QModelIndex &parent) const {
  return static_cast<int>(m_client_ids.size());
}

QVariant ClientModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid()) {
    qCWarning(logClientModel) << "ClientModel::data: index is invalid";
    return QVariant();
  }

  if (index.row() >= m_client_ids.size()) {
    qCWarning(logClientModel) << "ClientModel::data: index is out of range";
    return QVariant();
  }

  const obx_id client_id{m_client_ids.at(index.row())};
  const auto client = m_db->get_client(client_id);

  QVariant result;
  switch (role) {
  case ClientModel::ClientRoles::Id:
    result = QVariant::fromValue(client_id);
    break;
  case Qt::DisplayRole:
    result = QString::fromStdString(client->name + " " + client->last_name);
    break;
  case ClientModel::ClientRoles::Full_object:
    result = QVariant::fromValue(*client);
    break;
  default:
    qCWarning(logClientModel) << "ClientModel::data: wrong role, selected role = " << role;
    break;
  }

  return result;
}

bool ClientModel::setData(const QModelIndex &index, const QVariant &value,
                          int role) {
  if (!index.isValid()) {
    qCWarning(logClientModel) << "ClientModel::updateClient: index is invalid";
    return false;
  }

  if (index.row() >= m_client_ids.size()) {
    qCWarning(logClientModel) << "ClientModel::updateClient: index is out of range";
    return false;
  }

  if (role != Qt::EditRole) {
    qCWarning(logClientModel) << "ClientModel::updateClient: wrong role";
    return false;
  }

  
  // const auto old_client = *m_db->get_client(m_client_ids.at(index.row()));
  const auto client = value.value<Client>();

  m_db->add_client(client);
  emit dataChanged(index, index, {role});
  return true;
}

void ClientModel::add_new_client(const Client &client) {
  int new_row_index = m_client_ids.size();
  const auto id = m_db->add_client(client);
  
  beginInsertRows(QModelIndex(), new_row_index, new_row_index);
  m_client_ids.push_back(id);
  endInsertRows();
}