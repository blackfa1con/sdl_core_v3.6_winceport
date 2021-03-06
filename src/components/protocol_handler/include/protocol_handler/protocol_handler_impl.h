/**
 * \file protocol_handler.h
 * \brief ProtocolHandlerImpl class header file.
 *
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

#ifndef SRC_COMPONENTS_PROTOCOL_HANDLER_INCLUDE_PROTOCOL_HANDLER_PROTOCOL_HANDLER_IMPL_H_
#define SRC_COMPONENTS_PROTOCOL_HANDLER_INCLUDE_PROTOCOL_HANDLER_PROTOCOL_HANDLER_IMPL_H_

#include <map>
#include <memory>
#include <set>
#include "utils/prioritized_queue.h"
#include "utils/message_queue.h"
#include "utils/threads/thread.h"
#include "utils/threads/message_loop_thread.h"
#include "utils/shared_ptr.h"

#include "protocol_handler/protocol_handler.h"
#include "protocol_handler/protocol_packet.h"
#include "protocol_handler/session_observer.h"
#include "protocol_handler/protocol_observer.h"
#include "transport_manager/common.h"
#include "transport_manager/transport_manager.h"
#include "transport_manager/transport_manager_listener_empty.h"
#ifdef TIME_TESTER
#include "time_metric_observer.h"
#endif  // TIME_TESTER

/**
 *\namespace NsProtocolHandler
 *\brief Namespace for SmartDeviceLink ProtocolHandler related functionality.
 */
namespace protocol_handler {
class ProtocolObserver;
class SessionObserver;

class MessagesFromMobileAppHandler;
class MessagesToMobileAppHandler;

using transport_manager::TransportManagerListenerEmpty;

/**
 * @brief Type definition for variable that hold shared pointer to raw message.
 */
typedef utils::SharedPtr<protocol_handler::ProtocolPacket> ProtocolFramePtr;

typedef std::multimap<int32_t, RawMessagePtr> MessagesOverNaviMap;
typedef std::set<ProtocolObserver*> ProtocolObservers;
typedef transport_manager::ConnectionUID ConnectionID;

namespace impl {
/*
 * These dummy classes are here to locally impose strong typing on different
 * kinds of messages
 * Currently there is no type difference between incoming and outgoing messages
 * TODO(ik): replace these with globally defined message types
 * when we have them.
 */
struct RawFordMessageFromMobile: public ProtocolFramePtr {
  explicit RawFordMessageFromMobile(const ProtocolFramePtr& message)
      : ProtocolFramePtr(message) {}
  // PrioritizedQueue requires this method to decide which priority to assign
  size_t PriorityOrder() const { return MessagePriority::FromServiceType(ServiceTypeFromByte(
      get()->service_type())).OrderingValue(); }
};

struct RawFordMessageToMobile: public ProtocolFramePtr {
  explicit RawFordMessageToMobile(const ProtocolFramePtr& message, bool final_message)
      : ProtocolFramePtr(message), is_final(final_message) {}
  // PrioritizedQueue requires this method to decide which priority to assign
  size_t PriorityOrder() const { return MessagePriority::FromServiceType(ServiceTypeFromByte(
      get()->service_type())).OrderingValue(); }
  // Signals whether connection to mobile must be closed after processing this message
  bool is_final;
};

// Short type names for prioritized message queues
typedef threads::MessageLoopThread<
               utils::PrioritizedQueue<RawFordMessageFromMobile> > FromMobileQueue;
typedef threads::MessageLoopThread<
               utils::PrioritizedQueue<RawFordMessageToMobile> > ToMobileQueue;
}

/**
 * \class ProtocolHandlerImpl
 * \brief Class for handling message exchange between Transport and higher
 * layers. Receives message in form of array of bytes, parses its protocol,
 * handles according to parsing results (version number, start/end session etc
 * and if needed passes message to JSON Handler or notifies Connection Handler
 * about activities around sessions.
 */
class ProtocolHandlerImpl
    : public ProtocolHandler,
      public TransportManagerListenerEmpty,
      public impl::FromMobileQueue::Handler,
      public impl::ToMobileQueue::Handler {
  public:
    /**
     * \brief Constructor
     * \param transportManager Pointer to Transport layer handler for
     * message exchange.
     */
    explicit ProtocolHandlerImpl(
      transport_manager::TransportManager* transport_manager_param);

