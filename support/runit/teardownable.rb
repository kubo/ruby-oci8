require 'runit/method_mappable'
module RUNIT
  module Teardownable
    include MethodMappable
    @@teardowns = {}
    def attach_teardown(teardown_method, *methods)
      attach_method(@@teardowns, teardown_method, *methods)
    end
    private :attach_teardown
    def invoke_teardown(m)
      invoke_method(@@teardowns, m)
    end
    private :invoke_teardown
  end
end
      
