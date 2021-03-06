/* Copyright (c) 2013, Ford Motor Company
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following
 * disclaimer in the documentation and/or other materials provided with the
 * distribution.
 *
 * Neither the name of the Ford Motor Company nor the names of its contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef TEST_COMPONENTS_POLICY_INCLUDE_MOCK_PT_REPRESENTATION_H_
#define TEST_COMPONENTS_POLICY_INCLUDE_MOCK_PT_REPRESENTATION_H_

#include "gmock/gmock.h"
#include "policy/pt_representation.h"
#include "rpc_base/rpc_base.h"
#include "./types.h"

namespace policy_table = ::rpc::policy_table_interface_base;

namespace policy {

class MockPTRepresentation : virtual public PTRepresentation {
  public:
    MOCK_METHOD3(CheckPermissions,
                 CheckPermissionResult(const PTString& app_id, const PTString& hmi_level,
                                       const PTString& rpc));
    MOCK_METHOD0(IsPTPreloaded,
                 bool());
    MOCK_METHOD0(IgnitionCyclesBeforeExchange,
                 int());
    MOCK_METHOD1(KilometersBeforeExchange,
                 int(int current));
    MOCK_METHOD2(SetCountersPassedForSuccessfulUpdate,
                 bool(int kilometers, int days_after_epoch));
    MOCK_METHOD1(DaysBeforeExchange,
                 int(int current));
    MOCK_METHOD0(IncrementIgnitionCycles,
                 void());
    MOCK_METHOD0(ResetIgnitionCycles,
                 void());
    MOCK_METHOD0(TimeoutResponse,
                 int());
    MOCK_METHOD1(SecondsBetweenRetries,
                 bool(std::vector<int>* seconds));
    MOCK_METHOD0(GetVehicleData,
                 VehicleData());
    MOCK_METHOD2(GetUserFriendlyMsg,
                 std::vector<UserFriendlyMessage>(const std::vector<std::string>& msg_code,
                     const std::string& language));
    MOCK_METHOD1(GetUpdateUrls,
                 EndpointUrls(int service_type));
    MOCK_METHOD1(GetNotificationsNumber,
                 int(policy_table::Priority priority));
    MOCK_METHOD0(Init,
                 InitResult());
    MOCK_METHOD0(Close,
                 bool());
    MOCK_METHOD0(Clear,
                 bool());
    MOCK_METHOD0(Drop,
                 bool());
    MOCK_CONST_METHOD0(GenerateSnapshot,
                       utils::SharedPtr<policy_table::Table>());
    MOCK_METHOD1(Save,
                 bool(const policy_table::Table& table));
    MOCK_CONST_METHOD0(UpdateRequired,
                       bool());
    MOCK_METHOD1(SaveUpdateRequired,
                 void(bool value));
    MOCK_METHOD3(GetInitialAppData,
                 bool(const std::string& app_id, StringArray* nicknames, StringArray* app_types));
    MOCK_CONST_METHOD1(IsApplicationRevoked, bool(const std::string& app_id));
    MOCK_METHOD1(GetFunctionalGroupings,
                 bool(policy_table::FunctionalGroupings& groups));
    MOCK_CONST_METHOD1(IsApplicationRepresented, bool(const std::string& app_id));
    MOCK_CONST_METHOD1(IsDefaultPolicy, bool(const std::string& app_id));
    MOCK_METHOD1(SetDefaultPolicy, bool(const std::string& app_id));
    MOCK_CONST_METHOD1(IsPredataPolicy, bool(const std::string& app_id));
};

}
// namespace policy

#endif  // TEST_COMPONENTS_POLICY_INCLUDE_MOCK_PT_REPRESENTATION_H_
