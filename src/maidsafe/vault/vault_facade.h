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

} // namespace vault

} // namespace maidsafe

#endif // MAIDSAFE_VAULT_VAULT_FACADE_H
