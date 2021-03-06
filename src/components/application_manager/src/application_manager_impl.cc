/**
 * Copyright (c) 2013, Ford Motor Company
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
#ifdef MODIFY_FUNCTION_SIGN
#include <global_first.h>
#endif
#include <climits>
#include <string>
#include <fstream>

#include "application_manager/application_manager_impl.h"
#include "application_manager/mobile_command_factory.h"
#include "application_manager/commands/command_impl.h"
#include "application_manager/commands/command_notification_impl.h"
#include "application_manager/message_helper.h"
#include "application_manager/mobile_message_handler.h"
#include "application_manager/policies/policy_handler.h"
#include "connection_handler/connection_handler_impl.h"
#include "formatters/formatter_json_rpc.h"
#include "formatters/CFormatterJsonSDLRPCv2.hpp"
#include "formatters/CFormatterJsonSDLRPCv1.hpp"
#include "config_profile/profile.h"
#include "utils/threads/thread.h"
#include "utils/file_system.h"
#include "application_manager/application_impl.h"
#include "usage_statistics/counter.h"
#include <time.h>

#ifdef MODIFY_FUNCTION_SIGN
#include <lib_msp_vr.h>
#include <utils/global.h>
#endif

namespace application_manager {

CREATE_LOGGERPTR_GLOBAL(logger_, "ApplicationManager")

uint32_t ApplicationManagerImpl::corelation_id_ = 0;
const uint32_t ApplicationManagerImpl::max_corelation_id_ = UINT_MAX;

namespace formatters = NsSmartDeviceLink::NsJSONHandler::Formatters;
namespace jhs = NsSmartDeviceLink::NsJSONHandler::strings;

ApplicationManagerImpl::ApplicationManagerImpl()
  : audio_pass_thru_active_(false),
    is_distracting_driver_(false),
    is_vr_session_strated_(false),
    hmi_cooperating_(false),
    is_all_apps_allowed_(true),
    hmi_handler_(NULL),
    connection_handler_(NULL),
    policy_manager_(NULL),
    hmi_so_factory_(NULL),
    mobile_so_factory_(NULL),
    protocol_handler_(NULL),
    messages_from_mobile_("application_manager::FromMobileThreadImpl", this),
    messages_to_mobile_("application_manager::ToMobileThreadImpl", this),
    messages_from_hmi_("application_manager::FromHMHThreadImpl", this),
    messages_to_hmi_("application_manager::ToHMHThreadImpl", this),
    request_ctrl_(),
    hmi_capabilities_(this),
    unregister_reason_(mobile_api::AppInterfaceUnregisteredReason::IGNITION_OFF),
    media_manager_(NULL),
    resume_ctrl_(this)
#ifdef TIME_TESTER
    , metric_observer_(NULL)
#endif  // TIME_TESTER
{
  LOG4CXX_INFO(logger_, "Creating ApplicationManager");
  media_manager_ = media_manager::MediaManagerImpl::instance();
  CreatePoliciesManager();
#ifdef MODIFY_FUNCTION_SIGN
	audio_pass_thru_read_file_thread_ = new threads::Thread("AudioPassThruReadFileThread", new AudioPassThruReadFileThread(this));
#endif
}

bool ApplicationManagerImpl::InitThread(threads::Thread* thread) {
  if (!thread) {
    LOG4CXX_ERROR(logger_, "Failed to allocate memory for thread object");
    return false;
  }
  LOG4CXX_INFO(
    logger_,
    "Starting thread with stack size "
    << profile::Profile::instance()->thread_min_stack_size());
  if (!thread->start()) {
    /*startWithOptions(
     threads::ThreadOptions(
     profile::Profile::instance()->thread_min_stack_size()))*/
    LOG4CXX_ERROR(logger_, "Failed to start thread");
    return false;
  }
  return true;
}

ApplicationManagerImpl::~ApplicationManagerImpl() {
  LOG4CXX_INFO(logger_, "Destructing ApplicationManager.");

  if (policy_manager_) {
    LOG4CXX_INFO(logger_, "Unloading policy library.");
    policy::PolicyHandler::instance()->UnloadPolicyLibrary();
  }
  policy_manager_ = NULL;
  media_manager_ = NULL;
  hmi_handler_ = NULL;
  connection_handler_ = NULL;
#ifdef MODIFY_FUNCTION_SIGN
  if(hmi_so_factory_ != NULL){
    delete hmi_so_factory_;
  }
#endif
  hmi_so_factory_ = NULL;
#ifdef MODIFY_FUNCTION_SIGN
  if(mobile_so_factory_ != NULL){
    delete mobile_so_factory_;
  }
#endif
  mobile_so_factory_ = NULL;
  protocol_handler_ = NULL;
  media_manager_ = NULL;
}

bool ApplicationManagerImpl::Stop() {
  LOG4CXX_INFO(logger_, "Stop ApplicationManager.");
  try {
    UnregisterAllApplications();
  } catch (...) {
    LOG4CXX_ERROR(logger_,
                  "An error occured during unregistering applications.");
  }
  MessageHelper::SendOnSdlCloseNotificationToHMI();
  return true;
}

ApplicationSharedPtr ApplicationManagerImpl::application(int32_t app_id) const {
  sync_primitives::AutoLock lock(applications_list_lock_);

  std::set<ApplicationSharedPtr>::const_iterator it =
    application_list_.begin();
  for (; it != application_list_.end(); ++it) {
    if ((*it)->app_id() == app_id) {
      return (*it);
    }
  }
  return ApplicationSharedPtr();
}

ApplicationSharedPtr ApplicationManagerImpl::application_by_hmi_app(
  int32_t hmi_app_id) const {
  sync_primitives::AutoLock lock(applications_list_lock_);

  std::set<ApplicationSharedPtr>::const_iterator it =
    application_list_.begin();
  for (; it != application_list_.end(); ++it) {
    if ((*it)->hmi_app_id() == hmi_app_id) {
      return (*it);
    }
  }
  return ApplicationSharedPtr();
}

