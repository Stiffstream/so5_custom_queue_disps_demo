#pragma once

#include <custom_queue_disps/demand_queue.hpp>

namespace custom_queue_disps
{

namespace one_thread
{

namespace impl
{

class dispatcher_t;

using dispatcher_shptr_t = std::shared_ptr< dispatcher_t >;

class dispatcher_handle_maker_t;

} /* namespace impl */

//
// dispatcher_handle_t
//

/*!
 * A class that can be seen as a smart pointer to a dispatcher instance.
 *
 * While there is at least one non-empty dispatcher_handle the dispatcher
 * will be alive.
 *
 * @note
 * Dispatcher binders created by binder() method also have a shared_ptr
 * that references the dispatcher instance. So the dispatcher will be
 * stopped and destroyed only when all dispatcher_handle and binders
 * are gone.
 */
class [[nodiscard]] dispatcher_handle_t
   {
      friend class impl::dispatcher_handle_maker_t;

      impl::dispatcher_shptr_t m_disp;

      dispatcher_handle_t( impl::dispatcher_shptr_t disp );

      [[nodiscard]]
      bool
      empty() const noexcept;

   public :
      dispatcher_handle_t() noexcept = default;

      /*!
       * Creates and returns a binder that will use @a demand_queue
       * for agents bound via that binder.
       *
       * The @a demand_queue should be created by a user. Something
       * like that:
       * @code
       * class my_queue final : public custom_queue_disps::demand_queue_t {
       *    ...
       * };
       * ...
       * auto disp = custom_queue_disps::one_thread::make_dispatcher(env);
       * ...
       * auto binder = disp.binder(std::make_shared<my_queue>(...));
       * coop.make_agent_with_binder<some_agent>(binder, ...);
       * coop.make_agent_with_binder<another_agent>(binder, ...);
       * @endcode
       *
       * The same @a demand_queue can be used for the creation of
       * several binders (maybe a user finds some sence in that action).
       * But all those binders should be created by the same
       * dispatcher_handle.
       */
      [[nodiscard]]
      so_5::disp_binder_shptr_t
      binder( demand_queue_shptr_t demand_queue ) const;

      /*!
       * Returns true if dispatcher_handler is not empty and holds
       * a reference to the dispatcher.
       */
      [[nodiscard]]
      operator bool() const noexcept { return !empty(); }

      /*!
       * Returns true if dispatcher_handler is empty and doesn't hold
       * a reference to the dispatcher.
       */
      [[nodiscard]]
      bool
      operator!() const noexcept { return empty(); }

      /*!
       * If dispatcher_handler is not empty then removes a reference
       * and make the dispatcher_handler empty. The dispatcher can be
       * destroyed after that action.
       *
       * Does nothing is dispatcher_handler is already empty.
       */
      void
      reset() noexcept;
   };

//
// make_dispatcher
//
/*!
 * Creates and returns a new instance of one_thread dispatcher.
 *
 * Usage example:
 * @code
 * so_5::environment_t & env = ...;
 * env.introduce_coop([](so_5::coop_t & coop) {
 *    auto disp = custom_queue_disps::one_thread::make_dispatcher(coop.environment());
 *    coop.make_agent_with_binder<some_agent>(
 *       disp.binder(std::make_shared<my_queue>(...)),
 *       ...);
 * });
 * @endcode
 */
[[nodiscard]]
dispatcher_handle_t
make_dispatcher(
   so_5::environment_t & env );

} /* namespace one_thread */

} /* namespace custom_queue_disps */

