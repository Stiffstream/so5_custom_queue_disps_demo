#include <custom_queue_disps/one_thread.hpp>

#include <so_5/all.hpp>

#include <queue>

namespace demo
{

//
// demo_agent_t
//
class demo_agent_t final : public so_5::agent_t
   {
   public:
      struct hello final : public so_5::signal_t {};
      struct bye final : public so_5::signal_t {};
      struct complete final : public so_5::signal_t {};

      demo_agent_t( context_t ctx, std::string name )
         :  so_5::agent_t{ std::move(ctx) }
         ,  m_name{ std::move(name) }
         {}

      void
      so_define_agent() override
         {
            so_subscribe_self()
               .event( &demo_agent_t::on_hello )
               .event( &demo_agent_t::on_bye )
               .event( &demo_agent_t::on_complete );
         }

      void
      so_evt_start() override
         {
            std::cout << m_name << ": started" << std::endl;

            so_5::send< hello >( *this );
            so_5::send< bye >( *this );
            so_5::send< complete >( *this );
         }

      void
      so_evt_finish() override
         {
            std::cout << m_name << ": finished" << std::endl;
         }

   private:
      const std::string m_name;

      void
      on_hello( mhood_t<hello> )
         {
            std::cout << m_name << ": hello" << std::endl;
         }

      void
      on_bye( mhood_t<bye> )
         {
            std::cout << m_name << ": bye" << std::endl;
         }

      void
      on_complete( mhood_t<complete> )
         {
            std::cout << m_name << ": complete" << std::endl;

            so_deregister_agent_coop_normally();
         }
   };

//
// simple_fifo_t
//
class simple_fifo_t final : public custom_queue_disps::demand_queue_t
   {
      std::queue< so_5::execution_demand_t > m_queue;

   public:
      simple_fifo_t() = default;

      [[nodiscard]]
      bool
      empty() const noexcept override { return m_queue.empty(); }

      [[nodiscard]]
      std::optional<so_5::execution_demand_t>
      try_extract() noexcept override
         {
            std::optional<so_5::execution_demand_t> result{
               std::move(m_queue.front())
            };
            m_queue.pop();

            return result;
         }

      void
      push( so_5::execution_demand_t demand ) override
         {
            m_queue.push( std::move(demand) );
         }
   };

//
// hardcoded_priorities_t
//
class hardcoded_priorities_t final : public custom_queue_disps::demand_queue_t
   {
      using priority_t = std::uint_fast8_t;

      static constexpr priority_t lowest{ 0u };
      static constexpr priority_t low{ 1u };
      static constexpr priority_t normal{ 2u };
      static constexpr priority_t high{ 3u };
      static constexpr priority_t highest{ 4u };

      [[nodiscard]]
      static priority_t
      detect_priority( const so_5::execution_demand_t & d ) noexcept
         {
            if( so_5::agent_t::get_demand_handler_on_start_ptr()
                  == d.m_demand_handler )
               return highest;

            if( so_5::agent_t::get_demand_handler_on_finish_ptr()
                  == d.m_demand_handler )
               return lowest;

            if( std::type_index{ typeid(demo_agent_t::bye) } == d.m_msg_type )
               return high;

            if( std::type_index{ typeid(demo_agent_t::hello) } == d.m_msg_type )
               return low;

            return normal;
         }

      struct actual_demand_t
         {
            so_5::execution_demand_t m_demand;
            priority_t m_priority;

            actual_demand_t(
               so_5::execution_demand_t demand,
               priority_t priority )
               :  m_demand{ std::move(demand) }
               ,  m_priority{ priority }
               {}

            [[nodiscard]]
            bool
            operator<( const actual_demand_t & o ) const noexcept
               {
                  return m_priority < o.m_priority;
               }
         };

      std::priority_queue< actual_demand_t > m_queue;

   public:
      hardcoded_priorities_t() = default;

      [[nodiscard]]
      bool
      empty() const noexcept override { return m_queue.empty(); }

      [[nodiscard]]
      std::optional<so_5::execution_demand_t>
      try_extract() noexcept override
         {
            std::optional<so_5::execution_demand_t> result{
               m_queue.top().m_demand
            };
            m_queue.pop();

            return result;
         }

