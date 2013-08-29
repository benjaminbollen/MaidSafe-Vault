/* Copyright 2012 MaidSafe.net limited

This MaidSafe Software is licensed under the MaidSafe.net Commercial License, version 1.0 or later,
and The General Public License (GPL), version 3. By contributing code to this project You agree to
the terms laid out in the MaidSafe Contributor Agreement, version 1.0, found in the root directory
of this project at LICENSE, COPYING and CONTRIBUTOR respectively and also available at:

http://www.novinet.com/license

Unless required by applicable law or agreed to in writing, software distributed under the License is
distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
implied. See the License for the specific language governing permissions and limitations under the
License.
*/

#ifndef MAIDSAFE_VAULT_PMID_NODE_SERVICE_INL_H_
#define MAIDSAFE_VAULT_PMID_NODE_SERVICE_INL_H_

#include <exception>
#include <string>

#include "maidsafe/common/log.h"

#include "maidsafe/nfs/client/messages.pb.h"
#include "maidsafe/nfs/vault/messages.h"
#include "maidsafe/nfs/client/messages.h"
#include "maidsafe/data_types/data_name_variant.h"
#include "maidsafe/routing/message.h"
#include "maidsafe/vault/messages.h"
#include "maidsafe/vault/pmid_manager/pmid_manager.pb.h"