    /**
     * \brief Destructor
     */
    ~ProtocolHandlerImpl();

    /**
     * \brief Adds pointer to higher layer handler for message exchange
     * \param observer Pointer to object of the class implementing
     * IProtocolObserver
     */
    void AddProtocolObserver(ProtocolObserver* observer);

    /**
     * \brief Removes pointer to higher layer handler for message exchange
     * \param observer Pointer to object of the class implementing
     * IProtocolObserver.
     */
    void RemoveProtocolObserver(ProtocolObserver* observer);

    /**
     * \brief Sets pointer for Connection Handler layer for managing sessions
     * \param observer Pointer to object of the class implementing
     * ISessionObserver
     */
    void set_session_observer(SessionObserver* observer);

    /**
     * \brief Method for sending message to Mobile Application
     * \param message Message with params to be sent to Mobile App
     */
    void SendMessageToMobileApp(const RawMessagePtr& message,
                                bool final_message) OVERRIDE;

    /**
     * \brief Sends number of processed frames in case of binary nav streaming
     * \param connection_key Id of connection over which message is to be sent
     * \param number_of_frames Number of frames processed by
     * streaming server and displayed to user.
     */
    void SendFramesNumber(int32_t connection_key, int32_t number_of_frames);

#ifdef TIME_TESTER
    /**
     * @brief Setup observer for time metric.
     *
     * @param observer - pointer to observer
     */
    void SetTimeMetricObserver(PHMetricObserver* observer);
#endif  // TIME_TESTER


    /*
     * Prepare and send heartbeat message to mobile
     */
    void SendHeartBeat(int32_t connection_id, uint8_t session_id);

    /**
      * \brief Sends ending session to mobile application
      * \param connection_id Identifier of connection within which
      * session exists
      * \param session_id ID of session to be ended
      */
    void SendEndSession(int32_t connection_id, uint8_t session_id);

  protected:

    /**
     * \brief Sends acknowledgement of starting session to mobile application
     * with session number and hash code for second version of protocol
     * \param connection_handle Identifier of connection within which session
     * was started
     * \param session_id ID of session to be sent to mobile application
     * \param protocol_version Version of protocol used for communication
     * \param hash_code For second version of protocol: identifier of session
     * to be sent to
     * mobile app for using when ending session
     * \param service_type Type of session: RPC or BULK Data. RPC by default
     */
    void SendStartSessionAck(
      ConnectionID connection_id,
      uint8_t session_id,
      uint8_t protocol_version,
      uint32_t hash_code = 0,
      uint8_t service_type = SERVICE_TYPE_RPC);

    /**
     * \brief Sends fail of starting session to mobile application
     * \param connection_handle Identifier of connection within which session
     * ment to be started
     * \param session_id ID of session to be sent to mobile application
     * \param protocol_version Version of protocol used for communication
     * \param service_type Type of session: RPC or BULK Data. RPC by default
     */
    void SendStartSessionNAck(
      ConnectionID connection_id,
      uint8_t session_id,
      uint8_t protocol_version,
      uint8_t service_type = SERVICE_TYPE_RPC);

    /**
     * \brief Sends acknowledgement of end session/service to mobile application
     * with session number and hash code for second version of protocol
     * \param connection_handle Identifier of connection within which session
     * was started
     * \param session_id ID of session to be sent to mobile application
     * \param protocol_version Version of protocol used for communication
     * \param hash_code For second version of protocol: identifier of session
     * to be sent to
     * mobile app for using when ending session.
     * \param service_type Type of session: RPC or BULK Data. RPC by default
     */
    void SendEndSessionAck(
      ConnectionID connection_id ,
      uint8_t session_id,
      uint8_t protocol_version,
      uint32_t hash_code = 0,
      uint8_t service_type = SERVICE_TYPE_RPC);

