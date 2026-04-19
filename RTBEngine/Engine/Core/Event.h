#pragma once

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

namespace RTBEngine {
    namespace Core {
        namespace Detail {
            struct EventStateBase {
                virtual ~EventStateBase() = default;
            };
        }

        class EventSubscription {
        public:
            EventSubscription() = default;
            EventSubscription(const EventSubscription&) = delete;
            EventSubscription& operator=(const EventSubscription&) = delete;

            EventSubscription(EventSubscription&& other) noexcept
            {
                MoveFrom(std::move(other));
            }

            EventSubscription& operator=(EventSubscription&& other) noexcept
            {
                if (this == &other) {
                    return *this;
                }

                Reset();
                MoveFrom(std::move(other));
                return *this;
            }

            ~EventSubscription()
            {
                Reset();
            }

            void Reset()
            {
                if (listenerId == 0) {
                    state.reset();
                    unsubscribe = {};
                    return;
                }

                if (unsubscribe) {
                    unsubscribe(state, listenerId);
                }

                state.reset();
                unsubscribe = {};
                listenerId = 0;
            }

            bool IsValid() const
            {
                return listenerId != 0 && !state.expired();
            }

        private:
            using UnsubscribeCallback = std::function<void(const std::weak_ptr<Detail::EventStateBase>&, std::uint64_t)>;

            EventSubscription(
                const std::weak_ptr<Detail::EventStateBase>& subscriptionState,
                UnsubscribeCallback unsubscribeCallback,
                std::uint64_t id)
                : state(subscriptionState)
                , unsubscribe(std::move(unsubscribeCallback))
                , listenerId(id)
            {
            }

            void MoveFrom(EventSubscription&& other)
            {
                state = std::move(other.state);
                unsubscribe = std::move(other.unsubscribe);
                listenerId = other.listenerId;

                other.state.reset();
                other.unsubscribe = {};
                other.listenerId = 0;
            }

            std::weak_ptr<Detail::EventStateBase> state;
            UnsubscribeCallback unsubscribe;
            std::uint64_t listenerId = 0;

            template<typename TPayload>
            friend class Event;
        };

        template<typename TPayload>
        class Event {
        public:
            using Callback = std::function<void(const TPayload&)>;

            Event() = default;
            Event(const Event&) = delete;
            Event& operator=(const Event&) = delete;
            Event(Event&&) noexcept = default;
            Event& operator=(Event&&) noexcept = default;
            ~Event() = default;

            EventSubscription Subscribe(Callback callback)
            {
                if (!callback) {
                    return {};
                }

                EnsureState();

                const std::uint64_t listenerId = state->nextListenerId++;
                state->listeners.emplace_back(listenerId, std::move(callback));
                return EventSubscription(state, &Event::UnsubscribeFromState, listenerId);
            }

            void Clear()
            {
                state = std::make_shared<State>();
            }

            bool HasListeners() const
            {
                return state && !state->listeners.empty();
            }

            void Invoke(const TPayload& payload) const
            {
                if (!state || state->listeners.empty()) {
                    return;
                }

                const auto listeners = state->listeners;
                for (const auto& listenerEntry : listeners) {
                    if (listenerEntry.second) {
                        listenerEntry.second(payload);
                    }
                }
            }

        private:
            struct State : Detail::EventStateBase {
                std::vector<std::pair<std::uint64_t, Callback>> listeners;
                std::uint64_t nextListenerId = 1;
            };

            static void UnsubscribeFromState(const std::weak_ptr<Detail::EventStateBase>& weakState, std::uint64_t listenerId)
            {
                const std::shared_ptr<Detail::EventStateBase> lockedState = weakState.lock();
                if (!lockedState) {
                    return;
                }

                const std::shared_ptr<State> typedState = std::static_pointer_cast<State>(lockedState);
                auto& listeners = typedState->listeners;
                listeners.erase(
                    std::remove_if(
                        listeners.begin(),
                        listeners.end(),
                        [listenerId](const auto& listenerEntry) {
                            return listenerEntry.first == listenerId;
                        }),
                    listeners.end());
            }

            void EnsureState()
            {
                if (!state) {
                    state = std::make_shared<State>();
                }
            }

            std::shared_ptr<State> state = std::make_shared<State>();
        };

        template<>
        class Event<void> {
        public:
            using Callback = std::function<void()>;

            Event() = default;
            Event(const Event&) = delete;
            Event& operator=(const Event&) = delete;
            Event(Event&&) noexcept = default;
            Event& operator=(Event&&) noexcept = default;
            ~Event() = default;

            EventSubscription Subscribe(Callback callback)
            {
                if (!callback) {
                    return {};
                }

                EnsureState();

                const std::uint64_t listenerId = state->nextListenerId++;
                state->listeners.emplace_back(listenerId, std::move(callback));
                return EventSubscription(state, &Event::UnsubscribeFromState, listenerId);
            }

            void Clear()
            {
                state = std::make_shared<State>();
            }

            bool HasListeners() const
            {
                return state && !state->listeners.empty();
            }

            void Invoke() const
            {
                if (!state || state->listeners.empty()) {
                    return;
                }

                const auto listeners = state->listeners;
                for (const auto& listenerEntry : listeners) {
                    if (listenerEntry.second) {
                        listenerEntry.second();
                    }
                }
            }

        private:
            struct State : Detail::EventStateBase {
                std::vector<std::pair<std::uint64_t, Callback>> listeners;
                std::uint64_t nextListenerId = 1;
            };

            static void UnsubscribeFromState(const std::weak_ptr<Detail::EventStateBase>& weakState, std::uint64_t listenerId)
            {
                const std::shared_ptr<Detail::EventStateBase> lockedState = weakState.lock();
                if (!lockedState) {
                    return;
                }

                const std::shared_ptr<State> typedState = std::static_pointer_cast<State>(lockedState);
                auto& listeners = typedState->listeners;
                listeners.erase(
                    std::remove_if(
                        listeners.begin(),
                        listeners.end(),
                        [listenerId](const auto& listenerEntry) {
                            return listenerEntry.first == listenerId;
                        }),
                    listeners.end());
            }

            void EnsureState()
            {
                if (!state) {
                    state = std::make_shared<State>();
                }
            }

            std::shared_ptr<State> state = std::make_shared<State>();
        };
    }
}
