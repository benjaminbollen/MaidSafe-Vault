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

#include "maidsafe/vault/maid_manager.h"

#ifndef MAIDSAFE_VAULT_DATA_MANAGER_H_
#define MAIDSAFE_VAULT_DATA_MANAGER_H_

namespace maidsafe {

namespace vault {

template <typename DataManagerType, typename VersionManagerType>
class NaeManager {
 public:
  using account_type = MaidManagerAccount;
  template <typename T>
  void HandleGet(SourceAddress from, Identity data_name);
  template <typename T>
  void HandlePut(SourceAddress /* from */, Identity /* data_name */, DataType /* data */) {}
};  // becomes a dispatcher as its now multiple personas

template <typename FacadeType>
class VersionManager {};

template <typename FacadeType>
class DataManager {
 public:
  using account_type = MaidManagerAccount;
  template <typename T>
  void HandleGet(SourceAddress from, Identity data_name) {
    // FIXME(dirvine) We need to pass along the full source address to retain the ReplyTo field
    // :01/02/2015
    static_cast<Child*>(this)
        ->template Get<T>(data_name, std::get<0>(from), [](asio::error_code error) {
          if (error)
            LOG(kWarning) << "could not send from datamanager ";
        });
  }
  template <typename T>
  void HandlePut(SourceAddress /* from */, Identity /* data_name */, DataType /* data */) {}
  void HandleChurn(CloseGroupDifference) {
    // send all account info to the group of each key and delete it - wait for refreshed accounts
  }
};

} // namespace vault

} // namespace maidsafe

#endif