ApplicationSharedPtr ApplicationManagerImpl::application_by_policy_id(
  const std::string& policy_app_id) const {
  sync_primitives::AutoLock lock(applications_list_lock_);

  std::vector<ApplicationSharedPtr> result;
#ifdef OS_WINCE
  for (std::set<ApplicationSharedPtr>::const_iterator it = application_list_.begin();
#else
  for (std::set<ApplicationSharedPtr>::iterator it = application_list_.begin();
#endif
       application_list_.end() != it;
       ++it) {
    if (policy_app_id.compare((*it)->mobile_app_id()->asString()) == 0) {
      return *it;
    }
  }
  return ApplicationSharedPtr();
}

ApplicationSharedPtr ApplicationManagerImpl::active_application() const {
  // TODO(DK) : check driver distraction
#ifdef OS_WINCE
  for (std::set<ApplicationSharedPtr>::const_iterator it = application_list_.begin();
#else
  for (std::set<ApplicationSharedPtr>::iterator it = application_list_.begin();
#endif
       application_list_.end() != it;
       ++it) {
    if ((*it)->IsFullscreen()) {
      return *it;
    }
  }
  return ApplicationSharedPtr();
}
#ifdef MODIFY_FUNCTION_SIGN
ApplicationSharedPtr ApplicationManagerImpl::audible_application() const {
#ifdef OS_WINCE
	for (std::set<ApplicationSharedPtr>::const_iterator it = application_list_.begin();
#else
	for (std::set<ApplicationSharedPtr>::iterator it = application_list_.begin();
#endif
		application_list_.end() != it;
		++it) {
		if ((*it)->IsAudible()) {
			return *it;
		}
	}
	return ApplicationSharedPtr();
}
#endif

std::vector<ApplicationSharedPtr> ApplicationManagerImpl::applications_by_button(
  uint32_t button) {
  std::vector<ApplicationSharedPtr> result;
  for (std::set<ApplicationSharedPtr>::iterator it = application_list_.begin();
       application_list_.end() != it; ++it) {
    if ((*it)->IsSubscribedToButton(
          static_cast<mobile_apis::ButtonName::eType>(button))) {
      result.push_back(*it);
    }
  }
  return result;
}

std::vector<utils::SharedPtr<Application>> ApplicationManagerImpl::IviInfoUpdated(
VehicleDataType vehicle_info, int value) {
  // Notify Policy Manager if available about info it's interested in,
  // i.e. odometer etc
  switch (vehicle_info) {
    case ODOMETER:
      policy::PolicyHandler::instance()->KmsChanged(value);
      break;
    default:
      break;
  }

  std::vector<utils::SharedPtr<application_manager::Application>> result;
  for (std::set<utils::SharedPtr<application_manager::Application>>::iterator it = application_list_.begin();
       application_list_.end() != it; ++it) {
    if ((*it)->IsSubscribedToIVI(static_cast<uint32_t>(vehicle_info))) {
      result.push_back(*it);
    }
  }
  return result;
}

std::vector<ApplicationSharedPtr> ApplicationManagerImpl::applications_with_navi() {
  std::vector<ApplicationSharedPtr> result;
  for (std::set<ApplicationSharedPtr>::iterator it = application_list_.begin();
       application_list_.end() != it;
       ++it) {
    if ((*it)->allowed_support_navigation()) {
      result.push_back(*it);
    }
  }
  return result;
}

ApplicationSharedPtr ApplicationManagerImpl::RegisterApplication(
  const utils::SharedPtr<smart_objects::SmartObject>&
  request_for_registration) {
  smart_objects::SmartObject& message = *request_for_registration;
  uint32_t connection_key =
    message[strings::params][strings::connection_key].asInt();

  if (false == is_all_apps_allowed_) {
    LOG4CXX_INFO(logger_,
                 "RegisterApplication: access to app's disabled by user");
    utils::SharedPtr<smart_objects::SmartObject> response(
      MessageHelper::CreateNegativeResponse(
        connection_key, mobile_apis::FunctionID::RegisterAppInterfaceID,
        message[strings::params][strings::correlation_id].asUInt(),
        mobile_apis::Result::DISALLOWED));
    ManageMobileCommand(response);
    return ApplicationSharedPtr();
  }

  // app_id is SDL "internal" ID
  // original app_id can be gotten via ApplicationImpl::mobile_app_id()
  uint32_t app_id = 0;
  std::list<int32_t> sessions_list;
  uint32_t device_id = 0;

  if (connection_handler_) {
    connection_handler::ConnectionHandlerImpl* con_handler_impl =
      static_cast<connection_handler::ConnectionHandlerImpl*>(

        connection_handler_);
    if (con_handler_impl->GetDataOnSessionKey(connection_key, &app_id,
        &sessions_list, &device_id)
        == -1) {
      LOG4CXX_ERROR(logger_,
                    "Failed to create application: no connection info.");
      utils::SharedPtr<smart_objects::SmartObject> response(
        MessageHelper::CreateNegativeResponse(
          connection_key, mobile_apis::FunctionID::RegisterAppInterfaceID,
          message[strings::params][strings::correlation_id].asUInt(),
          mobile_apis::Result::GENERIC_ERROR));
      ManageMobileCommand(response);
      return ApplicationSharedPtr();
    }
  }

  smart_objects::SmartObject& params = message[strings::msg_params];

  const std::string mobile_app_id = params[strings::app_id].asString();
  ApplicationSharedPtr application(
    new ApplicationImpl(app_id, mobile_app_id, policy_manager_));
  if (!application) {
    usage_statistics::AppCounter count_of_rejections_sync_out_of_memory(
      policy_manager_, mobile_app_id,
      usage_statistics::REJECTIONS_SYNC_OUT_OF_MEMORY);
    ++count_of_rejections_sync_out_of_memory;

    utils::SharedPtr<smart_objects::SmartObject> response(
      MessageHelper::CreateNegativeResponse(
        connection_key, mobile_apis::FunctionID::RegisterAppInterfaceID,
        message[strings::params][strings::correlation_id].asUInt(),
        mobile_apis::Result::OUT_OF_MEMORY));
    ManageMobileCommand(response);
    return ApplicationSharedPtr();
  }

  const std::string& name =
    message[strings::msg_params][strings::app_name].asString();

  application->set_name(name);
  application->set_device(device_id);
  application->set_grammar_id(GenerateGrammarID());
  mobile_api::Language::eType launguage_desired =
    static_cast<mobile_api::Language::eType>(params[strings::language_desired]
        .asInt());
  application->set_language(launguage_desired);
  application->usage_report().RecordAppRegistrationVuiLanguage(
    launguage_desired);

  mobile_api::Language::eType hmi_display_language_desired =
    static_cast<mobile_api::Language::eType>(params[strings::hmi_display_language_desired]
        .asInt());
  application->set_ui_language(hmi_display_language_desired);
  application->usage_report().RecordAppRegistrationGuiLanguage(
    hmi_display_language_desired);

  Version version;
  int32_t min_version =
    message[strings::msg_params][strings::sync_msg_version]
    [strings::minor_version].asInt();

  /*if (min_version < APIVersion::kAPIV2) {
    LOG4CXX_ERROR(logger_, "UNSUPPORTED_VERSION");
    utils::SharedPtr<smart_objects::SmartObject> response(
      MessageHelper::CreateNegativeResponse(
        connection_key, mobile_apis::FunctionID::RegisterAppInterfaceID,
        message[strings::params][strings::correlation_id],
        mobile_apis::Result::UNSUPPORTED_VERSION));
    ManageMobileCommand(response);
    delete application;
    return NULL;
  }*/
  version.min_supported_api_version = static_cast<APIVersion>(min_version);

  int32_t max_version =
    message[strings::msg_params][strings::sync_msg_version]
    [strings::major_version].asInt();

  /*if (max_version > APIVersion::kAPIV2) {
    LOG4CXX_ERROR(logger_, "UNSUPPORTED_VERSION");
    utils::SharedPtr<smart_objects::SmartObject> response(
      MessageHelper::CreateNegativeResponse(
        connection_key, mobile_apis::FunctionID::RegisterAppInterfaceID,
        message[strings::params][strings::correlation_id],
        mobile_apis::Result::UNSUPPORTED_VERSION));
    ManageMobileCommand(response);
    delete application;
    return NULL;
  }*/
  version.max_supported_api_version = static_cast<APIVersion>(max_version);
  application->set_version(version);

#ifdef MODIFY_FUNCTION_SIGN
	if (!application->IsSubscribedToButton(static_cast<mobile_apis::ButtonName::eType>(hmi_apis::Common_ButtonName::CUSTOM_BUTTON))) {
		LOG4CXX_INFO(logger_, "Subscribe CUSTOM_BUTTON to button list");
		application->SubscribeToButton(static_cast<mobile_apis::ButtonName::eType>(hmi_apis::Common_ButtonName::CUSTOM_BUTTON));
	}
#endif

  application->set_mobile_app_id(message[strings::msg_params][strings::app_id]);
  ProtocolVersion protocol_version = static_cast<ProtocolVersion>(
      message[strings::params][strings::protocol_version].asInt());
  application->set_protocol_version(protocol_version);

  if (ProtocolVersion::kV3 == protocol_version) {
    if (connection_handler_) {
      connection_handler_->StartSessionHeartBeat(connection_key);
    }
  }

  sync_primitives::AutoLock lock(applications_list_lock_);

  application_list_.insert(application);

  return application;
}

bool ApplicationManagerImpl::RemoveAppDataFromHMI(ApplicationSharedPtr app) {
  return true;
}

bool ApplicationManagerImpl::LoadAppDataToHMI(ApplicationSharedPtr app) {
  return true;
}

bool ApplicationManagerImpl::ActivateApplication(ApplicationSharedPtr app) {
  if (!app) {
    LOG4CXX_ERROR(logger_, "Null-pointer application received.");
    NOTREACHED();
    return false;
  }
  bool is_exist=false;
  bool is_new_app_media = app->is_media_application();

  applications_list_lock_.Ackquire();
  for (std::set<ApplicationSharedPtr>::iterator it = application_list_.begin();
       application_list_.end() != it;
       ++it) {
    ApplicationSharedPtr curr_app = *it;
    if (app->app_id() == curr_app->app_id()) {
      is_exist=true; 
    } else {
      if (is_new_app_media) {
        if (curr_app->IsAudible()) {
          curr_app->MakeNotAudible();
          MessageHelper::SendHMIStatusNotification(*curr_app);
        }
      }
      if (curr_app->IsFullscreen()) {
        MessageHelper::ResetGlobalproperties(curr_app);
      }
    }
  }
  applications_list_lock_.Release();
  if (is_exist)  {
    if (app->IsFullscreen()) {
      LOG4CXX_WARN(logger_, "Application is already active.");
      return false;
    }
    if (mobile_api::HMILevel::eType::HMI_LIMITED !=
      app->hmi_level()) {
      if (app->has_been_activated()) {
        MessageHelper::SendAppDataToHMI(app);
      }
    }
    if (!app->MakeFullscreen()) {
      return false;
    }
    MessageHelper::SendHMIStatusNotification(*app);
  }
  return true;
}

mobile_apis::HMILevel::eType ApplicationManagerImpl::PutApplicationInLimited(
  ApplicationSharedPtr app) {
  DCHECK(app.get())

  bool is_new_app_media = app->is_media_application();
  mobile_api::HMILevel::eType result = mobile_api::HMILevel::HMI_LIMITED;

  for (std::set<ApplicationSharedPtr>::iterator it = application_list_.begin();
       application_list_.end() != it;
       ++it) {
    ApplicationSharedPtr curr_app = *it;
    if (app->app_id() == curr_app->app_id()) {
      continue;
    }

    if (curr_app->hmi_level() == mobile_api::HMILevel::HMI_LIMITED) {
      result = mobile_api::HMILevel::HMI_BACKGROUND;
      break;
    }
    if (curr_app->hmi_level() == mobile_api::HMILevel::HMI_FULL) {
      if (curr_app->is_media_application()) {
        result = mobile_api::HMILevel::HMI_BACKGROUND;
        break;
      } else {
        result = mobile_api::HMILevel::HMI_LIMITED;
      }
    }

  }
  app->set_hmi_level(result);
  return result;
}

mobile_api::HMILevel::eType ApplicationManagerImpl::PutApplicationInFull(
  ApplicationSharedPtr app) {
  DCHECK(app.get())

  bool is_new_app_media = app->is_media_application();
  mobile_api::HMILevel::eType result = mobile_api::HMILevel::HMI_FULL;

  std::set<ApplicationSharedPtr>::iterator it = application_list_.begin();
  for (; application_list_.end() != it; ++it) {
    ApplicationSharedPtr curr_app = *it;
    if (app->app_id() == curr_app->app_id()) {
      continue;
    }

    if (is_new_app_media) {
      if (curr_app->hmi_level() == mobile_api::HMILevel::HMI_FULL) {
        if (curr_app->is_media_application()) {
          result = mobile_api::HMILevel::HMI_BACKGROUND;
          break;
        } else {
          result = mobile_api::HMILevel::HMI_LIMITED;
        }
      }
      if (curr_app->hmi_level() == mobile_api::HMILevel::HMI_LIMITED) {
        result = mobile_api::HMILevel::HMI_BACKGROUND;
        break;
      }
    } else {
      if (curr_app->hmi_level() == mobile_api::HMILevel::HMI_FULL) {
        result = mobile_api::HMILevel::HMI_BACKGROUND;
        break;
      }
      if (curr_app->hmi_level() == mobile_api::HMILevel::HMI_LIMITED) {
        result = mobile_api::HMILevel::HMI_FULL;
      }
    }
  }

  if (result == mobile_api::HMILevel::HMI_FULL) {
    app->set_hmi_level(result);
    MessageHelper::SendActivateAppToHMI(app->app_id());
  }
  return result;
}

void ApplicationManagerImpl::DeactivateApplication(ApplicationSharedPtr app) {
  MessageHelper::ResetGlobalproperties(app);
}

void ApplicationManagerImpl::ConnectToDevice(uint32_t id) {
  // TODO(VS): Call function from ConnectionHandler
  if (!connection_handler_) {
    LOG4CXX_WARN(logger_, "Connection handler is not set.");
    return;
  }

  connection_handler_->ConnectToDevice(id);
}

void ApplicationManagerImpl::OnHMIStartedCooperation() {
  hmi_cooperating_ = true;
  LOG4CXX_INFO(logger_, "ApplicationManagerImpl::OnHMIStartedCooperation()");

  utils::SharedPtr<smart_objects::SmartObject> is_vr_ready(
      MessageHelper::CreateModuleInfoSO(
          static_cast<uint32_t>(hmi_apis::FunctionID::VR_IsReady)));
  ManageHMICommand(is_vr_ready);

  utils::SharedPtr<smart_objects::SmartObject> is_tts_ready(
      MessageHelper::CreateModuleInfoSO(hmi_apis::FunctionID::TTS_IsReady));
  ManageHMICommand(is_tts_ready);

  utils::SharedPtr<smart_objects::SmartObject> is_ui_ready(
      MessageHelper::CreateModuleInfoSO(hmi_apis::FunctionID::UI_IsReady));
  ManageHMICommand(is_ui_ready);

  utils::SharedPtr<smart_objects::SmartObject> is_navi_ready(
      MessageHelper::CreateModuleInfoSO(
          hmi_apis::FunctionID::Navigation_IsReady));
  ManageHMICommand(is_navi_ready);

  utils::SharedPtr<smart_objects::SmartObject> is_ivi_ready(
      MessageHelper::CreateModuleInfoSO(
          hmi_apis::FunctionID::VehicleInfo_IsReady));
  ManageHMICommand(is_ivi_ready);

  utils::SharedPtr<smart_objects::SmartObject> button_capabilities(
      MessageHelper::CreateModuleInfoSO(
          hmi_apis::FunctionID::Buttons_GetCapabilities));
  ManageHMICommand(button_capabilities);
}

uint32_t ApplicationManagerImpl::GetNextHMICorrelationID() {
  if (corelation_id_ < max_corelation_id_) {
    corelation_id_++;
  } else {
    corelation_id_ = 0;
  }

  return corelation_id_;
}

bool ApplicationManagerImpl::begin_audio_pass_thru() {
  sync_primitives::AutoLock lock(audio_pass_thru_lock_);
  if (audio_pass_thru_active_) {
    return false;
  } else {
    audio_pass_thru_active_ = true;
    return true;
  }
}

bool ApplicationManagerImpl::end_audio_pass_thru() {
  sync_primitives::AutoLock lock(audio_pass_thru_lock_);
  if (audio_pass_thru_active_) {
    audio_pass_thru_active_ = false;
    return true;
  } else {
    return false;
  }
}

void ApplicationManagerImpl::set_driver_distraction(bool is_distracting) {
  is_distracting_driver_ = is_distracting;
}

void ApplicationManagerImpl::set_vr_session_started(const bool& state) {
  is_vr_session_strated_ = state;
}

void ApplicationManagerImpl::set_all_apps_allowed(const bool& allowed) {
  is_all_apps_allowed_ = allowed;
}

void ApplicationManagerImpl::StartAudioPassThruThread(int32_t session_key,
    int32_t correlation_id, int32_t max_duration, int32_t sampling_rate,
    int32_t bits_per_sample, int32_t audio_type
#ifdef MODIFY_FUNCTION_SIGN
		,bool is_save, bool is_send
		,const std::string &save_path
#endif
		) {
  LOG4CXX_INFO(logger_, "START MICROPHONE RECORDER");
	LOG4CXX_INFO(logger_, "is_save=" << is_save << " is_send=" << is_send);
  if (NULL != media_manager_) {
#ifdef MODIFY_FUNCTION_SIGN
		LOG4CXX_INFO(logger_, "sampling_rate=" << sampling_rate << ", bits_per_sample=" << bits_per_sample);
		if (sampling_rate < 0 || sampling_rate >= 4 || bits_per_sample < 0 || bits_per_sample >= 2)
			return;
		int32_t smpl_rate[4] = { 8000, 16000, 22050, 44100 };
		int32_t bit_rate[2] = {8,16};

		is_save_ = is_save;
		is_send_ = is_send;
		Global::utf8MultiToAnsiMulti(save_path, save_path_);
		
		LOG4CXX_INFO(logger_, "duration=" << max_duration << ", sampling_rate=" << smpl_rate[sampling_rate] << ", bits_per_sample=" << bit_rate[bits_per_sample] << ", audio_type=" << audio_type);
		msp_vrparm_set(MSP_VR_PASSTHRU, max_duration, smpl_rate[sampling_rate], bit_rate[bits_per_sample], audio_type);
		//msp_vr_set_state(MSP_RECORD_START);
		//timer_.setUnitRate(1000);
		//timer_.start(1);
		clearAudioPassThruData();
		msp_passthru_start(application_manager::ApplicationManagerImpl::instance()->onAudioPassThruDataSend);
		LOG4CXX_INFO(logger_, "msp_passthru_start()");
#else
    media_manager_->StartMicrophoneRecording(
      session_key,
      profile::Profile::instance()->recording_file_name(),
      max_duration);
#endif
  }
}

#ifdef MODIFY_FUNCTION_SIGN
void ApplicationManagerImpl::StartAudioPassThruReadFileThread(const std::string &read_path)
{
	Global::utf8MultiToAnsiMulti(read_path, read_path_);
	audio_pass_thru_read_file_thread_->start();
}


void ApplicationManagerImpl::StopAudioPassThruReadFileThread()
{
	audio_pass_thru_read_file_thread_->stop();
}
#endif

#ifdef MODIFY_FUNCTION_SIGN
void ApplicationManagerImpl::onAudioPassThruDataSend(char *data,int len)
{
	printf("passthru len=%d\n",  len);
	std::vector<unsigned char> vecData(len);
#ifdef OS_WINCE
	memcpy(vecData._Myfirst, data, len);
#else
	memcpy(vecData.data(), data, len);
#endif
	application_manager::ApplicationManagerImpl *application_manager = application_manager::ApplicationManagerImpl::instance();
	bool is_send = application_manager->is_send();
	bool is_save = application_manager->is_save();
	if (is_send){
		application_manager::ApplicationSharedPtr audible_app = application_manager->audible_application();
		if (audible_app){
			int app_id = audible_app->app_id();
			application_manager->SendAudioPassThroughNotification(app_id, vecData);
		}else{
			LOG4CXX_INFO(logger_, "audible_app is NULL");
		}
	}
	if (is_save){
		application_manager->appendAudioPassThruData(vecData);
	}
	
}
#endif

void ApplicationManagerImpl::SendAudioPassThroughNotification(
  uint32_t session_key,
  std::vector<uint8_t> binaryData) {
  LOG4CXX_TRACE_ENTER(logger_);

  if (!audio_pass_thru_active_) {
    LOG4CXX_ERROR(logger_, "Trying to send PassThroughNotification"
                  " when PassThrough is not active");
    return;
  }
  smart_objects::SmartObject* on_audio_pass = NULL;
  on_audio_pass = new smart_objects::SmartObject();

  if (NULL == on_audio_pass) {
    LOG4CXX_ERROR_EXT(logger_, "OnAudioPassThru NULL pointer");

    return;
  }

  LOG4CXX_INFO_EXT(logger_, "Fill smart object");

  (*on_audio_pass)[strings::params][strings::message_type] =
    application_manager::MessageType::kNotification;

  (*on_audio_pass)[strings::params][strings::connection_key] =
    static_cast<int32_t>(session_key);
  (*on_audio_pass)[strings::params][strings::function_id] =
    mobile_apis::FunctionID::OnAudioPassThruID;

  LOG4CXX_INFO_EXT(logger_, "Fill binary data");
  // binary data
  (*on_audio_pass)[strings::params][strings::binary_data] =
    smart_objects::SmartObject(binaryData);

  LOG4CXX_INFO_EXT(logger_, "After fill binary data");

  LOG4CXX_INFO_EXT(logger_, "Send data");
  CommandSharedPtr command =
    MobileCommandFactory::CreateCommand(&(*on_audio_pass));
  command->Init();
  command->Run();
  command->CleanUp();
}

void ApplicationManagerImpl::StopAudioPassThru(int32_t application_key) {
  LOG4CXX_TRACE_ENTER(logger_);
  sync_primitives::AutoLock lock(audio_pass_thru_lock_);
  if (NULL != media_manager_) {
		LOG4CXX_INFO(logger_, "if (NULL != media_manager_)");
#ifdef MODIFY_FUNCTION_SIGN
	  msp_passthru_stop();
		if (is_save_){
			LOG4CXX_INFO(logger_, "if (is_save_)");
			std::vector<uint8_t> data;
			getAudioPassThruData(data);
			LOG4CXX_INFO(logger_, "save_path_=" << save_path_ << " data size=" << data.size());
			//for (std::vector<uint8_t>::iterator iter = data.begin(); iter != data.end(); iter++){
			//	LOG4CXX_INFO(logger_, *iter << " ");
			//}
			int type = 0;
			int max_duration = 0;
			int sampling_rate = 0;
			int bits_per_sample = 0;
			int audio_type = 0;
			msp_vrparm_get(&type, &max_duration, &sampling_rate, &bits_per_sample, &audio_type);
			std::vector<uint8_t> wav_data;
			soxr_pcm_to_wav(data, sampling_rate, bits_per_sample, wav_data);
			LOG4CXX_INFO(logger_, "wav data size=" << wav_data.size());
			bool writeOK = file_system::Write(save_path_, wav_data);
			LOG4CXX_INFO(logger_, "file_system::Write save_path_ is '" << save_path_ << "' is " << writeOK);
		}
		StopAudioPassThruReadFileThread();
#else
    media_manager_->StopMicrophoneRecording(application_key);
#endif
  }
}

std::string ApplicationManagerImpl::GetDeviceName(
  connection_handler::DeviceHandle handle) {
  DCHECK(connection_handler_ != 0);

  std::string device_name = "";
  connection_handler::ConnectionHandlerImpl* con_handler_impl =
    static_cast<connection_handler::ConnectionHandlerImpl*>(
      connection_handler_);
  if (con_handler_impl->GetDataOnDeviceID(handle, &device_name,
                                          NULL) == -1) {
    LOG4CXX_ERROR(logger_, "Failed to extract device name for id " << handle);
  } else {
    LOG4CXX_INFO(logger_, "\t\t\t\t\tDevice name is " << device_name);
  }

  return device_name;
}

void ApplicationManagerImpl::OnMessageReceived(
  const protocol_handler::RawMessagePtr& message) {
  LOG4CXX_INFO(logger_, "ApplicationManagerImpl::OnMessageReceived");

  if (!message) {
    LOG4CXX_ERROR(logger_, "Null-pointer message received.");
    NOTREACHED();
    return;
  }

  utils::SharedPtr<Message> outgoing_message = ConvertRawMsgToMessage(message);

  if (outgoing_message) {
    messages_from_mobile_.PostMessage(
      impl::MessageFromMobile(outgoing_message));
  } else {
    LOG4CXX_WARN(logger_, "Incorrect message received");
  }
}

void ApplicationManagerImpl::OnMobileMessageSent(
  const protocol_handler::RawMessagePtr& message) {
  LOG4CXX_INFO(logger_, "ApplicationManagerImpl::OnMobileMessageSent");
}

void ApplicationManagerImpl::OnMessageReceived(
  hmi_message_handler::MessageSharedPointer message) {
  LOG4CXX_INFO(logger_, "ApplicationManagerImpl::OnMessageReceived");

  if (!message) {
    LOG4CXX_ERROR(logger_, "Null-pointer message received.");
    NOTREACHED();
    return;
  }

  messages_from_hmi_.PostMessage(impl::MessageFromHmi(message));
}

void ApplicationManagerImpl::OnErrorSending(
  hmi_message_handler::MessageSharedPointer message) {
  return;
}

void ApplicationManagerImpl::OnDeviceListUpdated(
  const connection_handler::DeviceList& device_list) {
  LOG4CXX_INFO(logger_, "ApplicationManagerImpl::OnDeviceListUpdated");

  smart_objects::SmartObject* update_list = new smart_objects::SmartObject;
  smart_objects::SmartObject& so_to_send = *update_list;
  so_to_send[jhs::S_PARAMS][jhs::S_FUNCTION_ID] =
    hmi_apis::FunctionID::BasicCommunication_UpdateDeviceList;
  so_to_send[jhs::S_PARAMS][jhs::S_MESSAGE_TYPE] =
    hmi_apis::messageType::request;
  so_to_send[jhs::S_PARAMS][jhs::S_PROTOCOL_VERSION] = 3;
  so_to_send[jhs::S_PARAMS][jhs::S_PROTOCOL_TYPE] = 1;
  so_to_send[jhs::S_PARAMS][jhs::S_CORRELATION_ID] = GetNextHMICorrelationID();
#ifdef MODIFY_FUNCTION_SIGN
  utils::SharedPtr<smart_objects::SmartObject> so_msg_params = MessageHelper::CreateDeviceListSO(
        device_list);
  smart_objects::SmartObject* msg_params = so_msg_params.get();
#else
  smart_objects::SmartObject* msg_params = MessageHelper::CreateDeviceListSO(
        device_list);
#endif
  if (!msg_params) {
    LOG4CXX_WARN(logger_, "Failed to create sub-smart object.");
    delete update_list;
    return;
  }
  so_to_send[jhs::S_MSG_PARAMS] = *msg_params;
  ManageHMICommand(update_list);
}

void ApplicationManagerImpl::OnApplicationListUpdated(
    const connection_handler::DeviceHandle& device_handle) {
  LOG4CXX_INFO(logger_, "OnApplicationListUpdated. device_handle" << device_handle);
  DCHECK(connection_handler_ != 0);

  std::string device_name = "";
  std::list<uint32_t> applications_ids;
  connection_handler::ConnectionHandlerImpl* con_handler_impl =
    static_cast<connection_handler::ConnectionHandlerImpl*>(
      connection_handler_);
  if (con_handler_impl->GetDataOnDeviceID(device_handle, &device_name,
                                          &applications_ids) == -1) {
    LOG4CXX_ERROR(logger_, "Failed to extract device list for id " << device_handle);
    return;
  }

  smart_objects::SmartObject* request = MessageHelper::CreateModuleInfoSO(
                                          hmi_apis::FunctionID::BasicCommunication_UpdateAppList);
  (*request)[strings::msg_params][strings::applications] =
      smart_objects::SmartObject(smart_objects::SmartType_Array);

  smart_objects::SmartObject& applications =
      (*request)[strings::msg_params][strings::applications];

  uint32_t app_count = 0;
  for (std::list<uint32_t>::iterator it = applications_ids.begin();
       it != applications_ids.end(); ++it) {
    ApplicationSharedPtr app = application(*it);

    if (!app.valid()) {
      LOG4CXX_ERROR(logger_, "application not foud , id = " << *it);
      continue;
    }

    smart_objects::SmartObject hmi_application(smart_objects::SmartType_Map);;
    if (false == MessageHelper::CreateHMIApplicationStruct(app, hmi_application)) {
      LOG4CXX_ERROR(logger_, "can't CreateHMIApplicationStruct ', id = " << *it);
      continue;
    }
    applications[app_count++] = hmi_application;
  }
  if (app_count > 0) {
    ManageHMICommand(request);
  } else {
    LOG4CXX_WARN(logger_, "Empty applications list");
  }
}

void ApplicationManagerImpl::RemoveDevice(
    const connection_handler::DeviceHandle& device_handle) {
  LOG4CXX_INFO(logger_, "device_handle " << device_handle);
}

bool ApplicationManagerImpl::IsAudioStreamingAllowed(uint32_t connection_key) const {
  ApplicationSharedPtr app = application(connection_key);

  if (!app) {
    LOG4CXX_INFO(logger_, "An application is not registered.");
    return false;
  }

  const mobile_api::HMILevel::eType& hmi_level = app->hmi_level();

  if (mobile_api::HMILevel::HMI_FULL == hmi_level ||
      mobile_api::HMILevel::HMI_LIMITED == hmi_level) {
    return true;
  }

  return false;
}

bool ApplicationManagerImpl::IsVideoStreamingAllowed(uint32_t connection_key) const {
  ApplicationSharedPtr app = application(connection_key);

  if (!app) {
    LOG4CXX_INFO(logger_, "An application is not registered.");
    return false;
  }

  const mobile_api::HMILevel::eType& hmi_level = app->hmi_level();

  if (mobile_api::HMILevel::HMI_FULL == hmi_level &&
#ifdef MODIFY_FUNCTION_SIGN
			true){
#else
      app->hmi_supports_navi_streaming()) {
#endif
    return true;
  }
#ifdef MODIFY_FUNCTION_SIGN
  LOG4CXX_INFO(logger_, "An application's level is not HMI_FULL. is " << hmi_level 
		<< ", hmi supports navi streaming is " << app->hmi_supports_navi_streaming());
#endif
  return false;
}

uint32_t ApplicationManagerImpl::GenerateGrammarID() {
  return rand();
}

uint32_t ApplicationManagerImpl::GenerateNewHMIAppID() {
  uint32_t hmi_app_id = rand();
  while (resume_ctrl_.IsHMIApplicationIdExist(hmi_app_id)) {
    hmi_app_id = rand();
  }

  return hmi_app_id;
}

void ApplicationManagerImpl::ReplaceMobileByHMIAppId(
    smart_objects::SmartObject& message) {
  MessageHelper::PrintSmartObject(message);
#ifndef BUILD_TARGET_LIB
  flush(std::cout);
#endif
  if (message.keyExists(strings::app_id)) {
    ApplicationSharedPtr application =
        ApplicationManagerImpl::instance()->application(
          message[strings::app_id].asUInt());
    if (application.valid()) {
      LOG4CXX_INFO(logger_, "ReplaceMobileByHMIAppId from " << message[strings::app_id].asInt()
                   << " to " << application->hmi_app_id());
      message[strings::app_id] = application->hmi_app_id();
    }
  } else {
    switch (message.getType()) {
      case smart_objects::SmartType::SmartType_Array: {
        smart_objects::SmartArray* message_array = message.asArray();
        smart_objects::SmartArray::iterator it = message_array->begin();
        for(; it != message_array->end(); ++it) {
          ReplaceMobileByHMIAppId(*it);
        }
        break;
      }
      case smart_objects::SmartType::SmartType_Map: {
        std::set<std::string> keys = message.enumerate();
        std::set<std::string>::const_iterator key = keys.begin();
        for (; key != keys.end(); ++key) {
          std::string k = *key;
          ReplaceMobileByHMIAppId(message[*key]);
        }
        break;
      }
      default: {
        break;
      }
    }
  }
}

void ApplicationManagerImpl::ReplaceHMIByMobileAppId(
  smart_objects::SmartObject& message) {
  if (message.keyExists(strings::app_id)) {
    ApplicationSharedPtr application =
      ApplicationManagerImpl::instance()->application_by_hmi_app(
        message[strings::app_id].asUInt());

    if (application.valid()) {
      LOG4CXX_INFO(logger_, "ReplaceHMIByMobileAppId from " << message[strings::app_id].asInt()
                   << " to " << application->app_id());
      message[strings::app_id] = application->app_id();
    }
  } else {
    switch (message.getType()) {
      case smart_objects::SmartType::SmartType_Array: {
        smart_objects::SmartArray* message_array = message.asArray();
        smart_objects::SmartArray::iterator it = message_array->begin();
        for(; it != message_array->end(); ++it) {
          ReplaceHMIByMobileAppId(*it);
        }
        break;
      }
      case smart_objects::SmartType::SmartType_Map: {
        std::set<std::string> keys = message.enumerate();
        std::set<std::string>::const_iterator key = keys.begin();
        for (; key != keys.end(); ++key) {
          ReplaceHMIByMobileAppId(message[*key]);
        }
        break;
      }
      default: {
        break;
      }
    }
  }
}

bool ApplicationManagerImpl::OnServiceStartedCallback(
  const connection_handler::DeviceHandle& device_handle,
  const int32_t& session_key,
  const protocol_handler::ServiceType& type) {
  LOG4CXX_INFO(logger_,
               "OnServiceStartedCallback " << type << " in session " << session_key);
  ApplicationSharedPtr app = application(session_key);

  switch (type) {
    case protocol_handler::kRpc: {
      LOG4CXX_INFO(logger_, "RPC service is about to be started.");
      break;
    }
    case protocol_handler::kMobileNav: {
      LOG4CXX_INFO(logger_, "Video service is about to be started.");
      if (media_manager_) {
        if (!app) {
          LOG4CXX_ERROR_EXT(logger_, "An application is not registered.");
          return false;
        }
        if (app->allowed_support_navigation()) {
          media_manager_->StartVideoStreaming(session_key);
        } else {
          return false;
        }
      }
      break;
    }
    case protocol_handler::kAudio: {
      LOG4CXX_INFO(logger_, "Audio service is about to be started.");
      if (media_manager_) {
        if (!app) {
          LOG4CXX_ERROR_EXT(logger_, "An application is not registered.");
          return false;
        }
        if (app->allowed_support_navigation()) {
          media_manager_->StartAudioStreaming(session_key);
        } else {
          return false;
        }
      }
      break;
    }
    default: {
      LOG4CXX_WARN(logger_, "Unknown type of service to be started.");
      break;
    }
  }

  return true;
}

void ApplicationManagerImpl::OnServiceEndedCallback(const int32_t& session_key,
    const protocol_handler::ServiceType& type) {
  LOG4CXX_INFO_EXT(
    logger_,
    "OnServiceEndedCallback " << type  << " in session " << session_key);

  switch (type) {
    case protocol_handler::kRpc: {
      LOG4CXX_INFO(logger_, "Remove application.");
      UnregisterApplication(session_key, mobile_apis::Result::INVALID_ENUM,
                            false);
		PRINTMSG(1, (L"\n%s, line:%d\n", __FUNCTIONW__, __LINE__));
      break;
    }
    case protocol_handler::kMobileNav: {
      LOG4CXX_INFO(logger_, "Stop video streaming.");
      if (media_manager_) {
        media_manager_->StopVideoStreaming(session_key);
      }
      break;
    }
    case protocol_handler::kAudio: {
      LOG4CXX_INFO(logger_, "Stop audio service.");
      if (media_manager_) {
        media_manager_->StopAudioStreaming(session_key);
      }
      break;
    }
    default:
      LOG4CXX_WARN(logger_, "Unknown type of service to be ended.");
      break;
  }
}

void ApplicationManagerImpl::set_hmi_message_handler(
  hmi_message_handler::HMIMessageHandler* handler) {
  hmi_handler_ = handler;
}

void ApplicationManagerImpl::set_connection_handler(
  connection_handler::ConnectionHandler* handler) {
  connection_handler_ = handler;
}

connection_handler::ConnectionHandler*
ApplicationManagerImpl::connection_handler() {
  return connection_handler_;
}

void ApplicationManagerImpl::set_protocol_handler(
  protocol_handler::ProtocolHandler* handler) {
  protocol_handler_ = handler;
}

void ApplicationManagerImpl::StartDevicesDiscovery() {
  connection_handler::ConnectionHandlerImpl::instance()->
  StartDevicesDiscovery();
}

void ApplicationManagerImpl::SendMessageToMobile(
  const utils::SharedPtr<smart_objects::SmartObject>& message,
  bool final_message) {
  LOG4CXX_INFO(logger_, "ApplicationManagerImpl::SendMessageToMobile");

  if (!message) {
    LOG4CXX_ERROR(logger_, "Null-pointer message received.");
    NOTREACHED();
    return;
  }

  if (!protocol_handler_) {
    LOG4CXX_WARN(logger_, "No Protocol Handler set");
    return;
  }

  ApplicationSharedPtr app = application(
                               (*message)[strings::params][strings::connection_key].asUInt());

  if (!app) {
    LOG4CXX_ERROR_EXT(logger_,
                      "No application associated with connection key");
    if ((*message)[strings::msg_params].keyExists(strings::result_code) &&
        ((*message)[strings::msg_params][strings::result_code] ==
         NsSmartDeviceLinkRPC::V1::Result::UNSUPPORTED_VERSION)) {
      (*message)[strings::params][strings::protocol_version] =
        ProtocolVersion::kV1;
    } else {
      (*message)[strings::params][strings::protocol_version] =
        ProtocolVersion::kV3;
    }
  } else {
    (*message)[strings::params][strings::protocol_version] =
      app->protocol_version();
  }

  mobile_so_factory().attachSchema(*message);
  LOG4CXX_INFO(
    logger_,
    "Attached schema to message, result if valid: " << message->isValid());

  // Messages to mobile are not yet prioritized so use default priority value
  utils::SharedPtr<Message> message_to_send(new Message(
        protocol_handler::MessagePriority::kDefault));
  if (!ConvertSOtoMessage((*message), (*message_to_send))) {
    LOG4CXX_WARN(logger_, "Can't send msg to Mobile: failed to create string");
    return;
  }

  smart_objects::SmartObject& msg_to_mobile = *message;
  if (msg_to_mobile[strings::params].keyExists(strings::correlation_id)) {
    request_ctrl_.terminateRequest(
      msg_to_mobile[strings::params][strings::correlation_id].asUInt());
  }

  messages_to_mobile_.PostMessage(impl::MessageToMobile(message_to_send,
                                  final_message));
#ifdef MODIFY_FUNCTION_SIGN
	SendSDLLogToHMI(message);
#endif
}

bool ApplicationManagerImpl::ManageMobileCommand(
  const utils::SharedPtr<smart_objects::SmartObject>& message) {
  LOG4CXX_INFO(logger_, "ApplicationManagerImpl::ManageMobileCommand");

  if (!message) {
    LOG4CXX_WARN(logger_, "Null-pointer message received.");
    NOTREACHED()
    return false;
  }

#ifdef DEBUG
  MessageHelper::PrintSmartObject(*message);
#endif

  LOG4CXX_INFO(logger_, "Trying to create message in mobile factory.");
  CommandSharedPtr command = MobileCommandFactory::CreateCommand(message);

  if (!command) {
    LOG4CXX_WARN(logger_, "Failed to create mobile command from smart object");
    return false;
  }
#ifdef MODIFY_FUNCTION_SIGN
	if ((*message)[strings::params][strings::message_type].asInt() == MessageType::kRequest){
		SendSDLLogToHMI(message);
	}
#endif

  mobile_apis::FunctionID::eType function_id =
    static_cast<mobile_apis::FunctionID::eType>(
      (*message)[strings::params][strings::function_id].asInt());

  // Notifications from HMI have no such parameter
  uint32_t correlation_id =
    (*message)[strings::params].keyExists(strings::correlation_id)
    ? (*message)[strings::params][strings::correlation_id].asUInt()
    : 0;

  uint32_t connection_key =
    (*message)[strings::params][strings::connection_key].asUInt();

  uint32_t protocol_type =
    (*message)[strings::params][strings::protocol_type].asUInt();

  ApplicationSharedPtr app;

  if (((mobile_apis::FunctionID::RegisterAppInterfaceID != function_id) &&
       (protocol_type == commands::CommandImpl::mobile_protocol_type_)) &&
      (mobile_apis::FunctionID::UnregisterAppInterfaceID != function_id)) {
    app = ApplicationManagerImpl::instance()->application(connection_key);
    if (!app) {
      LOG4CXX_ERROR_EXT(logger_, "APPLICATION_NOT_REGISTERED");
      smart_objects::SmartObject* response =
        MessageHelper::CreateNegativeResponse(
          connection_key,
          static_cast<int32_t>(function_id),
          correlation_id,
          static_cast<int32_t>(mobile_apis::Result::APPLICATION_NOT_REGISTERED));

      SendMessageToMobile(response);
      return false;
    }

    // Message for "CheckPermission" must be with attached schema
    mobile_so_factory().attachSchema(*message);

    if (policy_manager_) {
      const std::string stringified_functionID =
          MessageHelper::StringifiedFunctionID(function_id);
      LOG4CXX_INFO(
        logger_,
        "Checking permissions for  " << app->mobile_app_id()->asString()  <<
        " in " << MessageHelper::StringifiedHMILevel(app->hmi_level()) <<
        " rpc " << stringified_functionID);
      policy::CheckPermissionResult result = policy_manager_->CheckPermissions(
          app->mobile_app_id()->asString(),
          MessageHelper::StringifiedHMILevel(app->hmi_level()),
          stringified_functionID);

      if (app->hmi_level() == mobile_apis::HMILevel::HMI_NONE
          && function_id != mobile_apis::FunctionID::UnregisterAppInterfaceID) {
        app->usage_report().RecordRpcSentInHMINone();
      }

      if (result.hmi_level_permitted != policy::kRpcAllowed) {
        LOG4CXX_WARN(logger_, "Request blocked by policies. "
                     << "Function: "
                     << stringified_functionID
                     << ", FunctionID: "
                     << static_cast<int32_t>(function_id)
                     << " Application HMI status: "
                     << static_cast<int32_t>(app->hmi_level()));

        app->usage_report().RecordPolicyRejectedRpcCall();

        mobile_apis::Result::eType check_result =
          mobile_apis::Result::DISALLOWED;

        switch (result.hmi_level_permitted) {
          case policy::kRpcDisallowed:
            check_result = mobile_apis::Result::DISALLOWED;
            break;
          case policy::kRpcUserDisallowed:
            check_result = mobile_apis::Result::USER_DISALLOWED;
            break;
          default:
            check_result = mobile_apis::Result::INVALID_ENUM;
            break;
        }

        smart_objects::SmartObject* response =
          MessageHelper::CreateBlockedByPoliciesResponse(function_id,
              check_result, correlation_id, connection_key);

        ApplicationManagerImpl::instance()->SendMessageToMobile(response);
        return true;
      }
    }
  }

  if (command->Init()) {
    if ((*message)[strings::params][strings::message_type].asInt() ==
        mobile_apis::messageType::request) {
      // get application hmi level
      mobile_api::HMILevel::eType app_hmi_level =
        mobile_api::HMILevel::INVALID_ENUM;
      if (app) {
        app_hmi_level = app->hmi_level();
      }

      request_controller::RequestController::TResult result =
        request_ctrl_.addRequest(command, app_hmi_level);

      if (result == request_controller::RequestController::SUCCESS) {
        LOG4CXX_INFO(logger_, "Perform request");
      } else if (result ==
                 request_controller::RequestController::
                 TOO_MANY_PENDING_REQUESTS) {
        LOG4CXX_ERROR_EXT(logger_, "Unable to perform request: " <<
                          "TOO_MANY_PENDING_REQUESTS");

        smart_objects::SmartObject* response =
          MessageHelper::CreateNegativeResponse(
            connection_key,
            static_cast<int32_t>(function_id),
            correlation_id,
            static_cast<int32_t>(mobile_apis::Result::TOO_MANY_PENDING_REQUESTS));

        SendMessageToMobile(response);
        return false;
      } else if (result ==
                 request_controller::RequestController::TOO_MANY_REQUESTS) {
        LOG4CXX_ERROR_EXT(logger_, "Unable to perform request: " <<
                          "TOO_MANY_REQUESTS");

        MessageHelper::SendOnAppInterfaceUnregisteredNotificationToMobile(
          connection_key,
          mobile_api::AppInterfaceUnregisteredReason::TOO_MANY_REQUESTS);

        UnregisterApplication(connection_key,
                              mobile_apis::Result::TOO_MANY_PENDING_REQUESTS,
                              false);
		PRINTMSG(1, (L"\n%s, line:%d\n", __FUNCTIONW__, __LINE__));
        return false;
      } else if (result ==
                 request_controller::RequestController::
                 NONE_HMI_LEVEL_MANY_REQUESTS) {
        LOG4CXX_ERROR_EXT(logger_, "Unable to perform request: " <<
                          "REQUEST_WHILE_IN_NONE_HMI_LEVEL");

        MessageHelper::SendOnAppInterfaceUnregisteredNotificationToMobile(
          connection_key, mobile_api::AppInterfaceUnregisteredReason::
          REQUEST_WHILE_IN_NONE_HMI_LEVEL);

        UnregisterApplication(connection_key, mobile_apis::Result::INVALID_ENUM,
                              false);
		PRINTMSG(1, (L"\n%s, line:%d\n", __FUNCTIONW__, __LINE__));
        return false;
      } else {
        LOG4CXX_ERROR_EXT(logger_, "Unable to perform request: Unknown case");
        return false;
      }
    }

    command->Run();
  }

  return true;
}

void ApplicationManagerImpl::SendMessageToHMI(
  const utils::SharedPtr<smart_objects::SmartObject>& message) {
  LOG4CXX_INFO(logger_, "ApplicationManagerImpl::SendMessageToHMI");

  if (!message) {
    LOG4CXX_WARN(logger_, "Null-pointer message received.");
    NOTREACHED();
    return;
  }

  if (!hmi_handler_) {
    LOG4CXX_WARN(logger_, "No HMI Handler set");
    return;
  }

  // SmartObject |message| has no way to declare priority for now
  utils::SharedPtr<Message> message_to_send(
    new Message(protocol_handler::MessagePriority::kDefault));
  if (!message_to_send) {
    LOG4CXX_ERROR(logger_, "Null pointer");
    return;
  }

  hmi_so_factory().attachSchema(*message);
  LOG4CXX_INFO(
    logger_,
    "Attached schema to message, result if valid: " << message->isValid());

#ifdef HMI_JSON_API
  if (!ConvertSOtoMessage(*message, *message_to_send)) {
    LOG4CXX_WARN(logger_,
                 "Cannot send message to HMI: failed to create string");
    return;
  }
#endif  // HMI_JSON_API

#ifdef HMI_DBUS_API
  message_to_send->set_smart_object(*message);
#endif  // HMI_DBUS_API

  messages_to_hmi_.PostMessage(impl::MessageToHmi(message_to_send));
}

bool ApplicationManagerImpl::ManageHMICommand(
  const utils::SharedPtr<smart_objects::SmartObject>& message) {
  LOG4CXX_INFO(logger_, "ApplicationManagerImpl::ManageHMICommand");

  if (!message) {
    LOG4CXX_WARN(logger_, "Null-pointer message received.");
    NOTREACHED();
    return false;
  }

#ifdef DEBUG
  MessageHelper::PrintSmartObject(*message);
#endif

  CommandSharedPtr command = HMICommandFactory::CreateCommand(message);

  if (!command) {
    LOG4CXX_WARN(logger_, "Failed to create command from smart object");
    return false;
  }

  if (command->Init()) {
    command->Run();
    if (command->CleanUp()) {
      return true;
    }
  }
  return false;
}

#ifdef MODIFY_FUNCTION_SIGN
void ApplicationManagerImpl::SendSDLLogToHMI(const utils::SharedPtr<smart_objects::SmartObject> &pMessage)
{
	std::string strMessageType;
	// timeout 
	//   int nResultCode = (*pMessage)[strings::msg_params][strings::result_code];
	//   LOG4CXX_INFO(logger_, "ApplicationManagerImpl::SendSDLLogToHMI nResultCode = " << nResultCode);
	//   if(nResultCode == mobile_api::Result::TIMED_OUT){
	//     LOG4CXX_INFO(logger_, "if(nResultCode == mobile_api::Result::TIMED_OUT){");
	//     return;
	//   }
	// 	printf("patrick-pan: message_type=%d\n", (*pMessage)[strings::params][strings::message_type].asInt());
	// 	printf("patrick-pan: message_type=%s\n", (*pMessage)[strings::params][strings::message_type].asString().c_str());
	if (!pMessage) {
		LOG4CXX_ERROR(logger_, "Null-pointer message received.");
		return;
	}
	// convert enums(as int) to strings
	smart_objects::SmartObject formattedMessage(*pMessage);
	try {
		formattedMessage.getSchema().unapplySchema(formattedMessage);  // converts enums(as int) to strings
	}
	catch (...) {
		LOG4CXX_ERROR(logger_, "format message fail.");
	}
	
	formattedMessage[strings::params][strings::connection_key] = (*pMessage)[strings::params][strings::connection_key].asUInt();
	
	NsSmartDeviceLink::NsSmartObjects::SmartObject *result =
		new NsSmartDeviceLink::NsSmartObjects::SmartObject;

	if (!result) {
		LOG4CXX_ERROR(logger_, "Memory allocation failed.");
		return;
	}
	NsSmartDeviceLink::NsSmartObjects::SmartObject& notify = *result;

	notify[strings::params][strings::message_type] = MessageType::kNotification;
	notify[strings::params][strings::function_id] = hmi_apis::FunctionID::BasicCommunication_SDLLog;
	notify[strings::params][strings::protocol_type] = 1;
	notify[strings::params][strings::protocol_version] = ProtocolVersion::kV2;
	std::string strFunctionName = formattedMessage[strings::params][strings::function_id].asString();
	strFunctionName = strFunctionName.substr(0, strFunctionName.length() - 2);
	uint32_t app_id = formattedMessage[strings::params][strings::connection_key].asUInt();
	notify[strings::msg_params]["app_id"] = app_id;
	notify[strings::msg_params]["correlation_id"] = formattedMessage[strings::params][strings::correlation_id];
	notify[strings::msg_params]["function"] = strFunctionName;
	notify[strings::msg_params]["type"] = formattedMessage[strings::params][strings::message_type].asString();
	notify[strings::msg_params]["data"] = formattedMessage[strings::msg_params];
	//SendMessageToHMI(result);

	ManageHMICommand(result);

}
#endif

void ApplicationManagerImpl::CreateHMIMatrix(HMIMatrix* matrix) {
}

void ApplicationManagerImpl::CreatePoliciesManager() {
  LOG4CXX_INFO(logger_, "CreatePoliciesManager");
  policy_manager_ = policy::PolicyHandler::instance()->LoadPolicyLibrary();
  if (policy_manager_) {
    LOG4CXX_INFO(logger_, "Policy library is loaded, now initing PT");
    policy::PolicyHandler::instance()->InitPolicyTable();
  }
}

bool ApplicationManagerImpl::CheckPolicies(smart_objects::SmartObject* message,
    ApplicationSharedPtr app) {
  return true;
}

bool ApplicationManagerImpl::CheckHMIMatrix(
  smart_objects::SmartObject* message) {
  return true;
}

bool ApplicationManagerImpl::ConvertMessageToSO(
  const Message& message, smart_objects::SmartObject& output) {
  LOG4CXX_INFO(
    logger_,
    "\t\t\tMessage to convert: protocol " << message.protocol_version()
//#ifdef MODIFY_FUNCTION_SIGN
//		<< "; connection_key " << message.connection_key()
//#endif
    << "; json " << message.json_message());

  switch (message.protocol_version()) {
    case ProtocolVersion::kV3:
    case ProtocolVersion::kV2: {
      if (!formatters::CFormatterJsonSDLRPCv2::fromString(
            message.json_message(),
            output,
            message.function_id(),
            message.type(),
            message.correlation_id())
          || !mobile_so_factory().attachSchema(output)
          || ((output.validate() != smart_objects::Errors::OK)
              && (output.validate() !=
                  smart_objects::Errors::UNEXPECTED_PARAMETER))) {
        LOG4CXX_WARN(logger_, "Failed to parse string to smart object :"
                     << message.json_message());
#ifdef MODIFY_FUNCTION_SIGN
				output[strings::params][strings::connection_key] =
					message.connection_key();
				smart_objects::SmartObject* pRequest = new smart_objects::SmartObject(output);
				SendSDLLogToHMI(pRequest);
#endif
        utils::SharedPtr<smart_objects::SmartObject> response(
          MessageHelper::CreateNegativeResponse(
            message.connection_key(), message.function_id(),
            message.correlation_id(), mobile_apis::Result::INVALID_DATA));
        ManageMobileCommand(response);
        return false;
      }
      LOG4CXX_INFO(
        logger_,
        "Convertion result for sdl object is true" << " function_id "
        << output[jhs::S_PARAMS][jhs::S_FUNCTION_ID].asInt());
      output[strings::params][strings::connection_key] =
        message.connection_key();
      output[strings::params][strings::protocol_version] =
        message.protocol_version();
      if (message.binary_data()) {
        output[strings::params][strings::binary_data] =
          *(message.binary_data());
      }
      break;
    }
    case ProtocolVersion::kHMI: {
      int32_t result = formatters::FormatterJsonRpc::FromString <
                       hmi_apis::FunctionID::eType, hmi_apis::messageType::eType > (
                         message.json_message(), output);
      LOG4CXX_INFO(
        logger_,
        "Convertion result: " << result << " function id "
        << output[jhs::S_PARAMS][jhs::S_FUNCTION_ID].asInt());
      if (!hmi_so_factory().attachSchema(output)) {
        LOG4CXX_WARN(logger_, "Failed to attach schema to object.");
        return false;
      }
      if (output.validate() != smart_objects::Errors::OK &&
          output.validate() != smart_objects::Errors::UNEXPECTED_PARAMETER) {
        LOG4CXX_WARN(
          logger_,
          "Incorrect parameter from HMI");
        output.erase(strings::msg_params);
        output[strings::params][hmi_response::code] =
          hmi_apis::Common_Result::INVALID_DATA;
        output[strings::msg_params][strings::info] =
          std::string("Received invalid data on HMI response");
      }
      break;
    }
    case ProtocolVersion::kV1: {
      static NsSmartDeviceLinkRPC::V1::v4_protocol_v1_2_no_extra v1_shema;

      if (message.function_id() == 0 || message.type() == kUnknownType) {
        LOG4CXX_ERROR(logger_, "Message received: UNSUPPORTED_VERSION");

        int32_t conversation_result =
          formatters::CFormatterJsonSDLRPCv1::fromString <
          NsSmartDeviceLinkRPC::V1::FunctionID::eType,
          NsSmartDeviceLinkRPC::V1::messageType::eType > (
            message.json_message(), output);

        if (formatters::CFormatterJsonSDLRPCv1::kSuccess
            == conversation_result) {

          smart_objects::SmartObject params = smart_objects::SmartObject(smart_objects::SmartType::SmartType_Map);

          output[strings::params][strings::message_type] =
            NsSmartDeviceLinkRPC::V1::messageType::response;
          output[strings::params][strings::connection_key] = message.connection_key();

          output[strings::msg_params] =
            smart_objects::SmartObject(smart_objects::SmartType::SmartType_Map);
          output[strings::msg_params][strings::success] = false;
          output[strings::msg_params][strings::result_code] =
            NsSmartDeviceLinkRPC::V1::Result::UNSUPPORTED_VERSION;

          smart_objects::SmartObject* msg_to_send = new smart_objects::SmartObject(output);
          v1_shema.attachSchema(*msg_to_send);
          SendMessageToMobile(msg_to_send);
          return false;
        }
      }

      break;
    }
    default:
      // TODO(PV):
      //  removed NOTREACHED() because some app can still have vesion 1.
      LOG4CXX_WARN(
        logger_,
        "Application used unsupported protocol :" << message.protocol_version()
        << ".");
      return false;
  }

  LOG4CXX_INFO(logger_, "Successfully parsed message into smart object");
  return true;
}

bool ApplicationManagerImpl::ConvertSOtoMessage(
  const smart_objects::SmartObject& message, Message& output) {
  LOG4CXX_INFO(logger_, "Message to convert");

  if (smart_objects::SmartType_Null == message.getType()
      || smart_objects::SmartType_Invalid == message.getType()) {
    LOG4CXX_WARN(logger_, "Invalid smart object received.");
    return false;
  }

  LOG4CXX_INFO(
    logger_,
    "Message with protocol: "
    << message.getElement(jhs::S_PARAMS).getElement(jhs::S_PROTOCOL_TYPE)
    .asInt());

  std::string output_string;
  switch (message.getElement(jhs::S_PARAMS).getElement(jhs::S_PROTOCOL_TYPE)
          .asInt()) {
    case 0: {
      if (message.getElement(jhs::S_PARAMS).getElement(jhs::S_PROTOCOL_VERSION).asInt() == 1) {
        if (!formatters::CFormatterJsonSDLRPCv1::toString(message,
            output_string)) {
          LOG4CXX_WARN(logger_, "Failed to serialize smart object");
          return false;
        }
        output.set_protocol_version(application_manager::kV1);
      } else {
        if (!formatters::CFormatterJsonSDLRPCv2::toString(message,
            output_string)) {
          LOG4CXX_WARN(logger_, "Failed to serialize smart object");
          return false;
        }
        output.set_protocol_version(
          static_cast<ProtocolVersion>(
            message.getElement(jhs::S_PARAMS).getElement(
              jhs::S_PROTOCOL_VERSION).asUInt()));
      }

      break;
    }
    case 1: {
      if (!formatters::FormatterJsonRpc::ToString(message, output_string)) {
        LOG4CXX_WARN(logger_, "Failed to serialize smart object");
        return false;
      }
      output.set_protocol_version(application_manager::kHMI);
      break;
    }
    default:
      NOTREACHED();
      return false;
  }

  LOG4CXX_INFO(logger_, "Convertion result: " << output_string);

  output.set_connection_key(
    message.getElement(jhs::S_PARAMS).getElement(strings::connection_key)
    .asInt());

  output.set_function_id(
    message.getElement(jhs::S_PARAMS).getElement(jhs::S_FUNCTION_ID).asInt());

  output.set_correlation_id(
    message.getElement(jhs::S_PARAMS).getElement(jhs::S_CORRELATION_ID)
    .asInt());
  output.set_message_type(
    static_cast<MessageType>(message.getElement(jhs::S_PARAMS).getElement(
                               jhs::S_MESSAGE_TYPE).asInt()));

  // Currently formatter creates JSON = 3 bytes for empty SmartObject.
  // workaround for notification. JSON must be empty
  if (mobile_apis::FunctionID::OnAudioPassThruID
      != message.getElement(jhs::S_PARAMS).getElement(strings::function_id)
      .asInt()) {
    output.set_json_message(output_string);
  }

  if (message.getElement(jhs::S_PARAMS).keyExists(strings::binary_data)) {
    application_manager::BinaryData* binaryData =
      new application_manager::BinaryData(
      message.getElement(jhs::S_PARAMS).getElement(strings::binary_data)
      .asBinary());

    if (NULL == binaryData) {
      LOG4CXX_ERROR(logger_, "Null pointer");
      return false;
    }
    output.set_binary_data(binaryData);
  }

  LOG4CXX_INFO(logger_, "Successfully parsed smart object into message");
  return true;
}

utils::SharedPtr<Message> ApplicationManagerImpl::ConvertRawMsgToMessage(
  const protocol_handler::RawMessagePtr& message) {
  DCHECK(message);
  utils::SharedPtr<Message> outgoing_message;

  LOG4CXX_INFO(logger_, "Service type." << message->service_type());

  if (message->service_type() != protocol_handler::kRpc
      &&
      message->service_type() != protocol_handler::kBulk) {
    // skip this message, not under handling of ApplicationManager
    LOG4CXX_INFO(logger_, "Skipping message; not the under AM handling.");
    return outgoing_message;
  }

  Message* convertion_result = NULL;
  if (message->protocol_version() == 1) {
    convertion_result =
      MobileMessageHandler::HandleIncomingMessageProtocolV1(message);
  } else if ((message->protocol_version() == 2) ||
             (message->protocol_version() == 3)) {
    convertion_result =
      MobileMessageHandler::HandleIncomingMessageProtocolV2(message);
  } else {
    LOG4CXX_WARN(logger_, "Unknown protocol version.");
    return outgoing_message;
  }

  if (convertion_result) {
    outgoing_message = convertion_result;
  } else {
    LOG4CXX_ERROR(logger_, "Received invalid message");
  }
  return outgoing_message;
}

void ApplicationManagerImpl::ProcessMessageFromMobile(
  const utils::SharedPtr<Message>& message) {
  LOG4CXX_INFO(logger_, "ApplicationManagerImpl::ProcessMessageFromMobile()");
#ifdef TIME_TESTER
  AMMetricObserver::MessageMetricSharedPtr metric(new AMMetricObserver::MessageMetric());
  metric->begin = date_time::DateTime::getCurrentTime();
#endif  // TIME_TESTER
  utils::SharedPtr<smart_objects::SmartObject> so_from_mobile(
    new smart_objects::SmartObject);

  if (!so_from_mobile) {
    LOG4CXX_ERROR(logger_, "Null pointer");
    return;
  }

  if (!ConvertMessageToSO(*message, *so_from_mobile)) {
    LOG4CXX_ERROR(logger_, "Cannot create smart object from message");
    return;
  }
#ifdef TIME_TESTER
  metric->message = so_from_mobile;
#endif  // TIME_TESTER

  if (!ManageMobileCommand(so_from_mobile)) {
    LOG4CXX_ERROR(logger_, "Received command didn't run successfully");
  }
#ifdef TIME_TESTER
  metric->end = date_time::DateTime::getCurrentTime();
  if (metric_observer_) {
    metric_observer_->OnMessage(metric);
  }
#endif  // TIME_TESTER
}

void ApplicationManagerImpl::ProcessMessageFromHMI(
  const utils::SharedPtr<Message>& message) {
  LOG4CXX_INFO(logger_, "ApplicationManagerImpl::ProcessMessageFromHMI()");
  utils::SharedPtr<smart_objects::SmartObject> smart_object(
    new smart_objects::SmartObject);

  if (!smart_object) {
    LOG4CXX_ERROR(logger_, "Null pointer");
    return;
  }

#ifdef HMI_JSON_API
  if (!ConvertMessageToSO(*message, *smart_object)) {
    LOG4CXX_ERROR(logger_, "Cannot create smart object from message");
    return;
  }
#endif  // HMI_JSON_API

#ifdef HMI_DBUS_API
  *smart_object = message->smart_object();
#endif  // HMI_DBUS_API

  LOG4CXX_INFO(logger_, "Converted message, trying to create hmi command");
  if (!ManageHMICommand(smart_object)) {
    LOG4CXX_ERROR(logger_, "Received command didn't run successfully");
  }
}

hmi_apis::HMI_API& ApplicationManagerImpl::hmi_so_factory() {
  if (!hmi_so_factory_) {
    hmi_so_factory_ = new hmi_apis::HMI_API;
    if (!hmi_so_factory_) {
      LOG4CXX_ERROR(logger_, "Out of memory");
      NOTREACHED();
    }
  }
  return *hmi_so_factory_;
}

mobile_apis::MOBILE_API& ApplicationManagerImpl::mobile_so_factory() {
  if (!mobile_so_factory_) {
    mobile_so_factory_ = new mobile_apis::MOBILE_API;
    if (!mobile_so_factory_) {
      LOG4CXX_ERROR(logger_, "Out of memory.");
      NOTREACHED();
    }
  }
  return *mobile_so_factory_;
}

HMICapabilities& ApplicationManagerImpl::hmi_capabilities() {
  return hmi_capabilities_;
}

#ifdef TIME_TESTER
void ApplicationManagerImpl::SetTimeMetricObserver(AMMetricObserver* observer) {
  metric_observer_ = observer;
}
#endif  // TIME_TESTER

void ApplicationManagerImpl::addNotification(const CommandSharedPtr& ptr) {
  notification_list_.push_back(ptr);
}

void ApplicationManagerImpl::removeNotification(const CommandSharedPtr& ptr) {
  std::list<CommandSharedPtr>::iterator it = notification_list_.begin();
  for (; notification_list_.end() != it; ++it) {
    if (*it == ptr) {
      notification_list_.erase(it);
      break;
    }
  }
}

void ApplicationManagerImpl::updateRequestTimeout(uint32_t connection_key,
    uint32_t mobile_correlation_id,
    uint32_t new_timeout_value) {
  request_ctrl_.updateRequestTimeout(connection_key, mobile_correlation_id,
                                     new_timeout_value);
}

#ifdef MODIFY_FUNCTION_SIGN
void ApplicationManagerImpl::getAudioPassThruData(std::vector<unsigned char>& data)
{
	data = audio_pass_thru_data_;
}

void ApplicationManagerImpl::appendAudioPassThruData(std::vector<unsigned char> &data)
{
	audio_pass_thru_data_.insert(audio_pass_thru_data_.end(), data.begin(), data.end());
}

void ApplicationManagerImpl::clearAudioPassThruData()
{
	audio_pass_thru_data_.clear();
}

bool ApplicationManagerImpl::is_save()
{
	return is_save_;
}

bool ApplicationManagerImpl::is_send()
{
	return is_send_;
}

const std::string& ApplicationManagerImpl::save_path()
{
	return save_path_;
}

const std::string& ApplicationManagerImpl::read_path()
{
	return read_path_;
}

#endif
		
#ifdef MODIFY_FUNCTION_SIGN
VRStatus ApplicationManagerImpl::handleVRCommand(const int vrCommandId, const std::string& vrCommandName)
{
	LOG4CXX_INFO(logger_, "vrCommandId is " << vrCommandId);
	ApplicationSharedPtr active_app = active_application();
	int max_cmd_id = profile::Profile::instance()->max_cmd_id();
	if(vrCommandId >= VRASSISTCOMMAND_HELP && vrCommandId <= VRASSISTCOMMAND_HELP_MAX){
		// handle help
		if(vrCommandId == VRASSISTCOMMAND_HELP){
			return VRSTATUS_FAIL;
		}
		int app_id = vrCommandId - VRASSISTCOMMAND_HELP;
		if(app_id == active_app->app_id()){
			MessageHelper::SendVRCommandHelpToHMI(vrCommandName);
			MessageHelper::SendVRCommandTTSToHMI();
		}
		return VRSTATUS_NULL;
	}else if(vrCommandId >= VRASSISTCOMMAND_EXIT && vrCommandId <= VRASSISTCOMMAND_EXIT_MAX){
		// handle exit
		if(vrCommandId == VRASSISTCOMMAND_EXIT){
			MessageHelper::SendVRCancelToHMI();
			return VRSTATUS_SUCCESS;
		}
		int app_id = vrCommandId - VRASSISTCOMMAND_EXIT;
		if(app_id == active_app->app_id()){
			MessageHelper::SendVRExitAppToHMI();
			return VRSTATUS_SUCCESS;
		}
		return VRSTATUS_FAIL;
	}else if(vrCommandId == VRASSISTCOMMAND_CANCEL){
		// handle cancel
		MessageHelper::SendVRCancelToHMI();
		return VRSTATUS_SUCCESS;
	}else{
		if(active_app){
			if(vrCommandId > max_cmd_id + 1){
				// handle switch app vr
				int app_id = vrCommandId - max_cmd_id;
				//if(active_app->app_id() != app_id){
					MessageHelper::SendVRSwitchAppToHMI(app_id, vrCommandName);
				//}
				return VRSTATUS_SUCCESS;
			}else{
				// handle app's vr
	// 			MessageHelper::SendVROnCommandToMobile(vrCommandId, active_app->app_id());
				MessageHelper::SendVRResultToHMI(vrCommandId, vrCommandName);
				return VRSTATUS_SUCCESS;
			}
		}else{
			if(vrCommandId > max_cmd_id + 1){
				// handle switch app vr
				int app_id = vrCommandId - max_cmd_id;
				MessageHelper::SendVRSwitchAppToHMI(app_id, vrCommandName);
				return VRSTATUS_SUCCESS;
			}else{
				return VRSTATUS_FAIL;
			}
		}
	}
}
		
ApplicationSharedPtr ApplicationManagerImpl::fetchAppliation(const std::string& vrCommandName)
{
	const std::set<ApplicationSharedPtr>& apps = ApplicationManagerImpl::instance()
                                         ->applications();

#ifdef OS_WINCE
	for(std::set<ApplicationSharedPtr>::const_iterator it = ApplicationManagerImpl::instance()->applications().begin();
#else
	for(std::set<ApplicationSharedPtr>::iterator it = ApplicationManagerImpl::instance()->applications().begin();
#endif
      it != ApplicationManagerImpl::instance()->applications().end(); it++){
		if ((*it)->vr_synonyms()) {
			const smart_objects::SmartArray* vr_synonyms = (*it)->vr_synonyms()->asArray();
			smart_objects::SmartArray::const_iterator it_array =
					vr_synonyms->begin();
			smart_objects::SmartArray::const_iterator it_array_end =
					vr_synonyms->end();
			for (; it_array != it_array_end; ++it_array) {
// 					LOG4CXX_INFO(logger_, (*it_array).asString().c_str());
				LOG4CXX_INFO(logger_, "my_app_id = " << (*it)->app_id());
				if(std::string::npos != vrCommandName.find((*it_array).asString())){
					return (*it);
				}
			}
		}
	}
	return ApplicationSharedPtr();
}

#endif

const uint32_t ApplicationManagerImpl::application_id
(const int32_t correlation_id) {
  // ykazakov: there is no erase for const iterator for QNX
  std::map<const int32_t, const uint32_t>::iterator it =
    appID_list_.find(correlation_id);
  if (appID_list_.end() != it) {
    const uint32_t app_id = it->second;
    appID_list_.erase(it);
    return app_id;
  } else {
    return 0;
  }
}

void ApplicationManagerImpl::set_application_id(const int32_t correlation_id,
    const uint32_t app_id) {
  appID_list_.insert(std::pair<const int32_t, const uint32_t>
                     (correlation_id, app_id));
}

void ApplicationManagerImpl::SetUnregisterAllApplicationsReason(
  mobile_api::AppInterfaceUnregisteredReason::eType reason) {
  unregister_reason_ = reason;
}

void ApplicationManagerImpl::HeadUnitReset(
  mobile_api::AppInterfaceUnregisteredReason::eType reason) {
  switch (reason) {
    case mobile_api::AppInterfaceUnregisteredReason::MASTER_RESET:
      policy::PolicyHandler::instance()->ResetPolicyTable();
      break;
    case mobile_api::AppInterfaceUnregisteredReason::FACTORY_DEFAULTS:
      policy::PolicyHandler::instance()->ClearUserConsent();
      break;
  }
}

void ApplicationManagerImpl::UnregisterAllApplications() {
  LOG4CXX_INFO(logger_, "ApplicationManagerImpl::UnregisterAllApplications " <<
               unregister_reason_);

  hmi_cooperating_ = false;

  bool is_ignition_off =
      unregister_reason_ ==
      mobile_api::AppInterfaceUnregisteredReason::IGNITION_OFF ? true : false;

  std::set<ApplicationSharedPtr>::iterator it = application_list_.begin();
  while (it != application_list_.end()) {
    MessageHelper::SendOnAppInterfaceUnregisteredNotificationToMobile(
      (*it)->app_id(), unregister_reason_);

    UnregisterApplication((*it)->app_id(), mobile_apis::Result::INVALID_ENUM,
                          is_ignition_off);
	PRINTMSG(1, (L"\n%s, line:%d\n", __FUNCTIONW__, __LINE__));
    it = application_list_.begin();    
  }
  if (is_ignition_off) {
   resume_controller().IgnitionOff();
  }
}

void ApplicationManagerImpl::UnregisterApplication(
  const uint32_t& app_id, mobile_apis::Result::eType reason,
  bool is_resuming) {
  LOG4CXX_INFO(logger_,
               "ApplicationManagerImpl::UnregisterApplication " << app_id);
  PRINTMSG(1, (L"ApplicationManagerImpl::UnregisterApplication()\n"));

  switch (reason) {
    case mobile_apis::Result::DISALLOWED:
    case mobile_apis::Result::USER_DISALLOWED:
    case mobile_apis::Result::INVALID_CERT:
    case mobile_apis::Result::EXPIRED_CERT:
    case mobile_apis::Result::TOO_MANY_PENDING_REQUESTS: {
      application(app_id)->usage_report().RecordRemovalsForBadBehavior();
      break;
    }
  }
  ApplicationSharedPtr app_to_remove;
  {
    sync_primitives::AutoLock lock(applications_list_lock_);

    std::set<ApplicationSharedPtr>::const_iterator it = application_list_.begin();
    for (; it != application_list_.end(); ++it) {
      if ((*it)->app_id() == app_id) {
        app_to_remove = *it;
      }
    }
    application_list_.erase(app_to_remove);
  }

  if (!app_to_remove) {
    LOG4CXX_INFO(logger_, "Application is already unregistered.");
    return;
  }
#ifdef MODIFY_FUNCTION_SIGN
	// do nothing
#else
  if (is_resuming) {
    resume_ctrl_.SaveApplication(app_to_remove);
  }
#endif
  if (audio_pass_thru_active_) {
    // May be better to put this code in MessageHelper?
    end_audio_pass_thru();
    StopAudioPassThru(app_id);
    MessageHelper::SendStopAudioPathThru();
  }
  MessageHelper::SendOnAppUnregNotificationToHMI(app_to_remove);

  request_ctrl_.terminateAppRequests(app_id);

  //  {
  //    sync_primitives::AutoLock lock(applications_list_lock_);

  //  }
  return;
}

void ApplicationManagerImpl::Handle(const impl::MessageFromMobile& message) {
  LOG4CXX_INFO(logger_, "Received message from Mobile side");

  if (!message) {
    LOG4CXX_ERROR(logger_, "Null-pointer message received.");
    return;
  }
  ProcessMessageFromMobile(message);
}

void ApplicationManagerImpl::Handle(const impl::MessageToMobile& message) {
  protocol_handler::RawMessage* rawMessage = 0;
  if (message->protocol_version() == application_manager::kV1) {
    rawMessage = MobileMessageHandler::HandleOutgoingMessageProtocolV1(message);
  } else if ((message->protocol_version() == application_manager::kV2) ||
             (message->protocol_version() == application_manager::kV3)) {
    rawMessage = MobileMessageHandler::HandleOutgoingMessageProtocolV2(message);
  } else {
    return;
  }
  if (!rawMessage) {
    LOG4CXX_ERROR(logger_, "Failed to create raw message.");
    return;
  }

  if (!protocol_handler_) {
    LOG4CXX_WARN(logger_,
                 "Protocol Handler is not set; cannot send message to mobile.");
    return;
  }

  protocol_handler_->SendMessageToMobileApp(rawMessage, message.is_final);

  LOG4CXX_INFO(logger_, "Message for mobile given away.");
}

void ApplicationManagerImpl::Handle(const impl::MessageFromHmi& message) {
  LOG4CXX_INFO(logger_, "Received message from hmi");

  if (!message) {
    LOG4CXX_ERROR(logger_, "Null-pointer message received.");
    return;
  }

  ProcessMessageFromHMI(message);
}

void ApplicationManagerImpl::Handle(const impl::MessageToHmi& message) {
  LOG4CXX_INFO(logger_, "Received message to hmi");
  if (!hmi_handler_) {
    LOG4CXX_ERROR(logger_, "Observer is not set for HMIMessageHandler");
    return;
  }

  hmi_handler_->SendMessageToHMI(message);
  LOG4CXX_INFO(logger_, "Message from hmi given away.");
}

void ApplicationManagerImpl::Mute(VRTTSSessionChanging changing_state) {
  mobile_apis::AudioStreamingState::eType state =
    hmi_capabilities_.attenuated_supported()
    ? mobile_apis::AudioStreamingState::ATTENUATED
    : mobile_apis::AudioStreamingState::NOT_AUDIBLE;

  std::set<ApplicationSharedPtr>::const_iterator it = application_list_.begin();
  std::set<ApplicationSharedPtr>::const_iterator itEnd = application_list_.end();
  for (; it != itEnd; ++it) {
    if ((*it)->is_media_application()) {
      if (kTTSSessionChanging == changing_state) {
        (*it)->set_tts_speak_state(true);
      }
      if ((*it)->audio_streaming_state() != state) {
        (*it)->set_audio_streaming_state(state);
        MessageHelper::SendHMIStatusNotification(*(*it));
      }
    }
  }
}

void ApplicationManagerImpl::Unmute(VRTTSSessionChanging changing_state) {
  std::set<ApplicationSharedPtr>::const_iterator it = application_list_.begin();
  std::set<ApplicationSharedPtr>::const_iterator itEnd = application_list_.end();
  for (; it != itEnd; ++it) {
    if ((*it)->is_media_application()) {
      if (kTTSSessionChanging == changing_state) {
        (*it)->set_tts_speak_state(false);
      }
      if ((!(vr_session_started())) &&
          ((*it)->audio_streaming_state() !=
           mobile_apis::AudioStreamingState::AUDIBLE)) {
        (*it)->set_audio_streaming_state(
          mobile_apis::AudioStreamingState::AUDIBLE);
        MessageHelper::SendHMIStatusNotification(*(*it));
      }
    }
  }
}

mobile_apis::Result::eType ApplicationManagerImpl::SaveBinary(
  const std::vector<uint8_t>& binary_data, const std::string& file_path,
  const std::string& file_name, const uint32_t offset) {
  LOG4CXX_INFO(logger_,
               "SaveBinaryWithOffset  binary_size = " << binary_data.size()
               << " offset = " << offset);

  if (binary_data.size() > file_system::GetAvailableDiskSpace(file_path)) {
    LOG4CXX_ERROR(logger_, "Out of free disc space.");
    return mobile_apis::Result::OUT_OF_MEMORY;
  }

  const std::string full_file_path = file_path + "/" + file_name;
  uint32_t file_size = file_system::FileSize(full_file_path);
  std::ofstream* file_stream;
  if (offset != 0) {
    if (file_size != offset) {
      LOG4CXX_INFO(logger_,
                   "ApplicationManagerImpl::SaveBinaryWithOffset offset"
                   << " does'n match existing file size");
      return mobile_apis::Result::INVALID_DATA;
    }
    file_stream = file_system::Open(full_file_path, std::ios_base::app);
  } else {
    LOG4CXX_INFO(
      logger_,
      "ApplicationManagerImpl::SaveBinaryWithOffset offset is 0, rewrite");
    // if offset == 0: rewrite file
    file_stream = file_system::Open(full_file_path, std::ios_base::out);
  }
#ifdef OS_WINCE
  if (!file_system::Write(file_stream, binary_data._Myfirst,
                          binary_data.size())) {
#else
  if (!file_system::Write(file_stream, binary_data.data(),
                          binary_data.size())) {
#endif
    file_system::Close(file_stream);
    return mobile_apis::Result::GENERIC_ERROR;
  }

  file_system::Close(file_stream);
  LOG4CXX_INFO(logger_, "Successfully write data to file");
  return mobile_apis::Result::SUCCESS;
}

uint32_t ApplicationManagerImpl::GetAvailableSpaceForApp(
  const std::string& app_name) {
  const uint32_t app_quota = profile::Profile::instance()->app_dir_quota();
  std::string app_storage_path =
    profile::Profile::instance()->app_storage_folder();

  app_storage_path += "/";
  app_storage_path += app_name;

  if (file_system::DirectoryExists(app_storage_path)) {
    uint32_t size_of_directory = file_system::DirectorySize(app_storage_path);
    if (app_quota < size_of_directory) {
      return 0;
    }

    uint32_t current_app_quota = app_quota - size_of_directory;
    uint32_t available_disk_space =
      file_system::GetAvailableDiskSpace(app_storage_path);

    if (current_app_quota > available_disk_space) {
      return available_disk_space;
    } else {
      return current_app_quota;
    }
  } else {
    return app_quota;
  }
}

bool ApplicationManagerImpl::IsHMICooperating() const {
  return hmi_cooperating_;
}

#ifdef MODIFY_FUNCTION_SIGN
ApplicationManagerImpl::AudioPassThruReadFileThread::AudioPassThruReadFileThread(ApplicationManagerImpl* applicationManagerImpl)
{
	stop_flag_ = false;
	thread_main_running_ = false;
	applicationManagerImpl_ = applicationManagerImpl;
}

ApplicationManagerImpl::AudioPassThruReadFileThread::~AudioPassThruReadFileThread()
{}

void ApplicationManagerImpl::AudioPassThruReadFileThread::threadMain()
{
	LOG4CXX_INFO(logger_, "Begin AudioPassThruReadFileThread");
	thread_main_running_ = true;
	std::vector<uint8_t> wav_data;
	LOG4CXX_INFO(logger_, "read wav path is " << applicationManagerImpl_->read_path());
	file_system::ReadBinaryFile(applicationManagerImpl_->read_path(), wav_data);
	LOG4CXX_INFO(logger_, "read file wav data size is " << wav_data.size());
	if (wav_data.size() != 0){
		int bytes_per_second = (int)(wav_data[28] | (wav_data[29] << 8) | (wav_data[30] << 16) | (wav_data[31] << 24));
		LOG4CXX_INFO(logger_, "bytes_per_second=" << bytes_per_second);
		int bytes_per_interval = bytes_per_second / 4;
		LOG4CXX_INFO(logger_, "bytes_per_interval=" << bytes_per_interval);
		std::vector<uint8_t> data;
		soxr_wav_to_pcm(wav_data, data);
		LOG4CXX_INFO(logger_, "read file pcm data size is " << data.size());
		std::vector<uint8_t>::iterator begin_iter = data.begin();
		std::vector<uint8_t>::iterator end_iter = data.end() - begin_iter < bytes_per_interval ? data.end() : begin_iter + bytes_per_interval;
		LOG4CXX_INFO(logger_, "stop_flag_=" << stop_flag_);
		while (!stop_flag_){
			if (begin_iter == end_iter){
				break;
			}
			std::vector<uint8_t> send_data(begin_iter, end_iter);
			int app_id = applicationManagerImpl_->active_application()->app_id();
			//LOG4CXX_INFO(logger_, "send data size is " << send_data.size());
			applicationManagerImpl_->SendAudioPassThroughNotification(app_id, send_data);
			begin_iter = end_iter;
			end_iter = data.end() - begin_iter < bytes_per_interval ? data.end() : begin_iter + bytes_per_interval;
#ifdef OS_WIN32
			Sleep(250);
#else
			usleep(250000);
#endif
		}
	}
	stop_flag_ = false;
	thread_main_running_ = false;
	LOG4CXX_INFO(logger_, "End AudioPassThruReadFileThread");
}

bool ApplicationManagerImpl::AudioPassThruReadFileThread::exitThreadMain()
{
	LOG4CXX_INFO(logger_, "thread_main_running_=" << thread_main_running_);
	if (thread_main_running_){
		stop_flag_ = true;
	}
	LOG4CXX_INFO(logger_, "set stop_flag_=" << stop_flag_);
	return true;
}

#endif
}  // namespace application_manager
