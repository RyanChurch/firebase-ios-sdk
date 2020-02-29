/*
 * Copyright 2019 Google
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "Firestore/core/test/firebase/firestore/testutil/async_testing.h"

#include <string>
#include <utility>

#include "Firestore/core/src/firebase/firestore/util/async_queue.h"
#include "Firestore/core/src/firebase/firestore/util/executor.h"
#include "absl/strings/str_cat.h"
#include "gtest/gtest.h"

namespace firebase {
namespace firestore {
namespace testutil {

using testing::TestInfo;
using testing::UnitTest;
using util::AsyncQueue;
using util::Executor;

std::unique_ptr<util::Executor> ExecutorForTesting(const char* name) {
  const TestInfo* test_info = UnitTest::GetInstance()->current_test_info();

  std::string label = absl::StrCat("firestore.testing.", name);
  if (test_info) {
    absl::StrAppend(&label, ".", test_info->test_suite_name(), ".",
                    test_info->name());
  }

  return Executor::CreateSerial(label.c_str());
}

std::shared_ptr<util::AsyncQueue> AsyncQueueForTesting() {
  return AsyncQueue::Create(ExecutorForTesting("worker"));
}

// MARK: - Expectation

Expectation::Expectation()
    : promise_(std::make_shared<std::promise<void>>()),
      future_(promise_->get_future().share()) {
}

void Expectation::Fulfill() {
  promise_->set_value();
}

std::function<void()> Expectation::AsCallback() const {
  // Additional copy required because putting `promise_` in the capture list
  // is not allowed. Generalized capture in C++14 would allow direct
  // initialization.
  auto promise = promise_;
  return [promise] { promise->set_value(); };
}

// MARK: - AsyncTest

std::future<void> Async(std::function<void()> action) {
  std::packaged_task<void()> task(std::move(action));
  auto future = task.get_future();

  std::thread thread(std::move(task));
  thread.detach();
  return future;
}

void Await(const std::future<void>& future, std::chrono::milliseconds timeout) {
  std::future_status result = future.wait_for(timeout);
  if (result == std::future_status::ready) {
    return;
  }

  ADD_FAILURE() << "Test timed out after " << timeout.count() << " ms";
}

void Await(const std::shared_future<void>& future,
           std::chrono::milliseconds timeout) {
  std::future_status result = future.wait_for(timeout);
  if (result == std::future_status::ready) {
    return;
  }

  ADD_FAILURE() << "Test timed out after " << timeout.count() << " ms";
}

// NOLINTNEXTLINE(runtime/references)
void Await(Expectation& expectation, std::chrono::milliseconds timeout) {
  return Await(expectation.get_future(), timeout);
}

void SleepFor(int millis) {
  std::this_thread::sleep_for(std::chrono::milliseconds(millis));
}

}  // namespace testutil
}  // namespace firestore
}  // namespace firebase
