/*
 * \file test_dbus_adapter.cc
 * \brief
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

#include <pthread.h>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "hmi_message_handler/mock_dbus_message_controller.h"
#include "hmi_message_handler/mock_subscriber.h"

using ::testing::_;

namespace test {
namespace components {
namespace hmi_message_handler {

ACTION_P(SignalTest, test) {
  if (test->thread_id != pthread_self()) {
    pthread_mutex_lock(&test->test_mutex);
    pthread_cond_signal(&test->test_cond);
    pthread_mutex_unlock(&test->test_mutex);
  } else {
    test->one_thread = true;
  }
}

class DBusMessageControllerTest : public ::testing::Test {
 public:
  volatile bool one_thread;
  pthread_t thread_id;
  static pthread_mutex_t test_mutex;
  static pthread_cond_t test_cond;

 protected:
  MockDBusMessageController* controller_;
  MockSubscriber* subscriber_;

  static void SetUpTestCase() {

  }

  static void TearDownTestCase() {

  }

  virtual void SetUp() {
    const std::string kService = "sdl.core.test_api";
    const std::string kPath = "/dbus_test";
    controller_ = new MockDBusMessageController(kService, kPath);
    subscriber_ = new MockSubscriber(kService, kPath);
    ASSERT_TRUE(controller_->Init());
    ASSERT_TRUE(subscriber_->Start());
  }

  virtual void TearDown() {
    delete controller_;
    delete subscriber_;
  }

  bool waitCond(int seconds) {
    if (one_thread)
      return true;
    timespec elapsed;
    clock_gettime(CLOCK_REALTIME, &elapsed);
    elapsed.tv_sec += seconds;
    return pthread_cond_timedwait(&test_cond, &test_mutex, &elapsed) != ETIMEDOUT;
  }
};

pthread_mutex_t DBusMessageControllerTest::test_mutex;
pthread_cond_t DBusMessageControllerTest::test_cond;

TEST_F(DBusMessageControllerTest, Receive) {
  std::string text = "Test message for call method DBus";
  EXPECT_CALL(*controller_, Recv(text)).Times(1).WillOnce(SignalTest(this));
  subscriber_->Send(text);
  EXPECT_TRUE(waitCond(1));
}

TEST_F(DBusMessageControllerTest, DISABLED_Send) {
  const std::string kText = "Test message for signal DBus";
//  EXPECT_CALL(*subscriber_, Receive(kText)).Times(1);
  controller_->Send(kText);
}

}  // namespace hmi_message_handler
}  // namespace components
}  // namespace test

int main(int argc, char** argv) {
testing::InitGoogleTest(&argc, argv);
return RUN_ALL_TESTS();
}