namespace maidsafe {

namespace vault {

template<>
void PmidNodeService::HandleMessage<nfs::GetRequestFromDataManagerToPmidNode>(
    const nfs::GetRequestFromDataManagerToPmidNode& message,
    const typename nfs::GetRequestFromDataManagerToPmidNode::Sender& sender,
    const typename nfs::GetRequestFromDataManagerToPmidNode::Receiver& receiver) {
#ifndef TESTING
    ValidateGetSender(message, sender);
#endif
  {
    std::lock_guard<mutex> lock(accumulator_mutex_);
    if (accumulator_.CheckHandled(message))
      return;
  }
  HandleGetMessage(message, sender, receiver);
}

template<>
void PmidNodeService::HandleMessage<nfs::DeleteRequestFromPmidManagerToPmidNode>(
    const nfs::DeleteRequestFromPmidManagerToPmidNode& message,
    const typename nfs::DeleteRequestFromPmidManagerToPmidNode::Sender& sender,
    const typename nfs::DeleteRequestFromPmidManagerToPmidNode::Receiver& receiver) {
#ifndef TESTNG
  ValidateSender(message, sender);
#endif
  {
    std::lock_guard<mutex> lock(accumulator_mutex_);
    if (accumulator_.CheckHandled(message))
      return;

    if (Accumulator<PmidNodeServiceMessages>::AddResult::kSuccess !=
            accumulator_.AddPendingRequest(
                message,
                sender,
                Accumulator<PmidNodeServiceMessages>::AddRequestPredicate(kDeleteRequestsRequired)))
      return;
  }
  HandleDeleteMessage(message, sender, receiver);
}

template<>
void PmidNodeService::HandleMessage<nfs::PutRequestFromPmidManagerToPmidNode>(
    const nfs::PutRequestFromPmidManagerToPmidNode& message,
    const typename nfs::PutRequestFromPmidManagerToPmidNode::Sender& sender,
    const typename nfs::PutRequestFromPmidManagerToPmidNode::Receiver& receiver) {
#ifndef TESTNG
  ValidateSender(message, sender);
#endif
  {
    std::lock_guard<mutex> lock(accumulator_mutex_);
    if (accumulator_.CheckHandled(message))
      return;

    if (Accumulator<PmidNodeServiceMessages>::AddResult::kSuccess !=
            accumulator_.AddPendingRequest(
                message,
                sender,
                Accumulator<PmidNodeServiceMessages>::AddRequestPredicate(kPutRequestsRequired)))
      return;
  }
  HandlePutMessage(message, sender, receiver);
}

template<>
void PmidNodeService::HandleMessage<nfs::GetPmidAccountResponseFromPmidManagerToPmidNode>(
    const nfs::GetPmidAccountResponseFromPmidManagerToPmidNode& message,
    const typename nfs::GetPmidAccountResponseFromPmidManagerToPmidNode::Sender& sender,
    const typename nfs::GetPmidAccountResponseFromPmidManagerToPmidNode::Receiver& /*receiver*/) {
#ifndef TESTNG
  ValidateSender(message, sender);
#endif
  Accumulator<PmidNodeServiceMessages>::AddResult result;
  std::vector<PmidNodeServiceMessages> responses;
  {
    std::lock_guard<mutex> lock(accumulator_mutex_);
    if (accumulator_.CheckHandled(message))
      return;
    Accumulator<PmidNodeServiceMessages>::AddRequestPredicate add_request_predicate(
        [&](const std::vector<PmidNodeServiceMessages>& requests_in) {
          if (requests_in.size() < 2)
            return Accumulator<PmidNodeServiceMessages>::AddResult::kWaiting;
          std::vector<protobuf::PmidAccountResponse> pmid_account_responses;
          protobuf::PmidAccountResponse pmid_account_response;
          nfs_client::protobuf::DataNameAndContentOrReturnCode data;
          nfs::GetPmidAccountResponseFromPmidManagerToPmidNode response;
          for (auto& request : requests_in) {
            response = boost::get<nfs::GetPmidAccountResponseFromPmidManagerToPmidNode>(request);
            if (data.ParseFromString(response.contents->content.string())) {
              if (data.has_serialised_data_name_and_content()) {
                if (pmid_account_response.ParseFromString(
                        data.serialised_data_name_and_content())) {
                  pmid_account_responses.push_back(pmid_account_response);
                } else {
                  LOG(kWarning) << "Failed to parse the contents";
                }
              }
            } else {
              LOG(kWarning) << "Failed to parse the contents of the response";
            }
          }
          if ((static_cast<uint16_t>(requests_in.size()) >= (routing::Parameters::node_group_size / 2 + 1)) &&
               pmid_account_responses.size() >= routing::Parameters::node_group_size / 2)
            return Accumulator<PmidNodeServiceMessages>::AddResult::kSuccess;
          if ((requests_in.size() == routing::Parameters::node_group_size) ||
               (requests_in.size() - pmid_account_responses.size() >
                    routing::Parameters::node_group_size / 2))
            return Accumulator<PmidNodeServiceMessages>::AddResult::kFailure;
          return Accumulator<PmidNodeServiceMessages>::AddResult::kWaiting;
        });
    result = accumulator_.AddPendingRequest(message, sender, add_request_predicate);
    if (result == Accumulator<PmidNodeServiceMessages>::AddResult::kFailure) {
      accumulator_.SetHandled(message, sender);
      return;
    }
    if (result == Accumulator<PmidNodeServiceMessages>::AddResult::kSuccess) {
      responses = accumulator_.Get(message);
      accumulator_.SetHandled(message, sender);
    }
  }
  if (result == Accumulator<PmidNodeServiceMessages>::AddResult::kSuccess) {
    std::vector<nfs::GetPmidAccountResponseFromPmidManagerToPmidNode> typed_responses;
    for (auto response : responses)
      typed_responses.push_back(
          boost::get<nfs::GetPmidAccountResponseFromPmidManagerToPmidNode>(response));
    HandleAccountResponses(typed_responses);
  }
  if (result == Accumulator<PmidNodeServiceMessages>::AddResult::kFailure) {
    SendAccountRequest();
  }
}

template<>
void PmidNodeService::HandlePutMessage<nfs::PutRequestFromPmidManagerToPmidNode>(
    const nfs::PutRequestFromPmidManagerToPmidNode& message,
    const typename nfs::PutRequestFromPmidManagerToPmidNode::Sender& sender,
    const typename nfs::PutRequestFromPmidManagerToPmidNode::Receiver& /*receiver*/) {
  try {
    permanent_data_store_.Put(nfs_vault::DataName(message.contents->name),
                              message.contents->content);
    typedef nfs::PutResponseFromPmidNodeToPmidManager NfsMessage;
    typedef routing::Message<NfsMessage::Sender, NfsMessage::Receiver> RoutingMessage;
    NfsMessage nfs_message(nfs_vault::DataName(message.contents->name));
    RoutingMessage routing_message(nfs_message.Serialise(),
                                   NfsMessage::Sender(routing_.kNodeId()),
                                   NfsMessage::Receiver(routing_.kNodeId()));
    routing_.Send(routing_message);
    {
      std::lock_guard<mutex> lock(accumulator_mutex_);
      accumulator_.SetHandled(message, sender);
    }
  } catch(const maidsafe_error& error) {
    typedef PutResponseFromPmidNodeToDataManager NfsMessage;
    typedef routing::Message<NfsMessage::Sender, NfsMessage::Receiver> RoutingMessage;
    NfsMessage nfs_message(DataNameAndContentAndReturnCode(
                               message.contents->name.type,
                               message.contents->name.raw_name,
                               message.contents->content,
                               nfs_client::ReturnCode(error)));
    RoutingMessage routing_message(nfs_message.Serialise(),
                                   NfsMessage::Sender(routing_.kNodeId()),
                                   NfsMessage::Receiver(
                                       NodeId(message.contents->name.raw_name.string())));
    routing_.Send(routing_message);
    std::lock_guard<mutex> lock(accumulator_mutex_);
    accumulator_.SetHandled(message, sender);
  }
}

template<>
void PmidNodeService::HandleGetMessage(const nfs::GetRequestFromDataManagerToPmidNode& message,
    const typename nfs::GetRequestFromDataManagerToPmidNode::Sender& sender,
    const typename nfs::GetRequestFromDataManagerToPmidNode::Receiver& /*receiver*/) {
  typedef nfs::GetResponseFromPmidNodeToDataManager NfsMessage;
  typedef routing::Message<NfsMessage::Sender, NfsMessage::Receiver> RoutingMessage;
  nfs_vault::DataName data_name(message.contents->type, message.contents->raw_name);
  try {
    auto content(permanent_data_store_.Get(data_name));
    NfsMessage nfs_message(nfs_client::DataNameAndContentOrReturnCode(
        nfs_vault::DataNameAndContent(DataTagValue(message.contents->type),
                                      message.contents->raw_name,
                                      content)));
    RoutingMessage routing_message(nfs_message.Serialise(),
                                   NfsMessage::Sender(routing::SingleId(routing_.kNodeId())),
                                   NfsMessage::Receiver(
                                       NodeId(message.contents->raw_name.string())));
    routing_.Send(routing_message);
    {
      std::lock_guard<mutex> lock(accumulator_mutex_);
      accumulator_.SetHandled(message, sender);
    }
  } catch (const maidsafe_error& error) {
    NfsMessage nfs_message(
        nfs_client::DataNameAndContentOrReturnCode(
            nfs_client::DataNameAndReturnCode(data_name, nfs_client::ReturnCode(error))));
    RoutingMessage routing_message(nfs_message.Serialise(),
                                   NfsMessage::Sender(routing::SingleId(routing_.kNodeId())),
                                   NfsMessage::Receiver(
                                       NodeId(message.contents->raw_name.string())));
    routing_.Send(routing_message);
    {
      std::lock_guard<mutex> lock(accumulator_mutex_);
      accumulator_.SetHandled(message, sender);
    }
  } catch(const std::exception& /*ex*/) {
  }
}

template<>
void PmidNodeService::HandleDeleteMessage(
    const nfs::DeleteRequestFromPmidManagerToPmidNode& message,
    const typename nfs::DeleteRequestFromPmidManagerToPmidNode::Sender& sender,
    const typename nfs::DeleteRequestFromPmidManagerToPmidNode::Receiver& /*receiver*/) {
  try {
    {
      permanent_data_store_.Delete(nfs_vault::DataName(message.contents->type,
                                                       message.contents->raw_name));
      accumulator_.SetHandled(message, sender);
    }
  } catch(const std::exception& /*ex*/) {
    std::lock_guard<mutex> lock(accumulator_mutex_);
    accumulator_.SetHandled(message, sender);
  }
}


template<>
bool PmidNodeService::GetFromCache<nfs::GetRequestFromMaidNodeToDataManager>(
    const nfs::GetRequestFromMaidNodeToDataManager& message,
    const typename nfs::GetRequestFromMaidNodeToDataManager::Sender& sender,
    const typename nfs::GetRequestFromMaidNodeToDataManager::Receiver& receiver) {
  return DoGetFromCache(message, sender, receiver);
}

template<>
bool PmidNodeService::GetFromCache<nfs::GetRequestFromPmidNodeToDataManager>(
    const nfs::GetRequestFromPmidNodeToDataManager& message,
    const typename nfs::GetRequestFromPmidNodeToDataManager::Sender& sender,
    const typename nfs::GetRequestFromPmidNodeToDataManager::Receiver& receiver) {
  return DoGetFromCache(message, sender, receiver);
}

template<typename T>
bool PmidNodeService::DoGetFromCache(const T& message,
                                     const typename T::Sender& sender,
                                     const typename T::Receiver& receiver) {
  auto data_name(GetDataNameVariant(message.contents->type, message.contents->raw_name));
  if (!boost::apply_visitor(cacheable_visitor(), data_name))
    return false;
  if (boost::apply_visitor(long_term_cacheable_visitor(), data_name))
    return CacheGet(message, sender, receiver, IsLongTermCacheable());
  return CacheGet(message, sender, receiver, IsShortTermCacheable());
}

template<typename T>
bool PmidNodeService::CacheGet(const T& message,
                               const typename T::Sender& sender,
                               const typename T::Receiver& receiver,
                               IsShortTermCacheable) {
  try {
    auto content(std::make_shared<NonEmptyString>(mem_only_cache_.Get(*message.contents)));
    active_.Send([=]() {
                   SendCachedData(message, sender, receiver, content);
                 });
  } catch(maidsafe_error& /*error*/) {
    return false;
  }
  return true;
}

template<typename T>
bool PmidNodeService::CacheGet(const T& message,
                               const typename T::Sender& sender,
                               const typename T::Receiver& receiver,
                               IsLongTermCacheable) {
  try {
    auto content(std::make_shared<NonEmptyString>(cache_data_store_.Get(*message.contents)));
    active_.Send([=]() {
                   SendCachedData(message, sender, receiver, content);
                 });
  } catch(maidsafe_error& /*error*/) {
    return false;
  }
  return true;
}

template<>
void PmidNodeService::StoreInCache<nfs::GetResponseFromDataManagerToMaidNode>(
    const nfs::GetResponseFromDataManagerToMaidNode& message,
    const typename nfs::GetResponseFromDataManagerToMaidNode::Sender& /*sender*/,
    const typename nfs::GetResponseFromDataManagerToMaidNode::Receiver& /*receiver*/) {
  auto data_name(GetDataNameVariant(message.contents->data->name.type,
                                    message.contents->data->name.raw_name));
  if (!boost::apply_visitor(cacheable_visitor(), data_name))
    return;
  if (boost::apply_visitor(long_term_cacheable_visitor(), data_name))
    CacheStore(message, data_name, IsLongTermCacheable());
  else
    CacheStore(message, data_name, IsShortTermCacheable());
}

template<typename T>
void PmidNodeService::CacheStore(const T& message,
                                 const DataNameVariant& data_name,
                                 IsShortTermCacheable) {
  return mem_only_cache_.Store(data_name, message.contents->data->content);
}

template<typename T>
void PmidNodeService::CacheStore(const T& message,
                                 const DataNameVariant& data_name,
                                 IsLongTermCacheable) {
  return cache_data_store_.Store(data_name, message.contents->data->content);
}

template <typename T>
void PmidNodeService::SendCachedData(const T& message,
                                     const typename T::Sender& sender,
                                     const typename T::Receiver& /*receiver*/,
                                     const std::shared_ptr<NonEmptyString> content) {
  typedef nfs::GetCachedResponseFromPmidNodeToMaidNode NfsMessage;
  typedef routing::Message<NfsMessage::Sender, NfsMessage::Receiver> RoutingMessage;
  NfsMessage nfs_message(nfs_client::DataNameAndContentOrReturnCode(
      nfs_vault::DataNameAndContent(DataTagValue(message.contents->type),
                                    message.contents->raw_name,
                                    *content)));
  RoutingMessage routing_message(nfs_message.Serialise(),
                                 NfsMessage::Sender(routing::SingleId(routing_.kNodeId())),
                                 NfsMessage::Receiver(sender));
  routing_.Send(routing_message);
}

template<>
void PmidNodeService::ValidateSender(
    const nfs::PutRequestFromPmidManagerToPmidNode& message,
    const typename nfs::PutRequestFromPmidManagerToPmidNode::Sender& /*sender*/) const {
  if (!SenderIsConnectedVault(message, routing_))
    ThrowError(VaultErrors::permission_denied);

  if (!FromPmidManager(message) || !ForThisPersona(message))
    ThrowError(CommonErrors::invalid_parameter);
}

template<>
void PmidNodeService::ValidateSender(
    const nfs::GetRequestFromDataManagerToPmidNode& message,
    const typename nfs::GetRequestFromDataManagerToPmidNode::Sender& /*sender*/) const {
  if (!SenderInGroupForMetadata(message, routing_))
    ThrowError(VaultErrors::permission_denied);

  if (!FromDataManager(message) || !ForThisPersona(message))
    ThrowError(CommonErrors::invalid_parameter);
}

template<>
void PmidNodeService::ValidateSender(
    const nfs::DeleteRequestFromPmidManagerToPmidNode& message,
    const typename nfs::DeleteRequestFromPmidManagerToPmidNode::Sender& /*sender*/) const {
  if (!SenderIsConnectedVault(message, routing_))
    ThrowError(VaultErrors::permission_denied);

  if (!FromPmidManager(message) || !ForThisPersona(message))
    ThrowError(CommonErrors::invalid_parameter);
}

}  // namespace vault

}  // namespace maidsafe

#endif  // MAIDSAFE_VAULT_PMID_NODE_SERVICE_INL_H_
