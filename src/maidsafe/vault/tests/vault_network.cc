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

#include "maidsafe/vault/tests/vault_network.h"

#ifndef MAIDSAFE_WIN32
#include <ulimit.h>
#endif

#include <algorithm>
#include <string>

#include "maidsafe/common/test.h"
#include "maidsafe/common/log.h"
#include "maidsafe/passport/detail/fob.h"

#include "maidsafe/vault_manager/vault_config.h"

#include "maidsafe/vault/tests/tests_utils.h"

namespace maidsafe {

namespace vault {

namespace test {

PublicKeyGetter Client::public_key_getter_;

void PublicKeyGetter::operator()(const NodeId& node_id,
                                 const routing::GivePublicKeyFunctor& give_key,
                                 const std::vector<passport::PublicPmid>& public_pmids,
                                 nfs_client::DataGetter& data_getter) {
  passport::PublicPmid::Name name(Identity(node_id.string()));
  std::lock_guard<std::mutex> lock(mutex_);
  if (!public_pmids.empty()) {
    LOG(kVerbose) << "Local list contains " << public_pmids.size() << " pmids";
    try {
      auto itr(std::find_if(
               std::begin(public_pmids), std::end(public_pmids),
               [&name](const passport::PublicPmid& pmid) {
                 return pmid.name() == name;
               }));
      if (itr == std::end(public_pmids)) {
        LOG(kWarning) << "PublicPmid not found locally" << HexSubstr(name.value) << " from local";
      } else {
        LOG(kVerbose) << "Success in retreiving PublicPmid locally" << HexSubstr(name.value);
        give_key((*itr).public_key());
        return;
      }
    } catch (...) {
      LOG(kError) << "Failed to get PublicPmid " << HexSubstr(name.value) << " locally";
    }
  } else {
    auto future(data_getter.Get(name));
    try {
      give_key(future.get().public_key());
    }
    catch (const std::exception& error) {
      LOG(kError) << "Failure to retrieve " << DebugId(node_id) << " : "
                  << boost::diagnostic_information(error);
    }
  }
}

VaultNetwork::VaultNetwork()
    : asio_service_(2), mutex_(), bootstrap_condition_(), network_up_condition_(),
      bootstrap_done_(false), network_up_(false), vaults_(), clients_(), bootstrap_contacts_(),
      public_pmids_(), key_chains_(kNetworkSize + 2),
      vault_dir_(fs::unique_path((fs::temp_directory_path()))), network_size_(kNetworkSize)
#ifndef MAIDSAFE_WIN32
      , kUlimitFileSize([]()->long {  // NOLINT
                          long current_size(ulimit(UL_GETFSIZE));  // NOLINT
                          if (current_size < kLimitsFiles)
                            ulimit(UL_SETFSIZE, kLimitsFiles);
                          return current_size;
                        }())
#endif
{
  for (const auto& key : key_chains_.keys)
    public_pmids_.push_back(passport::PublicPmid(key.pmid));
}

void VaultNetwork::Bootstrap() {
  LOG(kVerbose) << "Creating zero state routing network...";
  routing::NodeInfo node_info1(MakeNodeInfo(key_chains_.keys[0].pmid)),
                    node_info2(MakeNodeInfo(key_chains_.keys[1].pmid));
  routing::Functors functors1, functors2;
  functors1.request_public_key = functors2.request_public_key  = [&, this](
      NodeId node_id, const routing::GivePublicKeyFunctor& give_key) {
    std::lock_guard<std::mutex> lock(this->mutex_);
    auto itr(std::find_if(std::begin(this->public_pmids_), std::end(this->public_pmids_),
                          [node_id](const passport::PublicPmid& pmid) {
                            return pmid.name()->string() == node_id.string();
                          }));
    assert(itr != std::end(this->public_pmids_));
    give_key(itr->public_key());
  };

  functors1.typed_message_and_caching.group_to_group.message_received =
      functors2.typed_message_and_caching.group_to_group.message_received =
      [&](const routing::GroupToGroupMessage&) {};  // NOLINT
  functors1.typed_message_and_caching.group_to_single.message_received =
      functors2.typed_message_and_caching.group_to_single.message_received =
      [&](const routing::GroupToSingleMessage&) {};  // NOLINT
  functors1.typed_message_and_caching.single_to_group.message_received =
      functors2.typed_message_and_caching.single_to_group.message_received =
      [&](const routing::SingleToGroupMessage&) {};  // NOLINT
  functors1.typed_message_and_caching.single_to_single.message_received =
      functors2.typed_message_and_caching.single_to_single.message_received =
      [&](const routing::SingleToSingleMessage&) {};  // NOLINT
  functors1.typed_message_and_caching.single_to_group_relay.message_received =
      functors2.typed_message_and_caching.single_to_group_relay.message_received =
      [&](const routing::SingleToGroupRelayMessage&) {};  // NOLINT

  bootstrap_contacts_.push_back(boost::asio::ip::udp::endpoint(GetLocalIp(),
                       maidsafe::test::GetRandomPort()));
  bootstrap_contacts_.push_back(boost::asio::ip::udp::endpoint(GetLocalIp(),
                       maidsafe::test::GetRandomPort()));
  routing::Routing routing1(key_chains_.keys[0].pmid), routing2(key_chains_.keys[1].pmid);

  auto a1 = std::async(std::launch::async, [&, this] {
    return routing1.ZeroStateJoin(functors1, bootstrap_contacts_[0], bootstrap_contacts_[1],
        node_info2);
  });
  auto a2 = std::async(std::launch::async, [&, this] {
    return routing2.ZeroStateJoin(functors2, bootstrap_contacts_[1], bootstrap_contacts_[0],
            node_info1);
  });
  if (a1.get() != 0 || a2.get() != 0) {
    LOG(kError) << "SetupNetwork - Could not start bootstrap nodes.";
    BOOST_THROW_EXCEPTION(MakeError(RoutingErrors::not_connected));
  }
  bootstrap_done_ = true;
  bootstrap_condition_.notify_one();
  // just wait till process receives termination signal
  LOG(kInfo) << "Bootstrap nodes are running"  << "Endpoints: " << bootstrap_contacts_[0]
             << " and " << bootstrap_contacts_[1];
  {
    std::mutex mutex;
    std::unique_lock<std::mutex> lock(mutex);
    assert(network_up_condition_.wait_for(lock, std::chrono::seconds(300),
                                          [this]() {
                                            return this->network_up_;
                                          }));
  }
  {
    std::lock_guard<std::mutex> lock(mutex_);
    LOG(kInfo) << "clean bootstrap endpoints and all boot from 5483";
    bootstrap_contacts_.clear();
    bootstrap_contacts_.push_back(boost::asio::ip::udp::endpoint(GetLocalIp(), 5483));
  }
}

void VaultNetwork::SetUp() {
  auto bootstrap = std::async(std::launch::async, [&, this] {
    this->Bootstrap();
  });
  std::mutex mutex;
  std::unique_lock<std::mutex> lock(mutex);
  assert(this->bootstrap_condition_.wait_for(lock, std::chrono::seconds(5),
                                             [this]() {
                                               return this->bootstrap_done_;
                                             }));
  LOG(kVerbose) << "Starting vaults...";
  std::vector<std::future<bool>> futures;
  for (size_t index(2); index < 4; ++index) {
    futures.push_back(std::async(std::launch::async,
                      [index, this] {
                        LOG(kInfo) << "starting vault " << index;
                        return this->Create(key_chains_.keys.at(index).pmid);
                      }));
    Sleep(std::chrono::seconds(3));
  }

  this->network_up_ = true;
  this->network_up_condition_.notify_one();
  bootstrap.get();
  Sleep(std::chrono::seconds(3));

  for (size_t index(4); index < (network_size_ + 2); ++index) {
    futures.push_back(std::async(std::launch::async,
                      [index, this] {
                        LOG(kInfo) << "starting vault " << index;
                        return this->Create(key_chains_.keys.at(index).pmid);
                      }));
    Sleep(std::chrono::seconds(3));
  }

  for (size_t index(0); index < network_size_; ++index) {
    LOG(kInfo) << "checking vault " << index;
    try {
      EXPECT_TRUE(futures[index].get()) << " failing with index "<< index;
    }
    catch (const std::exception& e) {
      LOG(kError) << "Exception getting future from creating vault " << index << ": "
                  << boost::diagnostic_information(e);
    }
    LOG(kVerbose) << index << " returns.";
  }
  Sleep(std::chrono::seconds(5));
  LOG(kVerbose) << "Network is up...";
}

void VaultNetwork::TearDown() {
  LOG(kInfo) << "VaultNetwork TearDown";
  for (auto& client : clients_)
    client->Stop();
  Sleep(std::chrono::seconds(1));
  for (auto& client : clients_)
    client.reset();
  Sleep(std::chrono::seconds(1));
  clients_.clear();
  for (auto& vault : vaults_)
    vault->Stop();
  Sleep(std::chrono::seconds(1));
  for (auto& vault : vaults_)
    vault.reset();
//   Sleep(std::chrono::seconds(3));
  vaults_.clear();

//   while (vaults_.size() > 0) {
//     vaults_.erase(vaults_.begin());
//     Sleep(std::chrono::milliseconds(200));
//   }
#ifndef MAIDSAFE_WIN32
  ulimit(UL_SETFSIZE, kUlimitFileSize);
#endif
}

bool VaultNetwork::Create(const passport::detail::Fob<passport::detail::PmidTag>& pmid) {
  std::string path_str("vault" + RandomAlphaNumericString(6));
  auto vault_root_dir(vault_dir_ / path_str);
  fs::create_directory(vault_root_dir);
  try {
    LOG(kVerbose) << "vault joining: " << vaults_.size() << " id: "
                  << DebugId(NodeId(pmid.name()->string()));
    vault_manager::VaultConfig vault_config(pmid, vault_root_dir, DiskUsage(1000000000),
                                            bootstrap_contacts_);
    vault_config.test_config.public_pmid_list = public_pmids_;
    vaults_.emplace_back(new Vault(vault_config, [](const boost::asio::ip::udp::endpoint&) {}));
    LOG(kSuccess) << "vault joined: " << vaults_.size() << " id: "
                  << DebugId(NodeId(pmid.name()->string()));
    return true;
  }
  catch (const std::exception& ex) {
    LOG(kError) << "vault failed to join: " << vaults_.size() << " because: "
                << boost::diagnostic_information(ex);
    return false;
  }
}

bool VaultNetwork::Add() {
  auto node_keys(key_chains_.Add());
  public_pmids_.push_back(passport::PublicPmid(node_keys.pmid));
  for (size_t index(0); index < vaults_.size(); ++index) {
    vaults_[index]->AddPublicPmid(passport::PublicPmid(node_keys.pmid));
  }
  auto future(std::async(std::launch::async,
                         [this] {
                           return this->Create(this->key_chains_.keys.back().pmid);
                         }));
  Sleep(std::chrono::seconds(std::min(vaults_.size() / 10 + 1, size_t(3))));
  try {
    return future.get();
  }
  catch (const std::exception& e) {
    LOG(kError) << "Exception getting future from creating vault "
                << boost::diagnostic_information(e);
    return false;
  }
}

bool VaultNetwork::AddClient(bool register_pmid) {
  passport::detail::AnmaidToPmid node_keys;
  if (register_pmid) {
    EXPECT_TRUE(Add());
    node_keys = *key_chains_.keys.rbegin();
  } else {
    node_keys = key_chains_.Add();
    public_pmids_.push_back(passport::PublicPmid(node_keys.pmid));
  }
  try {
    LOG(kVerbose) << "Client joining: " << clients_.size();
    clients_.emplace_back(new Client(node_keys, bootstrap_contacts_, public_pmids_, register_pmid));
    LOG(kVerbose) << "Client joined: " << clients_.size();
    return true;
  }
  catch (const std::exception& error) {
    LOG(kVerbose) << "Client failed to join: " << clients_.size() << " "
                  << boost::diagnostic_information(error);
    return false;
  }
}

Client::Client(const passport::detail::AnmaidToPmid& keys,
               const std::vector<UdpEndpoint>& endpoints,
               std::vector<passport::PublicPmid>& public_pmids,
               bool register_pmid_for_client)
    : asio_service_(2), functors_(), routing_(keys.maid), nfs_(),
      data_getter_(asio_service_, routing_),
      public_pmids_(public_pmids) {
  nfs_.reset(new nfs_client::MaidNodeNfs(
      asio_service_, routing_, passport::PublicPmid::Name(Identity(keys.pmid.name().value))));
  {
    auto future(RoutingJoin(endpoints));
    auto status(future.wait_for(std::chrono::seconds(10)));
    if (status == std::future_status::timeout || !future.get()) {
      LOG(kError) << "can't join routing network";
      BOOST_THROW_EXCEPTION(MakeError(RoutingErrors::not_connected));
    }
    LOG(kSuccess) << "Client node joined routing network";
  }
  {
    passport::PublicMaid public_maid(keys.maid);
    passport::PublicAnmaid public_anmaid(keys.anmaid);
    auto future(nfs_->CreateAccount(nfs_vault::AccountCreation(public_maid, public_anmaid),
                                    std::chrono::seconds(20)));
    auto status(future.wait_for(boost::chrono::seconds(20)));
    if (status == boost::future_status::timeout) {
      LOG(kError) << "can't create account";
      BOOST_THROW_EXCEPTION(MakeError(VaultErrors::failed_to_handle_request));
    }
    // waiting for syncs resolved
    boost::this_thread::sleep_for(boost::chrono::seconds(2));
    LOG(kSuccess) << "Account created for maid " << HexSubstr(public_maid.name()->string());
  }
  {
    try {
      nfs_->Put(passport::PublicPmid(keys.pmid));
    }
    catch (...) {
      BOOST_THROW_EXCEPTION(MakeError(CommonErrors::unknown));
    }
    Sleep(std::chrono::seconds(2));
  }
  if (register_pmid_for_client) {
    try {
      auto register_pmid_future(nfs_->RegisterPmid(
                                    nfs_vault::PmidRegistration(keys.maid, keys.pmid, false)));
      register_pmid_future.get();
    }
    catch (const maidsafe_error& error) {
      LOG(kError) << "Pmid Registration Failed " << boost::diagnostic_information(error);
      throw;
    }
    Sleep(std::chrono::seconds(2));
    passport::PublicPmid::Name pmid_name(Identity(keys.pmid.name().value));

    try {
      auto get_health_future(nfs_->GetPmidHealth(pmid_name));
      LOG(kVerbose) << "The fetched PmidHealth for pmid_name "
                    << HexSubstr(pmid_name.value.string()) << " is " << get_health_future.get();
    }
    catch (const maidsafe_error& error) {
      LOG(kError) << "Pmid Health Retreival Failed " << boost::diagnostic_information(error);
      throw;
    }
    // waiting for the GetPmidHealth updating corresponding accounts
    boost::this_thread::sleep_for(boost::chrono::seconds(2));
    LOG(kSuccess) << "Pmid Registered created for the client node to store chunks";
  }
}

std::future<bool> Client::RoutingJoin(const std::vector<UdpEndpoint>& peer_endpoints) {
  std::shared_ptr<std::promise<bool>> join_promise(std::make_shared<std::promise<bool>>());
  functors_.network_status = [join_promise](int result) {
    LOG(kVerbose) << "Network health: " << result;
    if (result == 100) {
      LOG(kInfo) << "Connected to enough vaults";
      try {
        join_promise->set_value(true);
        LOG(kInfo) << "join_promise set to true";
      } catch (...) {
        LOG(kError) << "can't set join_promise";
      }
    }
  };
  functors_.typed_message_and_caching.group_to_group.message_received =
      [&](const routing::GroupToGroupMessage& msg) { nfs_->HandleMessage(msg); };  // NOLINT
  functors_.typed_message_and_caching.group_to_single.message_received =
      [&](const routing::GroupToSingleMessage& msg) { nfs_->HandleMessage(msg); };  // NOLINT
  functors_.typed_message_and_caching.single_to_group.message_received =
      [&](const routing::SingleToGroupMessage& msg) { nfs_->HandleMessage(msg); };  // NOLINT
  functors_.typed_message_and_caching.single_to_single.message_received =
      [&](const routing::SingleToSingleMessage& msg) { nfs_->HandleMessage(msg); };  // NOLINT
//  functors_.typed_message_and_caching.single_to_group_relay.message_received =
//      [&](const routing::SingleToGroupRelayMessage &msg) { nfs_->HandleMessage(msg); };
  functors_.request_public_key =
      [&, this](const NodeId& node_id, const routing::GivePublicKeyFunctor& give_key) {
        public_key_getter_(node_id, give_key, public_pmids_, data_getter_);
      };
  routing_.Join(functors_, peer_endpoints);
  return std::move(join_promise->get_future());
}

KeyChain::KeyChain(size_t size) {
  while (size-- > 0)
    Add();
}

passport::detail::AnmaidToPmid KeyChain::Add() {
  passport::Anmaid anmaid;
  passport::Maid maid(anmaid);
  passport::Anpmid anpmid;
  passport::Pmid pmid(anpmid);
  passport::detail::AnmaidToPmid node_keys(anmaid, maid, anpmid, pmid);
  keys.push_back(node_keys);
  return node_keys;
}

}  // namespace test

}  // namespace vault

}  // namespace maidsafe

