// implementation of stop token + stop source + jthread // 15.03.26// ZeroK

#pragma once

#include <thread>
#include <atomic>
#include <memory>
#include <type_traits>

namespace zerok {
    class stop_token;
    class stop_source;
    class jthread;


    // stop_token provides an interface to check whether a stop request has been made
    // or can ever be made associated to the stop_source object
    class stop_token {
        private:
            std::shared_ptr<std::atomic<bool>> _token_ptr {
                std::make_shared<std::atomic<bool>>(false) 
            };

        public:
            stop_token () noexcept = default;
            stop_token (const stop_token&) noexcept = default;
            stop_token& operator=(const stop_token&) noexcept = default;
            stop_token (stop_token&&) noexcept = default;
            stop_token& operator=(stop_token&&) noexcept = default;
            ~stop_token () noexcept = default;

            // check whether stop is requested
            [[nodiscard]] bool stop_requested () const noexcept {
                return _token_ptr->load(std::memory_order_acquire);
            }

            // set cancelled state, return previous state
            [[nodiscard]] bool cancel () const noexcept {
                return !_token_ptr->exchange(true, std::memory_order_acq_rel);
            }
    };


    // stop_source implements stop request
    // stop request made on stop_source is visible to all stop_source and stop_token obj
    // a stop request cant be cancelled
    class stop_source {
        private:
            stop_token _token;

        public:
            stop_source () noexcept = default;
            stop_source (const stop_token&) noexcept = default;
            stop_source& operator=(const stop_token&) noexcept = default;
            stop_source (stop_token&&) noexcept = default;
            stop_source& operator=(stop_token&&) noexcept = default;
            ~stop_source () noexcept = default;

            // ask for cancellation, return previous cancelled state
            [[nodiscard]] bool request_stop () const noexcept {
                return _token.cancel();
            }

            // check whether cancellation has been requested
            [[nodiscard]] bool stop_requested () const noexcept {
                return _token.stop_requested();
            }

            [[nodiscard]] stop_token get_token () const noexcept {
                return _token;
            }
    };


    // joinable and interruptible thread
    // follows RAII mechanism of joining + stop request
    // functionally same as std::thread
    // compile time check whether F can accpet stop_token with if constexpr + is_invocable_v
    class jthread {
        private:
            std::thread _jt;
            stop_source _ss;

        public:
            jthread () = delete;

            template <typename F, typename... Args>
            explicit jthread(F&& f, Args&&... args) {
                stop_token st = _ss.get_token();

                // accept stop_token as first argument
                if constexpr (std::is_invocable_v<std::decay_t<F>, 
                                                  stop_token, 
                                                  std::decay_t<Args>...>) {
                    _jt = std::thread (
                            std::forward<F>(f),
                            st,
                            std::forward<Args>(args)...
                        );
                } 
                // F doesnt accept stop_token as argument
                else {
                    static_assert (
                        std::is_invocable_v<std::decay_t<F>, std::decay_t<Args>...>,
                        "F must be invocable with args, with or without stop_token.\n"
                    );
                
                    _jt = std::thread (
                            std::forward<F>(f),
                            std::forward<Args>(args)...
                        );
                }
            }

            jthread (const jthread&) = delete;
            jthread& operator=(const jthread&) = delete;

            jthread (jthread&&) noexcept = default;
            jthread& operator=(jthread&&) noexcept = default;

            // request stop must before join
            // otherwise deadlock
            ~jthread () noexcept {
                request_stop();
                if (joinable()) join();
            }

            void detach () {
                if (joinable())
                    _jt.detach();
            }

            void join () {
                _jt.join();
            }

            void request_stop () {
                if (!_ss.stop_requested())
                    _ss.request_stop();
            }

            [[nodiscard]] bool joinable () const {
                return _jt.joinable();
            }
            
    };
}  // namespace zerok
