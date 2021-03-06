/*
 Copyright (c) 2013, Ford Motor Company
 All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:

 Redistributions of source code must retain the above copyright notice, this
 list of conditions and the following disclaimer.

 Redistributions in binary form must reproduce the above copyright notice,
 this list of conditions and the following
 disclaimer in the documentation and/or other materials provided with the
 distribution.

 Neither the name of the Ford Motor Company nor the names of its contributors
 may be used to endorse or promote products derived from this software
 without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef SRC_COMPONENTS_POLICY_INCLUDE_POLICY_SQL_PT_REPRESENTATION_H_
#define SRC_COMPONENTS_POLICY_INCLUDE_POLICY_SQL_PT_REPRESENTATION_H_

#include <string>
#include <vector>
#include "policy/pt_representation.h"
#include "rpc_base/rpc_base.h"
#include "./types.h"

namespace policy_table = rpc::policy_table_interface_base;

namespace policy {

namespace dbms {
class SQLDatabase;
}  // namespace dbms

class SQLPTRepresentation : public virtual PTRepresentation {
  public:
    SQLPTRepresentation();
    ~SQLPTRepresentation();
    virtual CheckPermissionResult CheckPermissions(const PTString& app_id,
        const PTString& hmi_level,
        const PTString& rpc);

    virtual bool IsPTPreloaded();
    virtual int IgnitionCyclesBeforeExchange();
    virtual int KilometersBeforeExchange(int current);
    virtual bool SetCountersPassedForSuccessfulUpdate(int kilometers,
        int days_after_epoch);
    virtual int DaysBeforeExchange(int current);
    virtual void IncrementIgnitionCycles();
    virtual void ResetIgnitionCycles();
    virtual int TimeoutResponse();
    virtual bool SecondsBetweenRetries(std::vector<int>* seconds);

    virtual VehicleData GetVehicleData();

    virtual std::vector<UserFriendlyMessage> GetUserFriendlyMsg(
      const std::vector<std::string>& msg_codes, const std::string& language);

    virtual EndpointUrls GetUpdateUrls(int service_type);

    virtual int GetNotificationsNumber(policy_table::Priority priority);
    InitResult Init();
    bool Close();
    bool Clear();
    bool Drop();
    virtual utils::SharedPtr<policy_table::Table> GenerateSnapshot() const;
    bool Save(const policy_table::Table& table);
    bool GetInitialAppData(const std::string& app_id, StringArray* nicknames =
                             NULL,
                           StringArray* app_hmi_types = NULL);
    bool GetFunctionalGroupings(policy_table::FunctionalGroupings& groups);

  protected:
    virtual void GatherModuleMeta(policy_table::ModuleMeta* meta) const;
    virtual void GatherModuleConfig(policy_table::ModuleConfig* config) const;
    virtual bool GatherUsageAndErrorCounts(
      policy_table::UsageAndErrorCounts* counts) const;
    virtual void GatherDeviceData(policy_table::DeviceData* data) const;
    virtual bool GatherFunctionalGroupings(
      policy_table::FunctionalGroupings* groups) const;
    virtual bool GatherConsumerFriendlyMessages(
      policy_table::ConsumerFriendlyMessages* messages) const;
    virtual bool GatherApplicationPolicies(
      policy_table::ApplicationPolicies* apps) const;

    bool GatherAppGroup(const std::string& app_id,
                        policy_table::Strings* app_groups) const;
    bool GatherAppType(const std::string& app_id,
                       policy_table::AppHMITypes* app_types) const;
    bool GatherNickName(const std::string& app_id,
                        policy_table::Strings* nicknames) const;

    virtual bool SaveModuleMeta(const policy_table::ModuleMeta& meta);
    virtual bool SaveModuleConfig(const policy_table::ModuleConfig& config);
    virtual bool SaveUsageAndErrorCounts(
      const policy_table::UsageAndErrorCounts& counts);
    virtual bool SaveDeviceData(const policy_table::DeviceData& devices);
    virtual bool SaveFunctionalGroupings(
      const policy_table::FunctionalGroupings& groups);
    virtual bool SaveConsumerFriendlyMessages(
      const policy_table::ConsumerFriendlyMessages& messages);
    virtual bool SaveApplicationPolicies(
      const policy_table::ApplicationPolicies& apps);

    virtual bool SaveMessageString(const std::string& type,
                                   const std::string& lang,
                                   const policy_table::MessageString& strings);

    bool SaveAppGroup(const std::string& app_id,
                      const policy_table::Strings& app_groups);
    bool SaveNickname(const std::string& app_id,
                      const policy_table::Strings& nicknames);
    bool SaveAppType(const std::string& app_id,
                     const policy_table::AppHMITypes& types);

    bool UpdateRequired() const;
    void SaveUpdateRequired(bool value);

    bool IsApplicationRevoked(const std::string& app_id) const;
    bool IsApplicationRepresented(const std::string& app_id) const;
    virtual bool IsDefaultPolicy(const std::string& app_id) const;
    virtual bool IsPredataPolicy(const std::string& app_id) const;
    virtual bool SetDefaultPolicy(const std::string& app_id);

    dbms::SQLDatabase* db() const;
    virtual bool SetIsDefault(const std::string& app_id, bool is_default) const;

  private:
    static const std::string kDatabaseName;
    dbms::SQLDatabase* db_;

    bool SaveRpcs(int64_t group_id, const policy_table::Rpc& rpcs);
    bool SaveServiceEndpoints(const policy_table::ServiceEndpoints& endpoints);
    bool SaveSecondsBetweenRetries(
      const policy_table::SecondsBetweenRetries& seconds);
    bool SaveNumberOfNotificationsPerMinute(
      const policy_table::NumberOfNotificationsPerMinute& notifications);
    bool SaveMessageType(const std::string& type);
    bool SaveLanguage(const std::string& code);
};
}  //  namespace policy

#endif  // SRC_COMPONENTS_POLICY_INCLUDE_POLICY_SQL_PT_REPRESENTATION_H_
