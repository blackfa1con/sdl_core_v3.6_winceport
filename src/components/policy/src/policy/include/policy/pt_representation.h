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

#ifndef SRC_COMPONENTS_POLICY_INCLUDE_POLICY_PT_REPRESENTATION_H_
#define SRC_COMPONENTS_POLICY_INCLUDE_POLICY_PT_REPRESENTATION_H_

#include <vector>
#include <string>
#include "policy/policy_types.h"
#include "./types.h"

namespace policy_table = rpc::policy_table_interface_base;

namespace policy {

/**
 * @struct Data about vehicle
 */
struct VehicleData {
#ifdef OS_WIN32
	VehicleData(){
		vehicle_make = "";
		vehicle_model = "";
		vehicle_year = 0;
	}
	VehicleData(const std::string &vehicle_make_data, const std::string &vehicle_model_data, int vehicle_year_data){
		vehicle_make = vehicle_make_data;
		vehicle_model = vehicle_model_data;
		vehicle_year = vehicle_year_data;
	}
	std::string vehicle_make;
	std::string vehicle_model;
#else
  const std::string vehicle_make;
  const std::string vehicle_model;
#endif
  int vehicle_year;
};

enum InitResult {
  NONE = 0,
  EXISTS,
  SUCCESS,
  FAIL
};

class PTRepresentation {
  public:
    virtual ~PTRepresentation() {
    }

    /**
     * @brief Check if specified RPC for specified application
     * has permission to be executed in specified HMI Level
     * and also its permitted params.
     * @param app_id Id of application provided during registration
     * @param hmi_level Current HMI Level of application
     * @param rpc Name of RPC
     * @return CheckPermissionResult containing flag if HMI Level is allowed
     * and list of allowed params.
     */
    virtual CheckPermissionResult CheckPermissions(const PTString& app_id,
        const PTString& hmi_level,
        const PTString& rpc) = 0;

    /**
     * @brief Returns true if Policy Table was not updated yet
     * from preloaded pt file.
     */
    virtual bool IsPTPreloaded() = 0;

    /**
     * Gets number of ignition cycles before next update policy table
     * @return number of ignition cycles
     */
    virtual int IgnitionCyclesBeforeExchange() = 0;

    /**
     * Gets value in kilometers before next update policy table
     * @param current value in kilometers from the odometers
     * @return value in kilometers
     */
    virtual int KilometersBeforeExchange(int current) = 0;

    /**
     * @brief Sets kilometers and days after epoch, that passed for recieved
     * successful PT UPdate
     */
    virtual bool SetCountersPassedForSuccessfulUpdate(int kilometers,
        int days_after_epoch) = 0;

    /**
     * Gets value in days before next update policy table
     * @param current value in days after epoch
     * @return value in days
     */
    virtual int DaysBeforeExchange(int current) = 0;

    /**
     * @brief Increment number of ignition cycles since last exchange by 1
     */
    virtual void IncrementIgnitionCycles() = 0;

    /**
     * @brief Reset number of ignition cycles since last exchange to 0
     */
    virtual void ResetIgnitionCycles() = 0;

    /**
     * @brief Returns timeout to wait for a response of PT update
     * @return value in seconds
     */
    virtual int TimeoutResponse() = 0;

    /**
     * @brief Returns number of seconds between each try of sending PTS
     * @param seconds Return value: array of 5 elements
     * @return bool Success of operation
     */
    virtual bool SecondsBetweenRetries(std::vector<int>* seconds) = 0;

    /**
     * @brief Get information about vehicle
     */
    virtual VehicleData GetVehicleData() = 0;

    /**
     * @brief Get message text for displaying/pronouncing for user
     * dependent on language and context.
     * @param msg_codes Context of message (Driver distraction, Grant permission etc)
     * @param language Language of the message
     * @return Array of appropriate messages parameters
     */
    virtual std::vector<UserFriendlyMessage> GetUserFriendlyMsg(
      const std::vector<std::string>& msg_codes, const std::string& language) = 0;

    /**
     * @brief Get list of URL to send PTS to
     * @param service_type If URLs for specific service are preset,
     * return them otherwise default URLs.
     */
    virtual EndpointUrls GetUpdateUrls(int service_type) = 0;

    /**
     * @brief Get allowed number of notifications
     * depending on application priority.
     * @param priority Priority of application
     */
    virtual int GetNotificationsNumber(policy_table::Priority priority) = 0;

    /**
     * @brief Initialized Policy Table (load)
     * @return bool Success of operation
     */
    virtual InitResult Init() = 0;

    /**
     * @brief Close policy table
     * @return bool Success of operation
     */
    virtual bool Close() = 0;

    /**
     * @brief Removes policy table content.
     * @return bool Success of operation
     */
    virtual bool Clear() = 0;

    /**
     * Drops policy table (schema and content)
     * @return true if successfully
     */
    virtual bool Drop() = 0;

    /**
     * @brief Get snapshot of Policy Table
     * including app_policies, functional_groups,
     * device_info, statistics, excluding user messages
     * @return Generated structure for obtaining Json string.
     */
    virtual utils::SharedPtr<policy_table::Table> GenerateSnapshot() const = 0;

    /**
     * Saves policy table in storage
     * @param table policy table
     * @return true if successfully
     */
    virtual bool Save(const policy_table::Table& table) = 0;

    /**
     * Gets flag updateRequired
     * @return true if update is required
     */
    virtual bool UpdateRequired() const = 0;

    /**
     * Saves flag updateRequired
     */
    virtual void SaveUpdateRequired(bool value) = 0;

    /*
     Retrieves data from app_policies about app on its registration:
     app_id - id of registered app; all outputs are filled in only if not null
     output: nicknames Synonyms for application
     output: app_types Section on HMI where app can appear (Navigation, Phone etc)
     */
    virtual bool GetInitialAppData(const std::string& app_id,
                                   StringArray* nicknames = NULL,
                                   StringArray* app_types = NULL) = 0;

    /**
     * Checks if the application is revoked
     * @param app_id application id
     * @return true if application is revoked
     */
    virtual bool IsApplicationRevoked(const std::string& app_id) const = 0;

    /**
     * @brief Get functional groupings from DB
     * @param groups Known functional groupings
     * @return true, if succeeded, otherwise - false
     */
    virtual bool GetFunctionalGroupings(policy_table::FunctionalGroupings& groups) = 0;

    /**
     * Checks if the application is represented in policy table
     * @param app_id application id
     * @return true if application is represented in policy table
     */
    virtual bool IsApplicationRepresented(const std::string& app_id) const = 0;

    /**
     * Checks if the application has default policy
     * @param app_id application id
     * @return true if application has default policy
     */
    virtual bool IsDefaultPolicy(const std::string& app_id) const = 0;

    /**
         * Checks if the application has pre_data policy
         * @param app_id application id
         * @return true if application has pre_data policy
         */
    virtual bool IsPredataPolicy(const std::string& app_id) const = 0;

    /**
     * Sets default policy for application
     * @param app_id application id
     * @return true if success
     */
    virtual bool SetDefaultPolicy(const std::string& app_id) = 0;
};

}  //  namespace policy

#endif  //  SRC_COMPONENTS_POLICY_INCLUDE_POLICY_PT_REPRESENTATION_H_
