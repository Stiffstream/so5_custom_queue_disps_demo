#include <custom_queue_disps/one_thread.hpp>

#include <optional>

namespace custom_queue_disps
{

namespace one_thread
{

namespace impl
{

//
// dispatcher_data_t
//
/*!
 * Dispatcher's internal data.
 *
 * Described separately because this data has to be shared between
 * dispatcher and actual_event_queue.
 */
struct dispatcher_data_t
   {
      std::mutex m_lock;
      std::condition_variable m_wakeup_cv;

      bool m_shutdown{ false };

      /*!
       * The head of queue of non-empty subqueues.
       *
       * nullptr means that there is no non-empty subqueues.
       */
      demand_queue_t * m_head{ nullptr };
      /*!
       * The tail of queue of non-empty subqueues.
       *
       * It is used for quick addition of new subqueue to the
       * list of non-empty subqueues.
       */
      demand_queue_t * m_tail{ nullptr };
   };

using dispatcher_data_shptr_t =
      std::shared_ptr< dispatcher_data_t >;

//
// actual_event_queue_t
//
/*!
 * An implementation of SObjectizer's event_queue interface.
 *
 * Performs addition of a new demand to demand_queue provided by a user.
 * If demand_queue was empty before the addition then includes this
 * demand_queue into dispatcher's list of non-empty subqueues and
 * wakes the dispatcher up.
 */
class actual_event_queue_t final : public so_5::event_queue_t
   {
      demand_queue_shptr_t m_demand_queue;
      dispatcher_data_shptr_t m_disp_data;

   public:
      actual_event_queue_t(
         demand_queue_shptr_t demand_queue,
         dispatcher_data_shptr_t disp_data ) noexcept
         :  m_demand_queue{ std::move(demand_queue) }
         ,  m_disp_data{ std::move(disp_data) }
         {}

      void
      push( so_5::execution_demand_t demand ) override
         {
            std::lock_guard< std::mutex > lock{ m_disp_data->m_lock };

            auto & q = *m_demand_queue;
            const bool queue_was_empty = q.empty();
            q.push( std::move(demand) );

            if( queue_was_empty )
               {
                  // The following block of code shouldn't throw.
                  [&]() noexcept {
                     const bool disp_was_sleeping =
                           (nullptr == m_disp_data->m_head);

                     if( disp_was_sleeping )
                        {
                           m_disp_data->m_head = m_disp_data->m_tail = &q;

                           m_disp_data->m_wakeup_cv.notify_one();
                        }
                     else
                        {
                           m_disp_data->m_tail->set_next( &q );
                           m_disp_data->m_tail = &q;
                        }
                  }();
               }

            //NOTE: if the queue wasn't empty it is already in active queue.
            //So there is no need to modity active queue.
         }
   };

//
// actual_disp_binder_t
//
/*!
 * An implementation of SObjectizer's disp_binder interface.
 *
 * Holds an actual_event_queue. It's safe because binder will outlive
 * all agents that was bound via that binder.
 *
 * The only bind() method has an actual implementation, all other
 * inherited methods are left empty intentionally.
 */
class actual_disp_binder_t final : public so_5::disp_binder_t
   {
      actual_event_queue_t m_event_queue;

   public:
      actual_disp_binder_t(
         demand_queue_shptr_t demand_queue,
         dispatcher_data_shptr_t disp_data ) noexcept
         :  m_event_queue{ std::move(demand_queue), std::move(disp_data) }
         {}

      void
      preallocate_resources(
         so_5::agent_t & /*agent*/ ) override
         {}

      void
      undo_preallocation(
         so_5::agent_t & /*agent*/ ) noexcept override
         {}

      void
      bind(
         so_5::agent_t & agent ) noexcept override
         {
            agent.so_bind_to_dispatcher( m_event_queue );
         }

