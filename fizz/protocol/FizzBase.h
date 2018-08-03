/*
 *  Copyright (c) 2018-present, Facebook, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <fizz/protocol/Params.h>
#include <folly/Overload.h>

namespace fizz {

/**
 * FizzBase defines an async method of communicating with the fizz state
 * machine. Given a const reference to state, and a reference to
 * transportReadBuf, FizzBase will consume the transportReadBuf and process
 * events as applicable. visitor is variant visitor that is expected to process
 * Actions as they are received. A DestructorGuard on owner will be taken when
 * async actions are in flight, during which time this class must not be
 * deleted.
 */
template <typename Derived, typename ActionMoveVisitor, typename StateMachine>
class FizzBase {
 public:
  FizzBase(
      const typename StateMachine::StateType& state,
      folly::IOBufQueue& transportReadBuf,
      ActionMoveVisitor& visitor,
      folly::DelayedDestructionBase* owner)
      : state_(state),
        transportReadBuf_(transportReadBuf),
        visitor_(visitor),
        owner_(owner) {}

  /**
   * Server only: Called to write new session ticket to client.
   */
  void writeNewSessionTicket(WriteNewSessionTicket writeNewSessionTicket);

  /**
   * Called to write application data.
   */
  void appWrite(AppWrite appWrite);

  /**
   * Called to write early application data.
   */
  void earlyAppWrite(EarlyAppWrite appWrite);

  /**
   * Called when the application wants to close the connection.
   */
  void appClose();

  /**
   * Called to pause processing of transportReadBuf until new data is available.
   *
   * Call newTransportData() to resume processing.
   */
  void waitForData();

  /**
   * Called to notify that new transport data is available in transportReadBuf.
   */
  void newTransportData();

  /**
   * Calls error callbacks on any pending events and prevents any further events
   * from being processed. Should be called when an error is received from
   * either the state machine or the transport that will cause the connection to
   * abort. Note that this does not stop a currently processing event.
   */
  void moveToErrorState(const folly::AsyncSocketException& ex);

  /**
   * Returns true if in error state where no further events will be processed.
   */
  bool inErrorState() const;

  /**
   * Returns true if the state machine is actively processing an event or
   * action.
   */
  bool actionProcessing() const;

  /**
   * Returns an exported key material derived from the 1-RTT secret of the TLS
   * connection.
   */
  Buf getEkm(folly::StringPiece label, const Buf& context, uint16_t length)
      const;

 protected:
  void processActions(typename StateMachine::CompletedActions actions);

  void addProcessingActions(typename StateMachine::ProcessingActions actions);

  StateMachine machine_;
  const typename StateMachine::StateType& state_;
  folly::IOBufQueue& transportReadBuf_;

 private:
  void processPendingEvents();

  ActionMoveVisitor& visitor_;
  folly::DelayedDestructionBase* owner_;

  using PendingEvent =
      boost::variant<AppWrite, EarlyAppWrite, AppClose, WriteNewSessionTicket>;
  std::deque<PendingEvent> pendingEvents_;
  bool waitForData_{true};
  folly::Optional<folly::DelayedDestruction::DestructorGuard> actionGuard_;
  bool inProcessPendingEvents_{false};
  bool inErrorState_{false};
};
} // namespace fizz

#include <fizz/protocol/FizzBase-inl.h>
