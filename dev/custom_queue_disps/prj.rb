require 'rubygems'

gem 'Mxx_ru', '>= 1.3.0'

require 'mxx_ru/cpp'

MxxRu::Cpp::lib_target {

  target 'lib/custom_queue_disps'

  required_prj 'so_5/prj_s.rb'

  cpp_source 'one_thread.cpp'
}