      void
      unbind(
         so_5::agent_t & /*agent*/ ) noexcept override
         {}
   };

//
// dispatcher_t
//
/*!
 * The actual implementation of one_thread dispatcher.
 *
 * The dispatcher uses a separate thread for serving demands of
 * agents bound to the dispatcher.
 *
 * The dispatcher starts its work in the constructor and finishes
 * it in the destructor.
 */
class dispatcher_t final
   :  public std::enable_shared_from_this< dispatcher_t >
   {
      dispatcher_data_t m_disp_data;

      std::thread m_worker_thread;

      void
      thread_body() noexcept
         {
            const auto thread_id = so_5::query_current_thread_id();
            for(;;)
               {
                  std::unique_lock< std::mutex > lock{ m_disp_data.m_lock };
                  if( m_disp_data.m_shutdown )
                     return;

                  try_extract_and_execute_one_demand(
                        thread_id,
                        std::move(lock) );
               }
         }

      void
      try_extract_and_execute_one_demand(
         so_5::current_thread_id_t thread_id,
         std::unique_lock< std::mutex > unique_lock ) noexcept
         {
            auto demand = try_extract_demand_to_execute();
            if( !demand )
               {
                  m_disp_data.m_wakeup_cv.wait( unique_lock );
               }
            else
               {
                  // Demand should be executed with unblocked
                  // dispatcher's lock.
                  unique_lock.unlock();
                  demand->call_handler( thread_id );
               }
         }

      [[nodiscard]]
      std::optional< so_5::execution_demand_t >
      try_extract_demand_to_execute() noexcept
         {
            std::optional< so_5::execution_demand_t > result;

            if( !m_disp_data.m_head )
               return result;

            auto * dq = m_disp_data.m_head;
            m_disp_data.m_head = dq->next();
            dq->drop_next();

            if( !m_disp_data.m_head )
               m_disp_data.m_tail = nullptr;

            result = dq->try_extract();

            if( !dq->empty() )
               {
                  // The current demand queue is not empty yet.
                  // So it should be returned to the active queue.
                  if( m_disp_data.m_tail )
                     {
                        m_disp_data.m_tail->set_next( dq );
                     }
                  else
                     {
                        m_disp_data.m_head = m_disp_data.m_tail = dq;
                     }
               }

            return result;
         }

   public:
      dispatcher_t()
         {
            m_worker_thread = std::thread{ [this]{ thread_body(); } };
         }
      ~dispatcher_t()
         {
            {
               std::lock_guard< std::mutex > lock{ m_disp_data.m_lock };
               m_disp_data.m_shutdown = true;
               m_disp_data.m_wakeup_cv.notify_one();
            }
            m_worker_thread.join();
         }

      [[nodiscard]]
      so_5::disp_binder_shptr_t
      make_disp_binder(
         demand_queue_shptr_t demand_queue )
         {
            return std::make_shared< actual_disp_binder_t >(
                  std::move(demand_queue),
                  dispatcher_data_shptr_t{
                        shared_from_this(),
                        &m_disp_data
                  } );
         }
   };

//
// dispatcher_handle_maker_t
//
class dispatcher_handle_maker_t
   {
   public :
      static dispatcher_handle_t
      make( dispatcher_shptr_t disp ) noexcept
         {
            return { std::move(disp) };
         }
   };

} /* namespace impl */

//
// dispatcher_handle_t
//

dispatcher_handle_t::dispatcher_handle_t(
   impl::dispatcher_shptr_t disp )
   :  m_disp{ disp }
   {}

bool
dispatcher_handle_t::empty() const noexcept
   {
      return nullptr != m_disp.get();
   }

so_5::disp_binder_shptr_t
dispatcher_handle_t::binder( demand_queue_shptr_t demand_queue ) const
   {
      if( !m_disp )
         throw std::runtime_error( "empty dispatcher_handle" );

      return m_disp->make_disp_binder( std::move(demand_queue) );
   }

void
dispatcher_handle_t::reset() noexcept
   {
      m_disp.reset();
   }

//
// make_dispatcher
//
dispatcher_handle_t
make_dispatcher(
   // NOTE: SOEnv is not used at that moment.
   so_5::environment_t & /*env*/ )
   {
      return impl::dispatcher_handle_maker_t::make(
            std::make_shared< impl::dispatcher_t >() );
   }

} /* namespace one_thread */

} /* namespace custom_queue_disps */

