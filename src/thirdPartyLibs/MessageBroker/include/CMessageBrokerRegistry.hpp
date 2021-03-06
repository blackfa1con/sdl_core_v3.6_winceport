#ifndef CMESSAGEBROKERREGISTRY_H
#define CMESSAGEBROKERREGISTRY_H

#include <map>
#include <vector>
#include <iostream>
#include <string>

/**
 * \namespace NsMessageBroker
 * \brief MessageBroker related functions.
 */ 
namespace NsMessageBroker 
{

   /**
   * \class CMessageBrokerRegistry
   * \brief Singletone CMessageBrokerRegistry class implementation.
   */
   class CMessageBrokerRegistry
   {
   public:
      /**
      * \brief Singletone instantiator.
      * \return pointer to CMessageBroker instance
      */
      static CMessageBrokerRegistry* getInstance();

      /**
      * \brief Destructor.
      */
      ~CMessageBrokerRegistry();

      /**
      * \brief adds controller to the registry.
      * \param fd file descriptor of controller.
      * \param name name of controller.
      * \return false if already exist.
      */
      bool addController(int fd, std::string name);

      /**
      * \brief deletes controller from the registry.
      * \param name name of controller.
      */
      void deleteController(std::string name);

#ifdef MODIFY_FUNCTION_SIGN
			/**
			* \brief clear controller from the registry.
			*/
			void clearController();
#endif

      /**
      * \brief adds notification subscriber to the registry.
      * \param fd file descriptor of controller.
      * \param name name of property which should be observed.
      * \return false if already exist.
      */
      bool addSubscriber(int fd, std::string name);

      /**
      * \brief deletes notification subscriber from the registry.
      * \param fd file descriptor of controller.
      * \param name name of property which should be observed.
      */
      void deleteSubscriber(int fd, std::string name);

#ifdef MODIFY_FUNCTION_SIGN
			/**
			* \brief deletes notification subscriber from the registry.
			* \param fd file descriptor of controller.
			* \param name name of property which should be observed.
			*/
			void deleteSubscriber(std::string name);

			/**
			* \brief clear notification subscriber from the registry.
			* \param fd file descriptor of controller.
			* \param name name of property which should be observed.
			*/
			void clearSubscriber();
#endif

      /**
      * \brief gets controller fd from the registry by name.
      * \param name name of controller.
      * \return file descriptor of controller.
      */
      int getDestinationFd(std::string name);

      /**
      * \brief gets subscribers fd's.
      * \param name name of property.
      * \param result vector for results.
      * \return count of subscribers.
      */
      int getSubscribersFd(std::string name, std::vector<int>& result);
   private:
      /**
      * \brief Constructor.
      */
      CMessageBrokerRegistry();

      /**
      * \brief Map to store controllers information like ComponentName:socketFd.
      * For example PhoneController:1080
      */
      std::map <std::string, int> mControllersList;

      /**
      * \brief Map to store subscribers information like ComponentName.PropertyName:socketFd:.
      * For example PhoneController.onPhoneBookChanged:1080
      */
      std::multimap <std::string, int> mSubscribersList;
   };
} /* namespace NsMessageBroker */

#endif // CMESSAGEBROKERREGISTRY_H

