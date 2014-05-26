/*  Copyright 2014 MaidSafe.net limited

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

#include "maidsafe/common/test.h"

#include "maidsafe/passport/passport.h"
#include "maidsafe/passport/detail/fob.h"

#include "maidsafe/routing/routing_api.h"

#include "maidsafe/nfs/client/maid_node_nfs.h"
#include "maidsafe/nfs/client/data_getter.h"

#include "maidsafe/vault_manager/client_interface.h"

#include "maidsafe/vault/tests/vault_network.h"

namespace maidsafe {

namespace vault {

namespace test {

TEST(NetworkTest, FUNC_PutGet) {
  passport::detail::AnmaidToPmid keys;
  vault_manager::ClientInterface client_interface(keys.maid);
  auto future(client_interface.GetBootstrapContacts());
  auto bootstraps(future.get());
  std::vector<passport::PublicPmid> public_pmids;
  Client client(keys, bootstraps, public_pmids, false);

  ImmutableData data(NonEmptyString(RandomString(1024)));
  EXPECT_NO_THROW(client.nfs_->Put(data));
  EXPECT_NO_THROW(client.nfs_->Get(data.name()));
}

}  // namespace test

}  // namespace vault

}  // namespace maidsafe
