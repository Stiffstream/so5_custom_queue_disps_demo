require 'mxx_ru/cpp'

MxxRu::Cpp::exe_target {

  target 'demo_app'

  required_prj 'custom_queue_disps/prj.rb'
  required_prj 'so_5/prj_s.rb'

  cpp_source 'main.cpp'
}
