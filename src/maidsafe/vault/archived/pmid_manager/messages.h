/*  Copyright 2012 MaidSafe.net limited

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

#ifndef MAIDSAFE_VAULT_PMID_MANAGER_MESSAGES_H_
#define MAIDSAFE_VAULT_PMID_MANAGER_MESSAGES_H_

#include <cstdint>
#include <string>

#include "maidsafe/common/error.h"

namespace maidsafe {
namespace vault {

class PmidAccountResponse {
 public:
  explicit PmidAccountResponse(const std::string& serialised_pmid_accounts);
  PmidAccountResponse(const PmidAccountResponse& other);
  PmidAccountResponse(PmidAccountResponse&& other);
  PmidAccountResponse& operator=(PmidAccountResponse other);

  std::string Serialise() const;

  friend void swap(PmidAccountResponse& lhs, PmidAccountResponse& rhs) MAIDSAFE_NOEXCEPT;
  friend bool operator==(const PmidAccountResponse& lhs,
                         const PmidAccountResponse& rhs) MAIDSAFE_NOEXCEPT;

 private:
  PmidAccountResponse();
  std::string serialised_pmid_accounts_;
};

bool operator==(const PmidAccountResponse& lhs, const PmidAccountResponse& rhs) MAIDSAFE_NOEXCEPT;

}  // namespace vault

}  // namespace maidsafe

#endif  // MAIDSAFE_VAULT_PMID_MANAGER_MESSAGES_H_