    /**
     * \brief Sends fail of ending session to mobile application
     * \param connection_handle Identifier of connection within which
     * session exists
     * \param session_id ID of session ment to be ended
     * \param protocol_version Version of protocol used for communication
     * \param service_type Type of session: RPC or BULK Data. RPC by default
     */
    void SendEndSessionNAck(
      ConnectionID connection_id ,
      uint32_t session_id,
      uint8_t protocol_version,
      uint8_t service_type = SERVICE_TYPE_RPC);

  private:
    /*
     * Prepare and send heartbeat acknowledge message
     */
    RESULT_CODE SendHeartBeatAck(ConnectionID connection_id,
                          uint8_t session_id,
                          uint32_t message_id);

    /**
     * @brief Notifies about receiving message from TM.
     *
     * @param message Received message
     **/
    virtual void OnTMMessageReceived(
      const RawMessagePtr message);

    /**
     * @brief Notifies about error on receiving message from TM.
     *
     * @param error Occurred error
     **/
    virtual void OnTMMessageReceiveFailed(
      const transport_manager::DataReceiveError& error);

    /**
     * @brief Notifies about successfully sending message.
     *
     **/
    virtual void OnTMMessageSend(const RawMessagePtr message);

    /**
     * @brief Notifies about error occurred during
     * sending message.
     *
     * @param error Describes occurred error.
     * @param message Message during sending which error occurred.
     **/
    virtual void OnTMMessageSendFailed(
      const transport_manager::DataSendError& error,
      const RawMessagePtr& message);

    virtual void OnConnectionEstablished(
        const transport_manager::DeviceInfo& device_info,
        const transport_manager::ConnectionUID& connection_id);

    virtual void OnConnectionClosed(
        const transport_manager::ConnectionUID& connection_id);

    /**
     * @brief Notifies subscribers about message
     * recieved from mobile device.
     * @param message Message with already parsed header.
     */
    void NotifySubscribers(const RawMessagePtr& message);

    /**
     * \brief Sends message which size permits to send it in one frame.
     * \param connection_handle Identifier of connection through which message
     * is to be sent.
     * \param session_id ID of session through which message is to be sent.
     * \param protocol_version Version of Protocol used in message.
     * \param service_type Type of session, RPC or BULK Data
     * \param data_size Size of message excluding protocol header
     * \param data Message string
     * \param compress Compression flag
     * \param is_final_message if is_final_message = true - it is last message
     * \return \saRESULT_CODE Status of operation
     */
    RESULT_CODE SendSingleFrameMessage(
      ConnectionID connection_id,
      const uint8_t session_id,
      uint32_t protocol_version,
      const uint8_t service_type,
      const uint32_t data_size,
      const uint8_t* data,
      const bool compress,
      const bool is_final_message);

    /**
     * \brief Sends message which size doesn't permit to send it in one frame.
     * \param connection_handle Identifier of connection through which message
     * is to be sent.
     * \param session_id ID of session through which message is to be sent.
     * \param protocol_version Version of Protocol used in message.
     * \param service_type Type of session, RPC or BULK Data
     * \param data_size Size of message excluding protocol header
     * \param data Message string
     * \param compress Compression flag
     * \param max_data_size Maximum allowed size of single frame.
     * \param is_final_message if is_final_message = true - it is last message
     * \return \saRESULT_CODE Status of operation
     */
    RESULT_CODE SendMultiFrameMessage(
      ConnectionID connection_id,
      const uint8_t session_id,
      uint32_t protocol_version,
      const uint8_t service_type,
      const uint32_t data_size,
      const uint8_t* data,
      const bool compress,
      const uint32_t max_data_size,
      const bool is_final_message);

    /**
     * \brief Sends message already containing protocol header.
     * \param connection_handle Identifier of connection through which message
     * is to be sent.
     * \param packet Message with protocol header.
     * \return \saRESULT_CODE Status of operation
     */
    RESULT_CODE SendFrame(
      ConnectionID connection_id,
      const ProtocolPacket& packet);

