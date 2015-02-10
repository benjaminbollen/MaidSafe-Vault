/*  Copyright 2015 MaidSafe.net limited

    This MaidSafe Software is licensed to you under (1) the MaidSafe.net Commercial License,
    version 1.0 or later, or (2) The General Public License (GPL), version 3, depending on which
    licence you accepted on initial access to the Software (the "Licences").

    By contributing code to the MaidSafe Software, or to this project generally, you agree to be
    bound by the terms of the MaidSafe Contributor Agreement, version 1.0, found in the root
    directory of this project at LICENSE, COPYING and CONTRIBUTOR respectively and also
    available at: http://www.maidsafe.net/licenses

    Unless required by applicable law or agreed to in writing, the MaidSafe Software distributed
    under the GPL Licence is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS
    OF ANY KIND, either express or implied.

    See the Licences for the specific language governing permissions and limitations relating to
    use of the MaidSafe Software.                                                                 */

#include "maidsafe/routing/routing_node.h"
#include "maidsafe/vault/data_manager.h"
#include "maidsafe/vault/maid_manager.h"
#include "maidsafe/vault/pmid_manager.h"
#include "maidsafe/vault/pmid_node.h"


#ifndef MAIDSAFE_VAULT_VAULT_FACADE_H_
#define MAIDSAFE_VAULT_VAULT_FACADE_H_

namespace maidsafe {

namespace vault {

class VaultFacade : public MaidManager<VaultFacade>,
                    public DataManager<VaultFacade>,
                    public PmidManager<VaultFacade>,
                    public PmidNode<VaultFacade>,
                    public maidsafe::routing::RoutingNode<VaultFacade> {

 public:
  VaultFacade() = default;
  ~VaultFacade() = default;

  enum class FunctorType { FunctionOne, FunctionTwo };
  enum class DataType { ImmutableData, MutableData, End };

};

template <typename DataType>
void HandleGet(SourceAddress from, Authority /* from_authority */, Authority authority,
               DataType data_type, Identity data_name) {
  switch (authority) {
    case Authority::nae_manager:
      if (data_type == DataType::ImmutableData)
        DataManager::template HandleGet<ImmutableData>(from, data_name);
      else if (data_type == DataType::MutableData)
        DataManager::template HandleGet<MutableData>(from, data_name);
      break;
    case Authority::node_manager:
      if (data_type == DataType::ImmutableData)
        PmidManager::template HandleGet<ImmutableData>(from, data_name);
      else if (data_type == DataType::MutableData)
        PmidManager::template HandleGet<MutableData>(from, data_name);
      break;
    case Authority::managed_node:
      if (data_type == DataType::ImmutableData)
        PmidNode::template HandleGet<ImmutableData>(from, data_name);
      else if (data_type == DataType::MutableData)
        PmidNode::template HandleGet<MutableData>(from, data_name);
      break;
    default:
      break;
  }
}
template <typename DataType>
void HandlePut(SourceAddress from, Authority from_authority, Authority authority,
               DataType data_type) {
  switch (authority) {
    case Authority::nae_manager:
      if (from_authority != Authority::client_manager)
        break;
      if (data_type == DataType::ImmutableData)
        DataManager::template HandlePut<ImmutableData>(from, data_type);
      else if (data_type == DataType::MutableData)
        DataManager::template HandlePut<MutableData>(from, data_type);
      break;
    case Authority::node_manager:
      if (data_type == DataType::ImmutableData)
        PmidManager::template HandlePut<ImmutableData>(from, data_type);
      else if (data_type == DataType::MutableData)
        PmidManager::template HandlePut<MutableData>(from, data_type);
      break;
    case Authority::managed_node:
      if (data_type == DataType::ImmutableData)
        PmidNode::template HandlePut<ImmutableData>(from, data_type);
      else if (data_type == DataType::MutableData)
        PmidNode::template HandlePut<MutableData>(from, data_type);
      break;
    default:
      break;
  }
}

// default no post allowed unless implemented in upper layers
bool HandlePost(const SerialisedMessage&) { return false; }
// not in local cache do upper layers have it (called when we are in target group)
// template <typename DataType>
boost::expected<SerialisedMessage, maidsafe_error> HandleGet(Address) {
  return boost::make_unexpected(MakeError(CommonErrors::no_such_element));
}
// default put is allowed unless prevented by upper layers
bool HandlePut(Address, SerialisedMessage) { return true; }
// if the implementation allows any put of data in unauthenticated mode
bool HandleUnauthenticatedPut(Address, SerialisedMessage) { return true; }
void HandleChurn(CloseGroupDifference diff) {
  MaidManager::HandleChurn(diff);
  DataManager::HandleChurn(diff);
  PmidManager::HandleChurn(diff);
}

private:
// RoutingNode routing_node_;
};

} // namespace vault

} // namespace maidsafe

#endif // MAIDSAFE_VAULT_VAULT_FACADE_H
