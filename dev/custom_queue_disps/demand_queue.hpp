#pragma once

#include <so_5/all.hpp>

#include <optional>

namespace custom_queue_disps
{

//! A base class for all custom queues.
/*!
 * A user has to inherit from that class and make actual implementations
 * for empty(), try_extract() and push() methods.
 *
 * @note
 * A dispatcher guarantes that an instance of custom queue is protected
 * from multithreaded access when methods empty(), try_extract() and
 * push(). But if a user want to store some additional information
 * inside a queue and that information has to be modified concurrently
 * from outside of empty()/try_extract()/push() methods, then that
 * information should somehow be protected by a user.
 */
class demand_queue_t
   {
      //! The next item in the queue of not-empty agents' queues.
      //! Will be
      demand_queue_t * m_next{ nullptr };

   public:
      demand_queue_t() = default;
      virtual ~demand_queue_t() = default;

      [[nodiscard]]
      demand_queue_t *
      next() const noexcept { return m_next; }

      void
      set_next( demand_queue_t * q ) noexcept { m_next = q; }

      void
      drop_next() noexcept { set_next( nullptr ); }

      /*!
       * Should return false if the queue is empty.
       */
      [[nodiscard]]
      virtual bool
      empty() const noexcept = 0;

      /*!
       * Should return empty std::optional if there is no items
       * ready to process.
       *
       * @note
       * This is a possible scenario:
       * @code
       * demand_queue_t & q = ...;
       * if(!q.empty()) {
       *    // The queue is not empty, try to handle a demand from it.
       *    auto opt_demand = q.try_extract();
       *    if(opt_demand) {
       *       // There is an actual demand to process.
       *       ...
       *    }
       * }
       * @endcode
       * This scenario can be seen if there is, for example, a max waiting
       * time for a demand. If a demand waits more than the time specified
       * it should be ignored. In that case the queue can contain a demand, 
       * but waiting time for that demand will be checked in try_extract()
       * and, if the waiting time is too long, try_extract() should return
       * an empty std::optional.
       */
      [[nodiscard]]
      virtual std::optional<so_5::execution_demand_t>
      try_extract() noexcept = 0;

      /*!
       * Should store a @a demand in the queue or throw an exception
       * if this is impossible.
       */
      virtual void
      push( so_5::execution_demand_t demand ) = 0;
   };

/*!
 * A shorthand for shared_ptr for demand_queue.
 */
using demand_queue_shptr_t = std::shared_ptr< demand_queue_t >;

} /* namespace custom_queue_disps */