    /**
     * \brief Handles received message.
     * \param connection_handle Identifier of connection through which message
     * is received.
     * \param packet Received message with protocol header.
     * \return \saRESULT_CODE Status of operation
     */
    RESULT_CODE HandleMessage(
      ConnectionID connection_id ,
      const ProtocolFramePtr& packet);

    /**
     * \brief Handles message received in multiple frames. Collects all frames
     * of message.
     * \param connection_handle Identifier of connection through which message
     * is received.
     * \param packet Current frame of message with protocol header.
     * \return \saRESULT_CODE Status of operation
     */
    RESULT_CODE HandleMultiFrameMessage(
      ConnectionID connection_id ,
      const ProtocolFramePtr& packet);

    /**
     * \brief Handles message received in single frame.
     * \param connection_handle Identifier of connection through which message
     * is received.
     * \param packet Received message with protocol header.
     * \return \saRESULT_CODE Status of operation
     */
    RESULT_CODE HandleControlMessage(
      ConnectionID connection_id ,
      const ProtocolFramePtr& packet);

    RESULT_CODE HandleControlMessageEndSession(
      ConnectionID connection_id ,
      const ProtocolPacket& packet);

    RESULT_CODE HandleControlMessageStartSession(
      ConnectionID connection_id ,
      const ProtocolPacket& packet);

    RESULT_CODE HandleControlMessageHeartBeat(
      ConnectionID connection_id ,
      const ProtocolPacket& packet);

    /**
     * \brief Sends Mobile Navi Ack message
     */
    RESULT_CODE SendMobileNaviAck(
      ConnectionID connection_id ,
      int32_t connection_key);

    // threads::MessageLoopThread<*>::Handler implementations
    // CALLED ON raw_ford_messages_from_mobile_ thread!
    void Handle(const impl::RawFordMessageFromMobile& message);
    // CALLED ON raw_ford_messages_to_mobile_ thread!
    void Handle(const impl::RawFordMessageToMobile& message);
  private:
    /**
     *\brief Pointer on instance of class implementing IProtocolObserver
     *\brief (JSON Handler)
     */
    ProtocolObservers protocol_observers_;
#ifdef TIME_TESTER
    PHMetricObserver* metric_observer_;
#endif  // TIME_TESTER

    /**
     *\brief Pointer on instance of class implementing ISessionObserver
     *\brief (Connection Handler)
     */
    SessionObserver* session_observer_;

    /**
     *\brief Pointer on instance of Transport layer handler for message exchange.
     */
    transport_manager::TransportManager* transport_manager_;

    /**
     *\brief Map of frames for messages received in multiple frames.
     */
    std::map<int32_t, ProtocolFramePtr> incomplete_multi_frame_messages_;

    /**
     * \brief Map of messages (frames) recieved over mobile nave session
     * for map streaming.
     */
    MessagesOverNaviMap message_over_navi_session_;

    /**
     * \brief Untill specified otherwise, amount of message recievied
     * over streaming session to send Ack
     */
    const uint32_t kPeriodForNaviAck;

    /**
     *\brief Counter of messages sent in each session.
     */
    std::map<uint8_t, uint32_t> message_counters_;

    /**
     *\brief map for session last message.
     */
    std::map<uint8_t, uint32_t> sessions_last_message_id_;


    class IncomingDataHandler;
    std::auto_ptr<IncomingDataHandler> incoming_data_handler_;

    // Thread that pumps non-parsed messages coming from mobile side.
    impl::FromMobileQueue raw_ford_messages_from_mobile_;
    // Thread that pumps messages prepared to being sent to mobile side.
    impl::ToMobileQueue raw_ford_messages_to_mobile_;
};
}  // namespace protocol_handler

#endif  // SRC_COMPONENTS_PROTOCOL_HANDLER_INCLUDE_PROTOCOL_HANDLER_PROTOCOL_HANDLER_IMPL_H_
