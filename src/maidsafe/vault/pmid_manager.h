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

#ifndef MAIDSAFE_VAULT_PMID_MANAGER_H_
#define MAIDSAFE_VAULT_PMID_MANAGER_H_

struct PmidManagerAccount {
  PmidManagerAccount() = default;
  PmidManagerAccount(const PmidManagerAccount&) = default;
  PmidManagerAccount(PmidManagerAccount&& other) MAIDSAFE_NOEXCEPT
      : name(std::move(other.name)),
        stored(std::move(other.stored)),
        available(std::move(other.available)) {}
  ~PmidManagerAccount() = default;
  PmidManagerAccount& operator=(const PmidManagerAccount&) = default;
  PmidManagerAccount& operator=(PmidManagerAccount&& rhs) MAIDSAFE_NOEXCEPT {
    name = std::move(rhs.name);
    stored = std::move(rhs.stored);
    available = std::move(rhs.available);
    return *this;
  }

  bool operator==(const PmidManagerAccount& other) const {
    return std::tie(name, stored, available) == std::tie(other.name, other.stored, other.available);
  }
  bool operator!=(const PmidManagerAccount& other) const { return !operator==(other); }
  bool operator<(const PmidManagerAccount& other) const {
    return std::tie(name, stored, available) < std::tie(other.name, other.stored, other.available);
  }
  bool operator>(const PmidManagerAccount& other) { return operator<(other); }
  bool operator<=(const PmidManagerAccount& other) { return !operator>(other); }
  bool operator>=(const PmidManagerAccount& other) { return !operator<(other); }

  Address name;
  uint64_t stored;
  uint64_t available;
};

template <typename FacadeType>
class PmidManager {
 public:
  using account_type = MaidManagerAccount;
  template <typename T>
  void HandleGet(SourceAddress /* from */, Identity /* data_name */) {}
  template <typename T>
  void HandlePut(SourceAddress /* from */, Identity /* data_name */, DataType /* data */) {}
  void HandleChurn(CloseGroupDifference) {
    // send all account info to the group of each key and delete it - wait for refreshed accounts
  }
};

#endif
