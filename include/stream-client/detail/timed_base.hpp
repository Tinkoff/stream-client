#pragma once

#include <boost/asio/basic_waitable_timer.hpp>
#include <boost/asio/io_service.hpp>

#include <chrono>
#include <iostream>

namespace stream_client {
namespace detail {

/**
 * Base class with timeout handling.
 *
 * This class implements basic setup/reset for internal timer instance.
 * Instances of this class own boost::asio::io_service and boost::asio::basic_waitable_timer via std::unique_ptr.
 * Inherited classes should implement deadline_actor() which is called when timer goes off.
 *
 * @note This class does neither run nor poll its' io_service, it is up to descendants.
 *
 * @tparam Clock type of clock to use, see boost::asio::basic_waitable_timer
 */
template <typename Clock>
class timed_base
{
public:
    using clock_type = Clock;
    using time_duration_type = typename clock_type::duration;
    using time_point_type = typename clock_type::time_point;
    using timer_type = typename boost::asio::basic_waitable_timer<clock_type>;

    /// Indefinite duration
    static constexpr time_duration_type kInfiniteDuration = time_duration_type::max();
    /// Zero span duration
    static constexpr time_duration_type kZeroDuration = time_duration_type(0);
    /// Minimal resolvable duration (anything else may be less than timeout setup overhead)
    static constexpr time_duration_type kDurationResolution = time_duration_type(1000); // 1us
    /// Minimal timeout = 2 x kDurationResolution
    static constexpr time_duration_type kMinTimeout = 2 * kDurationResolution;

    /// RAII wrapper to set timer expiration
    class expiration final
    {
    public:
        expiration(timer_type* timer = nullptr)
            : timer_(timer)
        {
        }

        expiration(timer_type* timer, const time_duration_type& duration)
            : expiration(timer)
        {
            if (duration < kDurationResolution) {
                throw boost::system::system_error{boost::asio::error::timed_out};
            }
            if (duration == kInfiniteDuration) {
                timer_ = nullptr;
                return;
            }

            if (timer_) {
                timer_->expires_from_now(duration);
            }
        }

        expiration(timer_type* timer, const time_point_type& deadline)
            : expiration(timer)
        {
            if (timer_) {
                timer_->expires_at(deadline);
            }
        }

        // copy ctor
        expiration(const expiration& other) = default;
        // move ctor
        expiration(expiration&& other) noexcept
        {
            *this = std::move(other);
        }
        // copy assignment
        inline expiration& operator=(const expiration& other) = default;
        // move assignment
        inline expiration& operator=(expiration&& other) noexcept
        {
            timer_ = std::exchange(other.timer_, nullptr);
            return *this;
        }

        ~expiration()
        {
            if (timer_) {
                timer_->expires_from_now(kInfiniteDuration);
            }
        }

    private:
        timer_type* timer_;
    };

    /// Default constructor.
    timed_base()
        : deadline_fired_(false)
        , io_service_(std::make_unique<boost::asio::io_service>())
        , timer_(std::make_unique<timer_type>(*io_service_))
    {
        // No deadline is required until the first operation is started. We
        // set the deadline to positive infinity so the actor takes no action
        // until a specific deadline is set.
        timer_->expires_from_now(kInfiniteDuration);
        post_deadline();
    }

    /// Copy constructor is not permitted.
    timed_base(const timed_base<Clock>& other) = delete;
    /// Copy assignment is not permitted.
    timed_base<Clock>& operator=(const timed_base<Clock>& other) = delete;
    /// Move constructor.
    timed_base(timed_base<Clock>&& other) = default;
    /// Move assignment.
    timed_base& operator=(timed_base<Clock>&& other) = default;

    /// Destructor.
    virtual ~timed_base()
    {
        if (io_service_) {
            io_service_->stop();
        }
    }

    /// Non-const accessor to owned boost::asio::io_service.
    inline boost::asio::io_service& get_io_service()
    {
        return *io_service_;
    }
    /// Const accessor to owned boost::asio::io_service.
    inline const boost::asio::io_service& get_io_service() const
    {
        return *io_service_;
    }

    /**
     * Setup expiration and return its' handler.
     *
     * @tparam Time Type of @p timeout_or_deadline, either time_duration_type or time_point_type.
     * @param[in] timeout_or_deadline Expiration time-point.
     *
     * @returns expiration RAII-complaint expiration handler - reset timer on destruction
     */
    template <typename Time>
    expiration scope_expire(const Time& timeout_or_deadline)
    {
        deadline_fired_ = false;
        return expiration(timer_.get(), timeout_or_deadline);
    }

protected:
    /// To-be implemented timer handler. Called if timer goes past expiration point.
    virtual void deadline_actor() = 0;

    bool deadline_fired_; ///< Set before deadline_actor() called

private:
    inline void post_deadline()
    {
        timer_->async_wait([this](const boost::system::error_code& ec) { this->check_deadline(ec); });
    }

    void check_deadline(const boost::system::error_code& /*ec*/)
    {
        if (timer_->expires_at() <= clock_type::now()) {
            deadline_actor();
            deadline_fired_ = true;
            timer_->expires_from_now(kInfiniteDuration);
        }
        // if deadline fired or reset to new one, check_deadline() gets reposted
        post_deadline();
    }

    std::unique_ptr<boost::asio::io_service> io_service_; ///< Movable boost::asio::io_service handler
    std::unique_ptr<timer_type> timer_; ///< Movable boost::asio::basic_waitable_timer handler
};

template <typename Clock>
constexpr typename timed_base<Clock>::time_duration_type timed_base<Clock>::kInfiniteDuration;
template <typename Clock>
constexpr typename timed_base<Clock>::time_duration_type timed_base<Clock>::kZeroDuration;
template <typename Clock>
constexpr typename timed_base<Clock>::time_duration_type timed_base<Clock>::kDurationResolution;
template <typename Clock>
constexpr typename timed_base<Clock>::time_duration_type timed_base<Clock>::kMinTimeout;

//! timer based on wall clock time from the system-wide realtime clock.
using system_timed_base = timed_base<std::chrono::system_clock>;
//! timer based on monotonic clock that will never be adjusted.
using steady_timed_base = timed_base<std::chrono::steady_clock>;
//! timer based on the clock with the shortest tick period available
using high_resolution_timed_base = timed_base<std::chrono::high_resolution_clock>;

} // namespace detail
} // namespace stream_client