      void
      push( so_5::execution_demand_t demand ) override
         {
            const auto prio = detect_priority( demand );
            m_queue.emplace( std::move(demand), prio );
         }
   };

//
// dynamic_per_agent_priorities_t
//

class dynamic_per_agent_priorities_t final
   : public custom_queue_disps::demand_queue_t
   {
   public:
      using priority_t = std::uint_fast8_t;

      static constexpr priority_t lowest{ 0u };
      static constexpr priority_t low{ 1u };
      static constexpr priority_t normal{ 2u };
      static constexpr priority_t high{ 3u };
      static constexpr priority_t highest{ 4u };

   private:
      // Type of map from message to a priority.
      using type_to_prio_map_t = std::map< std::type_index, priority_t >;

      // Type of map from agent's pointer to a map of message priorities.
      using agent_to_prio_map_t =
            std::map< so_5::agent_t *, type_to_prio_map_t >;

      // Type of demand to be stored in the priority queue.
      struct actual_demand_t
         {
            so_5::execution_demand_t m_demand;
            priority_t m_priority;

            actual_demand_t(
               so_5::execution_demand_t demand,
               priority_t priority )
               :  m_demand{ std::move(demand) }
               ,  m_priority{ priority }
               {}

            [[nodiscard]]
            bool
            operator<( const actual_demand_t & o ) const noexcept
               {
                  return m_priority < o.m_priority;
               }
         };

      // The lock for priorities map.
      std::mutex m_prio_map_lock;
      // Container of agent's priorities.
      agent_to_prio_map_t m_agent_prios;

      // Queue of pending priorities.
      std::priority_queue< actual_demand_t > m_queue;

      // Not only detects a priority but also removes priorities
      // for an agent if `d` is the final demand for that agent.
      [[nodiscard]]
      priority_t
      handle_new_demand_priority( const so_5::execution_demand_t & d ) noexcept
         {
            if( so_5::agent_t::get_demand_handler_on_start_ptr()
                  == d.m_demand_handler )
               return highest;

            if( so_5::agent_t::get_demand_handler_on_finish_ptr()
                  == d.m_demand_handler )
               {
                  // There is no more need for priorities for that agent.
                  std::lock_guard< std::mutex > lock{ m_prio_map_lock };
                  m_agent_prios.erase( d.m_receiver );
                  return lowest;
               }

            {
               // We have to search priority for the message for that agent.
               std::lock_guard< std::mutex > lock{ m_prio_map_lock };
               auto it_agent = m_agent_prios.find( d.m_receiver );
               if( it_agent != m_agent_prios.end() )
                  {
                     auto it_msg = it_agent->second.find( d.m_msg_type );
                     if( it_msg != it_agent->second.end() )
                        return it_msg->second;
                  }
            }

            return normal;
         }

   public:
      dynamic_per_agent_priorities_t() = default;

      void
      define_priority(
         so_5::agent_t * receiver,
         std::type_index msg_type,
         priority_t priority )
         {
            std::lock_guard< std::mutex > lock{ m_prio_map_lock };

            m_agent_prios[ receiver ][ msg_type ] = priority;
         }

      [[nodiscard]]
      bool
      empty() const noexcept override { return m_queue.empty(); }

      [[nodiscard]]
      std::optional<so_5::execution_demand_t>
      try_extract() noexcept override
         {
            std::optional<so_5::execution_demand_t> result{
               m_queue.top().m_demand
            };
            m_queue.pop();

            return result;
         }

      void
      push( so_5::execution_demand_t demand ) override
         {
            const auto prio = handle_new_demand_priority( demand );
            m_queue.emplace( std::move(demand), prio );
         }
   };

void
demo_with_simple_fifo()
   {
      constexpr std::string_view demo_name{ "simple_fifo" };
      std::cout << "=== " << demo_name << " started ===" << std::endl;

      so_5::launch( [](so_5::environment_t & env) {
         env.introduce_coop( [](so_5::coop_t & coop) {
            auto disp = custom_queue_disps::one_thread::make_dispatcher(
                  coop.environment() );
            coop.make_agent_with_binder<demo_agent_t>(
                  disp.binder( std::make_shared<simple_fifo_t>() ),
                  "Alice" );
         } );
      } );

      std::cout << "=== " << demo_name << " finished ===" << std::endl;
   }

void
demo_with_hardcoded_priorities()
   {
      constexpr std::string_view demo_name{ "hardcoded_priorities" };
      std::cout << "=== " << demo_name << " started ===" << std::endl;

      so_5::launch( [](so_5::environment_t & env) {
         env.introduce_coop( [](so_5::coop_t & coop) {
            auto disp = custom_queue_disps::one_thread::make_dispatcher(
                  coop.environment() );
            coop.make_agent_with_binder<demo_agent_t>(
                  disp.binder( std::make_shared<hardcoded_priorities_t>() ),
                  "Alice" );
         } );
      } );

      std::cout << "=== " << demo_name << " finished ===" << std::endl;
   }

void
demo_with_dynamic_per_agent_priorities()
   {
      constexpr std::string_view demo_name{ "dynamic_per_agent_priorities" };
      std::cout << "=== " << demo_name << " started ===" << std::endl;

      so_5::launch( [](so_5::environment_t & env) {
         env.introduce_coop( [](so_5::coop_t & coop) {
            auto queue = std::make_shared<dynamic_per_agent_priorities_t>();

            auto binder = custom_queue_disps::one_thread::make_dispatcher(
                  coop.environment() ).binder( queue );

            auto * alice = coop.make_agent_with_binder<demo_agent_t>(
                  binder, "Alice" );
            auto * bob = coop.make_agent_with_binder<demo_agent_t>(
                  binder, "Bob" );

            // Custom priorities should be defined before the completion
            // of registration procedure.
            queue->define_priority( alice,
                  typeid(demo_agent_t::hello),
                  dynamic_per_agent_priorities_t::low );
            queue->define_priority( alice,
                  typeid(demo_agent_t::bye),
                  dynamic_per_agent_priorities_t::high );

            queue->define_priority( bob,
                  typeid(demo_agent_t::hello),
                  dynamic_per_agent_priorities_t::high );
            queue->define_priority( bob,
                  typeid(demo_agent_t::bye),
                  dynamic_per_agent_priorities_t::low );
         } );
      } );

      std::cout << "=== " << demo_name << " finished ===" << std::endl;
   }

} /* namespace demo */

int main()
   {
      demo::demo_with_simple_fifo();
      demo::demo_with_hardcoded_priorities();
      demo::demo_with_dynamic_per_agent_priorities();

      return 0;
   }

